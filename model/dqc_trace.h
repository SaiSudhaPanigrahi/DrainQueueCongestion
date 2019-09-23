#pragma once
#include <iostream>
#include <fstream>
#include <string>
namespace ns3{
enum DqcTraceEnable:uint8_t{
	E_DQC_OWD=0x01,
	E_DQC_RTT=0x02,
	E_DQC_BW=0x04,
	E_DQC_SENTSEQ=0X08,
	E_DQC_ALL=E_DQC_OWD|E_DQC_RTT|E_DQC_BW|E_DQC_SENTSEQ,
};
class DqcTrace{
public:
	DqcTrace(){}
	~DqcTrace();
	void Log(std::string name,uint8_t enable);
	void OnOwd(uint32_t seq,uint32_t owd,uint32_t size);
    void OnRtt(uint32_t seq,uint32_t rtt);
	void OnBw(int32_t kbps);
	void OnSentSeq(int32_t seq);
private:
	void Close();
	void OpenTraceOwdFile(std::string name);
    void OpenTraceRttFile(std::string name);
    void OpenTraceBandwidthFile(std::string name);
	void OpenTraceSentSeqFile(std::string name);
    void CloseTraceOwdFile();
    void CloseTraceRttFile();
    void CloseTraceBandwidthFile();
	void CloseTraceSentSeqFile();	
	std::fstream m_owd;
	std::fstream m_rtt;
    std::fstream m_bw;
	std::fstream m_sentSeq;
};
}
