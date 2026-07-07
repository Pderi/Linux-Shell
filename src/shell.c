#include "shell.h"
#include "builtins.h"
#include "executor.h"
#include "type.h"

#include <signal.h>

static volatile sig_atomic_t g_shell_exit = 0;

int shell_should_exit(void)
{
    return g_shell_exit != 0;
}

void shell_request_exit(void)
{
    g_shell_exit = 1;
}

int shell_execute(Pipeline *pl)
{
    if (!pl || pl->count <= 0)
        return 1;

    if (pl->count == 1 && pl->cmds[0].argv[0] &&
        is_builtin(pl->cmds[0].argv[0]))
        return executor_run_single_builtin(&pl->cmds[0], pl->background);

    return execute_pipeline(pl);
}
