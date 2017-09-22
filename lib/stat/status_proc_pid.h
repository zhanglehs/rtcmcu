#ifndef _STATUS_PROC_PID_H_
#define _STATUS_PROC_PID_H_

#include <stdint.h>

#define PROC_NAME_MAX_LEN 20

struct proc_pid_status
{
    char Name[PROC_NAME_MAX_LEN];
    //char State;
    uint8_t SleepAVG;
    //int Tgid;
    int Pid;
    //int PPid;
    //int TracerPid;
    //int Uid[4];
    //int Gid[4];
    //uint16_t FDSize;
    //int Groups[8];
    uint32_t VmPeak;
    uint32_t VmSize;
    //uint32_t VmLck;
    //uint32_t VmHWM;
    uint32_t VmRSS;
    //uint32_t VmData;
    //uint32_t VmStk;
    //uint32_t VmExe;
    //uint32_t VmLib;
    //uint32_t VmPTE;
    //uint32_t StaBrk;
    //uint32_t Brk;
    //uint32_t StaStk;
    //uint16_t Threads;
    //char SigQ[17];
    //char SigPnd[17];
    //char ShdPnd[17];
    //char SigBlk[17];
    //char SigIgn[17];
    //char SigCgt[17];
    //char CapInh[17];
    //char CapPrm[17];
    //char CapEff[17];
    //char Cpus_allowed[72];
    //char Mems_allowed[18];
};

int read_proc_pid_status(proc_pid_status& result, int pid);

#endif
