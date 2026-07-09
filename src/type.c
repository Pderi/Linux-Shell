#include "type.h"

#include <stdio.h>
#include <string.h>

static const char *builtin_table[] = {
    "cd", "echo", "pwd", "ls", "cat", "grep",
    "type", "history", "alias", "unalias", "exit", NULL
};

const char **builtin_names(void)
{
    return builtin_table;
}

int is_builtin(const char *name)
{
    if (!name)
        return 0;
    for (int i = 0; builtin_table[i]; i++) {
        if (strcmp(builtin_table[i], name) == 0)
            return 1;
    }
    return 0;
}

int builtin_type(Command *cmd)
{
    if (cmd->argc < 2) {
        fprintf(stderr, "type: missing argument\n");
        return 1;
    }

    const char *name = cmd->argv[1];
    if (is_builtin(name)) {
        printf("%s is a shell builtin\n", name);
        return 0;
    }

    printf("%s: not found\n", name);
    return 1;
}
