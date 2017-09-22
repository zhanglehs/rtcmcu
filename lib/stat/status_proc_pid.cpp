#include "status_proc_pid.h"
#include "proc_file_sys.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>

/*
int read_proc_pid_status(unsigned int pid, struct pid_stats *pst,
    unsigned int tgid)
{
    FILE *fp;
    char filename[128], line[256];

    if (tgid) {
        sprintf(filename, TASK_STATUS, tgid, pid);
    }
    else {
        sprintf(filename, PID_STATUS, pid);
    }

    if ((fp = fopen(filename, "r")) == NULL)
        // No such process 
        return 1;

    while (fgets(line, sizeof(line), fp) != NULL) {

        if (!strncmp(line, "Uid:", 4)) {
            sscanf(line + 5, "%u", &pst->uid);
        }
        else if (!strncmp(line, "Threads:", 8)) {
            sscanf(line + 9, "%u", &pst->threads);
        }
        else if (!strncmp(line, "voluntary_ctxt_switches:", 24)) {
            sscanf(line + 25, "%lu", &pst->nvcsw);
        }
        else if (!strncmp(line, "nonvoluntary_ctxt_switches:", 27)) {
            sscanf(line + 28, "%lu", &pst->nivcsw);
        }
    }

    fclose(fp);

    pst->pid = pid;
    pst->tgid = tgid;
    return 0;
}
*/

template<size_t Size>
struct StackStringPool
{
    StackStringPool()
    {
        memset(_str, 1, sizeof(_str));
        _end = _last = _str;
    }

    const char* begin() const { return _str; }
    const char* end() const { return _end; }
    const char* last() const { return _last; }

    const char* push_back(const char* str)
    {
        return push_back(str, strlen(str));
    }

    const char* push_back(const char* str, size_t len)
    {
        if (_end - _str + len >= Size)
            return NULL; // full

        _last = _end;
        strncpy(_end, str, len);
        _end[len] = '\0';
        _end += len + 1;
        return _last;
    }

private:

    char _str[Size];
    char* _end;
    char* _last;
};

typedef StackStringPool<1024> stack_string_pool_1k_t;

int colon_fields_reader(const char* filename, stack_string_pool_1k_t& strpool)
{
    FILE *fp = NULL;
    char line[256];
    int rc = 0;

    if ((fp = fopen(filename, "r")) == NULL)
        // No such process 
        return 0;

    while (fgets(line, sizeof(line), fp) != NULL)
    {
        // name
        char* icolon = strchr(line, ':');
        const int name_len = icolon - line;
        if (strpool.push_back(line, name_len) == NULL)
            break;

        // value
        char* val = strtok(line + name_len, ": \t\n");
        if (val == NULL)
        {
            if (strpool.push_back(" ") == NULL)
                break;
        }
        else if (strpool.push_back(val) == NULL)
            break;

        // the 3rd value while be igoned

        rc++;
    }

    fclose(fp);
    return rc;
}

int read_proc_pid_status(proc_pid_status& rs, int pid)
{
    char filename[128];
    sprintf(filename, PID_STATUS, pid);

    stack_string_pool_1k_t strpool;
    if (colon_fields_reader(filename, strpool) == 0)
        return 0;

    int rc = 0;
    const char* iter_str = strpool.begin();
    while (iter_str <= strpool.end())
    {
        if (strcmp("Name", iter_str) == 0)
        {
            iter_str += sizeof("Name");
            strncpy(rs.Name, iter_str, PROC_NAME_MAX_LEN);
            iter_str += strlen(iter_str) + 1;
            rc++;
        }
        else if (strcmp("SleepAVG", iter_str) == 0)
        {
            iter_str += sizeof("SleepAVG");
            rs.SleepAVG = strtol(iter_str, NULL, 10);
            iter_str += strlen(iter_str) + 1;
            rc++;
        }
        else if (strcmp("Pid", iter_str) == 0)
        {
            iter_str += sizeof("Pid");
            rs.Pid = strtol(iter_str, NULL, 10);
            iter_str += strlen(iter_str) + 1;
            rc++;
        }
        else if (strcmp("VmPeak", iter_str) == 0)
        {
            iter_str += sizeof("VmPeak");
            rs.VmPeak = strtoul(iter_str, NULL, 10);
            iter_str += strlen(iter_str) + 1;
            rc++;
        }
        else if (strcmp("VmSize", iter_str) == 0)
        {
            iter_str += sizeof("VmSize");
            rs.VmSize = strtoul(iter_str, NULL, 10);
            iter_str += strlen(iter_str) + 1;
            rc++;
        }
        else if (strcmp("VmRSS", iter_str) == 0)
        {
            iter_str += sizeof("VmRSS");
            rs.VmRSS = strtoul(iter_str, NULL, 10);
            iter_str += strlen(iter_str) + 1;
            rc++;
        }
        else
        {
            iter_str += strlen(iter_str) + 1;
            iter_str += strlen(iter_str) + 1;
        }
    }

    return rc;
}
