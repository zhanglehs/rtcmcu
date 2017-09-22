/**
 * \file
 * \brief 应用程序空间，一般server程序用这个比较方便
 * 
 * \note SampleApp 是一个例子
 */
 

#ifndef _APPLICATION_HPP_
#define _APPLICATION_HPP_

#include <sys/shm.h>
#include <string>
#include <cstring>
#include "config.hpp"

using namespace std;

/**
* haha
*/
class Application
{
protected:
    Application()
    {
    }
    Application(int argc, char* argv[]);
    virtual ~Application();
public:
    virtual void usage();
    static void sig_handler(int signal);
    static int    send_signal(pid_t pid, int sig);  
    static void init_daemon();

    int signal_server();

    void set_daemon(bool is_daemon)
    {
        _daemon = is_daemon;
    }
    bool get_daemon()
    {
        return _daemon;
    }
    int init();

    virtual void start();
    virtual void stop();

    virtual int parse_params();
    virtual int get_home_path();
    virtual void set_process_identity();
    string home_path(){return _home_path;}

    bool is_switch();
    static bool clean_shm(unsigned shm_key);    
    bool get_stopped()
    {
        return _stopped;
    }
    bool set_stopped(bool stopped)
    {
        return _stopped = stopped;
    }

    void  set_version(const string& version)
    {
        _version = version;
    }
    
    string get_version()
    {
        return _version;
    }
    
    string get_programname()
    {
        return _programname;
    }
protected:
    virtual void run() = 0;

    virtual int impl_init()
    {
        return 0;
    };
protected:
    Config        _config;
    string        _home_path;
    string        _dash_k_arg;
    string        _programname;
    string        _processidentity;
    
    static  bool _stopped;
    int             _argc;
    char**       _argv;
    bool          _daemon;
    
    string        _version;

private:
};

inline
bool Application::is_switch()
{
      return (strcmp(_dash_k_arg.c_str(), "switch") == 0);
}

inline
bool Application::clean_shm(unsigned shm_key)
{
    int ret = 0;
    ret = shmget(shm_key, 0, 0200);
    if(ret < 0)
    {
        return false;
    }

    ret = shmctl(ret, IPC_RMID, (struct shmid_ds*)0);
    if(ret < 0)
    {
        return false;
    }

    return true;
}

#endif
