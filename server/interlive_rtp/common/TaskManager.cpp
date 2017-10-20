#include "common/TaskManager.h"

TaskManger* TaskManger::m_inst = NULL;

TaskManger* TaskManger::Instance() {
  if (m_inst) {
    return m_inst;
  }
  m_inst = new TaskManger();
  return m_inst;
}

void TaskManger::DestroyInstance() {
  if (m_inst) {
    delete m_inst;
    m_inst = 0;
  }
}

TaskManger::TaskManger() {
  m_ev_base = NULL;
  m_trash_task = NULL;
}

TaskManger::~TaskManger() {
  for (auto it = m_tasks.begin(); it != m_tasks.end(); it++) {
    event_free(*it);
  }
  m_tasks.clear();
  if (m_trash_task) {
    event_free(m_trash_task);
    m_trash_task = NULL;
  }
}

void TaskManger::Init(struct event_base *ev_base) {
  m_ev_base = ev_base;
}

void TaskManger::PostTask(std::function<void()> task, unsigned int delay_ms /*= 0*/) {
  struct event *ev = event_new(m_ev_base, -1, 0, &TaskManger::OnTask, new std::function<void()>(task));
  m_tasks.insert(ev);
  struct timeval tv;
  tv.tv_sec = delay_ms / 1000;
  tv.tv_usec = (delay_ms % 1000) * 1000;
  event_add(ev, &tv);
}

void TaskManger::OnTask(evutil_socket_t fd, short event, void *arg) {
  TaskManger::Instance()->OnTaskImpl(fd, event, arg);
}

void TaskManger::OnTaskImpl(evutil_socket_t fd, short event, void *arg) {
  if (m_trash_task) {
    event_free(m_trash_task);
    m_trash_task = NULL;
  }

  std::function<void()> *task = (std::function<void()> *)arg;
  (*task)();
  delete task;

  struct event *ev = event_base_get_running_event(m_ev_base);
  m_tasks.erase(ev);
  m_trash_task = ev;
}
