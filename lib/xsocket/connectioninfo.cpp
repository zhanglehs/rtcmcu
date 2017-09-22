#include <iostream>
#include <stdio.h>
#include "connectioninfo.hpp"
#include "imessageprocessor.hpp"

// statics
long ConnectionInfo::_next_id = 2;

ConnectionInfo::ConnectionInfo()
        :_timer_id(-1),
        _total_read_bytes(0),
        _total_write_bytes(0),
        _packet_processed_once(PACKET_PROCESSED_ONCE),
        _read_buffer(rbuf,MAX_READ, MAX_READ),
        _write_buffer(wbuf,MAX_READ,MAX_READ)
{
    _open = false;
}

ConnectionInfo::ConnectionInfo(SocketChannel& channel, IMessageProcessor& p)
        :_timer_id(-1),
        _total_read_bytes(0),
        _total_write_bytes(0),
        _packet_processed_once(PACKET_PROCESSED_ONCE),        
        _channel(channel),
        _read_buffer(rbuf,MAX_READ, MAX_READ),
        _write_buffer(wbuf,MAX_READ,MAX_READ)
{
    _id = allocate_process_unique_id();
    _open = true;
    _proc = &p;
    _proc->accept(*this);
    // send connected event
    send_event(ConnectionEvent::CONNECTION_CONNECTED);
}

ConnectionInfo::~ConnectionInfo()
{
}


int ConnectionInfo::open(SocketChannel& channel, IMessageProcessor& p)
{
    _channel =  channel;
    _id = allocate_process_unique_id();
    _open = true;
    _proc = &p;
    _proc->accept(*this);
    // send connected event
    send_event(ConnectionEvent::CONNECTION_CONNECTED);

    return 0;
}

int   ConnectionInfo::bind(SocketChannel& channel, IMessageProcessor& p)
{
    _channel =  channel;
    _proc = &p;
    return 0;
}
    
int ConnectionInfo::open(IMessageProcessor& p)
{
    _id = allocate_process_unique_id();
    _open = true;
    _proc = &p;
    _proc->accept(*this);
    // send connected event
    send_event(ConnectionEvent::CONNECTION_CONNECTED);

    return 0;
}

int ConnectionInfo::open()
{
    _id = allocate_process_unique_id();
    _open = true;
    _proc->accept(*this);
    // send connected event
    send_event(ConnectionEvent::CONNECTION_CONNECTED);

    return 0;
}

int ConnectionInfo::run(SelectionKey& key)
{
    if (key.is_writable())
    {
        flush();
    }

    if (key.is_readable())
    {
        // read the data from the channel
        read((SocketChannel&)(*key.channel()), key);
    }

    return 0;
}

void ConnectionInfo::close()
{
    _channel.close();
    if (_open)
    {
        _open = false;
        _proc->remove(*this);

        // closed normally (or abnormally)
        send_event(ConnectionEvent::CONNECTION_CLOSED);
    }
}

bool ConnectionInfo::is_open()
{
    return _open;
}

void ConnectionInfo::add_event_listener(ConnectionEventListener& l)
{
    _event_handlers.insert(&l);
}

void ConnectionInfo::remove_event_listener(ConnectionEventListener& l)
{
    _event_handlers.erase(&l);
}


void ConnectionInfo::send_event(ConnectionEvent& event)
{
    for (ConnectionEventListernerSet::iterator iter = _event_handlers.begin(); iter != _event_handlers.end(); iter++)
    {
        ConnectionEventListener* l = (ConnectionEventListener*)(*iter);

        l->connection_event(event);
    }
}


void ConnectionInfo::send_event(int eventCode)
{
    ConnectionEvent event(*this, eventCode);
    send_event(event);
}

void ConnectionInfo::read(ByteChannel& channel, SelectionKey& key)
{
    int n = channel.read(_read_buffer);

    // if were are at End Of Stream, we cancel
    // the read selection key.
    if (n < 0)
    {
        close();
        return;
    }

    if(n == 0)
    {
        return;
    }
    _total_read_bytes+=n;

    // process the data
    process_data();

    return;
}

int ConnectionInfo::write(ByteBuffer& bb)
{
    if (!_open)
    {
        return -1;
    }

    try
    {
        if(_write_buffer.position() == 0)
        {
             _channel.write(bb);
            if((bb.limit() - bb.position()) > 0)
            {
                _write_buffer.put(bb);
            }
        }
        else
        {
            _write_buffer.put(bb);
        }
    }
    catch(BufferOverflowException& e)
    {
        return -1;
    }

    // flush the buffers
    return  flush();
}


int  ConnectionInfo::flush()
{
    int rv = 0;
    if (_write_buffer.position() > 0)
    {
        _write_buffer.flip();
        rv =  _channel.write(_write_buffer);
        efficient_compact(_write_buffer);
    }

    if (_write_buffer.position() > 0)
    {
        int old_interest_ops =  _channel.key()->interest_ops();
         _channel.key()->interest_ops(old_interest_ops|SelectionKey::OP_WRITE);
    }
    else
    {
        int old_interest_ops =  _channel.key()->interest_ops();
         _channel.key()->interest_ops(old_interest_ops&(~SelectionKey::OP_WRITE));
    }
    
    return rv;
}




bool ConnectionInfo::ready_to_write()
{
    return _write_buffer.hasRemaining();
}

void ConnectionInfo::process_data()
{
    // make a view on the data buffer.
    int expecting = 0;
    pre_process_data();

    // iterate through the view's data until we run out.
    //while (true)
    for(unsigned i=0; i<_packet_processed_once; i++)
    {
        // FRAMING
        // call the datagram factory to figure out
        // how much data we need.

        expecting = _proc->frame(_read_buffer);
        if (expecting == -1)  //frame failed we should close the connection
        {
            // invalid data event.
            //send_event(ConnectionEvent::CONNECTION_INVALID_PROTOCOL);

            // our read buffer is unintelligible.
            // we have no choice but to close the connection.
            close();
            return;
        }
        else if (_read_buffer.remaining() >= expecting)
        {
            // PROCESS
            // if we have enough data.
            // if we don't, go back to the I/O processor.
            // make a new process buffer.
            ByteBuffer processBuf;
            _read_buffer.slice(processBuf);
            processBuf.limit(expecting);

            // read past the data so the buffer position is right after
            // the datagram we just read. this is an important
            // step to take for subclasses who may want to record
            // where in the buffer datagrams begin & end.
            _read_buffer.position(_read_buffer.position() + expecting);

            // go process it
            // now we'll process it according to our RULES.
            _proc->process_input(*this, processBuf);
        }
        else
        {
            break;
        }
    }
    // do post processing cleanup
    post_process_data();
}


/**
 * prepares for a sequence of read operations
 * position <= limit and remaining() will reflect the number
 * of bytes processed.
 */
void ConnectionInfo::pre_process_data()
{
    _read_buffer.flip();
}

/**
 * performs post processing after interpreting the contents of the read buffer.
 */
void ConnectionInfo::post_process_data()
{
    // discard the processed data.
    efficient_compact(_read_buffer);
}

/**
 * Compacts a buffer. This is optimized here to avoid
 * a call to copyMemory() inside the NIO library that
 * is made even if there are no bytes to copy, i.e.
 * <code>remaining</code> returns 0.
 */
void ConnectionInfo::efficient_compact(ByteBuffer& bb)
{
    if (bb.hasRemaining())
    {
        bb.compact();
    }
    else
    {
        bb.clear();
    }
}


