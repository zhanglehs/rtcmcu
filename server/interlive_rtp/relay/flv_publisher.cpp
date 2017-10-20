#include "flv_publisher.h"
#include "../../util/log.h"
#include "util/util.h"
#include "errno.h"
#include "fragment/fragment.h"
#include <netinet/tcp.h>
#include <netinet/in.h>

using namespace fragment;
static struct event_base * g_ev_base = NULL;
static FLVPublisherManager*inst = NULL;
static void enable_write(FLVPublisher* p);
static void disable_write(FLVPublisher* p);
static void handle_read(FLVPublisher* p)
{
	if (p->state == FLV_PUBLISHER_STATE_ERROR)
	{
		return;
	}
	int ret = 1;
	while (ret > 0)
	{
		ret = buffer_read_fd(p->rb, p->fd);
		if (ret < 0) {
			p->state = FLV_PUBLISHER_STATE_ERROR;
			ERR("write packet error streamid %s ret %d",p->stream_id.unparse().c_str(),ret);	
			return;
		}
		p->read_bytes += ret;
		if (buffer_data_len(p->rb) < sizeof(proto_header)) {
			return;
		}
		while (buffer_data_len(p->rb) >= sizeof(proto_header))
		{
			proto_header pro_head;
			if (decode_header(p->rb, &pro_head) < 0) {
				p->state = FLV_PUBLISHER_STATE_ERROR;
				ERR("decode header error streamid %s",p->stream_id.unparse().c_str());
				return;
			}
			if (buffer_data_len(p->rb) < pro_head.size) {
				break;
			}

			if (pro_head.size < sizeof(proto_header))
			{
				p->state = FLV_PUBLISHER_STATE_ERROR;
				ERR("decode header error streamid %s",p->stream_id.unparse().c_str());
				return;
			}

			p->manager->processPacket(p,pro_head.cmd,pro_head.size);

			buffer_eat(p->rb, pro_head.size);
		}

		if (p->fd != -1)
		{
			buffer_try_adjust(p->rb);

			if (buffer_data_len(p->wb) > 0)
			{
			   enable_write(p);
			}
		}
	}
}

static void handle_write(FLVPublisher* p) {
	if (p->state == FLV_PUBLISHER_STATE_ERROR)
	{
		return;
	}

	if (p->state == FLV_PUBLISHER_STATE_CONNECTING)
	{
		p->state = FLV_PUBLISHER_STATE_CONNECTED;
		p->manager->sendPublishReq(p);
	}

	if (buffer_data_len(p->wb) > 0) {
		int ret = buffer_write_fd(p->wb, p->fd);
		if (ret < 0) {
			ERR("write error streamid %s ret %d",p->stream_id.unparse().c_str(),ret);
			p->state = FLV_PUBLISHER_STATE_ERROR;
			disable_write(p);
			return;
		}
		p->write_bytes += ret;
		buffer_try_adjust(p->wb);
	}

	if (buffer_data_len(p->wb) == 0) {
		disable_write(p);
	}
}

static void client_handler(int fd, short which, void* arg)
{
	FLVPublisher* p = (FLVPublisher*)arg;
	//assert(p != NULL);

	if (which & EV_READ) {
		handle_read(p);
	}

	if (p && p->state == FLV_PUBLISHER_STATE_ERROR)
	{
		FLVPublisherManager *mgr = p->manager;
		if (mgr)
		{
			mgr->destroyPublisher(p);
		} 
		else {
			ERR("write packet error mgr null streamid %s",p->stream_id.unparse().c_str());
		}
		return;
	}

	if (which & EV_WRITE) {
		handle_write(p);
	}
}

static void enable_write(FLVPublisher* p) {

	TRC("enable_write: enable write\n");
	event_del(&(p->ev));
	event_set(&(p->ev), p->fd, EV_READ | EV_WRITE | EV_PERSIST, client_handler,p);
	event_base_set(g_ev_base, &(p->ev));
	event_add(&(p->ev), 0);
}
static void disable_write(FLVPublisher* p) {
	TRC("disable_write: disable write\n");
	event_del(&(p->ev));
	event_set(&(p->ev), p->fd, EV_READ | EV_PERSIST, client_handler, p);
	event_base_set(g_ev_base, &(p->ev));
	event_add(&(p->ev), 0);
}

static void register_client_handler(FLVPublisher* p)
{
	event_set(&p->ev, p->fd, EV_READ | EV_WRITE | EV_PERSIST,
	client_handler, (void *)p);
	event_base_set(g_ev_base, &(p->ev));
	event_add(&p->ev, 0);
}

