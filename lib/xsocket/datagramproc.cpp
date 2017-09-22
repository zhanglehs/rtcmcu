#include "common.hpp"
#include "topicsconfig.hpp"
#include "datagramproc.hpp"

DatagramProc::DatagramProc(string root_path)
        :_root_path(root_path),
        _maxConnetions(1024),
        m_recv_num(0),
        m_recv_fwd_num(0),
        m_send_num(0),
        m_send_fwd_num(0)
{
}
DatagramProc::DatagramProc(IOverflowHandler& handler, uint32_t maxConnetions)
        :_maxConnetions(maxConnetions),
        m_recv_num(0),
        m_recv_fwd_num(0),
        m_send_num(0),
        m_send_fwd_num(0)
{
}

int DatagramProc::init()
{
    int rv = 0;
    string cfg_file = _root_path + "/conf/topics.conf";
    TopicsConfig topics_conf(cfg_file);
    TopicGroupVector topic_groups;

    rv = topics_conf.get_topicgroups(topic_groups);
    if(rv < 0)
    {
        LOG4CPLUS_ERROR(g_logger, "init topic conf failed");
        return -1;
    }

    //find out groups
    TopicGroupVector::iterator iter = topic_groups.begin();

    for(;iter != topic_groups.end(); iter++)
    {
        TopicGroup& group = *iter;
        shared_memory_transport* channel = new shared_memory_transport;
        string address = _root_path + "/keys/" + group.name;

        rv = channel->listen(address);

        if(rv < 0)
        {
            LOG4CPLUS_ERROR(g_logger,"init IO channel failed");
            return -1;
        }
        TopicConfVector::const_iterator iter_topics = group.topics.begin();
        for(; iter_topics !=  group.topics.end(); iter_topics++)
        {
            TopicConf topic_cfg = *iter_topics;
            _topicidChannels.insert(TopicidChannelMap::value_type(topic_cfg.topic.getID(), channel));
        }
    }

    return 0;
}

int  DatagramProc::frame(ByteBuffer& bb)
{
    DatagramFactory factory;
    return factory.frame(bb);
}

void DatagramProc::accept(ConnectionInfo& ci)
{
    _connections.insert(&ci);
    _idConnections.insert(ConnectionMap::value_type(ci.getId(), &ci));
}
void DatagramProc::remove(ConnectionInfo& ci)
{
    _connections.erase(&ci);
    _idConnections.erase(ci.getId());
    /*
    	UnBindConsumerDatagram unbind;
    	unbind.sessionid = ci.getId();

    	char buf[DatagramFactory::MQ_MAX_DATAGRAM_LENGTH];
    	ByteBuffer bb(buf, DatagramFactory::MQ_MAX_DATAGRAM_LENGTH, DatagramFactory::MQ_MAX_DATAGRAM_LENGTH);

    	DatagramFactory factory;
    	factory.encode(bb, unbind);
    	bb.flip();
    	_ioChannel.send(bb);
    */
}
void DatagramProc::process(ConnectionInfo& conn, ByteBuffer& packet)
{
    int rv = 0;
    DatagramFactory factory;

    int type = factory.getMsgType(packet);

    if(type == Datagram::DG_HEATBEAT)
    {
        IOverflowHandler h;
        char buf[DatagramFactory::MQ_MAX_DATAGRAM_LENGTH];

        ByteBuffer  buffer(buf, DatagramFactory::MQ_MAX_DATAGRAM_LENGTH, DatagramFactory::MQ_MAX_DATAGRAM_LENGTH);

        HeatbeatRespDatagram heatbeat_resp;

        factory.encode(buffer, heatbeat_resp);
        buffer.flip();
        conn.output(buffer, h);

        LOG4CPLUS_INFO(g_logger, "send a heatbeat resp");
    }
    else
    {

        factory.setSessionid(packet, conn.getId());

        m_recv_num++;

        int topic_id = factory.getTopicid(packet);
        TopicidChannelMap::iterator iter = _topicidChannels.find(topic_id);
        if(iter == _topicidChannels.end())
        {
            LOG4CPLUS_ERROR(g_logger,"can not find channel, topic = " <<topic_id);
            return ;
        }

        shared_memory_transport* channel  = iter->second;
        rv = channel->send(packet);
        if(rv>0)
        {
            m_recv_fwd_num++;
        }
        else
        {
            //_ioChannel.dump();
            LOG4CPLUS_ERROR(g_logger,"send packet to IO channel failed");
        }
    }
}

int DatagramProc::poll(uint32_t timeout)
{
    int rv = 0;

    TopicidChannelMap::const_iterator iter = _topicidChannels.begin();
    for(; iter != _topicidChannels.end(); iter++)
    {
        shared_memory_transport& channel = *(iter->second);
        poll_channel(channel);
    }
    return rv;
}

int DatagramProc::poll_channel(shared_memory_transport& channel)
{
    int rv = 0;
    char buf[DatagramFactory::MQ_MAX_DATAGRAM_LENGTH];
    ByteBuffer bb(buf, DatagramFactory::MQ_MAX_DATAGRAM_LENGTH, DatagramFactory::MQ_MAX_DATAGRAM_LENGTH);
    DatagramFactory factory;
    IOverflowHandler h;

    for(int i=0; i<DEFAULT_MAX_IN_DATAGRAMS; i++)
    {
        bb.clear();
        rv = channel.receive(bb);
        if(rv > 0)
        {
            //channel.dump();
            bb.flip();

            uint32_t sessionid = factory.getSessionid(bb);
            //uint32_t consumerseq = factory.getConsumerseq(bb);
            //cout<<"out a packet consumer seq = "<<consumerseq<<endl;
            ConnectionInfo* conn =  _idConnections[sessionid];

            m_send_num++;

            if(conn)
            {
                conn->output(bb,h);
                m_send_fwd_num++;
            }
            else
            {
                LOG4CPLUS_ERROR(g_logger,"can not find connection of session="<<sessionid);
            }
            //LOG4CPLUS_INFO(g_logger,"mtcp total send:fwd num = "<<m_send_num<<":"<<m_send_fwd_num);
        }
        else if(rv < 0)
        {
            LOG4CPLUS_ERROR(g_logger,"receive from IO channel failed");
            rv = -1;
            break;

        }
        else
        {
            break;
        }
    }

    return 0;
}

