#include "default_message_processor.hpp"


DefaultMessageProcessor::DefaultMessageProcessor( uint32_t worker_thread_num,
        uint32_t max_connetions)
        :_worker_thread_num(worker_thread_num)
        _max_connetions(max_connetions),
{
    //init();
}

int DefaultMessageProcessor::init()
{
    _thread_channels = new ThreadChannel[_worker_thread_num];
    _worker_threads  = new SearchThread[_worker_thread_num];
    _accessors = new ThreadChannel::Accessor[_worker_thread_num];

    //初始化 工作线程，每个工作线程分配一个channel accessor
    for(int i=0; i < _worker_thread_num; i++)
    {
        ThreadChannelAccessor client = _thread_channels[i].connect();
        ThreadChannelAccessor server = _thread_channels[i].bind();
        _worker_threads[i].set_channel_accessor(client);
        _accessors[i]  = server;
    }
}


void DefaultMessageProcessor::start()
{
    //启动工作线程
    for(int i=0; i < _worker_thread_num; i++)
    {
        _worker_threads[i].start();
    }
}

void DefaultMessageProcessor::process_input(ConnectionInfo& conn, ByteBuffer& packet)
{
    int rv = 0;

    int conn_id = conn.get_id();

    //解包，处理


    ChannelMessge msg;
    msg.conn_id = conn_id;
    msg.buf        = bb;
    //将消息发往处理线程处理
    rv = _accessors[conn_id%_worker_thread_num].send(msg);
    if(rv < 0)
    {
        //
    }
}


void DefaultMessageProcessor::process_output()
{
    int rv = 0;

    ChannelMessge msg;

    //扫描每个管道,收取返回消息
    for(int i=0; i < _worker_thread_num; i++)
    {
        // 从一个管道收取消息,至多收取DEFAULT_MAX_RECV_ONCE
        for(int j = 0; j < DEFAULT_MAX_RECV_ONCE; j++)
        {
            rv = _accessors[i].recv(msg);

            if( rv < 0)  //管道已空,继续收取下一个管道
            {
                break;
            }
            else        //将消息发送到相应的连接
            {
                ConnectionMap::iterator iter = _id_connections.find(msg.conn_id);

                if(iter != _id_connections.end()) //找到连接，将消息发送出去
                {
                    iter->write(*msg.buf);
                }
                else  //没有找到相应的连接
                {

                }
            }
        }
    }
}