static void close_client(FLVPublisher* p) {
	if (p->state >= FLV_PUBLISHER_STATE_CONNECTING && p->fd >0)
	{
		DBG("close publisher streamid %s",p->stream_id.unparse().c_str());
		event_del(&p->ev);
		close(p->fd);
	}

}

static int do_connect(FLVPublisher* p)
{
	DBG("connect ip %s port %d streamid %s",util_ip2str(p->ip),p->port,p->stream_id.unparse().c_str());
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(p->ip);
	addr.sin_port = htons(p->port);
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		p->state = FLV_PUBLISHER_STATE_ERROR;
		ERR("do_connect: create socket error, errno=%d, err=%s, streamid %s",
			errno, strerror(errno),p->stream_id.unparse().c_str());
		return -1;
	}
	util_set_nonblock(sock, true);
	p->fd = sock;

	int buf_size = 0;
	socklen_t opt_size = sizeof(buf_size);

	getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buf_size, &opt_size);
	DBG("do_connect: socket read buffer size=%d, ip=%s, port=%d",
		buf_size, util_ip2str(p->ip), p->port);

	int sock_rcv_buf_sz = 512 * 1024;
	int ret = 0;
	if (-1 == (ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
		(void*)&sock_rcv_buf_sz, sizeof(sock_rcv_buf_sz))))
	{
		ERR("do_connect: set forward sock rcv buf sz failed. error=%d %s streamid %s",
			errno, strerror(errno),p->stream_id.unparse().c_str());
		close(sock);
		sock = -1;
		p->state = FLV_PUBLISHER_STATE_ERROR;
		return -1;
	}

	if (-1 == (ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF,
		(void*)&sock_rcv_buf_sz, sizeof(sock_rcv_buf_sz))))
	{
		ERR("do_connect: set forward sock rcv buf sz failed. error=%d %s streamid %s",
			errno, strerror(errno),p->stream_id.unparse().c_str());
		close(sock);
		sock = -1;
		p->state = FLV_PUBLISHER_STATE_ERROR;
		return -1;
	}

	int val = 1;

	if (-1 == setsockopt(sock, IPPROTO_TCP, TCP_NODELAY,
		(void *)&val, sizeof(val))) {
			ERR("set tcp nodelay failed. fd = %d, error = %s",
				sock, strerror(errno));
			return -1;
	}


	getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &buf_size, &opt_size);
	DBG("do_connect: socket read buffer size=%d, ip=%s, port=%d",
		buf_size, util_ip2str(p->ip), p->port);

	int r = connect(sock, (struct sockaddr*) &addr, sizeof(addr));

	DBG("do_connect: connect rtn: %d, errno: %d", r, errno);

	if (r < 0) {
		if ((errno != EINPROGRESS) && (errno != EINTR)) {
			WRN("connect error: %d %s %s", errno, strerror(errno),p->stream_id.unparse().c_str());
			p->state = FLV_PUBLISHER_STATE_ERROR;
			return -1;
		}
		p->state = FLV_PUBLISHER_STATE_CONNECTING;
	}
	else if (r == 0) {
		p->state = FLV_PUBLISHER_STATE_CONNECTED;
		p->manager->sendPublishReq(p);
	}
	register_client_handler(p);

	return 0;
}

ModPublisherConfig::ModPublisherConfig()
{
	inited = false;
	memset(flv_receiver_ip, 0, sizeof(flv_receiver_ip));
	flv_receiver_port = 0;
}

ModPublisherConfig& ModPublisherConfig::operator=(const ModPublisherConfig& rhv)
{
	memmove(this, &rhv, sizeof(ModPublisherConfig));
	return *this;
}

void ModPublisherConfig::set_default_config()
{
	strcpy(flv_receiver_ip, "127.0.0.1");
	flv_receiver_port = 80;
}

bool ModPublisherConfig::load_config(xmlnode* xml_config)
{
	xmlnode *p = xmlgetchild(xml_config, "publisher", 0);

	if (inited || !p)
		return true;

	char *q = NULL;

	q = xmlgetattr(p, "flv_receiver_ip");
	if (q)
	{
		strncpy(flv_receiver_ip, q, 31);
	}
	

	q = xmlgetattr(p, "flv_receiver_port");
	if (q)
	{
		flv_receiver_port = atoi(q);
		if (flv_receiver_port <= 0)
		{
			ERR("publisher tacker_port not valid.");
			return false;
		}
	}
	
	inited = true;
	return true;
}

bool ModPublisherConfig::reload() const
{
	return true;
}

const char* ModPublisherConfig::module_name() const
{
	static const char* name = "publisher";
	return name;
}

void ModPublisherConfig::dump_config() const
{
	INF("tracker config: " "flv_receiver_ip=%s, flv_receiver_port=%hu", flv_receiver_ip, flv_receiver_port);
}


