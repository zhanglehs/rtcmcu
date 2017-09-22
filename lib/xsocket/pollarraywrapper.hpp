#ifndef _BASE_NET_POLLARRAYWTAPPER_H_
#define _BASE_NET_POLLARRAYWTAPPER_H_

#include <assert.h>
#include <unistd.h>
#include <sys/poll.h>
#include "selectablechannel.hpp"

class PollArrayWrapper
{
public:
    struct pollfd * _poll_array;

    // Number of valid entries in the _poll_array
    int _total_channels;// = 0;

    // File descriptor to write for interrupt
    int interrupt_fd;

public:
    PollArrayWrapper()
    {
    }
    ~PollArrayWrapper()
    {
        free();
    }

    int init(int max_channels)
    {
        _poll_array = new pollfd[max_channels+1];
        _total_channels = 1;

        return 0;
    }
    // Access methods for fd structures
    int get_event_ops(int i)
    {
        return _poll_array[i].events;
    }

    int get_revent_ops(int i)
    {
        return _poll_array[i].revents;
    }

    int get_fd(int i)
    {
        return _poll_array[i].fd;
    }

    void put_event_ops(int i, int event)
    {
        _poll_array[i].events = event;
        /*
        if(event == 0)
        {
            assert(0);
        }
        */
    }

    void put_revent_ops(int i, int revent)
    {
        _poll_array[i].revents = revent;
    }

    void put_fd(int i, int fd)
    {
        _poll_array[i].fd = fd;
    }
    void init_interrupt(int fd0, int fd1)
    {
        interrupt_fd = fd1;
        put_fd(0, fd0);
        put_event_ops(0, POLLIN);
        put_revent_ops(0, 0);
    }

    void release(int i)
    {
        return;
    }

    void free()
    {
        delete[] _poll_array;
        _poll_array = NULL;
    }

    /**
     * Prepare another pollfd struct for use.
     */
    void add_entry(SelectableChannel& sc)
    {
        put_fd(_total_channels, sc.get_fd());
         _poll_array[_total_channels].events = 0;
         _poll_array[_total_channels].revents = 0;
        _total_channels++;
    }

    /**
     * Writes the pollfd entry from the source wrapper at the source index
     * over the entry in the target wrapper at the target index. The source
     * array remains unchanged unless the source array and the target are
     * the same array.
     */
    static void replace_entry(PollArrayWrapper& source, int sindex,
                             PollArrayWrapper& target, int tindex)
    {
        target.put_fd(tindex, source.get_fd(sindex));
        target.put_event_ops(tindex, source.get_event_ops(sindex));
        target.put_revent_ops(tindex, source.get_revent_ops(sindex));
    }


    int poll(int numfds, int offset, long timeout)
    {
        int err = 0;

        err = ::poll(&_poll_array[offset], numfds, timeout);

        return err;
    }

    int  interrupt()
    {
        return interrupt(interrupt_fd);
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


};

#endif

