#include "fun_test.h"
#include <stdio.h>
#include "ack_frame.h"
#include "interval.h"
#include "proto_types.h"
#include "logging.h"
//test by printf
using namespace dqc;
using namespace std;
void proto_types_test()
{
        uint64_t seq=0;
    //seq=1<<32; will got 0xFFFFFFFF100000000;
    //just be careful for such error;
    seq=UINT32_C(1)<<31;
    DLOG(INFO)<<seq;
    uint8_t len=GetMinPktNumLen(seq);
}
void ack_frame_test(){
    PacketQueue packets;
    int i=1;
    for (i=1;i<10;i++){
        if(i==3){continue;}
        if(i==6){continue;}
        packets.Add(i);
    }
    packets.Print();
    packets.RemoveUpTo(6);
    //packets.Add(5);
   // packets.AddRange(1,3);
    packets.Print();
}
void interval_test(){
    Interval<PacketNumber> interval;
    if(interval.Empty()){
        printf("hello\n");
    }
    interval.SetMin(1);
    interval.SetMax(3);
    PacketNumber length=interval.Length();
    DLOG(INFO)<<length;
}

