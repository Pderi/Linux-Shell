#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "parser.h"

int executor_setup_redirection(const Command *cmd);
int executor_run_single_builtin(Command *cmd, int background);
int execute_pipeline(Pipeline *pl);

#endif
