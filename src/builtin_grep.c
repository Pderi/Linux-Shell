#include "builtins.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int strcase_contains(const char *hay, const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen == 0)
        return 1;

    for (const char *p = hay; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i] && tolower((unsigned char)p[i]) ==
                                  tolower((unsigned char)needle[i]))
            i++;
        if (i == nlen)
            return 1;
    }
    return 0;
}

static int grep_stream(FILE *fp, const char *pattern, int ignore_case, int show_name,
                       const char *label)
{
    char *line = NULL;
    size_t cap = 0;
    ssize_t n;
    int matched = 0;

    while ((n = getline(&line, &cap, fp)) != -1) {
        if (n > 0 && line[n - 1] == '\n')
            line[n - 1] = '\0';

        int hit = ignore_case ? strcase_contains(line, pattern)
                              : (strstr(line, pattern) != NULL);
        if (!hit)
            continue;

        if (show_name)
            printf("%s:", label);
        fputs(line, stdout);
        putchar('\n');
        matched = 1;
    }

    free(line);
    return matched ? 0 : 1;
}

int builtin_grep(Command *cmd)
{
    if (cmd->argc < 2) {
        fprintf(stderr, "grep: missing pattern\n");
        return 1;
    }

    const char *pattern = cmd->argv[1];
    int ignore_case = 0;
    int first_arg = 2;

    if (pattern[0] == '-') {
        for (const char *p = pattern + 1; *p; p++) {
            if (*p == 'i')
                ignore_case = 1;
        }
        if (cmd->argc < 3) {
            fprintf(stderr, "grep: missing pattern\n");
            return 1;
        }
        pattern = cmd->argv[2];
        first_arg = 3;
    }

    if (first_arg >= cmd->argc)
        return grep_stream(stdin, pattern, ignore_case, 0, NULL);

    int rc = 1;
    int multi = cmd->argc - first_arg > 1;
    for (int i = first_arg; i < cmd->argc; i++) {
        FILE *fp = fopen(cmd->argv[i], "r");
        if (!fp) {
            perror(cmd->argv[i]);
            rc = 1;
            continue;
        }
        if (grep_stream(fp, pattern, ignore_case, multi, cmd->argv[i]) == 0)
            rc = 0;
        fclose(fp);
    }
    return rc;
}
