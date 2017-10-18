#ifndef FORWARD_CLIENT_RTP_TCP_H
#define FORWARD_CLIENT_RTP_TCP_H

#include <map>
#include <common/proto.h>
//#include "../rtp_trans/rtp_trans_id.h"
#include "../rtp_trans/rtp_trans_manager.h"
#include "../streamid.h"
#include "event.h"
#include "config_manager.h"
#include "uploader/RtpTcpConnectionManager.h"

class FCRTPConfig : public ConfigModule
{
private:
	bool inited;

public:
	short listen_port;

public:
	FCRTPConfig();
	virtual ~FCRTPConfig();
	FCRTPConfig& operator=(const FCRTPConfig& rhv);
	virtual void set_default_config();
	virtual bool load_config(xmlnode* xml_config);
	virtual bool reload() const;
	virtual const char* module_name() const;
	virtual void dump_config() const;

private:
	bool load_config_unreloadable(xmlnode* xml_config);
	bool load_config_reloadable(xmlnode* xml_config);
	bool resove_config();
};

class RtpPullClient;

// TODO: zhangle, 如果一路流没有人拉流（即同时没有rtp和flv拉取），那么pull应该断开
class RtpPullTcpManager: public RtpTcpManager {
public:
  void StartPull(const StreamId_Ext& streamid);
  void StopPull(const StreamId_Ext& streamid);
  int Init(struct event_base * ev_base);

protected:
  std::map<StreamId_Ext, RtpPullClient*> m_clients;
};

class RtpPushClient;
// TODO: zhangle, rtp失败后应重试
class RtpPushTcpManager : public RtpTcpManager {
public:
  int Init(struct event_base * ev_base);
  void StartPush(const StreamId_Ext& streamid);
  void StopPush(const StreamId_Ext& streamid);

protected:
  std::map<StreamId_Ext, RtpPushClient*> m_clients;
};

#endif
