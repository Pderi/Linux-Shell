#include "builtins.h"
#include "alias.h"
#include "history.h"
#include "shell.h"
#include "type.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void expand_env_print(const char *arg)
{
    for (const char *p = arg; *p; p++) {
        if (*p != '$') {
            putchar(*p);
            continue;
        }
        p++;
        if (*p == '$') {
            putchar('$');
            continue;
        }

        char name[256];
        int i = 0;
        if (*p == '{') {
            p++;
            while (*p && *p != '}' && i < (int)sizeof(name) - 1)
                name[i++] = *p++;
            if (*p == '}')
                p++;
        } else {
            while (*p && (isalnum((unsigned char)*p) || *p == '_') &&
                   i < (int)sizeof(name) - 1)
                name[i++] = *p++;
            p--;
        }
        name[i] = '\0';

        const char *val = name[0] ? getenv(name) : NULL;
        if (val)
            fputs(val, stdout);
    }
}

static char *expand_tilde(const char *dir)
{
    if (!dir)
        return NULL;
    if (dir[0] != '~')
        return strdup(dir);

    const char *home = getenv("HOME");
    if (!home)
        return NULL;

    if (dir[1] == '\0')
        return strdup(home);

    if (dir[1] == '/') {
        size_t len = strlen(home) + strlen(dir + 1) + 1;
        char *path = malloc(len);
        if (!path)
            return NULL;
        snprintf(path, len, "%s%s", home, dir + 1);
        return path;
    }

    return strdup(dir);
}

static int builtin_cd(Command *cmd)
{
    char *expanded = NULL;
    const char *dir;

    if (cmd->argc <= 1) {
        dir = getenv("HOME");
    } else {
        expanded = expand_tilde(cmd->argv[1]);
        dir = expanded ? expanded : cmd->argv[1];
    }

    if (!dir) {
        fprintf(stderr, "cd: HOME not set\n");
        free(expanded);
        return 1;
    }
    if (chdir(dir) != 0) {
        perror("cd");
        free(expanded);
        return 1;
    }
    free(expanded);
    return 0;
}

static int builtin_echo(Command *cmd)
{
    for (int i = 1; i < cmd->argc; i++) {
        if (i > 1)
            putchar(' ');
        expand_env_print(cmd->argv[i]);
    }
    putchar('\n');
    fflush(stdout);
    return 0;
}

int run_builtin(Command *cmd)
{
    if (!cmd->argv[0])
        return 1;

    const char *name = cmd->argv[0];
    int rc = 1;

    if (strcmp(name, "cd") == 0)
        rc = builtin_cd(cmd);
    else if (strcmp(name, "echo") == 0)
        rc = builtin_echo(cmd);
    else if (strcmp(name, "type") == 0)
        rc = builtin_type(cmd);
    else if (strcmp(name, "history") == 0)
        rc = builtin_history(cmd);
    else if (strcmp(name, "alias") == 0)
        rc = builtin_alias(cmd);
    else if (strcmp(name, "unalias") == 0)
        rc = builtin_unalias(cmd);
    else if (strcmp(name, "exit") == 0) {
        shell_request_exit();
        return 0;
    } else
        return 1;

    fflush(stdout);
    fflush(stderr);
    return rc;
}
