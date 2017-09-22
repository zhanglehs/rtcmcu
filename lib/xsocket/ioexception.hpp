#ifndef _BASE_NET_NIO_EXCEPTION_H_
#define _BASE_NET_NIO_EXCEPTION_H_

#include <exception>
using namespace std;
class ClosedChannelException: public exception
{

};

class AlreadyBoundException: public exception
{
};
class NotYetBoundException:public exception
{

};
class AlreadyConnectedException:public exception
{
};
class ConnectionPendingException:public exception
{
};
class NoConnectionPendingException:public exception
{
};
class IllegalArgumentException:public exception
{
};
class IllegalBlockingModeException:public exception
{
};

class NotYetConnectedException:public exception
{
};


class InvalidMarkException:public exception
{
};
class BufferUnderflowException:public exception
{
};
class BufferOverflowException:public exception
{
};
class IndexOutOfBoundsException:public exception
{
};

class UnsupportedOperationException:public exception
{

};

class ReadOnlyBufferException:public exception
{
};

#endif

