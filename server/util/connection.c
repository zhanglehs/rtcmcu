#include "connection.h"
#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <event.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "log.h"
#include "util.h"
#include "utils/buffer.hpp"
#include "session.h"
#include "levent.h"

static void
conn_timeout(session * timer)
{
	connection *c = container_of(timer, connection, timer);

	assert(NULL == c->bind);
	DBG("connection timeout. #%d(%s:%hu)", c->fd, c->remote_ip,
		c->remote_port);
	conn_close(c);
}

int
conn_init(connection * c, int fd, size_t buffer_max,
struct event_base *eb, void(*handler) (int fd, short which,
	void *arg),
	session_manager * sm, int timeout_sec, int bind_flag)
{
	assert(-1 == c->fd);
	c->fd = fd;
	levent_set(&c->ev, fd, EV_READ | EV_WRITE | EV_PERSIST, handler,
		(void *)c);
	c->bind = NULL;
	c->bind_flag = bind_flag;
	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	int ret = getpeername(c->fd, (struct sockaddr *) &addr, &len);

	if (-1 == ret) {
		ERR("getpeername failed. fd = %d, error = %s", c->fd,
			strerror(errno));
		goto failed;
	}
	util_ip2str_no_r(addr.sin_addr.s_addr, c->remote_ip, sizeof(c->remote_ip));
	c->remote_port = ntohs(addr.sin_port);
	ret = getsockname(c->fd, (struct sockaddr *) &addr, &len);
	if (-1 == ret) {
		ERR("getsockname failed. fd = %d, error = %s", c->fd,
			strerror(errno));
		goto failed;
	}
	util_ip2str_no_r(addr.sin_addr.s_addr, c->local_ip, sizeof(c->local_ip));
	c->local_port = ntohs(addr.sin_port);
	c->r = buffer_create_max(16 * 1024, buffer_max);
	c->w = buffer_create_max(512 * 1024, buffer_max);
	if (NULL == c->r || NULL == c->w) {
		ERR("create read or write buffer failed. %d : %zu", (int)16 * 1024,
			buffer_max);
		goto failed;
	}
	SESSION_INIT(&c->timer, conn_timeout, timeout_sec * 1000);
	session_manager_attach(sm, &c->timer, util_get_curr_tick());
	event_base_set(eb, &(c->ev));
	levent_add(&(c->ev), 0);
	return 0;

failed:
	if (c->r)
		buffer_free(c->r);
	if (c->w)
		buffer_free(c->w);
	memset(c, 0, sizeof(*c));
	c->fd = -1;
	c->r = c->w = NULL;
	c->bind = NULL;
	c->bind_flag = -1;
	return -1;
}

void
conn_close(connection * c)
{
	if (-1 == c->fd) {
		WRN("connection is already closed...");
		return;
	}
	levent_del(&c->ev);
	close(c->fd);
    if (c->r)
		buffer_free(c->r);
	if (c->w)
		buffer_free(c->w);
	session_manager_detach(&c->timer);
	memset(c, 0, sizeof(*c));
	c->fd = -1;
	c->r = c->w = NULL;
	c->bind = NULL;
	c->bind_flag = -1;
}

void
conn_enable_write(connection * c, struct event_base *eb,
void(*handler) (int fd, short which, void *arg))
{
	if (c) {
		levent_del(&c->ev);
		levent_set(&c->ev, c->fd, EV_READ | EV_WRITE | EV_PERSIST,
			handler, (void *)c);
		event_base_set(eb, &c->ev);
		levent_add(&c->ev, 0);
	}
}

void
conn_disable_write(connection * c, struct event_base *eb,
void(*handler) (int fd, short which, void *arg))
{
	if (c) {
		levent_del(&c->ev);
		levent_set(&c->ev, c->fd, EV_READ | EV_PERSIST, handler, (void *)c);
		event_base_set(eb, &c->ev);
		levent_add(&c->ev, 0);
	}
}
