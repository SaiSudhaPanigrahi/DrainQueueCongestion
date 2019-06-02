#include "ns3/dqc_receiver.h"
#include "ns3/log.h"
#include "byte_codec.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE("dqcreceiver");    
void DqcReceiver::Bind(uint16_t port){
    if (m_socket== NULL) {
        m_socket = Socket::CreateSocket (GetNode (),UdpSocketFactory::GetTypeId ());
        auto local = InetSocketAddress{Ipv4Address::GetAny (), port};
        auto res = m_socket->Bind (local);
        NS_ASSERT (res == 0);
    }
    m_bindPort=port;
    m_socket->SetRecvCallback (MakeCallback(&DqcReceiver::RecvPacket,this));    
}
InetSocketAddress DqcReceiver::GetLocalAddress(){
    Ptr<Node> node=GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ipv4Address local_ip = ipv4->GetAddress (1, 0).GetLocal ();
	return InetSocketAddress{local_ip,m_bindPort};    
}
void DqcReceiver::StartApplication(){
}
void DqcReceiver::StopApplication(){
}
void DqcReceiver::RecvPacket(Ptr<Socket> socket){
	Address remoteAddr;
	auto packet = socket->RecvFrom (remoteAddr);
	if(!m_knowPeer){
        m_peerIp= InetSocketAddress::ConvertFrom (remoteAddr).GetIpv4 ();
	    m_peerPort= InetSocketAddress::ConvertFrom (remoteAddr).GetPort ();
		m_knowPeer=true;
	}
	uint32_t recv=packet->GetSize ();
	uint8_t buf[1000]={'\0'};
	memset(buf,0,1000);
	packet->CopyData(buf,recv);
    int64_t now=Simulator::Now().GetMilliSeconds();
    basic::DataReader r((char*)buf,recv);
    uint64_t seq=0;
    uint64_t sent_time=0;
    r.ReadUInt64(&seq);
    r.ReadUInt64(&sent_time);
    int64_t owd=now-sent_time;
    NS_LOG_INFO(seq<<" "<<owd);
}    
}
