/**
 * \file
 * \brief 谁知道这个干啥用的？补充下文档吧
 */
#ifndef _THREAD_CHANNEL_HPP_
#define  _THREAD_CHANNEL_HPP_

#include "circular_array_queue.hpp"


template <typename ElementType>
class ThreadChannel;

template <typename ElementType>
class ThreadChannelAccessor
{
public:
    enum
    {
        MODE_SERVER,
        MODE_CLIENT
    };

    typedef ElementType   value_type;
    typedef ThreadChannel<value_type> ThreadChannel;

    friend class ThreadChannel;
public:
    ThreadChannelAccessor()
    {

    }

    ThreadChannelAccessor(ThreadChannel& channel, int mode)
            :_channel(&channel),
            _workmode(mode)
    {

    }

public:
    int send(const value_type&  value);
    int recv(value_type& value);

private:
    ThreadChannel*     _channel;
    size_t                     _workmode;

};

template <typename ElementType>
int  ThreadChannelAccessor<ElementType>::send(const value_type&  value)
{
    if(_workmode == MODE_SERVER)
    {
        _channel->_to_client.push(value);
    }
    else
    {
        _channel->_to_server.push(value);
    }
}

template <typename ElementType>
int  ThreadChannelAccessor<ElementType>::recv(value_type& value)
{
    if(_workmode == MODE_SERVER)
    {
        if( !(_channel->_to_server.empty()))
        {
            value = _channel->_to_server.front();
            _channel->_to_server.pop();

            return 0;
        }
        else
        {
            return -1;
        }
    }
    else
    {
        if( !(_channel->_to_client.empty()))
        {
            value = _channel->_to_client.front();
            _channel->_to_client.pop();

            return 0;
        }
        else
        {
            return -1;
        }
    }

}


template <typename ElementType>
class ThreadChannel
{
public:
    typedef  ElementType           value_type;
    typedef  CircularArrayQueue<value_type> CircularArrayQueue;
    typedef  ThreadChannelAccessor<value_type>   Accessor;
    friend class Acceptor;
public:
    ThreadChannel(int capacity)
            :_to_server(capacity),
            _to_client(capacity)
    {

    }

    ~ThreadChannel()
    {

    }

    Accessor bind()
    {
        return Accessor(*this, Accessor::MODE_SERVER);
    }
    Accessor connect()
    {
        return Accessor(*this, Accessor::MODE_CLIENT);
    }

protected:
    CircularArrayQueue _to_server;
    CircularArrayQueue _to_client;

};

#endif
