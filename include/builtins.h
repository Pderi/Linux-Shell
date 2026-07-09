#ifndef BUILTINS_H
#define BUILTINS_H

#include "parser.h"

int run_builtin(Command *cmd);

int builtin_ls(Command *cmd);
int builtin_cat(Command *cmd);
int builtin_grep(Command *cmd);
int builtin_pwd(Command *cmd);

#endif
