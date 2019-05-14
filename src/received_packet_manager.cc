#include "received_packet_manager.h"
namespace dqc{
void ReceivdPacketManager::RecordPacketReceived(PacketNumber seq,ProtoTime time){
    if(first_received_){
        ack_frame_.largest_acked=seq;
        ack_frame_.packets.Add(seq);
        first_received_=false;
    }else{
        if(seq>LargestAcked(ack_frame_)){
            ack_frame_.packets.Add(seq);
            ack_frame_.largest_acked=seq;
        }
    }
}
}
