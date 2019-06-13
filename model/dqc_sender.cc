#include <string>
#include "ns3/simulator.h"
#include "ns3/dqc_sender.h"
#include "ns3/log.h"
#include "ns3/time_tag.h"
#include "byte_codec.h"
#include "proto_utils.h"
using namespace dqc;
namespace ns3{
NS_LOG_COMPONENT_DEFINE("dqcsender");
using namespace dqc;
// in order to get the ip addr of node
void ConvertIp32(uint32_t ip,std::string &addr){
 uint8_t first=(ip&0xff000000)>>24;
 uint8_t second=(ip&0x00ff0000)>>16;
 uint8_t third=(ip&0x0000ff00)>>8;
 uint8_t fourth=(ip&0x000000ff);
 std::string dot=std::string(".");
 addr=std::to_string(first)+dot+std::to_string(second)+dot+std::to_string(third)+dot+std::to_string(fourth);
}
int FakePackeWriter::SendTo(const char*buf,size_t size,dqc::SocketAddress &dst){
    Ptr<Packet> p=Create<Packet>((uint8_t*)buf,size);
	m_sender->SendToNetwork(p);
	return size;
}
DqcSender::DqcSender()
:m_writer(this)
,m_alarmFactory(new ProcessAlarmFactory(&m_timeDriver))
,m_connection(&m_clock,m_alarmFactory.get(),/*kBBR_DELAY*/kCubicBytes){}
void DqcSender::Bind(uint16_t port){
    if (m_socket== NULL) {
        m_socket = Socket::CreateSocket (GetNode (),UdpSocketFactory::GetTypeId ());
        auto local = InetSocketAddress{Ipv4Address::GetAny (), port};
        auto res = m_socket->Bind (local);
        NS_ASSERT (res == 0);
    }
    m_bindPort=port;
    m_socket->SetRecvCallback (MakeCallback(&DqcSender::RecvPacket,this));
    m_connection.set_packet_writer(&m_writer);
	m_connection.SetTraceSentSeq(this);
    m_stream=m_connection.GetOrCreateStream(m_streamId);
    m_stream->set_stream_vistor(this);
	SendPacketManager *sent_manager=m_connection.GetSentPacketManager();
	RttStats* rtt_stats=sent_manager->GetRttStats();
	rtt_stats->UpdateRtt(TimeDelta::FromMilliseconds(100),TimeDelta::FromMilliseconds(0),ProtoTime::Zero());
	NS_LOG_INFO(rtt_stats->smoothed_rtt().ToMicroseconds());
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
	std::string ip_str;
	ConvertIp32(addr.Get(),ip_str);
	m_remote=SocketAddress(ip_str,port);
	NS_LOG_INFO(m_remote.ToString());
}
void DqcSender::DataGenerator(int times){
    if(!m_stream){
        return ;
    }
    char data[1500];
    int i=0;
    for (i=0;i<1500;i++){
        data[i]=RandomLetter::Instance()->GetLetter();
    }
    std::string piece(data,1500);
    bool success=false;
    for(i=0;i<times;i++){
        if(m_pakcetLimit&&(m_packetGenerated>m_packetAllowed)){
            break;
        }
        success=m_stream->WriteDataToBuffer(piece);
        if(!success){
            break;
        }
        m_packetGenerated++;
    }
}
void DqcSender::StartApplication(){
	m_processTimer=Simulator::ScheduleNow(&DqcSender::Process,this);
}
void DqcSender::StopApplication(){
	
}
void DqcSender::RecvPacket(Ptr<Socket> socket){
	Address remoteAddr;
	auto p = socket->RecvFrom (remoteAddr);
	uint32_t recv=p->GetSize ();
    ProtoTime now=m_clock.Now();
    uint8_t buf[1500]={'\0'};
    p->CopyData(buf,recv);
    ProtoReceivedPacket packet((char*)buf,recv,now);
    m_connection.ProcessUdpPacket(m_self,m_remote,packet);
}
void DqcSender::SendToNetwork(Ptr<Packet> p){
	uint32_t ms=Simulator::Now().GetMilliSeconds();
	m_lastSentTs=ms;
	TimeTag tag;
    tag.SetSentTime (ms);
	p->AddPacketTag (tag);
	if(!m_traceBwCb.IsNull()){
		QuicBandwidth send_bw=m_connection.EstimatedBandwidth();
		m_traceBwCb((int32_t)send_bw.ToKBitsPerSecond());
		//NS_LOG_INFO("bw "<<std::to_string((int32_t)send_bw.ToKBitsPerSecond()));
	}
    m_socket->SendTo(p,0,InetSocketAddress{m_peerIp,m_peerPort});
}
void DqcSender::Process(){
    if(m_processTimer.IsExpired()){
		int64_t now_ms=Simulator::Now().GetMilliSeconds();
		//NS_LOG_INFO("now "<<std::to_string(Simulator::Now().GetMicroSeconds()));
		if(m_lastSentTs!=0){
			if((now_ms-m_lastSentTs)>5000){
				SendPacketManager *sent_manager=m_connection.GetSentPacketManager();
				int32_t largest_sent=(int32_t)(m_connection.GetMaxSentSeq().ToUint64());
				int32_t largest_acked=(int32_t)(sent_manager->largest_acked().ToUint64());
				int buffer=m_stream->BufferedBytes();
				ByteCount in_flight=0;
				ByteCount cwnd=0;
				sent_manager->InFlight(&in_flight,&cwnd);
				NS_LOG_INFO(std::to_string(largest_sent)<<" "<<std::to_string(largest_acked));
				NS_LOG_INFO(std::to_string(buffer)
				<<" "<<std::to_string(m_stream->get_send_buffer_len())
				<<" "<<sent_manager->CheckCanSend()
				<<" "<<std::to_string(in_flight)
				<<" cwnd "<<std::to_string(cwnd)
				);
			}
		}
    	ProtoTime now=m_clock.Now();
    	m_timeDriver.HeartBeat(now);
    	m_connection.Process();
        Time next=MicroSeconds(m_packetInteval);
        m_processTimer=Simulator::Schedule(next,&DqcSender::Process,this);
    }
}
}
