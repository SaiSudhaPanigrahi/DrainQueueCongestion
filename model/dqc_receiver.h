#pragma once
#include "ns3/simulator.h"
#include "ns3/application.h"
#include "ns3/socket.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/callback.h"
namespace ns3{
class DqcReceiver:public Application{
public:
    DqcReceiver(){}
    ~DqcReceiver(){}
	void Bind(uint16_t port);
	InetSocketAddress GetLocalAddress();
private:    
	virtual void StartApplication() override;
	virtual void StopApplication() override;
	void RecvPacket(Ptr<Socket> socket);
    bool m_knowPeer{false};   
    Ipv4Address m_peerIp;
    uint16_t m_peerPort;
    uint16_t m_bindPort;
    Ptr<Socket> m_socket;  
};    
}