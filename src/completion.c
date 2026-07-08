#include "completion.h"
#include "type.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <readline/readline.h>

#define MAX_MATCHES 512

static char **match_list;
static int match_count;
static int match_index;

static void free_matches(void)
{
    if (!match_list)
        return;
    for (int i = 0; i < match_count; i++)
        free(match_list[i]);
    free(match_list);
    match_list = NULL;
    match_count = 0;
    match_index = 0;
}

static int add_match(const char *name)
{
    for (int i = 0; i < match_count; i++) {
        if (strcmp(match_list[i], name) == 0)
            return 0;
    }
    if (match_count >= MAX_MATCHES)
        return -1;

    match_list[match_count] = strdup(name);
    if (!match_list[match_count])
        return -1;
    match_count++;
    return 0;
}

static void build_command_matches(const char *text)
{
    free_matches();
    match_count = 0;
    match_index = 0;
    match_list = calloc(MAX_MATCHES, sizeof(char *));
    if (!match_list)
        return;

    const char **builtins = builtin_names();
    for (int i = 0; builtins[i]; i++) {
        if (strncmp(builtins[i], text, strlen(text)) == 0)
            add_match(builtins[i]);
    }

    const char *pathenv = getenv("PATH");
    if (!pathenv)
        return;

    char *pathcopy = strdup(pathenv);
    if (!pathcopy)
        return;

    char *save = NULL;
    for (char *dir = strtok_r(pathcopy, ":", &save); dir; dir = strtok_r(NULL, ":", &save)) {
        DIR *d = opendir(dir);
        if (!d)
            continue;
        struct dirent *ent;
        while ((ent = readdir(d))) {
            if (strncmp(ent->d_name, text, strlen(text)) != 0)
                continue;
            char full[1024];
            snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
            if (access(full, X_OK) == 0)
                add_match(ent->d_name);
        }
        closedir(d);
    }
    free(pathcopy);
}

static char *command_generator(const char *text, int state)
{
    if (state == 0)
        build_command_matches(text);

    if (match_index >= match_count) {
        free_matches();
        return NULL;
    }
    return strdup(match_list[match_index++]);
}

static int completing_command_name(int start)
{
    if (!rl_line_buffer || start < 0)
        return 0;

    int i = start - 1;
    while (i >= 0 && (rl_line_buffer[i] == ' ' || rl_line_buffer[i] == '\t'))
        i--;

    if (i < 0)
        return 1;

    return rl_line_buffer[i] == '|';
}

static char **myshell_completion(const char *text, int start, int end)
{
    (void)end;
    rl_attempted_completion_over = 0;

    if (completing_command_name(start)) {
        rl_attempted_completion_over = 1;
        return rl_completion_matches(text, command_generator);
    }

    return rl_completion_matches(text, rl_filename_completion_function);
}

void completion_init(void)
{
    rl_attempted_completion_function = myshell_completion;
}
