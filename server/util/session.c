#include "session.h"

#include <assert.h>
#include "common.h"
#include "util.h"
#include "log.h"

#define TO_HANDLER(node) container_of(node, session, next_node)

int
session_manager_init(session_manager * fs)
{
	int i = 0;

	for (i = 0; i < (int)COUNT_OF(fs->list_array); i++) {
		INIT_LIST_HEAD(&fs->list_array[i]);
	}
	fs->last_idx = 0;
	fs->last_tick = util_get_curr_tick();
	fs->current_tick = fs->last_tick;
	return 0;
}

void
session_manager_fini(session_manager * fs)
{
#ifndef NDEBUG
	{
		int i = 0;

		for (i = 0; i < (int)COUNT_OF(fs->list_array); i++) {
			assert(list_empty(&fs->list_array[i]));
		}
	}
#else
	UNUSED(fs);
#endif
}

static size_t
calc_position(session_manager * fs, unsigned long current_tick,
unsigned long duration)
{
	unsigned long tmp_tick = current_tick;
	size_t ret = fs->last_idx;

	tmp_tick += duration;
	tmp_tick -= fs->last_tick;
	if (tmp_tick > 0)
		ret += tmp_tick / 1000;
	else
		ret += 1;
	return ret % COUNT_OF(fs->list_array);
}

static void
session_link_node(session_manager * fs, session * handler,
unsigned long current_tick)
{
	assert((size_t)-1 == handler->wheel_index);
	assert(0 != handler->duration);

	handler->wheel_index = calc_position(fs, current_tick, handler->duration);
	//  FTD_LOG_DBG("the index of handler(%p) is %zd.", handler, handler->wheel_index);
	list_add_tail(&handler->next_node, &fs->list_array[handler->wheel_index]);
}

static void
session_unlink_node(session_manager * fs, session * handler)
{
	assert((size_t)-1 != handler->wheel_index);
	assert(handler->wheel_index < COUNT_OF(fs->list_array));
	UNUSED(fs);

	list_del(&handler->next_node);
	handler->wheel_index = (size_t)-1;
}

// should iterate n elem untill n * 1000 >= current_tick - last_tick
void
session_manager_on_second(session_manager * fs, unsigned long current_tick)
{
	struct list_head *node = NULL;
	struct list_head *tmp = NULL;

	//FTD_LOG_DBG("last index of this session is %zd.", fs->last_idx);
	if (current_tick <= fs->current_tick ||
		current_tick - fs->current_tick >= 2 * 1000)
		WRN("miss some tick, record current_tick is %ld, but real is %ld",
		fs->current_tick, current_tick);

	fs->current_tick = current_tick;

	list_for_each_safe(node, tmp, &fs->list_array[fs->last_idx]) {
		session *handler = TO_HANDLER(node);
		unsigned long result = handler->last_active + handler->duration;

		if (fs->current_tick + 1000 >= result) {
			session_func timeout_func = handler->timeout_func;

			//FTD_LOG_DBG("handler(%p) is timeout.", handler);
			assert(fs->last_idx == handler->wheel_index);
			assert(fs == handler->session_mng_ptr);
			session_manager_detach(handler);

			(*timeout_func) (handler);
		}
		else {
			size_t new_pos = 0;
			unsigned long new_duration = result - current_tick;

			assert(NULL != handler->session_mng_ptr);
			new_pos =
				calc_position(handler->session_mng_ptr, current_tick,
				new_duration);
			if (new_pos != handler->wheel_index) {
				session_unlink_node(fs, handler);
				session_link_node(fs, handler, current_tick);
			}
		}
	}

	fs->last_tick = fs->current_tick;
	fs->last_idx++;
	fs->last_idx %= COUNT_OF(fs->list_array);
}



void
session_manager_attach(session_manager * fs, session * handler,
unsigned long current_tick)
{
	assert((size_t)-1 == handler->wheel_index);
	assert(0 != handler->duration);
	assert(NULL == handler->session_mng_ptr);

	handler->session_mng_ptr = fs;
	handler->last_active = current_tick;

	session_link_node(fs, handler, current_tick);
}

void
session_manager_update_duration(session * handler,
unsigned long current_tick,
unsigned long new_duration)
{
	assert(NULL != handler->session_mng_ptr);
	session_manager *fs = handler->session_mng_ptr;

	session_unlink_node(fs, handler);
	handler->duration = new_duration;
	session_link_node(fs, handler, current_tick);
}


void
session_manager_detach(session * handler)
{
	session_manager *fs = handler->session_mng_ptr;

	if (NULL != fs) {
		session_unlink_node(fs, handler);
		SESSION_INIT(handler, NULL, 0);
	}
}
