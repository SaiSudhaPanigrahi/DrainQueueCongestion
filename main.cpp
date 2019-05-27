#include <iostream>
#include <utility>
#include "logging.h"
#include "proto_con.h"
#include "send_packet_manager.h"
#include "byte_codec.h"
#include <string.h>
#include <stdint.h>
#include "fun_test.h"
#include <vector>
#include "proto_bandwidth.h"
#include "proto_bbr_sender.h"
int main(){
    su_platform_init();
    //SendPacketManager manager(nullptr);
    //manager.Test();
    //manager.Test2();
    test_test();
    su_platform_uninit();
    //ProtoBandwidth bw(ProtoBandwidth::FromBitsPerSecond(1000));
    //TimeDelta wait=bw.TransferTime(1000);
    //std::cout<<std::to_string(wait.ToMilliseconds());
}


