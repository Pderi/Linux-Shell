#include "executor.h"
#include "builtins.h"
#include "type.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int wait_pid_retry(pid_t pid, int *status)
{
    for (;;) {
        pid_t rc = waitpid(pid, status, 0);
        if (rc > 0)
            return 0;
        if (rc == 0)
            return -1;
        if (errno == EINTR)
            continue;
        return -1;
    }
}

static void reset_child_signals(void)
{
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
}

int executor_setup_redirection(const Command *cmd)
{
    if (cmd->in_file) {
        int fd = open(cmd->in_file, O_RDONLY);
        if (fd < 0) {
            perror(cmd->in_file);
            return -1;
        }
        if (dup2(fd, STDIN_FILENO) < 0) {
            perror("dup2");
            close(fd);
            return -1;
        }
        close(fd);
    }

    if (cmd->out_file) {
        int flags = O_WRONLY | O_CREAT | (cmd->append ? O_APPEND : O_TRUNC);
        int fd = open(cmd->out_file, flags, 0644);
        if (fd < 0) {
            perror(cmd->out_file);
            return -1;
        }
        if (dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2");
            close(fd);
            return -1;
        }
        close(fd);
    }

    return 0;
}

static void close_pipe_fds(int pipes[][2], int n)
{
    for (int i = 0; i < n; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

static void run_command_child(Command *cmd)
{
    if (!cmd || !cmd->argv[0]) {
        fprintf(stderr, "myshell: empty command\n");
        _exit(1);
    }

    reset_child_signals();

    if (executor_setup_redirection(cmd) < 0)
        _exit(1);

    if (is_builtin(cmd->argv[0])) {
        if (strcmp(cmd->argv[0], "exit") == 0)
            _exit(0);
        int rc = run_builtin(cmd);
        fflush(stdout);
        fflush(stderr);
        _exit(rc);
    }

    execvp(cmd->argv[0], cmd->argv);
    fprintf(stderr, "myshell: %s: command not found\n", cmd->argv[0]);
    _exit(127);
}

static int wait_pids(pid_t *pids, int n)
{
    int rc = 0;

    for (int i = 0; i < n; i++) {
        int status = 0;
        if (wait_pid_retry(pids[i], &status) < 0)
            continue;
        if (i != n - 1)
            continue;
        if (WIFEXITED(status))
            rc = WEXITSTATUS(status);
        else
            rc = 1;
    }
    return rc;
}

static void kill_pids(pid_t *pids, int n)
{
    for (int i = 0; i < n; i++)
        kill(pids[i], SIGTERM);
    for (int i = 0; i < n; i++)
        wait_pid_retry(pids[i], NULL);
}

static int run_pipeline_children(Pipeline *pl, int pipes[][2])
{
    int n = pl->count;
    pid_t pids[MAX_PIPELINE];
    int started = 0;

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            kill_pids(pids, started);
            return -1;
        }
        if (pid == 0) {
            if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) < 0)
                _exit(1);
            if (i < n - 1 && dup2(pipes[i][1], STDOUT_FILENO) < 0)
                _exit(1);
            close_pipe_fds(pipes, n - 1);
            run_command_child(&pl->cmds[i]);
        }
        pids[i] = pid;
        started++;
    }

    close_pipe_fds(pipes, n - 1);
    return wait_pids(pids, n);
}

static int execute_single(Command *cmd, int background)
{
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0)
        run_command_child(cmd);

    if (background) {
        printf("[%d] running in background\n", (int)pid);
        fflush(stdout);
        return 0;
    }

    int status;
    if (wait_pid_retry(pid, &status) < 0) {
        perror("waitpid");
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

int executor_run_single_builtin(Command *cmd, int background)
{
    if (!cmd->argv[0])
        return 1;

    if (strcmp(cmd->argv[0], "exit") == 0)
        return run_builtin(cmd);

    if (!cmd->in_file && !cmd->out_file && !background)
        return run_builtin(cmd);

    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return 1;
    }
    if (pid == 0) {
        reset_child_signals();
        if (executor_setup_redirection(cmd) < 0)
            _exit(1);
        int rc = run_builtin(cmd);
        fflush(stdout);
        fflush(stderr);
        _exit(rc);
    }

    if (background) {
        printf("[%d] running in background\n", (int)pid);
        fflush(stdout);
        return 0;
    }

    int status;
    if (wait_pid_retry(pid, &status) < 0) {
        perror("waitpid");
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

int execute_pipeline(Pipeline *pl)
{
    if (!pl || pl->count <= 0)
        return 1;

    if (pl->count == 1)
        return execute_single(&pl->cmds[0], pl->background);

    int n = pl->count;
    int pipes[MAX_PIPELINE][2];

    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            close_pipe_fds(pipes, i);
            return 1;
        }
    }

    if (pl->background) {
        pid_t bg = fork();
        if (bg < 0) {
            perror("fork");
            close_pipe_fds(pipes, n - 1);
            return 1;
        }
        if (bg == 0) {
            int rc = run_pipeline_children(pl, pipes);
            _exit(rc < 0 ? 1 : rc);
        }

        close_pipe_fds(pipes, n - 1);
        printf("[%d] running in background\n", (int)bg);
        fflush(stdout);
        return 0;
    }

    return run_pipeline_children(pl, pipes);
}
