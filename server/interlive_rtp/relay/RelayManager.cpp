#include "relay/RelayManager.h"

#include "relay/RtpRelay.h"

#define ENABLE_RTP_PULL
//#define ENABLE_RTP_PUSH

RelayManager* RelayManager::m_inst = NULL;

RelayManager* RelayManager::Instance() {
  if (m_inst) {
    return m_inst;
  }
  m_inst = new RelayManager();
  return m_inst;
}

void RelayManager::DestroyInstance() {
  if (m_inst) {
    delete m_inst;
    m_inst = NULL;
  }
}

RelayManager::RelayManager() {
  m_push_manager = new RtpPushTcpManager();
  m_pull_manager = new RtpPullTcpManager();
}

RelayManager::~RelayManager() {
  delete m_pull_manager;
  delete m_push_manager;
}

int RelayManager::Init(struct event_base *ev_base) {
  m_pull_manager->Init(ev_base);
  m_push_manager->Init(ev_base);
  return 0;
}

int RelayManager::StartPullRtp(const StreamId_Ext& stream_id) {
#ifdef ENABLE_RTP_PULL
  m_pull_manager->StartPull(stream_id);
#endif
  return 0;
}

int RelayManager::StopPullRtp(const StreamId_Ext& stream_id) {
  m_pull_manager->StopPull(stream_id);
  return 0;
}

int RelayManager::StartPushRtp(const StreamId_Ext& stream_id) {
#ifdef ENABLE_RTP_PUSH
  m_push_manager->StartPush(stream_id);
#endif
  return 0;
}

int RelayManager::StopPushRtp(const StreamId_Ext& stream_id) {
  m_push_manager->StopPush(stream_id);
  return 0;
}
