#include <iostream>
#include <utility>
#include <vector>
#include <string.h>
#include <stdint.h>
#include <algorithm>
#include "logging.h"
#include "proto_con.h"
#include "send_packet_manager.h"
#include "byte_codec.h"
#include "fun_test.h"
#include "proto_bandwidth.h"
#include "random.h"
#include <packet_number.h>

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
    /*dqc::Random rand;
    rand.seed(0x1234);
    for(int i=0;i<30;i++){
        printf("%d\n",rand.nextInt()%9);
    }*/
}


