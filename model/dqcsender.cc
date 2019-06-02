#include "dqcsender.h"
#include "ns3/log.h"
namespace ns3{
NS_LOG_COMPONENT_DEFINE("dqcsender");
using namespace dqc;   
DqcSender::DqcSender()
:m_bw(QuicBandwidth::FromKBitsPerSecond(200)){
    
}
void DqcSender::Test(){
    NS_LOG_INFO(m_bw);
}
}
