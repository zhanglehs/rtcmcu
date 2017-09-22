#include <unistd.h>
#include "runnable.hpp"
#include "selector.hpp"
#include "serversocketchannel.hpp"
#include "socketchannel.hpp"
#include "biniarchive.hpp"


int main()
{
    int rv;

    char buf[1024];
    memset(buf, 0, 1024);
    strcpy(buf, "hello , do you received the message");
    ByteBuffer bb(buf, strlen(buf), 1024);

    for(int j=0;j<1000;j++)
    {

        SocketChannel* socket = SocketChannel::open();
        //socket->configure_blocking(false);
        rv = socket->connect("127.0.0.1", 5000);
        if( rv < 0)
        {
            cout<<"connect failed"<<endl;
        }
        cout<<"connect success"<<endl;
        for(int i=0; i<300; i++)
        {
            socket->write(bb);
            bb.flip();
        }
        //sleep(2);
        delete socket;
        socket = NULL;
    }

}

