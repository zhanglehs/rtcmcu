#ifndef _COMMON_TASK_MANAGER_H_
#define _COMMON_TASK_MANAGER_H_

#include <event.h>
#include <functional>
#include <set>

class TaskManger {
public:
  static TaskManger* Instance();
  static void DestroyInstance();

  void Init(struct event_base *ev_base);
  void PostTask(std::function<void()> task, unsigned int delay_ms = 0);

protected:
  TaskManger();
  ~TaskManger();
  static void OnTask(evutil_socket_t fd, short event, void *arg);
  void OnTaskImpl(evutil_socket_t fd, short event, void *arg);
  struct event_base *m_ev_base;
  std::set<struct event*> m_tasks;
  struct event* m_trash_task;
  static TaskManger* m_inst;
};

#endif
