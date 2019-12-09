#include "sink_manager.h"
namespace ns3{
OneWayDelaySinkManager* OneWayDelaySinkManager::Instance(){
    static OneWayDelaySinkManager *const ins=new OneWayDelaySinkManager();
    return ins;
}
void OneWayDelaySinkManager::Destructor(){
    m_sinks.clear();
}
void OneWayDelaySinkManager::RegisterSink(OneWayDelaySink *sink){
    bool exist=false;
    for(auto it=m_sinks.begin();it!=m_sinks.end();it++){
        if(sink==(*it)){
            exist=true;
            break;
        }
    }
    if(!exist){
        m_sinks.push_back(sink);
    }
}
void OneWayDelaySinkManager::OnNewCreateSender(DqcSender *sender){
    uint32_t id=sender->GetId();
    for(auto it=m_sinks.begin();it!=m_sinks.end();it++){
        if((id>0)&&(*it)->NeedRegisterToSender(id)){
            sender->RegisterOnewayDelaySink(*it);
        }
    }
}
}
