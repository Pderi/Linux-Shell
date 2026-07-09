#ifndef TYPE_H
#define TYPE_H

#include "parser.h"

int          is_builtin(const char *name);
const char **builtin_names(void);
int          builtin_type(Command *cmd);

#endif
