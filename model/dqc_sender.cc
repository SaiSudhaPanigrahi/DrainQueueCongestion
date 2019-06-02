#include "ns3/dqc_sender.h"
#include "ns3/log.h"
#include "byte_codec.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE("dqcsender");
using namespace dqc;   
DqcSender::DqcSender(){}
void DqcSender::Bind(uint16_t port){
    if (m_socket== NULL) {
        m_socket = Socket::CreateSocket (GetNode (),UdpSocketFactory::GetTypeId ());
        auto local = InetSocketAddress{Ipv4Address::GetAny (), port};
        auto res = m_socket->Bind (local);
        NS_ASSERT (res == 0);
    }
    m_bindPort=port;
    m_socket->SetRecvCallback (MakeCallback(&DqcSender::RecvPacket,this));
}
InetSocketAddress DqcSender::GetLocalAddress(){
    Ptr<Node> node=GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ipv4Address local_ip = ipv4->GetAddress (1, 0).GetLocal ();
	return InetSocketAddress{local_ip,m_bindPort};
}
void DqcSender::ConfigurePeer(Ipv4Address addr,uint16_t port){
	m_peerIp=addr;
	m_peerPort=port;
}
void DqcSender::StartApplication(){
	m_processTimer=Simulator::ScheduleNow(&DqcSender::Process,this);
}
void DqcSender::StopApplication(){
	
}
void DqcSender::RecvPacket(Ptr<Socket> socket){
	Address remoteAddr;
	auto packet = socket->RecvFrom (remoteAddr);
	uint32_t recv=packet->GetSize ();    
}
void DqcSender::SendToNetwork(Ptr<Packet> p){
    m_socket->SendTo(p,0,InetSocketAddress{m_peerIp,m_peerPort});
}
void DqcSender::Process(){
    if(m_processTimer.IsExpired()){
        CreateAndSendPacket();
        Time next=MilliSeconds(m_packetInteval);
        m_processTimer=Simulator::Schedule(next,&DqcSender::Process,this);
    }
}
void DqcSender::CreateAndSendPacket(){
    char buf[1000]={'\0'};
    basic::DataWriter w(buf,1000);
    dqc::PacketNumber seq=AllocSeq();
    int64_t now=Simulator::Now().GetMilliSeconds();
    w.WriteUInt64(seq.ToUint64());
    w.WriteUInt64(now);
    Ptr<Packet> p=Create<Packet>((uint8_t*)buf,1000);
	SendToNetwork(p);
}
}
