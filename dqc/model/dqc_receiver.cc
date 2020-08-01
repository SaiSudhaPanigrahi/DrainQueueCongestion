#include "ns3/dqc_receiver.h"
#include "ns3/log.h"
#include "ns3/time_tag.h"
#include "byte_codec.h"
using namespace dqc;
namespace ns3{
NS_LOG_COMPONENT_DEFINE("dqcreceiver");
const int64_t mMaxDelayedAckTimeMs = 25;
/*class AckDelegate:public dqc::Alarm::Delegate{
public:
	AckDelegate(DqcReceiver *receiver):receiver_(receiver){}
	~AckDelegate(){}
    void OnAlarm() override{
    	receiver_->SendAckFrame();
    }
private:
	DqcReceiver *receiver_{nullptr};
};*/
DqcReceiver::DqcReceiver()
{
	m_recvManager.set_save_timestamps(true);
	m_frameEncoder.set_process_timestamps(true);
	m_frameDecoder.set_visitor(this);
}
void DqcReceiver::Bind(uint16_t port){
    if (m_socket== NULL) {
        m_socket = Socket::CreateSocket (GetNode (),UdpSocketFactory::GetTypeId ());
        auto local = InetSocketAddress{Ipv4Address::GetAny (), port};
        auto res = m_socket->Bind (local);
        NS_ASSERT (res == 0);
    }
    m_bindPort=port;
    m_socket->SetRecvCallback (MakeCallback(&DqcReceiver::RecvPacket,this));
    m_socket->SetEcnCallback (MakeCallback(&DqcReceiver::RecvEcnCallback,this));    
}
InetSocketAddress DqcReceiver::GetLocalAddress(){
    Ptr<Node> node=GetNode();
    Ptr<Ipv4> ipv4 = node->GetObject<Ipv4> ();
    Ipv4Address local_ip = ipv4->GetAddress (1, 0).GetLocal ();
	return InetSocketAddress{local_ip,m_bindPort};    
}
bool DqcReceiver::OnStreamFrame(dqc::PacketStream &frame){
    if(m_recvInterval.Empty()){
    	m_recvInterval.Add(frame.offset,frame.offset+frame.len);
    }else{
        if(m_recvInterval.IsDisjoint(frame.offset,frame.offset+frame.len)){
            m_recvInterval.Add(frame.offset,frame.offset+frame.len);
        }else{
            NS_LOG_INFO("redundancy "<<frame.offset);
        }
    }
	return true;
}
void DqcReceiver::OnError(dqc::ProtoFramer* framer){

}
void DqcReceiver::OnEcnMarkCount(uint64_t ecn_ce_count){}
bool DqcReceiver::OnAckFrameStart(dqc::PacketNumber largest_acked,
                     dqc::TimeDelta ack_delay_time){
	return true;
}
bool DqcReceiver::OnAckRange(dqc::PacketNumber start,
                dqc::PacketNumber end){
	return true;
}
bool DqcReceiver::OnAckTimestamp(dqc::PacketNumber packet_number,
		dqc::ProtoTime timestamp){
	return true;
}
bool DqcReceiver::OnAckFrameEnd(dqc::PacketNumber start){
	return true;
}
bool DqcReceiver::OnStopWaitingFrame(const dqc::PacketNumber least_unacked){
    NS_LOG_INFO("stop waiting "<<least_unacked.ToUint64());
    m_recvManager.DontWaitForPacketsBefore(least_unacked);
	return true;
}
void DqcReceiver::StartApplication(){
}
void DqcReceiver::StopApplication(){
	m_running=false;
	if(!m_traceOwdCb.IsNull()){
		//m_traceOwdCb((uint32_t)m_largestSeq.ToUint64(),0,0);
	}	
}
void DqcReceiver::SendAckFrame(){
    ProtoTime now=m_clock.Now();
    const AckFrame &ack_frame=m_recvManager.GetUpdateAckFrame(now);
    char buf[1500];
    basic::DataWriter w(buf,1500);
    ProtoPacketHeader header;
    header.packet_number=AllocSeq();
    AppendPacketHeader(header,&w);
    m_frameEncoder.AppendAckFrameAndTypeByte(ack_frame,&w);
    Ptr<Packet> p=Create<Packet>((uint8_t*)buf,w.length());
	SendToNetwork(p);
	m_recvManager.ResetAckStates();
}
void DqcReceiver::RecvPacket(Ptr<Socket> socket){
	if(!m_running){return;}
	Address remoteAddr;
	auto packet = socket->RecvFrom (remoteAddr);
	if(!m_knowPeer){
        m_peerIp= InetSocketAddress::ConvertFrom (remoteAddr).GetIpv4 ();
	    m_peerPort= InetSocketAddress::ConvertFrom (remoteAddr).GetPort ();
		m_knowPeer=true;
	}
	ProtoTime now=m_clock.Now();
	uint32_t recv=packet->GetSize ();
	TimeTag tag;
	packet->PeekPacketTag (tag);
	uint32_t owd=Simulator::Now().GetMilliSeconds()-tag.GetSentTime();
	uint8_t buf[1500]={'\0'};
	packet->CopyData(buf,recv);
    basic::DataReader r((char*)buf,recv);
    ProtoPacketHeader header;
    ProcessPacketHeader(&r,header);
    PacketNumber seq=header.packet_number;
    m_recvManager.RecordPacketReceived(seq,now);
    m_frameDecoder.ProcessFrameData(&r,header);
	if(!m_largestSeq.IsInitialized()){
		m_largestSeq=seq;
	}else{
		if(seq>m_largestSeq){
			m_largestSeq=seq;
		}
	}
	if(!m_traceOwdCb.IsNull()){
		m_traceOwdCb((uint32_t)seq.ToUint64(),owd,recv);
	}
    if(m_ecn_flag){
        NS_LOG_INFO("ecn mark");
        m_recvManager.AddEcnCount(recv);
        m_ecn_flag=false;
    }
    SendAckFrame();
}
void DqcReceiver::RecvEcnCallback(uint8_t ecn){
    if(ecn==0x03){
        m_ecn_flag=true;
    }
}
void DqcReceiver::SendToNetwork(Ptr<Packet> p){
    m_socket->SendTo(p,0,InetSocketAddress{m_peerIp,m_peerPort});
}
}
