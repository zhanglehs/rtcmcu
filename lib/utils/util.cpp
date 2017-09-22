#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "util.h"


int util_create_pid_file(const char *path, const char *process_name)
{
    char file_path[PATH_MAX];
    char real_path[PATH_MAX];
    char content[32];
    int fd = -1;
    int ret = -1;
    int len = -1;

    memset(file_path, 0, sizeof(file_path));
    memset(real_path, 0, sizeof(real_path));
    memset(content, 0, sizeof(content));
    if(NULL == realpath(path, real_path))
        return errno;

    ret =
        snprintf(file_path, sizeof(file_path) - 1, "%s/%s.pid", real_path,
                process_name);
    if(ret < 0 || ret >= sizeof(file_path) - 1)
        return -1;

    fd = open(file_path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if(-1 == fd)
        return errno;

    len = snprintf(content, sizeof(content) - 1, "%d", (int) getpid());
    if(len < 0 || len >= sizeof(content) - 1) {
        close(fd);
        return -2;
    }

    ret = write(fd, content, len);
    if(-1 == ret) {
        close(fd);
        unlink(file_path);
        return errno;
    }

    if(ret < len) {
        close(fd);
        unlink(file_path);
        return -3;
    }

    close(fd);
    return 0;
}

