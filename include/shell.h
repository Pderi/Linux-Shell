#ifndef SHELL_H
#define SHELL_H

#include "parser.h"

int shell_execute(Pipeline *pl);
int shell_should_exit(void);
void shell_request_exit(void);

#endif
