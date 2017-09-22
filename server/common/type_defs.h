#ifndef __TYPE_DEFS_H__
#define __TYPE_DEFS_H__

#include <stdint.h>

typedef uint32_t stream_id_t;
typedef uint16_t level_t;
typedef uint32_t session_id_t;
typedef uint64_t connectionid_t;
typedef uint16_t asn_t;
typedef uint16_t region_t;
typedef uint32_t hash_value_t;
typedef uint32_t cache_info_key_t;
typedef uint32_t task_id_t;

#ifndef __GNUC__
#define __GNUC__ 3
#endif

#endif/*__TYPE_DEFS_H__*/

