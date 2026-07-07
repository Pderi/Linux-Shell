#include "type.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *builtin_table[] = {
    "cd", "echo", "type", "history", "alias", "unalias", "exit", NULL
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

char *find_external_path(const char *name)
{
    if (!name || strchr(name, '/')) {
        if (name && access(name, X_OK) == 0)
            return strdup(name);
        return NULL;
    }

    const char *path = getenv("PATH");
    if (!path)
        return NULL;

    char *copy = strdup(path);
    if (!copy)
        return NULL;

    char *save = NULL;
    for (char *dir = strtok_r(copy, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        size_t len = strlen(dir) + strlen(name) + 2;
        char *full = malloc(len);
        if (!full)
            continue;
        snprintf(full, len, "%s/%s", dir, name);
        if (access(full, X_OK) == 0) {
            free(copy);
            return full;
        }
        free(full);
    }

    free(copy);
    return NULL;
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

    char *path = find_external_path(name);
    if (path) {
        printf("%s is %s\n", name, path);
        free(path);
        return 0;
    }

    printf("%s: not found\n", name);
    return 1;
}
