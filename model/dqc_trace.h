#pragma once
#include <iostream>
#include <fstream>
#include <string>
namespace ns3{
class DqcTrace{
public:
	DqcTrace(){}
	~DqcTrace();
	void OpenTraceOwdFile(std::string name);
    void OpenTraceRttFile(std::string name);
    void OpenTraceBandwidthFile(std::string name);
	void OnOwd(uint32_t seq,uint32_t owd);
    void OnRtt(uint32_t seq,uint32_t rtt);
	void OnBw(int32_t kbps);
private:
	void Close();
    void CloseTraceOwdFile();
    void CloseTraceRttFile();
    void CloseTraceBandwidthFile();
	std::fstream m_owd;
	std::fstream m_rtt;
    std::fstream m_bw;
};
}
