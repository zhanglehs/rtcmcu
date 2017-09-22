#ifndef SESSION_H
#define SESSION_H

#include <string.h>
#include <stdlib.h>
#include "list.h"

#ifdef _WIN32
typedef __SIZE_TYPE__ size_t;
#endif

#if (defined __cplusplus) && (!defined _WIN32)
extern "C"
{
#endif
typedef struct session_manager
{
    struct list_head list_array[64];
    size_t last_idx;
    unsigned long last_tick;
    unsigned long current_tick;
} session_manager;

struct session;
typedef void (*session_func) (struct session *);

typedef struct session
{
    unsigned long last_active;
    struct list_head next_node;
    session_manager *session_mng_ptr;
    size_t wheel_index;
    session_func timeout_func;
    unsigned long duration;
} session;

#define SESSION_INIT(session, func, new_duration_ms) do {   \
        memset(&(session)->next_node, 0, sizeof((session)->next_node)); \
        (session)->session_mng_ptr = NULL;                              \
        (session)->wheel_index = (size_t)-1;                    \
        (session)->timeout_func = func;                         \
        (session)->last_active = 0;                             \
        (session)->duration    = new_duration_ms;                   \
    } while(0)

#define SESSION_SETF(h, f) do {     \
        (h)->timeout_func = (f);                \
    } while (0)


int session_manager_init(session_manager * fs);
void session_manager_fini(session_manager *);

void session_manager_on_second(session_manager *, unsigned long current_tick);

void session_manager_attach(session_manager *, session *,
                            unsigned long current_tick);
void session_manager_update_duration(session *, unsigned long current_tick,
                                     unsigned long new_duration);
void session_manager_detach(session *);

static inline void
session_manager_on_active(session * h, unsigned long current_tick)
{
    h->last_active = current_tick;
}

#if (defined __cplusplus && !defined _WIN32)
}
#endif
#endif // FTD_SESSION_H
