#include "parser.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TOKEN 1024

static char *skip_space(char *p)
{
    while (*p && isspace((unsigned char)*p))
        p++;
    return p;
}

static void strip_background_flag(char *p, Pipeline *pl)
{
    if (!p || !*p)
        return;

    int in_quote = 0;
    char quote = 0;
    char *amp = NULL;

    for (char *q = p; *q; q++) {
        if (in_quote) {
            if (*q == quote)
                in_quote = 0;
            continue;
        }
        if (*q == '"' || *q == '\'') {
            in_quote = 1;
            quote = *q;
            continue;
        }
        if (*q == '&')
            amp = q;
    }

    if (!amp)
        return;

    for (char *q = amp + 1; *q; q++) {
        if (!isspace((unsigned char)*q))
            return;
    }

    pl->background = 1;
    *amp = '\0';
    size_t len = strlen(p);
    while (len > 0 && isspace((unsigned char)p[len - 1]))
        p[--len] = '\0';
}

static int read_next_arg(char **pp, char **out)
{
    char *p = skip_space(*pp);
    if (!*p)
        return -1;

    char buf[MAX_TOKEN];
    size_t len = 0;

    while (*p) {
        if (*p == '<' || *p == '>' || *p == '|')
            break;
        if (*p == '"' || *p == '\'') {
            char q = *p++;
            while (*p && *p != q) {
                if (len + 1 >= sizeof(buf))
                    return -1;
                buf[len++] = *p++;
            }
            if (*p != q)
                return -1;
            p++;
            continue;
        }
        if (isspace((unsigned char)*p))
            break;
        if (len + 1 >= sizeof(buf))
            return -1;
        buf[len++] = *p++;
    }

    if (len == 0)
        return -1;

    buf[len] = '\0';
    *out = strdup(buf);
    *pp = p;
    return *out ? 0 : -1;
}

static int read_redirect_path(char **pp, char **dest)
{
    return read_next_arg(pp, dest);
}

static int parse_segment(char *seg, Command *cmd)
{
    memset(cmd, 0, sizeof(*cmd));
    cmd->argv[0] = NULL;

    char *p = seg;
    while (*p) {
        p = skip_space(p);
        if (!*p)
            break;
        if (*p == '<') {
            p++;
            if (cmd->in_file)
                free(cmd->in_file);
            cmd->in_file = NULL;
            if (read_redirect_path(&p, &cmd->in_file) < 0)
                return -1;
            continue;
        }
        if (*p == '>') {
            int append = 0;
            p++;
            if (*p == '>') {
                append = 1;
                p++;
            }
            if (cmd->out_file)
                free(cmd->out_file);
            cmd->out_file = NULL;
            if (read_redirect_path(&p, &cmd->out_file) < 0)
                return -1;
            cmd->append = append;
            continue;
        }

        char *arg = NULL;
        if (read_next_arg(&p, &arg) < 0) {
            free(arg);
            return -1;
        }
        if (cmd->argc >= MAX_ARGS - 1) {
            free(arg);
            return -1;
        }
        cmd->argv[cmd->argc++] = arg;
        cmd->argv[cmd->argc] = NULL;
    }

    return cmd->argc > 0 ? 0 : -1;
}

static void free_command(Command *cmd)
{
    for (int i = 0; i < cmd->argc; i++)
        free(cmd->argv[i]);
    free(cmd->in_file);
    free(cmd->out_file);
    memset(cmd, 0, sizeof(*cmd));
}

void parser_free_pipeline(Pipeline *pl)
{
    if (!pl)
        return;
    for (int i = 0; i < pl->count; i++)
        free_command(&pl->cmds[i]);
    pl->count = 0;
    pl->background = 0;
}

int parse_line(const char *line, Pipeline *pl)
{
    if (!pl || !line)
        return -1;

    parser_free_pipeline(pl);

    char *copy = strdup(line);
    if (!copy)
        return -1;

    char *p = skip_space(copy);
    strip_background_flag(p, pl);

    size_t len = strlen(p);
    if (len == 0) {
        free(copy);
        return -1;
    }

    char *seg = p;
    while (seg && *seg) {
        if (pl->count >= MAX_PIPELINE) {
            fprintf(stderr, "myshell: pipeline too long\n");
            parser_free_pipeline(pl);
            free(copy);
            return -1;
        }

        char *bar = NULL;
        int in_quote = 0;
        char quote = 0;
        for (char *q = seg; *q; q++) {
            if (in_quote) {
                if (*q == quote)
                    in_quote = 0;
                continue;
            }
            if (*q == '"' || *q == '\'') {
                in_quote = 1;
                quote = *q;
                continue;
            }
            if (*q == '|') {
                bar = q;
                break;
            }
        }

        if (bar)
            *bar = '\0';

        seg = skip_space(seg);
        if (!*seg) {
            fprintf(stderr, "myshell: empty command in pipeline\n");
            parser_free_pipeline(pl);
            free(copy);
            return -1;
        }

        if (parse_segment(seg, &pl->cmds[pl->count]) < 0) {
            free_command(&pl->cmds[pl->count]);
            fprintf(stderr, "myshell: parse error\n");
            parser_free_pipeline(pl);
            free(copy);
            return -1;
        }
        pl->count++;

        seg = bar ? bar + 1 : NULL;
    }

    free(copy);
    return pl->count > 0 ? 0 : -1;
}
