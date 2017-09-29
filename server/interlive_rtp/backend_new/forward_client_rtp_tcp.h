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
class ForwardClientRtpTCPMgr: public RtpTcpManager {
public:
  void startStream(const StreamId_Ext& streamid);
  void stopStream(const StreamId_Ext& streamid);
  void set_main_base(struct event_base * main_base);
  static ForwardClientRtpTCPMgr* Instance();
  static void DestroyInstance();
private:
  std::map<StreamId_Ext, RtpPullClient*> m_clients;
  static ForwardClientRtpTCPMgr *m_inst;
};

#endif
