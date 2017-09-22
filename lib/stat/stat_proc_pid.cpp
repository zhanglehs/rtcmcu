#include "stat_proc_pid.h"
#include "proc_file_sys.h"

#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include <unistd.h>

#define MAX_COMM_LEN	128

static int general_proc_pid_stat_reader(proc_pid_stat& rs, const char* filename)
{
    int fd = 0, sz = 0, rc = 0, commsz = 0;
    char buffer[1024 + 1];

    if ((fd = open(filename, O_RDONLY)) < 0)
        return 0; // No such process

    sz = read(fd, buffer, 1024);
    close(fd);
    if (sz <= 0)
        return 0;
    buffer[sz] = '\0';

    char *start, *end;
    start = end = buffer;

    rc += sscanf(start, "%d", &(rs.pid));

    if ((start = strchr(buffer, '(')) == NULL)
        return rc;
    start += 1;
    if ((end = strrchr(start, ')')) == NULL)
        return rc;
    commsz = end - start;
    if (commsz >= MAX_COMM_LEN)
        return rc;
    memcpy(rs.comm, start, commsz);
    rs.comm[commsz] = '\0';
    rc++;
    start = end + 2;

    rc += sscanf(start,
        "%c %d %d %d %d %d %lu %lu %lu %lu "
        "%lu %lu %lu %ld %ld %ld %ld %ld %ld %lu "
        "%lu %ld %lu %lu %lu %lu %lu %lu %lu %lu "
        "%lu %lu %lu %lu %lu %d %d %lu %lu %llu", 
        &rs.state, &rs.ppid, &rs.pgrp, &rs.session, &rs.tty_nr, &rs.tpgid, &rs.flags, &rs.minflt, &rs.cminflt, &rs.majflt,
        &rs.cmajflt, &rs.utime, &rs.stime, &rs.cutime, &rs.cstime, &rs.priority, &rs.nice, &rs.num_threads, &rs.itrealvalue, &rs.starttime,
        &rs.vsize, &rs.rss, &rs.rlim, &rs.startcode, &rs.endcode, &rs.startstack, &rs.kstkesp, &rs.kstkeip, &rs.signal, &rs.blocked,
        &rs.sigignore, &rs.sigcatch, &rs.wchan, &rs.nswap, &rs.cnswap, &rs.exit_signal, &rs.processor, &rs.rt_priority, &rs.policy, &rs.delayacct_blkio_ticks);

    // Linux kernel 2.6.18: rc==42
    return rc;
}

int read_proc_pid_stat(proc_pid_stat& result, int pid)
{
    char filename[128];
    sprintf(filename, PID_STAT, pid);

    return general_proc_pid_stat_reader(result, filename);
}
