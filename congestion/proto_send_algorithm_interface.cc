#include "proto_send_algorithm_interface.h"
#include "rtt_stats.h"
#include "proto_bbr_sender.h"
#include "proto_bbr_sender_old.h"
#include "proto_bbr_sender_v0.h"
#include "pcc_sender.h"
#include "proto_delay_bbr_sender.h"
#include "tcp_cubic_sender_bytes.h"
namespace dqc{
SendAlgorithmInterface * SendAlgorithmInterface::Create(
        const ProtoClock *clock,
        const RttStats *rtt_stats,
        const UnackedPacketMap* unacked_packets,
        CongestionControlType type,
        Random *random,
		QuicConnectionStats* stats,
        QuicPacketCount initial_congestion_window){
    QuicPacketCount max_congestion_window = kDefaultMaxCongestionWindowPackets;
    switch(type){
        case kCubicBytes:{
            return new TcpCubicSenderBytes(clock,
                               rtt_stats,
                       		   false,
                               initial_congestion_window,
                               max_congestion_window,
                               stats
                               );
        }
        case kRenoBytes:{
            return new TcpCubicSenderBytes(clock,
                               rtt_stats,
                               true,
                               initial_congestion_window,
                               max_congestion_window,
                               stats
                               );
        }
        case kBBR_DELAY:{
            return new DelayBbrSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
        case kBBR_V0:{
            return new BbrSenderV0(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random);
        }
        case kBBR_OLD:{
            return new BbrSenderOld(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random);
        }
        case kBBR:{
            return new BbrSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
        case kPCC:{
            return new PccSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
        default:{
            return new BbrSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
    }
}
}
