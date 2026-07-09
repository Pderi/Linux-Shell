#include "builtins.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int ls_long(const char *name, struct stat *st)
{
    char mode[11];
    mode[0] = S_ISDIR(st->st_mode) ? 'd' : '-';
    mode[1] = (st->st_mode & S_IRUSR) ? 'r' : '-';
    mode[2] = (st->st_mode & S_IWUSR) ? 'w' : '-';
    mode[3] = (st->st_mode & S_IXUSR) ? 'x' : '-';
    mode[4] = (st->st_mode & S_IRGRP) ? 'r' : '-';
    mode[5] = (st->st_mode & S_IWGRP) ? 'w' : '-';
    mode[6] = (st->st_mode & S_IXGRP) ? 'x' : '-';
    mode[7] = (st->st_mode & S_IROTH) ? 'r' : '-';
    mode[8] = (st->st_mode & S_IWOTH) ? 'w' : '-';
    mode[9] = (st->st_mode & S_IXOTH) ? 'x' : '-';
    mode[10] = '\0';

    char timebuf[32];
    struct tm *tm_info = localtime(&st->st_mtime);
    if (tm_info)
        strftime(timebuf, sizeof(timebuf), "%b %e %H:%M", tm_info);
    else
        timebuf[0] = '\0';

    printf("%s %3ld %8ld %s %s\n", mode, (long)st->st_nlink, (long)st->st_size,
           timebuf, name);
    return 0;
}

static int list_file(const char *path, int long_fmt)
{
    struct stat st;

    if (stat(path, &st) < 0) {
        perror(path);
        return 1;
    }
    if (long_fmt)
        return ls_long(path, &st);
    printf("%s\n", path);
    return 0;
}

static int list_dir(const char *path, int long_fmt, int show_all)
{
    DIR *d = opendir(path);
    if (!d) {
        struct stat st;
        if (stat(path, &st) == 0 && !S_ISDIR(st.st_mode))
            return list_file(path, long_fmt);
        perror(path);
        return 1;
    }

    struct dirent *ent;
    int rc = 0;
    while ((ent = readdir(d))) {
        char full[4096];

        if (!show_all && ent->d_name[0] == '.')
            continue;

        if (long_fmt) {
            if (strcmp(path, ".") == 0)
                snprintf(full, sizeof(full), "%s", ent->d_name);
            else
                snprintf(full, sizeof(full), "%s/%s", path, ent->d_name);
            struct stat st;
            if (stat(full, &st) < 0) {
                perror(ent->d_name);
                rc = 1;
                continue;
            }
            ls_long(ent->d_name, &st);
        } else {
            printf("%s\n", ent->d_name);
        }
    }
    closedir(d);
    return rc;
}

int builtin_ls(Command *cmd)
{
    int long_fmt = 0;
    int show_all = 0;
    int first_path = 1;
    int rc = 0;

    for (int i = 1; i < cmd->argc; i++) {
        if (cmd->argv[i][0] == '-' && cmd->argv[i][1] != '\0') {
            for (const char *p = cmd->argv[i] + 1; *p; p++) {
                if (*p == 'l')
                    long_fmt = 1;
                else if (*p == 'a')
                    show_all = 1;
            }
            continue;
        }
        first_path = 0;
        if (list_dir(cmd->argv[i], long_fmt, show_all) != 0)
            rc = 1;
    }

    if (first_path && list_dir(".", long_fmt, show_all) != 0)
        rc = 1;
    return rc;
}
