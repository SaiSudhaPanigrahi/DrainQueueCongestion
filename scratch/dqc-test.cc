/*test quic bbr on ns3 platform*/
#include "ns3/core-module.h"
#include "ns3/dqc-module.h"
using namespace ns3;
using namespace dqc;
int main(){
    LogComponentEnable("dqcsender",ns3::LOG_ALL);
    ns3::DqcSender sender;
    sender.Test();
    return 0;
}
