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

#include "ip_access_control.h"
#include <util/util.h>
#include <util/log.h>

using namespace std;

namespace network
{
    IPAccessControlItem::IPAccessControlItem()
    {

    }

    IPAccessControlItem::IPAccessControlItem(IPAccessControlType type, string ip_str, int mask_num)
    {
        set(type, ip_str, mask_num);
    }

    IPAccessControlItem::IPAccessControlItem(IPAccessControlType type, in_addr_t ip, int mask_num)
    {
        set(type, ip, mask_num);
    }

    int IPAccessControlItem::set(IPAccessControlType type, string ip_str, int mask_num)
    {
        uint32_t ip;
        int ret = util_ipstr2ip(ip_str.c_str(), &ip);
        if (ret < 0)
        {
            ERR("ip %s is invalid", ip_str.c_str());
            return ret;
        }

        return set(type, ip, mask_num);
    }

    int IPAccessControlItem::set(IPAccessControlType type, in_addr_t ip, int mask_num)
    {
        _type = type;
        _ip = ip;
        if (mask_num < 0 || mask_num > 32)
        {
            return -1;
        }

        int shift = 32 - mask_num;
        _mask = (0xffffffffL << shift);
        return 0;
    }

    bool IPAccessControlItem::match(uint32_t input_ip)
    {
        if ((_ip & _mask) == (input_ip & _mask))
        {
            return true;
        }

        return false;
    }



    IPAccessControl::IPAccessControl()
    {

    }

    void IPAccessControl::add(IPAccessControlItem item)
    {
        _ip_table.push_back(item);
    }

    void IPAccessControl::clear()
    {
        _ip_table.clear();
    }

    bool IPAccessControl::access_valid(string ip_str)
    {
        uint32_t ip;
        int ret = util_ipstr2ip(ip_str.c_str(), &ip);
        if (ret < 0)
        {
            ERR("ip %s is invalid", ip_str.c_str());
            return ret;
        }

        return access_valid(ip);
    }

    bool IPAccessControl::access_valid(in_addr_t ip)
    {
        if (_ip_table.size() == 0)
        {
            return true;
        }

        AccessTable::iterator it = _ip_table.begin();

        for (; it != _ip_table.end(); it++)
        {
            IPAccessControlItem& item = *it;
            switch (item._type)
            {
            case IP_ACCESS_CONTROL_DENY:
                if (item.match(ip))
                {
                    return false;
                }
                break;

            case IP_ACCESS_CONTROL_ALLOW:
                if (item.match(ip))
                {
                    return true;
                }
                break;

            default:
                break;
            }
        }

        return true;
    }

    void IPAccessControl::add_deny_wan()
    {
        IPAccessControlItem item(IP_ACCESS_CONTROL_ALLOW, "10.0.0.0", 8);
        add(item);

        item.set(IP_ACCESS_CONTROL_ALLOW, "127.0.0.0", 8);
        add(item);

        item.set(IP_ACCESS_CONTROL_ALLOW, "172.16.0.0", 12);
        add(item);

        item.set(IP_ACCESS_CONTROL_ALLOW, "192.168.0.0", 16);
        add(item);

        item.set(IP_ACCESS_CONTROL_DENY, "0.0.0.0", 0);
        add(item);
    }
}
