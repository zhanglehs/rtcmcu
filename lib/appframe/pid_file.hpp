/**
 * \file
 * \brief 读写pid文件，一般deamon程序使用
 */
#ifndef _PID_FILE_HPP_
#define _PID_FILE_HPP_

class PidFile
{
public:
    static int   read_pid(const char* fname, pid_t& pid);
    static int   log_pid(const char* fname);    
    static bool is_running(char* fname);
    static bool is_running(pid_t pid);    
    static int   destroy_pidfile(char* fname);
};

#endif

