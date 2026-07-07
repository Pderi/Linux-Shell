#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <readline/history.h>
#include "history.h"

extern History g_history;

static int g_hist_limit = HISTSIZE;

static char *history_path(void)
{
    const char *home = getenv("HOME");
    if (!home)
        return NULL;
    size_t n = strlen(home) + strlen("/.myshell_history") + 1;
    char *path = malloc(n);
    if (!path)
        return NULL;
    snprintf(path, n, "%s/.myshell_history", home);
    return path;
}

void history_init(History *hist)
{
    hist->count = 0;
    hist->start = 0;
    for (int i = 0; i < HISTSIZE; i++)
        hist->entries[i] = NULL;

    const char *env = getenv("HISTSIZE");
    if (env) {
        int n = atoi(env);
        if (n > 0 && n <= HISTSIZE)
            g_hist_limit = n;
    }
}

int history_limit(void)
{
    return g_hist_limit;
}

void history_add(History *hist, const char *line)
{
    if (!line || !*line)
        return;

    if (hist->count > 0) {
        int last = (hist->start + hist->count - 1) % HISTSIZE;
        if (hist->entries[last] && strcmp(hist->entries[last], line) == 0)
            return;
    }

    if (hist->count == g_hist_limit) {
        char *dup = strdup(line);
        if (!dup)
            return;
        free(hist->entries[hist->start]);
        hist->entries[hist->start] = dup;
        hist->start = (hist->start + 1) % HISTSIZE;
        add_history(line);
        return;
    }

    int idx = (hist->start + hist->count) % HISTSIZE;
    hist->entries[idx] = strdup(line);
    if (hist->entries[idx]) {
        hist->count++;
        add_history(line);
    }
}

void history_sync_readline(const History *hist)
{
    clear_history();
    for (int i = 0; i < hist->count; i++) {
        int idx = (hist->start + i) % HISTSIZE;
        add_history(hist->entries[idx]);
    }
}

void history_print(const History *hist, int n)
{
    int total = hist->count;
    int show = n > 0 && n < total ? n : total;
    int begin = total - show;

    for (int i = begin; i < total; i++) {
        int idx = (hist->start + i) % HISTSIZE;
        printf("%5d  %s\n", i + 1, hist->entries[idx]);
    }
}

void history_load(History *hist)
{
    char *path = history_path();
    if (!path)
        return;

    FILE *fp = fopen(path, "r");
    free(path);
    if (!fp)
        return;

    char *line = NULL;
    size_t cap = 0;
    while (getline(&line, &cap, fp) != -1) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        history_add(hist, line);
    }
    free(line);
    fclose(fp);
}

void history_save(const History *hist)
{
    char *path = history_path();
    if (!path)
        return;

    FILE *fp = fopen(path, "w");
    free(path);
    if (!fp)
        return;

    for (int i = 0; i < hist->count; i++) {
        int idx = (hist->start + i) % HISTSIZE;
        fprintf(fp, "%s\n", hist->entries[idx]);
    }
    fclose(fp);
}

void history_free(History *hist)
{
    for (int i = 0; i < HISTSIZE; i++)
        free(hist->entries[i]);
    hist->count = 0;
    hist->start = 0;
}

int builtin_history(Command *cmd)
{
    int n = 0;
    if (cmd->argc > 1)
        n = atoi(cmd->argv[1]);
    history_print(&g_history, n);
    return 0;
}
