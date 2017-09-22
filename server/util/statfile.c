/*
 * author: hechao@youku.com
 * create: 2013.7.21
 */

#include "statfile.h"

#include <ctype.h>
#include <error.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>

/* Close fd and _try_ to get a /dev/null for it instead.
 * close() alone may trigger some bugs when a
 * process opens another file and gets fd = STDOUT_FILENO or STDERR_FILENO
 * and later tries to just print on stdout/stderr
 *
 * Returns 0 on success and -1 on failure (fd gets closed in all cases)
 */
static int
openDevNull(int fd)
{
    int tmpfd;

    close(fd);
    tmpfd = open("/dev/null", O_RDWR);
    if(tmpfd != -1 && tmpfd != fd) {
        dup2(tmpfd, fd);
        close(tmpfd);
    }
    return (tmpfd != -1) ? 0 : -1;
}

int
open_statfile_or_pipe(const char *statfile)
{
    int fd;

    if(statfile[0] == '|') {
        /* create write pipe and spawn process */

        int to_log_fds[2];

        if(pipe(to_log_fds)) {
            return -1;
        }

        /* fork, execve */
        switch (fork()) {
        case 0:
            /* child */
            close(STDIN_FILENO);

            /* dup the filehandle to STDIN */
            if(to_log_fds[0] != STDIN_FILENO) {
                if(STDIN_FILENO != dup2(to_log_fds[0], STDIN_FILENO)) {
                    exit(-1);
                }
                close(to_log_fds[0]);
            }
            close(to_log_fds[1]);

#if 0
#ifndef FD_CLOEXEC
            {
                int i;

                /* we don't need the client socket */
                for(i = 3; i < 256; i++) {
                    close(i);
                }
            }
#endif
#endif

            /* close old stderr */
            openDevNull(STDERR_FILENO);
            /* exec the log-process (skip the | ) */
            execl("/bin/sh", "sh", "-c", statfile + 1, NULL);

            exit(-1);
            break;
        case -1:
            /* error */
            return -1;
        default:
            close(to_log_fds[0]);
            fd = to_log_fds[1];
            break;
        }

    }
    else if(statfile[0] == '@') {
        char ip[32];
        uint16_t port;

        memset(ip, 0, sizeof(ip));

        if(sscanf(statfile + 1, "%[0-9.]:%hu", ip, &port) < 2) {
            perror("parse stat server addr error. errno: ");
            return -1;
        }
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if(-1 == fd) {
            return -1;
        }
        struct sockaddr_in log_svr_addr;

        log_svr_addr.sin_family = AF_INET;
        log_svr_addr.sin_addr.s_addr = inet_addr(ip);
        log_svr_addr.sin_port = htons(port);
        if(connect
           (fd, (struct sockaddr *) &log_svr_addr,
            sizeof(log_svr_addr)) < 0) {
            return -1;
        }

        //} else if (-1 == (fd = open(statfile, O_APPEND | O_WRONLY | O_CREAT | O_LARGEFILE, 0644))) {
    }
    else if(-1 == (fd = open(statfile, O_APPEND | O_WRONLY | O_CREAT, 0644))) {
        return -1;
    }

#ifdef FD_CLOEXEC
    fcntl(fd, F_SETFD, FD_CLOEXEC);
#endif

    return fd;
}
