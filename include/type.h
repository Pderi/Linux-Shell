#ifndef TYPE_H
#define TYPE_H

#include "parser.h"

int          is_builtin(const char *name);
const char **builtin_names(void);
char        *find_external_path(const char *name);
int          builtin_type(Command *cmd);

#endif
