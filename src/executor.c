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

static void sigchld_handler(int sig)
{
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
    errno = saved_errno;
}

void executor_setup_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);
}

static int wait_pid_blocking(pid_t pid, int *status)
{
    for (;;) {
        pid_t rc = waitpid(pid, status, 0);
        if (rc > 0)
            return 0;
        if (rc < 0 && errno == ECHILD) {
            if (status)
                *status = 0;
            return 0;
        }
        if (rc == 0)
            return -1;
        if (errno == EINTR)
            continue;
        return -1;
    }
}

static int wait_pid_retry(pid_t pid, int *status)
{
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &mask, &oldmask) < 0)
        return -1;

    int rc = wait_pid_blocking(pid, status);
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    return rc;
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

static int run_builtin_in_parent(Command *cmd)
{
    int saved_in = -1;
    int saved_out = -1;
    int rc;

    if (cmd->in_file)
        saved_in = dup(STDIN_FILENO);
    if (cmd->out_file)
        saved_out = dup(STDOUT_FILENO);

    if ((cmd->in_file || cmd->out_file) && executor_setup_redirection(cmd) < 0) {
        rc = 1;
        goto restore;
    }

    rc = run_builtin(cmd);
    fflush(stdout);
    fflush(stderr);

restore:
    if (saved_in >= 0) {
        dup2(saved_in, STDIN_FILENO);
        close(saved_in);
    }
    if (saved_out >= 0) {
        dup2(saved_out, STDOUT_FILENO);
        close(saved_out);
    }
    return rc;
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

    fprintf(stderr, "myshell: %s: command not found\n", cmd->argv[0]);
    _exit(127);
}

static int wait_pids(pid_t *pids, int n)
{
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &mask, &oldmask) < 0)
        return 1;

    int rc = 0;
    for (int i = 0; i < n; i++) {
        int status = 0;
        if (wait_pid_blocking(pids[i], &status) < 0)
            continue;
        if (i != n - 1)
            continue;
        if (WIFEXITED(status))
            rc = WEXITSTATUS(status);
        else
            rc = 1;
    }

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    return rc;
}

static void kill_pids(pid_t *pids, int n)
{
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);
    if (sigprocmask(SIG_BLOCK, &mask, &oldmask) < 0)
        return;

    for (int i = 0; i < n; i++)
        kill(pids[i], SIGTERM);
    for (int i = 0; i < n; i++)
        wait_pid_blocking(pids[i], NULL);

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
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
    if (!is_builtin(cmd->argv[0])) {
        fprintf(stderr, "myshell: %s: command not found\n", cmd->argv[0]);
        return 127;
    }

    if (!background && !cmd->in_file && !cmd->out_file)
        return run_builtin(cmd);

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

    int status = 0;
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

    if (!background && (cmd->in_file || cmd->out_file))
        return run_builtin_in_parent(cmd);

    if (!background && !cmd->in_file && !cmd->out_file)
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

    int status = 0;
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

    for (int i = 0; i < pl->count; i++) {
        if (!pl->cmds[i].argv[0]) {
            fprintf(stderr, "myshell: empty command in pipeline\n");
            return 1;
        }
        if (!is_builtin(pl->cmds[i].argv[0])) {
            fprintf(stderr, "myshell: %s: command not found\n", pl->cmds[i].argv[0]);
            return 127;
        }
    }

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
