#ifndef ALIAS_H
#define ALIAS_H

#include "parser.h"

typedef struct AliasNode {
    char *name;
    char *value;
    struct AliasNode *next;
} AliasNode;

void alias_init(void);
int  alias_set(const char *name, const char *value);
int  alias_unset(const char *name);
void alias_list(void);
char *alias_expand_line(const char *line);
void alias_free_all(void);
int  builtin_alias(Command *cmd);
int  builtin_unalias(Command *cmd);

#endif
