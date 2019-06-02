#pragma once
#include "ns3/event-id.h"
#include "ns3/callback.h"
#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "dqc_clock.h"
#include "proto_bandwidth.h"
#include "proto_time.h"
#include "packet_number.h"
namespace ns3{
class DqcSender: public Application{
public:
    DqcSender();
    ~DqcSender(){}
	void Bind(uint16_t port);
	InetSocketAddress GetLocalAddress();
	void ConfigurePeer(Ipv4Address addr,uint16_t port);    
private:
	virtual void StartApplication() override;
	virtual void StopApplication() override;
    void RecvPacket(Ptr<Socket> socket);
    void SendToNetwork(Ptr<Packet> p);
    void Process();
    void CreateAndSendPacket();
    dqc::PacketNumber AllocSeq(){
        return m_seq++;
    }
    Ipv4Address m_peerIp;
    uint16_t m_peerPort;
    uint16_t m_bindPort;
    dqc::PacketNumber m_seq{1};
    Ptr<Socket> m_socket;
    DqcSimuClock m_clock;
    EventId m_processTimer;
    int64_t m_packetInteval{1000};//1 second
};   
}
