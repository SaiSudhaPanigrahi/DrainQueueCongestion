#include <unistd.h>
#include <memory.h>
#include "ns3/dqc_trace.h"
#include "ns3/simulator.h"
namespace ns3{
DqcTrace::~DqcTrace(){
    Close();
}
void DqcTrace::Log(std::string name,uint8_t enable){
	if(enable&E_DQC_OWD){
		OpenTraceOwdFile(name);
	}
	if(enable&E_DQC_RTT){
		OpenTraceRttFile(name);
	}
	if(enable&E_DQC_BW){
		OpenTraceBandwidthFile(name);
	}
	if(enable&E_DQC_SENTSEQ){
		OpenTraceSentSeqFile(name);
	}
    if(enable&E_DQC_LOSS){
        OpenTraceLossPacketInfo(name);
    }
}
void DqcTrace::OpenTraceOwdFile(std::string name){
	char buf[FILENAME_MAX];
	memset(buf,0,FILENAME_MAX);
	std::string path = std::string (getcwd(buf, FILENAME_MAX)) + "/traces/"
			+name+"_owd.txt";
	m_owd.open(path.c_str(), std::fstream::out);    
}
void DqcTrace::OpenTraceRttFile(std::string name){
	char buf[FILENAME_MAX];
	memset(buf,0,FILENAME_MAX);
	std::string path = std::string (getcwd(buf, FILENAME_MAX)) + "/traces/"
			+name+"_rtt.txt";
	m_rtt.open(path.c_str(), std::fstream::out);    
}
void DqcTrace::OpenTraceBandwidthFile(std::string name){
	char buf[FILENAME_MAX];
	memset(buf,0,FILENAME_MAX);
	std::string path = std::string (getcwd(buf, FILENAME_MAX)) + "/traces/"
			+name+"_bw.txt";
	m_bw.open(path.c_str(), std::fstream::out);     
}
void DqcTrace::OpenTraceSentSeqFile(std::string name){
	char buf[FILENAME_MAX];
	memset(buf,0,FILENAME_MAX);
	std::string path = std::string (getcwd(buf, FILENAME_MAX)) + "/traces/"
			+name+"_sent_seq.txt";
	m_sentSeq.open(path.c_str(), std::fstream::out);  	
}
void DqcTrace::OpenTraceLossPacketInfo(std::string name){
	char buf[FILENAME_MAX];
	memset(buf,0,FILENAME_MAX);
	std::string path = std::string (getcwd(buf, FILENAME_MAX)) + "/traces/"
			+name+"_loss_seq.txt";
	m_lossInfo.open(path.c_str(), std::fstream::out);    
}
void DqcTrace::OnOwd(uint32_t seq,uint32_t owd,uint32_t size){
	char line [256];
	memset(line,0,256);
	if(m_owd.is_open()){
		float now=Simulator::Now().GetSeconds();
		sprintf (line, "%f %16d %16d %16d",
				now,seq,owd,size);
		m_owd<<line<<std::endl;
		m_owd.flush();
	}    
}
void DqcTrace::OnRtt(uint32_t seq,uint32_t rtt){
	char line [256];
	memset(line,0,256);
	if(m_rtt.is_open()){
		float now=Simulator::Now().GetSeconds();
		sprintf (line, "%f %16d %16d",
				now,seq,rtt);
		m_rtt<<line<<std::endl;
	}    
}
void DqcTrace::OnBw(int32_t kbps){
	char line [256];
	memset(line,0,256);
	if(m_bw.is_open()){
		float now=Simulator::Now().GetSeconds();
		sprintf (line, "%f %16d",
				now,kbps);
		m_bw<<line<<std::endl;
		m_bw.flush();
	}       
    
}
void DqcTrace::OnSentSeq(int32_t seq){
	char line [256];
	memset(line,0,256);
	if(m_sentSeq.is_open()){
		float now=Simulator::Now().GetSeconds();
		sprintf (line, "%f %16d",
				now,seq);
		m_sentSeq<<line<<std::endl;
	}     	
}
void DqcTrace::OnLossPacketInfo(int32_t seq,uint32_t rtt){
	char line [256];
	memset(line,0,256);
	if(m_lossInfo.is_open()){
		float now=Simulator::Now().GetSeconds();
		sprintf (line, "%f %16d %16d",
				now,seq,rtt);
		m_lossInfo<<line<<std::endl;
	}    
}
void DqcTrace::Close(){
    CloseTraceOwdFile();
    CloseTraceRttFile();
    CloseTraceBandwidthFile();
	CloseTraceSentSeqFile();
    CloseTraceLossPacketInfo();
}
void DqcTrace::CloseTraceOwdFile(){
	if(m_owd.is_open()){
		m_owd.close();
	}    
}
void DqcTrace::CloseTraceRttFile(){
	if(m_rtt.is_open()){
		m_rtt.close();
	}    
}
void DqcTrace::CloseTraceBandwidthFile(){
    if(m_bw.is_open()){
        m_bw.close();
    }
} 
void DqcTrace::CloseTraceSentSeqFile(){
    if(m_sentSeq.is_open()){
        m_sentSeq.close();
    }	
}
void DqcTrace::CloseTraceLossPacketInfo(){
    if(m_lossInfo.is_open()){
        m_lossInfo.close();
    }
}
}
