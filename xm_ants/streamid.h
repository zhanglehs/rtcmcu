/**
* @file     streamid.h
* @brief    This file defines stream id format for interlive \n
* @author   songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/07/22
*
*/

#ifndef __STREAMID_H__
#define __STREAMID_H__

#include <stdio.h>
#include <string>
#include <tr1/unordered_map>
#include <uuid/uuid.h>

#include "utils/city.h" 

#ifdef _WIN32
#define NULL 0
#endif

static bool output_uuid = false;
static bool little_endian = false;

#pragma pack(push)
#pragma pack(4)
class StreamId_Ext
{
public:
    StreamId_Ext()
    {
        uuid_clear(uuid);
    }

    StreamId_Ext(uint32_t input_id, uint8_t iunput_rate_type)
    {
        uuid_clear(uuid);
        stream_id_32 = input_id;
        rate_type = iunput_rate_type;
    }

    StreamId_Ext(const StreamId_Ext& stream_id)
    {
        uuid_copy(uuid, stream_id.uuid);
    }

    StreamId_Ext(const uuid_t& input_uuid)
    {
        uuid_copy(uuid, input_uuid);
    }

    StreamId_Ext& operator=(const uint32_t& input_id)
    {
        uuid_clear(uuid);
        stream_id_32 = input_id;
        return *this;
    }

    StreamId_Ext& operator=(const uuid_t& input_uuid)
    {
        uuid_copy(uuid, input_uuid);
        return *this;
    }

    StreamId_Ext& operator=(const StreamId_Ext& right)
    {
        uuid_copy(uuid, right.uuid);
        return *this;
    }

    int32_t parse(const char* str)
    {
        int32_t ret = uuid_parse(str, uuid);

        return ret;
    }

    bool operator<(const StreamId_Ext& right) const
    {
        return (uuid_compare(uuid, right.uuid) < 0);
    }

    bool operator>(const StreamId_Ext& right) const
    {
        return (uuid_compare(uuid, right.uuid) > 0);
    }

    bool operator==(const StreamId_Ext& right) const
    {
        return (uuid_compare(uuid, right.uuid) == 0);
    }

    bool operator!=(const StreamId_Ext& right) const
    {
        return (uuid_compare(uuid, right.uuid) != 0);
    }

    int parse(std::string& str)
    {
        if (str.length() == 36)
        {
            int ret = uuid_parse(str.c_str(), uuid);
            if (ret < 0)
            {
                return ret;
            }
        }
        else if (str.length() == 32)
        {
            if (little_endian)
            {
                for (int i = 0; i < 16; i++)
                {
                    int ii;
                    sscanf(str.c_str() + 2 * i, "%2x", &ii);
                    id_char[i] = ii;
                }
            }
            else
            {
                for (int i = 0; i < 16; i++)
                {
                    int ii;
                    sscanf(str.c_str() + 2 * i, "%2x", &ii);
                    id_char[15 - i] = ii;
                }
            }
        }
        else
        {
            int id_32 = atoi(str.c_str());
            uuid_clear(uuid);
            stream_id_32 = id_32;
        }

        return 0;
    }

    char* unparse(char* str)
    {
        uuid_unparse(uuid, str);
        return str;
    }

    std::string unparse() const
    {
        char c[40];
        if (output_uuid)
        {
            uuid_unparse(uuid, c);
        }
        else
        {
            if (little_endian)
            {
                for (int i = 0; i < 16; i++)
                {
                    sprintf(((char*)c) + i * 2, "%02x", id_char[i]);
                }
            }
            else
            {
                for (int i = 0; i < 16; i++)
                {
                    sprintf(((char*)c) + i * 2, "%02x", id_char[15 - i]);
                }
            }
        }

        return std::string(c);
    }

    uint32_t get_32bit_stream_id()
    {
        return stream_id_32;
    }

public:
    static uint32_t get_32bit_stream_id(StreamId_Ext& id)
    {
        return id.stream_id_32;
    }

    static uint8_t get_rate_type(StreamId_Ext& id)
    {
        return id.rate_type;
    }

public:
    union
    {
        uuid_t uuid;
        struct
        {
            uint32_t stream_id_32;
            uint8_t rate_type;
            uint8_t placeholder[11];
        };
        struct
        {
            uint8_t id_char[16];
        };
    };

};
#pragma pack(pop)

namespace std
{
    template <>
    class equal_to<StreamId_Ext>
    {
    public:
        bool operator()(const StreamId_Ext& left, const StreamId_Ext& right) const
        {
            return (left == right);
        }
    };

    namespace tr1
    {
        template <>
        class hash<StreamId_Ext>
        {
        public:
            size_t operator()(const StreamId_Ext& key) const
            {
                size_t hash_num = CityHash64((char*)(&key), sizeof(key));
                return hash_num;
            }
        };
    }
}

#endif /* __STREAMID_H__ */
