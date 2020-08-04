#include "proto_linux_sender.h"
#include "unacked_packet_map.h"
#include "flag_impl.h"
#include "flag_util_impl.h"
#include "rtt_stats.h"
#include "random.h"
#include "proto_constants.h"
#include <ostream>
#include <memory.h>
#include "ns-linux/ns3-func.h"
#include "ns-linux/tcp_congestion_manager.h"
namespace dqc{
namespace{
const float kDefaultHighGain = 2.885f;
const QuicByteCount kDefaultMinimumCongestionWindow = 4 * kMaxSegmentSize;
}
void init_sock(tcp_sock *tcp){
    memset(tcp,0,sizeof(tcp_sock));
    tcp->sk_state=TCP_ESTABLISHED;
    tcp->mss_cache=kDefaultTCPMSS;
    tcp->icsk_ca_state=TCP_CA_Open;
    tcp->sk_max_pacing_rate=~0U;
    minmax_reset(&tcp->rtt_min,tcp_jiffies32, ~0U);
}
LinuxSender::LinuxSender(ProtoTime now,
                     const RttStats* rtt_stats,
                     const UnackedPacketMapInfoInterface* unacked_packets,
                     QuicPacketCount initial_tcp_congestion_window,
                     QuicPacketCount max_tcp_congestion_window,
                     Random* random)
    : rtt_stats_(rtt_stats),
      unacked_packets_(unacked_packets),
      random_(random),
      round_trip_count_(0),
      congestion_window_(initial_tcp_congestion_window * kDefaultTCPMSS),
      initial_congestion_window_(initial_tcp_congestion_window *
                                 kDefaultTCPMSS),
      max_congestion_window_(max_tcp_congestion_window * kDefaultTCPMSS),
      min_congestion_window_(kDefaultMinimumCongestionWindow),
      exiting_quiescence_(false),
      last_sample_is_app_limited_(false),
      has_non_app_limited_sample_(false),
      flexible_app_limited_(false),
      recovery_state_(NOT_IN_RECOVERY),
      recovery_window_(max_congestion_window_),
      is_app_limited_recovery_(false),
      startup_bytes_lost_(0),
      always_get_bw_sample_when_acked_(
          GetQuicReloadableFlag(quic_always_get_bw_sample_when_acked)) {
    update_tcp_time();
    init_sock(&tcp_);
    tcp_.snd_cwnd=initial_tcp_congestion_window;
    tcp_.snd_cwnd_clamp=max_tcp_congestion_window;
    tcp_.delivered=sampler_.total_bytes_acked()/kDefaultTCPMSS;
    ns3::CongestionControlManager *cong_ops_manager=ns3::GetCCManager();
    CHECK(cong_ops_manager);
    e_=cong_ops_manager->get_ops("bbr");
    CHECK(e_);
    tcp_mstamp_refresh(&tcp_);
    if(e_){
        e_->init(&tcp_);
    }
    memset(&rate_sample_,0,sizeof(rate_sample_));  
}

LinuxSender::~LinuxSender() {}
void LinuxSender::SetInitialCongestionWindowInPackets(
    QuicPacketCount congestion_window) {
}

bool LinuxSender::InSlowStart() const {
//TODO
  return false;
}

void LinuxSender::OnPacketSent(ProtoTime sent_time,
                             QuicByteCount bytes_in_flight,
                             QuicPacketNumber packet_number,
                             QuicByteCount bytes,
                             HasRetransmittableData is_retransmittable) {

  last_sent_packet_ = packet_number;

  if (bytes_in_flight == 0 && sampler_.is_app_limited()) {
    exiting_quiescence_ = true;
  }
  tcp_mstamp_refresh(&tcp_);
  tcp_.tcp_wstamp_ns=tcp_.tcp_clock_cache;
  uint32_t inflight=(uint32_t)bytes_in_flight/kDefaultTCPMSS;
  tcp_.packets_out=inflight;
  sampler_.OnPacketSent(sent_time, packet_number, bytes, bytes_in_flight,
                        is_retransmittable);
}

bool LinuxSender::CanSend(QuicByteCount bytes_in_flight) {
  return bytes_in_flight < GetCongestionWindow();
}

QuicBandwidth LinuxSender::PacingRate(QuicByteCount bytes_in_flight) const {
  if(!has_seen_rtt_){
      auto rtt=rtt_stats_->initial_rtt();
      return kDefaultHighGain * QuicBandwidth::FromBytesAndTimeDelta(initial_congestion_window_,
                            rtt);
  }
  return QuicBandwidth::FromBytesPerSecond(tcp_.sk_pacing_rate);
}

QuicBandwidth LinuxSender::BandwidthEstimate() const {
  uint32_t bw=e_->bw_est(const_cast<tcp_sock*>(&tcp_));
  return QuicBandwidth::FromBytesPerSecond(bw);
}

QuicByteCount LinuxSender::GetCongestionWindow() const {
  return (QuicByteCount)tcp_.snd_cwnd*kDefaultTCPMSS;
}

QuicByteCount LinuxSender::GetSlowStartThreshold() const {
  return 0;
}

bool LinuxSender::InRecovery() const {
  return recovery_state_ != NOT_IN_RECOVERY;
}

bool LinuxSender::ShouldSendProbingPacket() const {
    return false;
}


void LinuxSender::AdjustNetworkParameters(QuicBandwidth bandwidth,
                                        TimeDelta rtt,
                                        bool allow_cwnd_to_decrease) {}

void LinuxSender::OnCongestionEvent(bool /*rtt_updated*/,
                                  QuicByteCount prior_in_flight,
                                  ProtoTime event_time,
                                  const AckedPacketVector& acked_packets,
                                  const LostPacketVector& lost_packets) {
  has_seen_rtt_=true; 
  update_tcp_time();
  tcp_mstamp_refresh(&tcp_);
  CHECK(!rtt_stats_->latest_rtt().IsZero());
  u32 rtt_us=rtt_stats_->latest_rtt().ToMicroseconds();
  tcp_update_rtt_min(&tcp_,rtt_us);
  TimeDelta srtt=rtt_stats_->SmoothedOrInitialRtt();
  tcp_.srtt_us=srtt.ToMicroseconds()<<3;
                              
  //const QuicByteCount total_bytes_acked_before = sampler_.total_bytes_acked();
  
  
  memset(&rate_sample_,0,sizeof(rate_sample_));
  rate_sample_.prior_in_flight=(u32)prior_in_flight/kDefaultTCPMSS;
  
  
  bool is_round_start = false;
  DiscardLostPackets(lost_packets);
  uint32_t acked_sacked=0;
  if (!acked_packets.empty()) {
    QuicPacketNumber last_acked_packet = acked_packets.rbegin()->packet_number;
    is_round_start = UpdateRoundTripCounter(last_acked_packet);
    UpdateBandwidthSample(event_time, acked_packets);
    UpdateRecoveryState(last_acked_packet, !lost_packets.empty(),
                        is_round_start);
    acked_sacked+=1;
  }
  // Calculate number of packets acked and lost.
  uint32_t  loss =lost_packets.size();
  uint32_t delivered=sampler_.total_bytes_acked()/kDefaultTCPMSS;
  if(delivered>tcp_.delivered){
       tcp_.delivered=delivered;
  }
  tcp_.delivered=(u32)sampler_.total_bytes_acked()/kDefaultTCPMSS;
  tcp_.delivered_mstamp=tcp_.tcp_mstamp;
  tcp_.lost+=loss;
  rate_sample_.losses=loss;
  rate_sample_.acked_sacked=acked_sacked;
  e_->cong_control(&tcp_,&rate_sample_);
  // Cleanup internal state.
  sampler_.RemoveObsoletePackets(unacked_packets_->GetLeastUnacked());
}

CongestionControlType LinuxSender::GetCongestionControlType() const {
  return kLinuxBBR;
}
void LinuxSender::DiscardLostPackets(const LostPacketVector& lost_packets) {
  for (const LostPacket& packet : lost_packets) {
    sampler_.OnPacketLost(packet.packet_number);
  }
}

bool LinuxSender::UpdateRoundTripCounter(QuicPacketNumber last_acked_packet) {
  if (!current_round_trip_end_.IsInitialized()||
      last_acked_packet > current_round_trip_end_) {
    round_trip_count_++;
    current_round_trip_end_ = last_sent_packet_;
    return true;
  }

  return false;
}

void LinuxSender::UpdateBandwidthSample(
    ProtoTime now,
    const AckedPacketVector& acked_packets) {
  for (const auto& packet : acked_packets) {
    if (!always_get_bw_sample_when_acked_ && packet.bytes_acked == 0) {
      // Skip acked packets with 0 in flight bytes when updating bandwidth.
      continue;
    }
    BandwidthSample bandwidth_sample =
        sampler_.OnPacketAcknowledged(now, packet.packet_number);
    if (always_get_bw_sample_when_acked_ &&
        !bandwidth_sample.state_at_send.is_valid) {
      // From the sampler's perspective, the packet has never been sent, or the
      // packet has been acked or marked as lost previously.
      continue;
    }
    rate_sample_.prior_delivered=(u32)bandwidth_sample.state_at_send.total_bytes_acked/kDefaultTCPMSS;
    u32 delivered=(u32)sampler_.total_bytes_acked()/kDefaultTCPMSS;
    if(delivered>=rate_sample_.prior_delivered){
        rate_sample_.delivered=delivered-rate_sample_.prior_delivered;
    }
    rate_sample_.snd_interval_us=bandwidth_sample.sent_time_interval.ToMicroseconds();
    rate_sample_.rcv_interval_us=bandwidth_sample.recv_time_interval.ToMicroseconds();
    rate_sample_.interval_us=std::max(rate_sample_.snd_interval_us,rate_sample_.rcv_interval_us);
    rate_sample_.rtt_us=bandwidth_sample.rtt.ToMicroseconds();
    last_sample_is_app_limited_ = bandwidth_sample.state_at_send.is_app_limited;
    has_non_app_limited_sample_ |=
        !bandwidth_sample.state_at_send.is_app_limited;
  }
}
void LinuxSender::UpdateRecoveryState(QuicPacketNumber last_acked_packet,
                                    bool has_losses,
                                    bool is_round_start) {
  // Exit recovery when there are no losses for a round.
  if (has_losses) {
    end_recovery_at_ = last_sent_packet_;
  }

  switch (recovery_state_) {
    case NOT_IN_RECOVERY:
      // Enter conservation on the first loss.
      if (has_losses) {
        recovery_state_ = CONSERVATION;
        tcp_.icsk_ca_state=TCP_CA_Recovery;
        e_->set_state(&tcp_,TCP_CA_Recovery);
        // This will cause the |recovery_window_| to be set to the correct
        // value in CalculateRecoveryWindow().
        recovery_window_ = 0;
        // Since the conservation phase is meant to be lasting for a whole
        // round, extend the current round as if it were started right now.
        current_round_trip_end_ = last_sent_packet_;
        if (GetQuicReloadableFlag(quic_bbr_app_limited_recovery) &&
            last_sample_is_app_limited_) {
          //QUIC_RELOADABLE_FLAG_COUNT(quic_bbr_app_limited_recovery);
          is_app_limited_recovery_ = true;
        }
      }
      break;

    case CONSERVATION:
      if (is_round_start) {
        recovery_state_ = GROWTH;
      }
      //QUIC_FALLTHROUGH_INTENDED;

    case GROWTH:
      // Exit recovery if appropriate.
      if (!has_losses && last_acked_packet > end_recovery_at_) {
        recovery_state_ = NOT_IN_RECOVERY;
        tcp_.icsk_ca_state=TCP_CA_Open;
        e_->set_state(&tcp_,TCP_CA_Open);
        is_app_limited_recovery_ = false;
      }

      break;
  }
  if (recovery_state_ != NOT_IN_RECOVERY && is_app_limited_recovery_) {
    sampler_.OnAppLimited();
  }
}
std::string LinuxSender::GetDebugState() const {
  return " ";
}

void LinuxSender::OnApplicationLimited(QuicByteCount bytes_in_flight) {
  if (bytes_in_flight >= GetCongestionWindow()) {
    return;
  }
  if (flexible_app_limited_ /*&& IsPipeSufficientlyFull()*/) {
    return;
  }
  sampler_.OnAppLimited();
}

}


