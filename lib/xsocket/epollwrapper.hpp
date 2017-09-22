#ifndef _BASE_NET_EPOLLARRAYWTAPPER_H_
#define _BASE_NET_EPOLLARRAYWTAPPER_H_
#include <unistd.h>
#include <sys/epoll.h>
#include "selectablechannel.hpp"

class EPollArrayWrapper
{
public:

    EPollArrayWrapper()
            :_revents(NULL)
    {


    }

    ~EPollArrayWrapper()
    {
		delete[] _revents;
        close();
    }

    int init(int maxfds)
    {
        _maxfds = maxfds;
        _revents = new epoll_event[_maxfds];
        _epfd = ::epoll_create(maxfds);
        if(_epfd < 0)
        {
            return -1;
        }

        return 0;
    }
    
    int close()
    {
        return ::close(_epfd);
    }
    int ctl_add(int fd, int events)
    {
        struct epoll_event ev;

        ev.events = events;
        ev.data.fd = fd;

        return ::epoll_ctl(_epfd, EPOLL_CTL_ADD, fd, &ev);
    }

    int ctl_mod(int fd, int events)
    {
        struct epoll_event ev;

        ev.events = events;
        ev.data.fd = fd;

        return ::epoll_ctl(_epfd, EPOLL_CTL_MOD, fd, &ev);
    }
    int ctl_del(int fd)
    {
        return ::epoll_ctl(_epfd, EPOLL_CTL_DEL, fd, NULL);
    }

    void init_interrupt(int fd0, int fd1)
    {
        _interrupt_fd = fd1;
        ctl_add(fd0, EPOLLIN);
    }

    int wait(long timeout)
    {
        return ::epoll_wait(_epfd, _revents, _maxfds, timeout);
    }

    struct epoll_event* get_revent_ops(int index)
    {
        return &_revents[index];
    }

    int  interrupt()
    {
        return interrupt(_interrupt_fd);
    }

    static  int interrupt(int fd)
    {
        int fakebuf[1];
        fakebuf[0] = 1;
        if (::write(fd, fakebuf, 1) < 0)
        {
            return -1;
        }
        return 0;
    }

public:
    int _epfd;
    int _maxfds;
    struct epoll_event* _revents;

    // File descriptor to write for interrupt
    int _interrupt_fd;

};


#endif

