/**
* @file
* @brief
* @author   songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2015/07/24
* @see
*/

#pragma once 

#include <list>
#include <netinet/in.h>
#include <string>

namespace network
{
    enum IPAccessControlType
    {
        IP_ACCESS_CONTROL_DENY = 0,
        IP_ACCESS_CONTROL_ALLOW,
    };

    class IPAccessControlItem
    {
        friend class IPAccessControl;
    public:
        IPAccessControlItem();
        IPAccessControlItem(IPAccessControlType, std::string ip_str, int mask_num);
        IPAccessControlItem(IPAccessControlType, in_addr_t ip, int mask_num);
        
        int set(IPAccessControlType, std::string ip_str, int mask_num);
        int set(IPAccessControlType, in_addr_t ip, int mask_num);
        bool match(uint32_t input_ip);

        IPAccessControlType _type;
        uint32_t _ip;
        uint32_t _mask;
    };

    class IPAccessControl
    {
    public:
        // use host byte order;
        IPAccessControl();

        // return true if valid
        bool access_valid(in_addr_t ip);
        bool access_valid(std::string ip);

        void add(IPAccessControlItem item);
        void clear();

        void add_deny_wan();
        //void add_deny_all();
        //void add_allow_all();
        //void add_allow_lan();

    protected:
        typedef std::list<IPAccessControlItem> AccessTable;
        AccessTable _ip_table;
    };
}
