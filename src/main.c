#include "alias.h"
#include "completion.h"
#include "history.h"
#include "input.h"
#include "parser.h"
#include "shell.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

History g_history;

static void setup_signals(void)
{
    signal(SIGCHLD, SIG_IGN);
}

static void trim_inplace(char *s)
{
    if (!s)
        return;
    char *p = s;
    while (*p && (*p == ' ' || *p == '\t'))
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == ' ' || s[len - 1] == '\t'))
        s[--len] = '\0';
}

static int process_command(const char *line)
{
    if (!line || !*line)
        return 0;

    char *expanded = alias_expand_line(line);
    if (!expanded) {
        fprintf(stderr, "myshell: out of memory\n");
        return 0;
    }

    Pipeline pl;
    memset(&pl, 0, sizeof(pl));
    if (parse_line(expanded, &pl) < 0) {
        free(expanded);
        return 0;
    }

    history_add(&g_history, line);
    shell_execute(&pl);
    parser_free_pipeline(&pl);
    free(expanded);
    return shell_should_exit();
}

int main(void)
{
    setup_signals();
    history_init(&g_history);
    history_load(&g_history);
    history_sync_readline(&g_history);
    alias_init();
    completion_init();

    for (;;) {
        char *raw = input_read_line();
        if (!raw)
            break;

        trim_inplace(raw);
        if (*raw && process_command(raw)) {
            free(raw);
            break;
        }
        free(raw);
    }

    history_save(&g_history);
    history_free(&g_history);
    alias_free_all();
    if (isatty(STDIN_FILENO))
        puts("");
    return 0;
}
