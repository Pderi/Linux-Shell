#ifndef PARSER_H
#define PARSER_H

#define MAX_PIPELINE 16
#define MAX_ARGS     64

typedef struct {
    char *argv[MAX_ARGS];
    int   argc;
    char *in_file;
    char *out_file;
    int   append;
} Command;

typedef struct {
    Command cmds[MAX_PIPELINE];
    int     count;
    int     background;
} Pipeline;

int  parse_line(const char *line, Pipeline *pl);
void parser_free_pipeline(Pipeline *pl);

#endif
