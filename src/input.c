#include "input.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <readline/readline.h>

static char *build_prompt(void)
{
    char cwd[1024];
    if (!getcwd(cwd, sizeof(cwd)))
        strcpy(cwd, "?");

    const char *mark = (getuid() == 0) ? "#" : "$";
    size_t len = strlen("myshell:") + strlen(cwd) + strlen(mark) + 1;
    char *prompt = malloc(len);
    if (!prompt)
        return NULL;
    snprintf(prompt, len, "myshell:%s%s", cwd, mark);
    return prompt;
}

static char *read_line_plain(void)
{
    char *line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, stdin);
    if (n < 0) {
        free(line);
        return NULL;
    }
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = '\0';
    return line;
}

char *input_read_line(void)
{
    if (!isatty(STDIN_FILENO))
        return read_line_plain();

    char *prompt = build_prompt();
    char *line = readline(prompt ? prompt : "myshell$ ");
    free(prompt);
    return line;
}
