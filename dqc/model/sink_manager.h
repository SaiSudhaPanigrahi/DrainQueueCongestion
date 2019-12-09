#pragma once
#include "ns3/dqc_sender.h"
namespace ns3{
class OneWayDelaySinkManager{
public:
    static OneWayDelaySinkManager* Instance();
    void Destructor();
    void RegisterSink(OneWayDelaySink *sink);
    void OnNewCreateSender(DqcSender *sender);
private:
    OneWayDelaySinkManager(){}
    ~OneWayDelaySinkManager(){}
    std::vector<OneWayDelaySink*> m_sinks;
};
}
