#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>

#include "pid_file.hpp"

int   PidFile::read_pid(const char* fname, pid_t& mypid)
{
    const int BUFFER_SIZE = sizeof(long) * 3 + 2;
    char buf[BUFFER_SIZE];
    int rv;

    if (!fname) 
    {
        return -1;
    }

    int fd = ::open(fname, O_RDONLY);
    if (fd < 0) 
    {
        return -1;
    }

    rv = ::read(fd, buf, BUFFER_SIZE - 1);
    if (rv <= 0 || rv == BUFFER_SIZE - 1 || !::isdigit(*buf)) 
    {
        return -1;
    }

    buf[rv] = '\0';
    mypid = strtol(buf, 0, 10);
    if(mypid < 0)
    {
        return -1;
    }

    ::close(fd);
    
    return mypid;
}

int   PidFile::log_pid(const char* fname)
{
    pid_t mypid;

    if (!fname) 
    {
        return -1;
    }


    mypid = getpid();

    int fd = ::open(fname, O_CREAT|O_WRONLY|O_TRUNC, S_IRUSR|S_IWUSR|S_IRGRP);
    if(fd < 0)
    {
        perror(0);
        return -1;
    }

    char buf[64];
    int len = snprintf(buf, 64, "%ld", (long)mypid);

    ::write(fd, buf, len);
    ::close(fd);

    return mypid;
    
}


bool PidFile::is_running(char* fname)
{
    int rv = 0;
    pid_t pid;
    rv = read_pid(fname, pid);
    if (rv < 0) 
    {
        return false;
    }

    return is_running(pid);
}

bool PidFile::is_running(pid_t pid)
{
    if (::kill(pid, 0) != 0) 
    {
        return false;
    }

    return true;   
}

int   PidFile::destroy_pidfile(char* fname)
{
    return -1;
}


