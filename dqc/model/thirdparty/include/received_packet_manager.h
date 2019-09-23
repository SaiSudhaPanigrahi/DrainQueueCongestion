#ifndef RECEIVED_PACKET_MANAGER_H_
#define RECEIVED_PACKET_MANAGER_H_
#include "ack_frame.h"
#include "proto_time.h"
namespace dqc{
class ReceivdPacketManager{
public:
    ReceivdPacketManager();
    ~ReceivdPacketManager(){}
    void RecordPacketReceived(PacketNumber seq,ProtoTime receive_time);
    const AckFrame &GetUpdateAckFrame(ProtoTime &now);
  // Deletes all missing packets before least unacked. The connection won't
  // process any packets with packet number before |least_unacked| that it
  // received after this call.
    void DontWaitForPacketsBefore(PacketNumber least_unacked);
private:
    ProtoTime time_largest_observed_;
    AckFrame ack_frame_;
    PacketNumber peer_least_packet_awaiting_ack_{0};
};

}//namespace dqc
#endif
