#pragma once
#include "proto_bandwidth.h"
namespace ns3{
class DqcSender{
public:
    DqcSender();
    ~DqcSender(){}
    void Test();
private:
    dqc::QuicBandwidth m_bw;
};   
}
