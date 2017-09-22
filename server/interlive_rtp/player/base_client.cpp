/************************************************************************
* author: zhangcan
* email: zhangcan@youku.com
* date: 2014-12-30
* copyright:youku.com
************************************************************************/

#include "base_client.h"

#include <assert.h>
#include <algorithm>
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

#include <ext/hash_map>

#include "util/util.h"
#include "util/log.h"
#include "util/access.h"
#include "util/levent.h"
#include "util/session.h"
#include "fragment/fragment.h"
#include "connection.h"
#include "target.h"
#include "player.h"

using namespace std;
using namespace __gnu_cxx;
using namespace fragment;
using namespace media_manager;

#define NOCACHE_HEADER "Expires: -1\r\nCache-Control: private, max-age=0\r\nPragma: no-cache\r\n"
#define WRITE_MAX (64 * 1024)

static void fragment_player_handler(const int fd, const short which, void *arg)
{
  BaseClient * ccs = (BaseClient *)arg;
  connection * c = ccs->get_connection();
  Player * p = (Player *)ccs->get_player();
  if (!ccs || !c || !(c->bind))
  {
    DBG("unexpected connection ");
    return;
  }

  if (which & EV_WRITE)
  {
    switch (c->bind_flag)
    {
    case PLAYER_V2_FRAGMENT:
      if (ccs->consume_data() == -1 || ccs->is_state_stop())
      {
        p->disconnect_client(*ccs);
      }
      return;
    default:
      DBG("unexpected connection bind_flag. %d", c->bind_flag);
    }
  }
  else
  {
    DBG("fragment_player_handler ccs is for reading");
    char buf[128] = { 0 };
    read(c->fd, buf, 128);
    p->disconnect_client(*ccs);
  }
}

void BaseClient::enable_consume_data()
{
  enable_write(get_connection(), fragment_player_handler);
  switch_to_eat();
  //    std::cout << "enable write" << std::endl;
}

void BaseClient::disable_consume_data()
{
  disable_write(get_connection(), fragment_player_handler);
  switch_to_hungry();
  //    std::cout << "disable write" << std::endl;
}

void BaseClient::enable_write(connection *c, void(*handler) (int fd, short which, void *arg))
{
  if (c)
  {
    levent_del(&c->ev);
    levent_set(&c->ev, c->fd, EV_READ | EV_WRITE | EV_PERSIST, handler, (void *) this);
    event_base_set(_main_base, &c->ev);
    levent_add(&c->ev, 0);
  }
}

void BaseClient::disable_write(connection *c, void(*handler) (int fd, short which, void *arg))
{
  if (c)
  {
    levent_del(&c->ev);
    levent_set(&c->ev, c->fd, EV_READ | EV_PERSIST, handler, (void *) this);
    event_base_set(_main_base, &c->ev);
    levent_add(&c->ev, 0);
  }
}

void BaseClient::switch_to_hungry()
{
  eat_state = BaseClient::EAT_HUNGRY;
}

void BaseClient::switch_to_eat()
{
  eat_state = BaseClient::EATING;
}

int BaseClient::consume_data()
{
  if (NULL == c_)
  {
    return 0;
  }

  if (0 == buffer_data_len(c_->w))
  {
    request_data();
  }

  int ret = 0;
  TRC("buffer_data_len= %lu", buffer_data_len(c_->w));
  if (0 == buffer_data_len(c_->w) && !is_state_stop()) {
    disable_consume_data();
    return 0;
  }
  else if (0 < buffer_data_len(c_->w))
  {
    if ((ret = buffer_write_fd_max(c_->w, c_->fd, buffer_data_len(c_->w))) < 0)
    {
      return -1;
    }
    else
    {
      _send_data += ret;
      _send_data_interval += ret;
    }
  }

  if (buffer_data_len(c_->w) == 0 && is_state_stop())
  {
    return -1;
  }

  TRC("fd write %d bytes before session active block id =%d", ret, get_play_request().get_global_block_seq_());
  set_update_time(time(NULL));
  buffer_try_adjust(c_->w);
  return ret;
}