FLVPublisher* FLVPublisherManager::startStream(const StreamId_Ext &sid) {
	FLVPublisher* p = findPublisherByStreamid(sid);
	if (!p)
	{
		p = createPublisher(sid);
		if (p)
		{
			initPublisher(p);
		}
	}
	return p;
}
void FLVPublisherManager::stopStream(const StreamId_Ext &sid) {
	FLVPublisher* p = findPublisherByStreamid(sid);
	if (p)
	{
		destroyPublisher(p);
	}
}

void FLVPublisherManager::processPacket(FLVPublisher* client,uint16_t cmd,uint32_t pkt_size) {
	switch (cmd)
	{
	case CMD_U2R_RSP_STATE_V2: {
		u2r_rsp_state_v2 rsp;
		decode_u2r_rsp_state_v2(&rsp, client->rb);
		if (rsp.result == 0)
		{
			client->state = FLV_PUBLISHER_STATE_STREAMING_HEADER;
		}
		DBG("recv publish response streamid %s ret %d",client->stream_id.unparse().c_str(),rsp.result);
		break;
	}
}
}

static void publisher_notify_stream_data(StreamId_Ext stream_id, uint8_t watch_type, void* arg) {
	FLVPublisherManager *manager = (FLVPublisherManager *)arg;
	manager->notifyStreamData(stream_id);
}

void FLVPublisherManager::notifyStreamData(StreamId_Ext &streamid) {
	FLVPublisher *p = findPublisherByStreamid(streamid);
	if (p)
	{
		if (p->state == FLV_PUBLISHER_STATE_STREAMING_HEADER)
		{
			FLVHeader flvheader(streamid);
			if (_cmng->get_miniblock_flv_header(streamid, flvheader) < 0)
			{
				WRN("req live header failed, streamid= %s, streamid %s",
					streamid.unparse().c_str(), p->stream_id.unparse().c_str());
				return;
			}
			else {
				buffer *buf = p->buf;
				u2r_streaming_v2 *stream = (u2r_streaming_v2 *)buffer_data_ptr(buf);
				memcpy(stream->streamid, &streamid, sizeof(StreamId_Ext));
				stream->payload_type = PAYLOAD_TYPE_FLV;
				buffer_append_ptr(buf,stream,sizeof(u2r_streaming_v2));
				flvheader.copy_header_to_buffer(buf);
				stream->payload_size = buffer_data_len(buf) - sizeof(u2r_streaming_v2);
				encode_u2r_streaming_v2(stream,p->wb);
				buffer_reset(buf);
				enable_write(p);
			}

			p->state = FLV_PUBLISHER_STATE_STREAMING_FIRST_TAG;
		} 
		if (p->state == FLV_PUBLISHER_STATE_STREAMING_FIRST_TAG)
		{
			FLVMiniBlock* block = _cmng->get_latest_miniblock(streamid);
      if (block)
			{
				int bid = block->get_seq();
				p->seq = bid + 1;

				buffer *buf = p->buf;
				u2r_streaming_v2 *stream = (u2r_streaming_v2 *)buffer_data_ptr(buf);
				memcpy(stream->streamid, &streamid, sizeof(StreamId_Ext));
				stream->payload_type = PAYLOAD_TYPE_FLV;
				buffer_append_ptr(buf,stream,sizeof(u2r_streaming_v2));
				block->copy_payload_to_buffer(buf, p->timeoffset, FLV_FLAG_BOTH);
				stream->payload_size = buffer_data_len(buf) - sizeof(u2r_streaming_v2);
				encode_u2r_streaming_v2(stream,p->wb);
				buffer_reset(buf);
				enable_write(p);
				p->state = FLV_PUBLISHER_STATE_STREAMING_STREAM_TAG;
			}
		} 
		if (p->state == FLV_PUBLISHER_STATE_STREAMING_STREAM_TAG)
		{
      int count = 0;
      fragment::FLVMiniBlock* block = NULL;
      while ((block = _cmng->get_miniblock_by_seq(streamid, p->seq)) != NULL) {
        int bid = block->get_seq();
        p->seq = bid + 1;

        buffer *buf = p->buf;
        u2r_streaming_v2 *stream = (u2r_streaming_v2 *)buffer_data_ptr(buf);
        memcpy(stream->streamid, &streamid, sizeof(StreamId_Ext));
        stream->payload_type = PAYLOAD_TYPE_FLV;
        buffer_append_ptr(buf, stream, sizeof(u2r_streaming_v2));
        block->copy_payload_to_buffer(buf, p->timeoffset, FLV_FLAG_BOTH);
        stream->payload_size = buffer_data_len(buf) - sizeof(u2r_streaming_v2);
        encode_u2r_streaming_v2(stream, p->wb);
        buffer_reset(buf);

        count++;
			}

			if (count > 0)
			{
				enable_write(p);
			}
		}
		
	} else {	
		startStream(streamid);
	}
}

