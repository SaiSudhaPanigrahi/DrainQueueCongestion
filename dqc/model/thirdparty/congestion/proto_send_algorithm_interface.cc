#include "proto_send_algorithm_interface.h"
#include "rtt_stats.h"
#include "proto_bbr_plus_sender.h"
#include "proto_bbr_sender.h"
#include "proto_delay_bbr_sender.h"
#include "proto_highrail_sender.h"
#include "proto_bbr_rand_sender.h"
#include "proto_tsunami_sender.h"
#include "tcp_cubic_sender_bytes.h"
#include "tcp_westwood_sender_bytes.h"
#include "tcp_westwood_sender_enhance.h"
#include "bbr2_sender.h"
#include "cubic_plus_sender_bytes.h"
#include "lia_sender_bytes.h"
#include "lia_plus_sender_bytes.h"
#include "pcc_sender.h"
#include "aimd_bbr_sender.h"
#include "proto_copa_sender.h"
#include "couple_bbr_sender.h"
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
        case kWestwood:{
            return new TcpWestwoodSenderBytes(clock,
                               rtt_stats,
                               true,
                               initial_congestion_window,
                               max_congestion_window,
                               stats
                               );
        }
        case kWestwoodEnhance:{
            return new TcpWestwoodSenderEnhance(clock,
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               stats
                               );
        }
        case kCubicPlus:{
            return new CubicPlusSender(clock,
                               rtt_stats,
							   unacked_packets,
                       		   false,
                               initial_congestion_window,
                               max_congestion_window,
                               stats,random
                               );
        }
        case kRenoPlus:{
            return new CubicPlusSender(clock,
                               rtt_stats,
							   unacked_packets,
                       		   true,
                               initial_congestion_window,
                               max_congestion_window,
                               stats,random
                               );
        }
        case kLiaBytes:{
            return new LiaSender(clock,
                               rtt_stats,
                       		   true,
                               initial_congestion_window,
                               max_congestion_window,
                               stats
                               );
        }
        case kLiaPlus:{
            return new LiaPlusSender(clock,
                               rtt_stats,
							   unacked_packets,
                       		   true,
                               initial_congestion_window,
                               max_congestion_window,
                               stats,random
                               );
        }
        case kCoupleBBR:{
            return new CoupleBbrSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
        case kBBRReno:{
            return new AimdBbrSender(clock,
                               rtt_stats,
							   unacked_packets,
                       		   true,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
        case kBBRCubic:{
            return new AimdBbrSender(clock,
                               rtt_stats,
							   unacked_packets,
                       		   false,
                               initial_congestion_window,
                               max_congestion_window,
                               random
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
        case kBBR:{
            return new BbrSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random,false
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
        case kBBRD:{
            return new BbrSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random,true
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
        case kCopa:{
            return new CopaSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
        case kPCC:{
            return new PccSender(rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random
                               );
        }
        case kVivace:{
            return new PccSender(rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random,kVivaceUtility
                               );
        }
        case kWebRTCVivace:{
            return new PccSender(rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random,kModifyVivaceUtility
                               );
        }
        default:{
            return new BbrSender(clock->Now(),
                               rtt_stats,
                               unacked_packets,
                               initial_congestion_window,
                               max_congestion_window,
                               random,false
                               );
        }
    }
}
}
