#ifndef FLV_PUBLISHER_H
#define FLV_PUBLISHER_H
#include <map>
#include "streamid.h"
#include "event.h"
//#include <common/proto.h>
#include "media_manager/cache_manager.h"
#include "config_manager.h"
#include "../util/buffer.hpp"
#include <list>

int publisher_init(struct event_base *main_base);
void publisher_on_millsecond(time_t t);
enum FLV_PUBLISHER_STATE {
	FLV_PUBLISHER_STATE_INIT,
	FLV_PUBLISHER_STATE_CONNECTING,
	FLV_PUBLISHER_STATE_CONNECTED,
	FLV_PUBLISHER_STATE_REQ,
	FLV_PUBLISHER_STATE_STREAMING_HEADER,
	FLV_PUBLISHER_STATE_STREAMING_FIRST_TAG,
	FLV_PUBLISHER_STATE_STREAMING_STREAM_TAG,
	FLV_PUBLISHER_STATE_ERROR,
	FLV_PUBLISHER_STATE_END
};

class ModPublisherConfig : public ConfigModule
{
private:
	bool inited;

public:
	char flv_receiver_ip[32];
	uint16_t flv_receiver_port;

	ModPublisherConfig();
	virtual ~ModPublisherConfig() {}
	ModPublisherConfig& operator=(const ModPublisherConfig& rhv);

	virtual void set_default_config();
	virtual bool load_config(xmlnode* xml_config);
	virtual bool reload() const;
	virtual const char* module_name() const;
	virtual void dump_config() const;
};

class FLVPublisherManager;
struct FLVPublisher {
	FLV_PUBLISHER_STATE state;
	buffer *rb, *wb;
	struct event ev;
	StreamId_Ext stream_id;
	uint32_t ip;
	uint16_t port;
	int      fd;
	FLVPublisherManager *manager;
	int64_t read_bytes;
	int64_t write_bytes;
	uint32_t timeoffset;
	buffer *buf;
        int64_t last_error_ts;
	int  seq;
};

class FLVPublisherManager/* :  public WhitelistObserver*/ {
public:
  void registerWatcher(media_manager::FlvCacheManager * cmng);
	FLVPublisher* startStream(const StreamId_Ext &sid);
	void stopStream(const StreamId_Ext &sid);
	void processPacket(FLVPublisher* client,uint16_t cmd,uint32_t pkt_size);
	void notifyStreamData(StreamId_Ext &streamid);
	void sendPublishReq(FLVPublisher* p);
	void set_main_base(struct event_base * main_base);
	//void update(const StreamId_Ext &streamid, WhitelistEvent ev);
	void destroyPublisher(FLVPublisher* p);
	void onTimer(time_t t);
	static FLVPublisherManager* getInst();
private:
	FLVPublisher* createPublisher(const StreamId_Ext &sid);
    void initPublisher(FLVPublisher* p);
	FLVPublisher* findPublisherByStreamid(const StreamId_Ext &sid);
private:
	std::map<StreamId_Ext,FLVPublisher *> publishermap;
  media_manager::FlvCacheManager * _cmng;
};
#endif
