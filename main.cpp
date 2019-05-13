#include <iostream>
#include <utility>
#include "logging.h"
#include "proto_con.h"
#include "send_packet_manager.h"
#include "byte_codec.h"
#include <string.h>
#include <stdint.h>
#include "fun_test.h"
#include "proto_framer.h"
#include "proto_time.h"
namespace dqc{
    const uint32_t kShit=1234;
}
namespace shit{
    const uint32_t kShit=4567;
}
using namespace dqc;
int main(){
    /*ProtoCon con;
    con.Test();
    AbstractAlloc *alloc=AbstractAlloc::Instance();
    alloc->CheckMemLeak();*/
    /*SendPacketManager manager(nullptr);
    manager.Test();
    manager.Test2();
    */
    //ack_frame_test();
    //proto_types_test();
    interval_test();
    DLOG(INFO)<<dqc::kShit<<" "<<shit::kShit;
}