FLVPublisher* FLVPublisherManager::createPublisher(const StreamId_Ext &sid) {
	FLVPublisher *ret = new FLVPublisher();
	if (ret)
	{
		ret->state = FLV_PUBLISHER_STATE_INIT;
		ret->wb = buffer_init(5*1024*1024);
		ret->rb = buffer_init(5*1024*1024);
		ret->buf = buffer_init(1024*1024);
		ret->stream_id = sid;
		ModPublisherConfig* publisher_config = (ModPublisherConfig*)ConfigManager::get_inst_config_module("publisher");
		util_ipstr2ip(publisher_config->flv_receiver_ip, &ret->ip);
		ret->port = publisher_config->flv_receiver_port;
		//util_ipstr2ip("127.0.0.1", &ret->ip);
		//ret->port = 80;
		ret->fd = -1;
		ret->manager = this;
                ret->last_error_ts = 0;
		publishermap[sid] = ret;
	}
	return ret;
}
void FLVPublisherManager::initPublisher(FLVPublisher* p) {
	do_connect(p);
}
void FLVPublisherManager::sendPublishReq(FLVPublisher* p) {
	DBG("send publish req streamid %s",p->stream_id.unparse().c_str());
	u2r_req_state_v2 req;
	req.version = 2;
	memcpy(req.streamid, &p->stream_id, STREAM_ID_LEN);
	memset((char*)req.token, 0, sizeof(req.token));
	memcpy((char*)req.token, "98765", 5);
	req.payload_type = PAYLOAD_TYPE_FLV;
	encode_u2r_req_state_v2(&req,p->wb);
}
void FLVPublisherManager::destroyPublisher(FLVPublisher* p) {
	close_client(p);
	if(findPublisherByStreamid(p->stream_id)) {
	   publishermap.erase(p->stream_id);
	}
	buffer_free(p->rb);
	buffer_free(p->wb);
	buffer_free(p->buf);
	delete p;
}
FLVPublisher* FLVPublisherManager::findPublisherByStreamid(const StreamId_Ext &sid) {
	FLVPublisher* ret = NULL;
	std::map<StreamId_Ext,FLVPublisher *>::iterator it = publishermap.find(sid);
	if(it != publishermap.end())
	{
		ret = it->second;
	}
	return ret;
}

void FLVPublisherManager::registerWatcher(media_manager::FlvCacheManager * cmng) {
	_cmng = cmng;
	cmng->register_watcher(publisher_notify_stream_data, media_manager::CACHE_WATCHING_FLV_MINIBLOCK, this);
}

FLVPublisherManager* FLVPublisherManager::getInst() {
	if (inst == NULL)
	{
		inst = new FLVPublisherManager();
	}
	return inst;
}

// 白名单通知某个流下线了，目前白名单通知机制被我注释掉了，所以该函数不会被调用
//void FLVPublisherManager::update(const StreamId_Ext &streamid, WhitelistEvent ev) {
//	if (ev == WHITE_LIST_EV_STOP)
//	{
//		FLVPublisherManager::getInst()->stopStream(streamid);
//	}
//}

void FLVPublisherManager::set_main_base(struct event_base * main_base) {
	g_ev_base = main_base;
}

void FLVPublisherManager::onTimer(time_t t) {
	std::list<FLVPublisher *> trash;
	std::map<StreamId_Ext,FLVPublisher*>::iterator it = publishermap.begin();
	for (;it != publishermap.end();it++)
	{
		FLVPublisher* p = it->second;
		if (p && p->state == FLV_PUBLISHER_STATE_ERROR)
		{
			if (p->last_error_ts == 0)
			{
				p->last_error_ts = time(NULL);
			} else if(time(NULL) - p->last_error_ts > 3) {
				trash.push_back(p);
			}
		}
	}

	std::list<FLVPublisher *>::iterator trash_it = trash.begin();
	for (;trash_it != trash.end();trash_it++)
	{
		destroyPublisher(*(trash_it));
	}
}

int publisher_init(struct event_base *main_base) {
	FLVPublisherManager::getInst()->set_main_base(main_base);
	FLVPublisherManager::getInst()->registerWatcher(media_manager::FlvCacheManager::Instance());
	//SINGLETON(WhitelistManager)->add_observer(FLVPublisherManager::getInst(), WHITE_LIST_EV_STOP);
  return 0;
}

void publisher_on_millsecond(time_t t) {
	FLVPublisherManager::getInst()->onTimer(t);
}
