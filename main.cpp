#include <iostream>
#include <utility>
#include "logging.h"
#include "proto_con.h"
#include "send_packet_manager.h"
int main(){
    ProtoCon con;
    con.Test();
    AbstractAlloc *alloc=AbstractAlloc::Instance();
    alloc->CheckMemLeak();
    /*SendPacketManager manager(nullptr);
    manager.Test();
    manager.Test2();
    ack_frame_test();*/
    return 0;
}


