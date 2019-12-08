#include "proto_send_algorithm_interface.h"
#include "rtt_stats.h"
#include "proto_bbr_plus_sender.h"
#include "proto_bbr_sender.h"
#include "proto_queue_limit.h"
#include "proto_delay_bbr_sender.h"
#include "proto_highrail_sender.h"
#include "proto_potential_sender.h"
#include "proto_bbr_rand_sender.h"
#include "proto_tsunami_sender.h"
#include "tcp_cubic_sender_bytes.h"
#include "bbr2_sender.h"
#include "proto_bbr_sender_shadow.h"
namespace dqc{
SendAlgorithmInterface * SendAlgorithmInterface::Create(
        const ProtoClock *clock,
        const RttStats *rtt_stats,
        const UnackedPacketMapInfoInterface* unacked_packets,
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
        case kQueueLimit:{
            return new QueueLimitSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random);
        }
        case kShadow:{
            return new BbrShadowSender(clock->Now(),
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
        case kBBRPlus:{
            return new BbrPlusSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
        case kBBRRand:{
            return new BbrRandSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
        case kTsunami:{
            return new TsunamiSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
        case kHighSpeedRail:{
            return new HighSeedRailSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
        case kBBRv2:{
            return new Bbr2Sender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random,
							   stats
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
