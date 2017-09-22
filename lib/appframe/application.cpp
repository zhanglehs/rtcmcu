#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>

#include "pid_file.hpp"
#include "application.hpp"


bool Application::_stopped = false;
Application::Application(int argc, char** argv)
        :_argc(argc),
        _argv(argv),
        _daemon(false)

{
    char* ptr = strrchr(argv[0], '/');
    if (ptr == NULL) 
    {
        _programname =argv[0];
    } 
    else 
    {
        _programname = (ptr + 1);
    }

}
Application::~Application()
{

}
void Application::usage()
{
    cout<<get_programname()<<" [-k [start|stop|restart]|-d|-v|-h]"<<endl;
}
void Application::sig_handler(int signal)
{
    cout<<"received signal"<<signal<<endl;
    _stopped = true;
}

int Application::send_signal(pid_t pid, int sig)
{
    if (::kill(pid, sig) < 0) 
    {
        printf("sending signal to server failed, error =[%s]", strerror(errno));
        return -1;
    }
    return 0;
}

int Application::get_home_path()
{
    return -1;
}
    
int Application::signal_server()
{
    int rv;
    pid_t otherpid;
    bool running = false;
    char status[256];
    
    string pidfile = _home_path+"/pid/"+_programname+_processidentity+".pid";

    rv = PidFile::read_pid(pidfile.c_str(), otherpid);
    if (rv < 0) 
    {
        snprintf(status, 256,"%s %s not running !\n", _programname.c_str(),_processidentity.c_str());
    }
    else 
    {
        if (PidFile::is_running(otherpid)) 
        {
            running = true;
            snprintf(status, 256,"%s %s (pid %d) already running !\n", _programname.c_str(), _processidentity.c_str(), otherpid);
        }
        else 
        {
            snprintf(status, 256,"%s %s (pid %d) not running ! \n", _programname.c_str(),_processidentity.c_str(), otherpid);
        }
    }

    if (!strcmp(_dash_k_arg.c_str(), "switch")) 
    {
        if (running) 
        {
            printf("%s\n", status);
            return -1;
        }
    }
    
    if (!strcmp(_dash_k_arg.c_str(), "start")) 
    {
        if (running) 
        {
            printf("%s\n", status);
            return -1;
        }
    }

    if (!strcmp(_dash_k_arg.c_str(), "stop")) 
    {
        if (!running)
        {
            printf("%s\n", status);
        }
        else
        {
            send_signal(otherpid, SIGTERM);
        }
        return -1;
    }

    if (!strcmp(_dash_k_arg.c_str(), "restart")) 
    {
        if (!running) 
        {
            printf("%s %s not running, trying to start\n",  _programname.c_str(),_processidentity.c_str());
        }
        else 
        {
            send_signal(otherpid, SIGHUP);
            return -1;
        }
    }

    if (!strcmp(_dash_k_arg.c_str(), "graceful")) 
    {
        if (!running) 
        {
            printf("%s %s not running, trying to start\n",  _programname.c_str(),_processidentity.c_str());
        }
        else 
        {
            send_signal(otherpid, SIGUSR1);
            return -1;
        }
    }

    return 0;
}


void Application::init_daemon()
{
    pid_t pid;

    pid = fork();
    if (pid > 0)
        exit(0);
    else if (pid < 0)
    {
        perror("fail to fork.");
        exit(1);
    }

    /* now the process is the leader of the session, the leader of its
       process grp, and have no controlling tty */
    if(setsid() == - 1)
    {
        perror("omail: setsid() fail. It seems impossible.");
        exit(1);
    }
    setpgid(0, 0);

    /*** change umask */
    umask(0027);

    /* ignore the SIGHUP, in case the session leader sending it when terminating.  */
    signal(SIGHUP, SIG_IGN);
    signal(SIGINT, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGURG, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGALRM, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    //signal(SIGTERM, SIG_IGN);

    pid = fork();
    if (pid > 0)
        exit(0);
    else if (pid < 0)
    {
        perror("fail to fork.");
        exit(1);
    }
    /*** close all inherited file/socket descriptors */
    //for(i = 0; i < getdtablesize(); i ++)
    //	close(i);
    close(0);
}
int Application::parse_params()
{
    char option;
    while((option = getopt(_argc, _argv, "k:dvh"))!=-1)
    {
        switch(option)
        {
        case 'k':
            _dash_k_arg = ::optarg;
            break;
        case 'd':
            set_daemon(true);
            break;
        case 'v':
            cout<<_programname<<" version is:"<<_version<<endl;
            exit(0);
        case 'h':
            usage();
            exit(0);
        default:
            //invalid options
            cout<<"unknown option ["<<option<<"]"<<endl;
            return -1;
        }

    }

    return 0;

}

int Application::init()
{
    int rv = 0;

    signal(SIGTERM, sig_handler);
    signal(SIGPIPE, SIG_IGN);

    rv = get_home_path();
    if(rv < 0)
    {
        printf("get home path failed\n");
        return -1;
    }
    set_process_identity();
    
    rv = parse_params();
    if(rv < 0)
    {
        usage();
        return -1;
    }
	
    if(!_dash_k_arg.empty())
    {
        if(signal_server() < 0)
        {
            return -1;
        }
    }
    
    if(get_daemon())
    {
        init_daemon();
    }

    rv = impl_init();
    if(rv >= 0)
    {
        string pidfile = _home_path+"/pid/"+_programname+_processidentity+".pid";
        PidFile::log_pid(pidfile.c_str());
    }

    return rv;
}

void Application::start()
{
    int rv = 0;
    if(rv < 0)
    {
        set_stopped(true);
    }
    
    while(!get_stopped())
    {
        run();
    }

    stop();
}

void Application::stop()
{
    cout<<_programname<<" "<<_processidentity<<" stopped"<<endl;
}

void Application::set_process_identity()
{
    _processidentity.clear();
}



