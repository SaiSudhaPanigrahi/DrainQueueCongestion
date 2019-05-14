#ifndef RECEIVED_PACKET_MANAGER_H_
#define RECEIVED_PACKET_MANAGER_H_
#include "ack_frame.h"
namespace dqc{
class ReceivdPacketManager{
public:
    ReceivdPacketManager(){}
    ~ReceivdPacketManager(){}
    void RecordPacketReceived(PacketNumber seq,ProtoTime time);
    const AckFrame &GetUpdateAckFrame(){
        return ack_frame_;
    }
private:
    bool first_received_{true};
    AckFrame ack_frame_;
};

}//namespace dqc
#endif
