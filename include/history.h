#ifndef HISTORY_H
#define HISTORY_H

#include "parser.h"

#define HISTSIZE 1000

typedef struct {
    char *entries[HISTSIZE];
    int   count;
    int   start;
} History;

void history_init(History *hist);
int  history_limit(void);
void history_add(History *hist, const char *line);
void history_sync_readline(const History *hist);
void history_print(const History *hist, int n);
void history_load(History *hist);
void history_save(const History *hist);
void history_free(History *hist);
int  builtin_history(Command *cmd);

#endif
