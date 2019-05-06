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
    PacketNumber  number=1234;
    PacketNumberHash hash;
    printf("%llu\n",hash(number));
    SendPacketManager manager;
    manager.Test();
    manager.Test2();
    return 0;
}


