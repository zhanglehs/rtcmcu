#ifndef _STAT_PROC_PID_H_
#define _STAT_PROC_PID_H_

#define PROC_NAME_MAX_LEN 20

struct proc_pid_stat
{
    int pid;
    char comm[PROC_NAME_MAX_LEN];
    char state;
    int ppid;
    int pgrp;
    int session;
    int tty_nr;
    int tpgid;
    unsigned long int flags;
    unsigned long int minflt;
    unsigned long int cminflt;
    unsigned long int majflt;
    unsigned long int cmajflt;
    unsigned long int utime;
    unsigned long int stime;
    unsigned long int cutime;
    unsigned long int cstime;
    unsigned long int priority;
    unsigned long int nice;
    unsigned long int num_threads;
    unsigned long int itrealvalue;
    unsigned long int starttime;
    unsigned long int vsize;
    unsigned long int rss;
    unsigned long int rlim;
    unsigned long int startcode;
    unsigned long int endcode;
    unsigned long int startstack;
    unsigned long int kstkesp;
    unsigned long int kstkeip;
    unsigned long int signal;
    unsigned long int blocked;
    unsigned long int sigignore;
    unsigned long int sigcatch;
    unsigned long int wchan;
    unsigned long int nswap;
    unsigned long int cnswap;
    int exit_signal;
    int processor;
    unsigned long int rt_priority;
    unsigned long int policy;
    unsigned long long int delayacct_blkio_ticks;
};

int read_proc_pid_stat(proc_pid_stat& result, int pid);

#endif
