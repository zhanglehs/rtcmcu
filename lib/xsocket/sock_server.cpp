#include <unistd.h>
#include <iostream>
#include "runnable.hpp"
#include "selector.hpp"
#include "serversocketchannel.hpp"
#include "socketchannel.hpp"
#include "biniarchive.hpp"


using namespace std;

int main()
{
    int rv;

    char buf[1024];
    ByteBuffer bb(buf, 1024, 1024);

    memset(buf, 0, 1024);
    Selector* selector = Selector::open();
    ServerSocketChannel* server_socket  = ServerSocketChannel::open();


    rv = server_socket->bind(5000);
    if (rv < 0)
    {
        return -1;
    }
    server_socket->configure_blocking(false);

    //向selector注册该channel
    SelectionKey* sk =  server_socket->enregister(*selector, SelectionKey::OP_ACCEPT);

    //delete server_socket;
    //delete selector;

    int i =0;
    int count =0;
    while(true)
    {
        int num_keys = selector->select(5000);



        //如果有我们注册的事情发生了，它的传回值就会大于0
        if (num_keys > 0)
        {

            SelectionKey* selected_key = NULL;
            while(selector->has_next())
            {
                selected_key = selector->next();

                if (selected_key == sk)
                {
                    cout<<"accept a connection"<<endl;
                    SocketChannel* incoming_channel = SocketChannel::open(false);
                    server_socket->accept(*incoming_channel);
                    incoming_channel->configure_blocking(false);
                    incoming_channel->enregister(*selector, SelectionKey::OP_READ);
                }
                else
                {
                    bb.clear();
                    SocketChannel* ch = (SocketChannel*)selected_key->channel();
                    rv = ch->read(bb);
                    if(rv > 0)
                    {
                        count+=rv;
                        //cout<<(char*)buf<<endl;
                        i++;
                    }
                    else
                    {
                        cout<<"read EOF, delete socket "<<i<<"count"<<count<<endl<<endl;
                        delete ch;
                    }
                }

            }

        }
        else
        {
            cout<<"select time out"<<endl;
        }
    }

    return 0;
}
