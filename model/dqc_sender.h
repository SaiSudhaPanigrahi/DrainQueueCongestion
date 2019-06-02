#pragma once
#include <memory>
#include "ns3/event-id.h"
#include "ns3/callback.h"
#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "dqc_clock.h"
#include "proto_stream.h"
#include "proto_bandwidth.h"
#include "proto_time.h"
#include "packet_number.h"
#include "socket_address.h"
#include "proto_socket.h"
#include "process_alarm_factory.h"
#include "proto_con.h"
namespace ns3{
class DqcSender;
class FakePackeWriter:public dqc::Socket{
public:
	FakePackeWriter(DqcSender *sender):m_sender(sender){}
	~FakePackeWriter(){}
	int SendTo(const char*buf,size_t size,dqc::SocketAddress &dst) override;
private:
	DqcSender *m_sender{nullptr};
};
class DqcSender: public Application,
public dqc::ProtoStream::StreamCanWriteVisitor{
public:
    DqcSender();
    ~DqcSender(){}
	void Bind(uint16_t port);
	InetSocketAddress GetLocalAddress();
	void ConfigurePeer(Ipv4Address addr,uint16_t port);    
	void OnCanWrite() override{
		DataGenerator(2);
	}
	void SendToNetwork(Ptr<Packet> p);
private:
	void DataGenerator(int times);
	virtual void StartApplication() override;
	virtual void StopApplication() override;
    void RecvPacket(Ptr<Socket> socket);
    void Process();
    dqc::PacketNumber AllocSeq(){
        return m_seq++;
    }
    FakePackeWriter m_writer;
    Ipv4Address m_peerIp;
    uint16_t m_peerPort;
    uint16_t m_bindPort;
    dqc::SocketAddress m_self;
    dqc::SocketAddress m_remote;
    dqc::PacketNumber m_seq{1};
    uint32_t m_streamId{0};
    Ptr<Socket> m_socket;
    DqcSimuClock m_clock;
    dqc::MainEngine m_timeDriver;
    std::shared_ptr<dqc::AlarmFactory> m_alarmFactory;
    dqc::ProtoCon m_connection;
    dqc::ProtoStream *m_stream{nullptr};
    EventId m_processTimer;
    int64_t m_packetInteval{1};//1 ms
    int m_packetGenerated{0};
    int m_packetAllowed{500};
};   
}
