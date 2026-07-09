#include "builtins.h"

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int copy_fd(int from, int to)
{
    char buf[4096];
    ssize_t n;

    while ((n = read(from, buf, sizeof(buf))) > 0) {
        ssize_t off = 0;
        while (off < n) {
            ssize_t w = write(to, buf + off, (size_t)(n - off));
            if (w < 0)
                return -1;
            off += w;
        }
    }
    return n < 0 ? -1 : 0;
}

int builtin_cat(Command *cmd)
{
    if (cmd->argc <= 1)
        return copy_fd(STDIN_FILENO, STDOUT_FILENO) < 0 ? 1 : 0;

    int rc = 0;
    for (int i = 1; i < cmd->argc; i++) {
        int fd = open(cmd->argv[i], O_RDONLY);
        if (fd < 0) {
            perror(cmd->argv[i]);
            rc = 1;
            continue;
        }
        if (copy_fd(fd, STDOUT_FILENO) < 0) {
            perror(cmd->argv[i]);
            rc = 1;
        }
        close(fd);
    }
    return rc;
}
