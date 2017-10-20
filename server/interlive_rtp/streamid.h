/**
* @file		streamid.h
* @brief	This file defines stream id format for interlive \n
* @author	songshenyi
* <pre><b>copyright: Youku</b></pre>
* <pre><b>email: </b>songshenyi@youku.com</pre>
* <pre><b>company: </b>http://www.youku.com</pre>
* <pre><b>All rights reserved.</b></pre>
* @date 2014/07/22
*
*/

#ifndef __STREAMID_H__
#define __STREAMID_H__

#include <common/type_defs.h>
#include <stdio.h>
#include <cstring>
#include <string>
#include <stdint.h>

#if __GNUC__ > 3
#include <tr1/unordered_map>
#endif

#ifdef __DEPRECATED
#undef __DEPRECATED
#include <ext/hash_map>
#define __DEPRECATED 1
#else
#include <ext/hash_map>
#endif

#include <uuid/uuid.h>

#ifdef _WIN32
#define NULL 0
#endif

static bool output_uuid = false;
static bool little_endian = false;

#pragma pack(1)
class StreamId_Ext
{
public:
  StreamId_Ext()
  {
    uuid_clear(uuid);
  }

  StreamId_Ext(uint32_t input_id, uint8_t iunput_rate_type = 0)
  {
    uuid_clear(uuid);
    stream_id_32 = input_id;
    rate_type = iunput_rate_type;
  }

  StreamId_Ext(const StreamId_Ext& stream_id)
  {
    id_uint64[0] = stream_id.id_uint64[0];
    id_uint64[1] = stream_id.id_uint64[1];
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
    id_uint64[0] = right.id_uint64[0];
    id_uint64[1] = right.id_uint64[1];
    return *this;
  }

  int32_t parse(const char* str, int radix = 10)
  {
    if (strlen(str) == 36)
    {
      int ret = uuid_parse(str, uuid);
      if (ret < 0)
      {
        return ret;
      }
    }
    else if (strlen(str) == 32)
    {
      if (little_endian)
      {
        for (int i = 0; i < 16; i++)
        {
          int ii;
          sscanf(str + 2 * i, "%2x", &ii);
          id_char[i] = ii;
        }
      }
      else
      {
        for (int i = 0; i < 16; i++)
        {
          int ii;
          sscanf(str + 2 * i, "%2x", &ii);
          id_char[15 - i] = ii;
        }
      }
    }
    else
    {
      int id_32 = strtol(str, (char **)NULL, radix);
      uuid_clear(uuid);
      stream_id_32 = id_32;
    }

    return 0;
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
    return (id_uint64[0] == right.id_uint64[0]) && (id_uint64[1] == right.id_uint64[1]);
  }

  bool operator!=(const StreamId_Ext& right) const
  {
    return (id_uint64[0] != right.id_uint64[0]) || (id_uint64[1] != right.id_uint64[1]);
  }

  int parse(std::string& str)
  {
    return parse(str.c_str());
  }

  char unparse_four_bits(char input) const
  {
    input &= 0x0f;
    if (input < 10)
    {
      return input + '0';
    }

    return input + 'A' - 10;
  }

  char* unparse(char* str) const
  {
    if (output_uuid)
    {
      uuid_unparse(uuid, str);
    }
    else
    {
      if (little_endian)
      {
        for (int i = 0; i < 16; i++)
        {
          char temp1 = id_char[i];
          temp1 >>= 4;
          str[i * 2] = unparse_four_bits(temp1);

          temp1 = id_char[i];
          temp1 &= 0x0f;
          str[i * 2 + 1] = unparse_four_bits(temp1);
        }
        str[32] = 0;
      }
      else
      {
        for (int i = 0; i < 16; i++)
        {
          char temp1 = id_char[15 - i];
          temp1 >>= 4;
          str[i * 2] = unparse_four_bits(temp1);

          temp1 = id_char[15 - i];
          temp1 &= 0x0f;
          str[i * 2 + 1] = unparse_four_bits(temp1);
        }
        str[32] = 0;
      }
    }

    return str;
  }

  std::string unparse() const
  {
    char str[40];
    unparse(str);
    return std::string(str);
  }

  const char* c_str() const
  {
    return unparse().c_str();
  }

  uint64_t get_hash_code() const
  {
    return id_uint64[0] ^ (id_uint64[1] << 32) ^ (id_uint64[1] >> 16);
  }

  StreamId_Ext& parse_default_hash_code(uint64_t hash_code)
  {
    id_uint64[1] = 0;

    id_uint64[0] = hash_code & 0xffffffff;
    id_char[8] = (hash_code >> 32) & 0xff;
    id_char[15] = (hash_code >> 40) & 0xff;

    return *this;
  }

  uint32_t get_32bit_stream_id() const
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
      uint8_t	rate_type;
      uint8_t placeholder[11];
    };
    struct
    {
      uint8_t id_char[16];
    };
    struct
    {
      uint64_t id_uint64[2];
    };
  };

};
#pragma pack()

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

#if __GNUC__ > 3
  namespace tr1
  {
    template <>
    class hash<StreamId_Ext>
    {
    public:
      size_t operator()(const StreamId_Ext& key) const
      {
        size_t hash_num = key.get_hash_code();
        return hash_num;
      }
    };
  }
#endif
}

namespace __gnu_cxx
{
  template <>
  class hash<StreamId_Ext>
  {
  public:
    size_t operator()(const StreamId_Ext& key) const
    {
      size_t hash_num = key.get_hash_code();
      return hash_num;
    }
  };
}

#endif /* __STREAMID_H__ */

