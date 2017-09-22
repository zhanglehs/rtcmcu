/**
* @file cmd_server.h
* @brief CMDServer's controller
*
* @author
* @copyright Youku All rights reserved.
* @email
* @company http://www.youku.com
* @date 2015-12-31
*/
#ifndef _CMD_SERVER_H_
#define _CMD_SERVER_H_
#include <list>
#include <map>
#include <string>
#include "tcp_server.h"
#include "tcp_connection.h"

namespace lcdn
{
namespace net
{
class EventLoop;
class InetAddress;
class CMDFSM;
class CMDFSMMgr;
class TCPConnection;
class CMDBaseRoleMgr;
const uint32_t DEFAULT_INPUT_BUF_SIZE = 2*1024*1024;
const uint32_t DEFAULT_OUTPUT_BUF_SIZE = 30*1024*1024;

/**
* @brief definite CMDServer, and generalize TCPServer::Delegate
*       
*/
class CMDServer : public TCPConnection::Delegate
{
public:
    /**
    * @brief CMDServer's constructor
    * @param loop event loop
    * @param net_addr server's addr info
    * @param name server's name
    * @return
    */
    CMDServer(EventLoop* loop,
        const InetAddress& net_addr,
        const std::string& name);
    /**
    * @brief CMDServer's destructor, release resources: _tcp_server, _delegate, and etc..
    * @param
    * @return
    */
    virtual ~CMDServer();
                /**
    * @brief start CMDServer
    * @param
    * @return
    */
    void start();
    /**
    * @brief close callback
    * @param conn contain information about tcp connection
    * @return
    */
    virtual void on_close(TCPConnection* conn);
    /**
    * @brief accept callback
    * @param conn contain information about tcp connection
    * @return
    */
    virtual void on_connect(TCPConnection* conn);
    /**
    * @brief read callback
    * @param conn contain information about tcp connection
    * @return
    */
    virtual void on_read(TCPConnection* conn);
    /**
    * @brief write callback
    * @param conn contain information about tcp connection
    * @return
    */
    virtual void on_write_completed(TCPConnection* conn);

    virtual void on_error(TCPConnection* conn);

    void on_second(time_t t);

    void register_role_mgr(CMDBaseRoleMgr* role_mgr);
    void remove_role_mgr(CMDBaseRoleMgr* role_mgr);
    void remove_all_role_mgrs();
    
    std::list<CMDBaseRoleMgr*> role_mgrs();
    CMDFSM* find_cmd_fsm(int fsm_id);

    /**
    * @brief get CMDFSMMgr
    * @param
    * @return manager of all fsms
    */
    CMDFSMMgr* get_fsm_mgr();
    
    inline void set_default_input_buf_size(size_t size)
    {
        _default_input_buf_size = size;
    }

    size_t default_input_buf_size()
    {
        return _default_input_buf_size;
    }

    inline void set_default_output_buf_size(size_t size)
    {
        _default_output_buf_size = size;
    }

    size_t default_output_buf_size()
    {
        return _default_output_buf_size;
    }

    void set_conn_delay(uint32_t delay)
    {
        _conn_delay = delay;
    } 


    uint32_t conn_delay()
    {
        return _conn_delay;
    }

private:
    TCPServer* _tcp_server;
    CMDFSMMgr* _fsm_mgr;
    size_t _default_input_buf_size;
    size_t _default_output_buf_size;
    EventLoop* _loop;
    std::list<CMDFSM*> _fsm_timeout_list;
    uint32_t _conn_delay;
    
};
} // namespace net
} // namespace lcdn

#endif // _CMD_SERVER_H_
