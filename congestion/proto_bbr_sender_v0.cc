// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "proto_bbr_sender_v0.h"
#include "unacked_packet_map.h"
#include "flag_impl.h"
#include "flag_util_impl.h"
#include "rtt_stats.h"
#include "random.h"
#include "proto_constants.h"
#include <ostream>
#include "ns3/log.h"

namespace dqc {

namespace {
// Constants based on TCP defaults.
//const QuicByteCount kMaxSegmentSize = kDefaultTCPMSS;
// The minimum CWND to ensure delayed acks don't reduce bandwidth measurements.
// Does not inflate the pacing rate.
const QuicByteCount kDefaultMinimumCongestionWindow = 4 * kMaxSegmentSize;

// The gain used for the slow start, equal to 2/ln(2).
const float kHighGain = 2.885f;
// The gain used in STARTUP after loss has been detected.
// 1.5 is enough to allow for 25% exogenous loss and still observe a 25% growth
// in measured bandwidth.
const float kStartupAfterLossGain = 1.5f;
// The gain used to drain the queue after the slow start.
const float kDrainGain = 1.f / kHighGain;
// The cycle of gains used during the PROBE_BW stage.
const float kPacingGain[] = {1.25, 0.75, 1, 1, 1, 1, 1, 1};

// The length of the gain cycle.
const size_t kGainCycleLength = sizeof(kPacingGain) / sizeof(kPacingGain[0]);
// The size of the bandwidth filter window, in round-trips.
const QuicRoundTripCount kBandwidthWindowSize = kGainCycleLength + 2;

// The time after which the current min_rtt value expires.
const TimeDelta kMinRttExpiry = TimeDelta::FromSeconds(10);
// The minimum time the connection can spend in PROBE_RTT mode.
const TimeDelta kProbeRttTime = TimeDelta::FromMilliseconds(200);
// If the bandwidth does not increase by the factor of |kStartupGrowthTarget|
// within |kRoundTripsWithoutGrowthBeforeExitingStartup| rounds, the connection
// will exit the STARTUP mode.
const float kStartupGrowthTarget = 1.25;
const QuicRoundTripCount kRoundTripsWithoutGrowthBeforeExitingStartup = 3;
// Coefficient of target congestion window to use when basing PROBE_RTT on BDP.
const float kModerateProbeRttMultiplier = 0.75;
// Coefficient to determine if a new RTT is sufficiently similar to min_rtt that
// we don't need to enter PROBE_RTT.
const float kSimilarMinRttThreshold = 1.125;

}  // namespace

BbrSenderV0::DebugState::DebugState(const BbrSenderV0& sender)
    : mode(sender.mode_),
      max_bandwidth(sender.max_bandwidth_.GetBest()),
      round_trip_count(sender.round_trip_count_),
      gain_cycle_index(sender.cycle_current_offset_),
      congestion_window(sender.congestion_window_),
      is_at_full_bandwidth(sender.is_at_full_bandwidth_),
      bandwidth_at_last_round(sender.bandwidth_at_last_round_),
      rounds_without_bandwidth_gain(sender.rounds_without_bandwidth_gain_),
      min_rtt(sender.min_rtt_),
      min_rtt_timestamp(sender.min_rtt_timestamp_),
      recovery_state(sender.recovery_state_),
      recovery_window(sender.recovery_window_),
      last_sample_is_app_limited(sender.last_sample_is_app_limited_),
      end_of_app_limited_phase(sender.sampler_->end_of_app_limited_phase()) {}

BbrSenderV0::DebugState::DebugState(const DebugState& state) = default;

BbrSenderV0::BbrSenderV0(ProtoTime now,const RttStats* rtt_stats,
                     const UnackedPacketMap* unacked_packets,
                     QuicPacketCount initial_tcp_congestion_window,
                     QuicPacketCount max_tcp_congestion_window,
                     Random* random)
    : rtt_stats_(rtt_stats),
      unacked_packets_(unacked_packets),
      random_(random),
      mode_(STARTUP),
      sampler_(new BandwidthSampler()),
      round_trip_count_(0),
      last_sent_packet_(0),
      current_round_trip_end_(0),
      max_bandwidth_(kBandwidthWindowSize, QuicBandwidth::Zero(), 0),
      max_ack_height_(kBandwidthWindowSize, 0, 0),
      aggregation_epoch_start_time_(ProtoTime::Zero()),
      aggregation_epoch_bytes_(0),
      bytes_acked_since_queue_drained_(0),
      max_aggregation_bytes_multiplier_(0),
      min_rtt_(TimeDelta::Zero()),
      min_rtt_timestamp_(ProtoTime::Zero()),
      congestion_window_(initial_tcp_congestion_window * kDefaultTCPMSS),
      initial_congestion_window_(initial_tcp_congestion_window *
                                 kDefaultTCPMSS),
      max_congestion_window_(max_tcp_congestion_window * kDefaultTCPMSS),
      min_congestion_window_(kDefaultMinimumCongestionWindow),
      pacing_rate_(QuicBandwidth::Zero()),
      pacing_gain_(1),
      congestion_window_gain_(1),
      congestion_window_gain_constant_(
          static_cast<float>(FLAG_quic_bbr_cwnd_gain)),
      rtt_variance_weight_(
          static_cast<float>(0.0/*FLAGS_quic_bbr_rtt_variation_weight*/)),
      num_startup_rtts_(kRoundTripsWithoutGrowthBeforeExitingStartup),
      exit_startup_on_loss_(false),
      cycle_current_offset_(0),
      last_cycle_start_(ProtoTime::Zero()),
      is_at_full_bandwidth_(false),
      rounds_without_bandwidth_gain_(0),
      bandwidth_at_last_round_(QuicBandwidth::Zero()),
      exiting_quiescence_(false),
      exit_probe_rtt_at_(ProtoTime::Zero()),
      probe_rtt_round_passed_(false),
      last_sample_is_app_limited_(false),
      recovery_state_(NOT_IN_RECOVERY),
      end_recovery_at_(0),
      recovery_window_(max_congestion_window_),
      rate_based_recovery_(false),
      slower_startup_(false),
      rate_based_startup_(false),
      initial_conservation_in_startup_(CONSERVATION),
      fully_drain_queue_(false),
      probe_rtt_based_on_bdp_(false),
      probe_rtt_skipped_if_similar_rtt_(false),
      probe_rtt_disabled_if_app_limited_(false),
      app_limited_since_last_probe_rtt_(false),
      min_rtt_since_last_probe_rtt_(TimeDelta::Infinite()) {
  EnterStartupMode();
}

BbrSenderV0::~BbrSenderV0() {}
bool BbrSenderV0::ShouldSendProbingPacket() const{
  if (pacing_gain_ <= 1) {
    return false;
  }
    return true;
}
void BbrSenderV0::SetInitialCongestionWindowInPackets(
    QuicPacketCount congestion_window) {
  if (mode_ == STARTUP) {
    initial_congestion_window_ = congestion_window * kDefaultTCPMSS;
    congestion_window_ = congestion_window * kDefaultTCPMSS;
  }
}

bool BbrSenderV0::InSlowStart() const {
  return mode_ == STARTUP;
}

void BbrSenderV0::OnPacketSent(ProtoTime sent_time,
                             QuicByteCount bytes_in_flight,
                             QuicPacketNumber packet_number,
                             QuicByteCount bytes,
                             HasRetransmittableData is_retransmittable) {
  last_sent_packet_ = packet_number;

  if (bytes_in_flight == 0 && sampler_->is_app_limited()) {
    exiting_quiescence_ = true;
  }

  if (!aggregation_epoch_start_time_.IsInitialized()) {
    aggregation_epoch_start_time_ = sent_time;
  }

  sampler_->OnPacketSent(sent_time, packet_number, bytes, bytes_in_flight,
                         is_retransmittable);
}

bool BbrSenderV0::CanSend(QuicByteCount bytes_in_flight) {
  return bytes_in_flight < GetCongestionWindow();
}

QuicBandwidth BbrSenderV0::PacingRate(QuicByteCount bytes_in_flight) const {
  if (pacing_rate_.IsZero()) {
    return kHighGain * QuicBandwidth::FromBytesAndTimeDelta(
                           initial_congestion_window_, GetMinRtt());
  }
  return pacing_rate_;
}

QuicBandwidth BbrSenderV0::BandwidthEstimate() const {
  return max_bandwidth_.GetBest();
}

QuicByteCount BbrSenderV0::GetCongestionWindow() const {
  if (mode_ == PROBE_RTT) {
    return ProbeRttCongestionWindow();
  }

  if (InRecovery() && !rate_based_recovery_ &&
      !(rate_based_startup_ && mode_ == STARTUP)) {
    return std::min(congestion_window_, recovery_window_);
  }

  return congestion_window_;
}

QuicByteCount BbrSenderV0::GetSlowStartThreshold() const {
  return 0;
}

bool BbrSenderV0::InRecovery() const {
  return recovery_state_ != NOT_IN_RECOVERY;
}

bool BbrSenderV0::IsProbingForMoreBandwidth() const {
  return (mode_ == PROBE_BW && pacing_gain_ > 1) || mode_ == STARTUP;
}
/*
void BbrSenderV0::SetFromConfig(const QuicConfig& config,
                              Perspective perspective) {
  if (config.HasClientRequestedIndependentOption(kLRTT, perspective)) {
    exit_startup_on_loss_ = true;
  }
  if (config.HasClientRequestedIndependentOption(k1RTT, perspective)) {
    num_startup_rtts_ = 1;
  }
  if (config.HasClientRequestedIndependentOption(k2RTT, perspective)) {
    num_startup_rtts_ = 2;
  }
  if (GetQuicReloadableFlag(quic_bbr_rate_recovery) &&
      config.HasClientRequestedIndependentOption(kBBRR, perspective)) {
    rate_based_recovery_ = true;
  }
  if (config.HasClientRequestedIndependentOption(kBBR1, perspective)) {
    max_aggregation_bytes_multiplier_ = 1.5;
  }
  if (config.HasClientRequestedIndependentOption(kBBR2, perspective)) {
    max_aggregation_bytes_multiplier_ = 2;
  }
  if (config.HasClientRequestedIndependentOption(kBBRS, perspective)) {
    slower_startup_ = true;
  }
  if (config.HasClientRequestedIndependentOption(kBBR3, perspective)) {
    fully_drain_queue_ = true;
  }
  if (config.HasClientRequestedIndependentOption(kBBS1, perspective)) {
    rate_based_startup_ = true;
  }
  if (config.HasClientRequestedIndependentOption(kBBS2, perspective)) {
    initial_conservation_in_startup_ = MEDIUM_GROWTH;
  }
  if (config.HasClientRequestedIndependentOption(kBBS3, perspective)) {
    initial_conservation_in_startup_ = GROWTH;
  }
  if (config.HasClientRequestedIndependentOption(kBBR4, perspective)) {
    max_ack_height_.SetWindowLength(2 * kBandwidthWindowSize);
  }
  if (config.HasClientRequestedIndependentOption(kBBR5, perspective)) {
    max_ack_height_.SetWindowLength(4 * kBandwidthWindowSize);
  }
  if (GetQuicReloadableFlag(quic_bbr_less_probe_rtt) &&
      config.HasClientRequestedIndependentOption(kBBR6, perspective)) {
    QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_bbr_less_probe_rtt, 1, 3);
    probe_rtt_based_on_bdp_ = true;
  }
  if (GetQuicReloadableFlag(quic_bbr_less_probe_rtt) &&
      config.HasClientRequestedIndependentOption(kBBR7, perspective)) {
    QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_bbr_less_probe_rtt, 2, 3);
    probe_rtt_skipped_if_similar_rtt_ = true;
  }
  if (GetQuicReloadableFlag(quic_bbr_less_probe_rtt) &&
      config.HasClientRequestedIndependentOption(kBBR8, perspective)) {
    QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_bbr_less_probe_rtt, 3, 3);
    probe_rtt_disabled_if_app_limited_ = true;
  }
  if (GetQuicReloadableFlag(quic_one_tlp) &&
      config.HasClientRequestedIndependentOption(kMIN1, perspective)) {
    QUIC_FLAG_COUNT_N(quic_reloadable_flag_quic_one_tlp, 2, 2);
    min_congestion_window_ = kMaxSegmentSize;
  }
}

void BbrSenderV0::AdjustNetworkParameters(QuicBandwidth bandwidth,
                                        TimeDelta rtt) {
  if (!bandwidth.IsZero()) {
    max_bandwidth_.Update(bandwidth, round_trip_count_);
  }
  if (!rtt.IsZero() && (min_rtt_ > rtt || min_rtt_.IsZero())) {
    min_rtt_ = rtt;
  }
}
*/
void BbrSenderV0::OnCongestionEvent(bool /*rtt_updated*/,
                                  QuicByteCount prior_in_flight,
                                  ProtoTime event_time,
                                  const AckedPacketVector& acked_packets,
                                  const LostPacketVector& lost_packets) {
  const QuicByteCount total_bytes_acked_before = sampler_->total_bytes_acked();

  bool is_round_start = false;
  bool min_rtt_expired = false;

  DiscardLostPackets(lost_packets);

  // Input the new data into the BBR model of the connection.
  if (!acked_packets.empty()) {
    QuicPacketNumber last_acked_packet = acked_packets.rbegin()->packet_number;
    is_round_start = UpdateRoundTripCounter(last_acked_packet);
    min_rtt_expired = UpdateBandwidthAndMinRtt(event_time, acked_packets);
    UpdateRecoveryState(last_acked_packet, !lost_packets.empty(),
                        is_round_start);

    const QuicByteCount bytes_acked =
        sampler_->total_bytes_acked() - total_bytes_acked_before;

    UpdateAckAggregationBytes(event_time, bytes_acked);
    if (max_aggregation_bytes_multiplier_ > 0) {
      if (unacked_packets_->bytes_in_flight() <=
          1.25 * GetTargetCongestionWindow(pacing_gain_)) {
        bytes_acked_since_queue_drained_ = 0;
      } else {
        bytes_acked_since_queue_drained_ += bytes_acked;
      }
    }
  }

  // Handle logic specific to PROBE_BW mode.
  if (mode_ == PROBE_BW) {
    UpdateGainCyclePhase(event_time, prior_in_flight, !lost_packets.empty());
  }

  // Handle logic specific to STARTUP and DRAIN modes.
  if (is_round_start && !is_at_full_bandwidth_) {
    CheckIfFullBandwidthReached();
  }
  MaybeExitStartupOrDrain(event_time);

  // Handle logic specific to PROBE_RTT.
  MaybeEnterOrExitProbeRtt(event_time, is_round_start, min_rtt_expired);

  // Calculate number of packets acked and lost.
  QuicByteCount bytes_acked =
      sampler_->total_bytes_acked() - total_bytes_acked_before;
  QuicByteCount bytes_lost = 0;
  for (const auto& packet : lost_packets) {
    bytes_lost += packet.bytes_lost;
  }

  // After the model is updated, recalculate the pacing rate and congestion
  // window.
  CalculatePacingRate();
  CalculateCongestionWindow(bytes_acked);
  CalculateRecoveryWindow(bytes_acked, bytes_lost);

  // Cleanup internal state.
  sampler_->RemoveObsoletePackets(unacked_packets_->GetLeastUnacked());
}

CongestionControlType BbrSenderV0::GetCongestionControlType() const {
  return kBBR_V0;
}

TimeDelta BbrSenderV0::GetMinRtt() const {
  return !min_rtt_.IsZero() ? min_rtt_ : rtt_stats_->initial_rtt();
}

QuicByteCount BbrSenderV0::GetTargetCongestionWindow(float gain) const {
  QuicByteCount bdp = GetMinRtt() * BandwidthEstimate();
  QuicByteCount congestion_window = gain * bdp;

  // BDP estimate will be zero if no bandwidth samples are available yet.
  if (congestion_window == 0) {
    congestion_window = gain * initial_congestion_window_;
  }

  return std::max(congestion_window, min_congestion_window_);
}

QuicByteCount BbrSenderV0::ProbeRttCongestionWindow() const {
  if (probe_rtt_based_on_bdp_) {
    return GetTargetCongestionWindow(kModerateProbeRttMultiplier);
  }
  return min_congestion_window_;
}

void BbrSenderV0::EnterStartupMode() {
  mode_ = STARTUP;
  pacing_gain_ = kHighGain;
  congestion_window_gain_ = kHighGain;
}

void BbrSenderV0::EnterProbeBandwidthMode(ProtoTime now) {
  mode_ = PROBE_BW;
  congestion_window_gain_ = congestion_window_gain_constant_;

  // Pick a random offset for the gain cycle out of {0, 2..7} range. 1 is
  // excluded because in that case increased gain and decreased gain would not
  // follow each other.
  cycle_current_offset_ = random_->nextInt() % (kGainCycleLength - 1);
  if (cycle_current_offset_ >= 1) {
    cycle_current_offset_ += 1;
  }

  last_cycle_start_ = now;
  pacing_gain_ = kPacingGain[cycle_current_offset_];
}

void BbrSenderV0::DiscardLostPackets(const LostPacketVector& lost_packets) {
  for (const LostPacket& packet : lost_packets) {
    sampler_->OnPacketLost(packet.packet_number);
  }
}

bool BbrSenderV0::UpdateRoundTripCounter(QuicPacketNumber last_acked_packet) {
  if (last_acked_packet > current_round_trip_end_) {
    round_trip_count_++;
    current_round_trip_end_ = last_sent_packet_;
    return true;
  }

  return false;
}

bool BbrSenderV0::UpdateBandwidthAndMinRtt(
    ProtoTime now,
    const AckedPacketVector& acked_packets) {
  TimeDelta sample_min_rtt = TimeDelta::Infinite();
  for (const auto& packet : acked_packets) {
    if (/*GetQuicReloadableFlag(quic_use_incremental_ack_processing3) &&*/
        packet.bytes_acked == 0) {
      // Skip acked packets with 0 in flight bytes when updating bandwidth.
      continue;
    }
    /*
    BandwidthSample bandwidth_sample =
        sampler_->OnPacketAcknowledged(now, packet.packet_number);
    last_sample_is_app_limited_ = bandwidth_sample.is_app_limited;
    if (!bandwidth_sample.rtt.IsZero()) {
      sample_min_rtt = std::min(sample_min_rtt, bandwidth_sample.rtt);
    }
    if (!bandwidth_sample.is_app_limited ||
        bandwidth_sample.bandwidth > BandwidthEstimate()) {
      max_bandwidth_.Update(bandwidth_sample.bandwidth, round_trip_count_);
    }
  }*/
      BandwidthSample bandwidth_sample =
        sampler_->OnPacketAcknowledged(now, packet.packet_number);
    last_sample_is_app_limited_ = bandwidth_sample.state_at_send.is_app_limited;
    /*has_non_app_limited_sample_ |=
        !bandwidth_sample.state_at_send.is_app_limited;*/
    if (!bandwidth_sample.rtt.IsZero()) {
      sample_min_rtt = std::min(sample_min_rtt, bandwidth_sample.rtt);
    }

    if (!bandwidth_sample.state_at_send.is_app_limited ||
        bandwidth_sample.bandwidth > BandwidthEstimate()) {
      max_bandwidth_.Update(bandwidth_sample.bandwidth, round_trip_count_);
    }
  }

  // If none of the RTT samples are valid, return immediately.
  if (sample_min_rtt.IsInfinite()) {
    return false;
  }
  min_rtt_since_last_probe_rtt_ =
      std::min(min_rtt_since_last_probe_rtt_, sample_min_rtt);

  // Do not expire min_rtt if none was ever available.
  bool min_rtt_expired =
      !min_rtt_.IsZero() && (now > (min_rtt_timestamp_ + kMinRttExpiry));

  if (min_rtt_expired || sample_min_rtt < min_rtt_ || min_rtt_.IsZero()) {
    //QUIC_DVLOG(2) << "Min RTT updated, old value: " << min_rtt_
     //             << ", new value: " << sample_min_rtt
     //             << ", current time: " << now.ToDebuggingValue();

    if (ShouldExtendMinRttExpiry()) {
      min_rtt_expired = false;
    } else {
      min_rtt_ = sample_min_rtt;
    }
    min_rtt_timestamp_ = now;
    // Reset since_last_probe_rtt fields.
    min_rtt_since_last_probe_rtt_ = TimeDelta::Infinite();
    app_limited_since_last_probe_rtt_ = false;
  }

  return min_rtt_expired;
}

bool BbrSenderV0::ShouldExtendMinRttExpiry() const {
  if (probe_rtt_disabled_if_app_limited_ && app_limited_since_last_probe_rtt_) {
    // Extend the current min_rtt if we've been app limited recently.
    return true;
  }
  const bool min_rtt_increased_since_last_probe =
      min_rtt_since_last_probe_rtt_ > min_rtt_ * kSimilarMinRttThreshold;
  if (probe_rtt_skipped_if_similar_rtt_ && app_limited_since_last_probe_rtt_ &&
      !min_rtt_increased_since_last_probe) {
    // Extend the current min_rtt if we've been app limited recently and an rtt
    // has been measured in that time that's less than 12.5% more than the
    // current min_rtt.
    return true;
  }
  return false;
}

void BbrSenderV0::UpdateGainCyclePhase(ProtoTime now,
                                     QuicByteCount prior_in_flight,
                                     bool has_losses) {
  // In most cases, the cycle is advanced after an RTT passes.
  bool should_advance_gain_cycling = now - last_cycle_start_ > GetMinRtt();

  // If the pacing gain is above 1.0, the connection is trying to probe the
  // bandwidth by increasing the number of bytes in flight to at least
  // pacing_gain * BDP.  Make sure that it actually reaches the target, as long
  // as there are no losses suggesting that the buffers are not able to hold
  // that much.
  if (pacing_gain_ > 1.0 && !has_losses &&
      prior_in_flight < GetTargetCongestionWindow(pacing_gain_)) {
    should_advance_gain_cycling = false;
  }

  // If pacing gain is below 1.0, the connection is trying to drain the extra
  // queue which could have been incurred by probing prior to it.  If the number
  // of bytes in flight falls down to the estimated BDP value earlier, conclude
  // that the queue has been successfully drained and exit this cycle early.
  if (pacing_gain_ < 1.0 && prior_in_flight <= GetTargetCongestionWindow(1)) {
    should_advance_gain_cycling = true;
  }

  if (should_advance_gain_cycling) {
    cycle_current_offset_ = (cycle_current_offset_ + 1) % kGainCycleLength;
    last_cycle_start_ = now;
    // Stay in low gain mode until the target BDP is hit.
    // Low gain mode will be exited immediately when the target BDP is achieved.
    if (fully_drain_queue_ && pacing_gain_ < 1 &&
        kPacingGain[cycle_current_offset_] == 1 &&
        prior_in_flight > GetTargetCongestionWindow(1)) {
      return;
    }
    pacing_gain_ = kPacingGain[cycle_current_offset_];
  }
}

void BbrSenderV0::CheckIfFullBandwidthReached() {
  if (last_sample_is_app_limited_) {
    return;
  }

  QuicBandwidth target = bandwidth_at_last_round_ * kStartupGrowthTarget;
  if (BandwidthEstimate() >= target) {
    bandwidth_at_last_round_ = BandwidthEstimate();
    rounds_without_bandwidth_gain_ = 0;
    return;
  }

  rounds_without_bandwidth_gain_++;
  if ((rounds_without_bandwidth_gain_ >= num_startup_rtts_) ||
      (exit_startup_on_loss_ && InRecovery())) {
    is_at_full_bandwidth_ = true;
  }
}

void BbrSenderV0::MaybeExitStartupOrDrain(ProtoTime now) {
  if (mode_ == STARTUP && is_at_full_bandwidth_) {
    mode_ = DRAIN;
    pacing_gain_ = kDrainGain;
    congestion_window_gain_ = kHighGain;
  }
  if (mode_ == DRAIN &&
      unacked_packets_->bytes_in_flight() <= GetTargetCongestionWindow(1)) {
    EnterProbeBandwidthMode(now);
  }
}

void BbrSenderV0::MaybeEnterOrExitProbeRtt(ProtoTime now,
                                         bool is_round_start,
                                         bool min_rtt_expired) {
  if (min_rtt_expired && !exiting_quiescence_ && mode_ != PROBE_RTT) {
    mode_ = PROBE_RTT;
    pacing_gain_ = 1;
    // Do not decide on the time to exit PROBE_RTT until the |bytes_in_flight|
    // is at the target small value.
    exit_probe_rtt_at_ = ProtoTime::Zero();
  }

  if (mode_ == PROBE_RTT) {
    sampler_->OnAppLimited();

    if (exit_probe_rtt_at_ == ProtoTime::Zero()) {
      // If the window has reached the appropriate size, schedule exiting
      // PROBE_RTT.  The CWND during PROBE_RTT is kMinimumCongestionWindow, but
      // we allow an extra packet since QUIC checks CWND before sending a
      // packet.
      if (unacked_packets_->bytes_in_flight() <
          ProbeRttCongestionWindow() + kMaxOutgoingPacketSize) {
        exit_probe_rtt_at_ = now + kProbeRttTime;
        probe_rtt_round_passed_ = false;
      }
    } else {
      if (is_round_start) {
        probe_rtt_round_passed_ = true;
      }
      if (now >= exit_probe_rtt_at_ && probe_rtt_round_passed_) {
        min_rtt_timestamp_ = now;
        if (!is_at_full_bandwidth_) {
          EnterStartupMode();
        } else {
          EnterProbeBandwidthMode(now);
        }
      }
    }
  }

  exiting_quiescence_ = false;
}

void BbrSenderV0::UpdateRecoveryState(QuicPacketNumber last_acked_packet,
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
        if (mode_ == STARTUP) {
          recovery_state_ = initial_conservation_in_startup_;
        }
        // This will cause the |recovery_window_| to be set to the correct
        // value in CalculateRecoveryWindow().
        recovery_window_ = 0;
        // Since the conservation phase is meant to be lasting for a whole
        // round, extend the current round as if it were started right now.
        current_round_trip_end_ = last_sent_packet_;
      }
      break;

    case CONSERVATION:
    case MEDIUM_GROWTH:
      if (is_round_start) {
        recovery_state_ = GROWTH;
      }
      //QUIC_FALLTHROUGH_INTENDED;

    case GROWTH:
      // Exit recovery if appropriate.
      if (!has_losses && last_acked_packet > end_recovery_at_) {
        recovery_state_ = NOT_IN_RECOVERY;
      }

      break;
  }
}

// TODO(ianswett): Move this logic into BandwidthSampler.
void BbrSenderV0::UpdateAckAggregationBytes(ProtoTime ack_time,
                                          QuicByteCount newly_acked_bytes) {
  // Compute how many bytes are expected to be delivered, assuming max bandwidth
  // is correct.
  QuicByteCount expected_bytes_acked =
      max_bandwidth_.GetBest() * (ack_time - aggregation_epoch_start_time_);
  // Reset the current aggregation epoch as soon as the ack arrival rate is less
  // than or equal to the max bandwidth.
  if (aggregation_epoch_bytes_ <= expected_bytes_acked) {
    // Reset to start measuring a new aggregation epoch.
    aggregation_epoch_bytes_ = newly_acked_bytes;
    aggregation_epoch_start_time_ = ack_time;
    return;
  }

  // Compute how many extra bytes were delivered vs max bandwidth.
  // Include the bytes most recently acknowledged to account for stretch acks.
  aggregation_epoch_bytes_ += newly_acked_bytes;
  max_ack_height_.Update(aggregation_epoch_bytes_ - expected_bytes_acked,
                         round_trip_count_);
}

void BbrSenderV0::CalculatePacingRate() {
  if (BandwidthEstimate().IsZero()) {
    return;
  }

  QuicBandwidth target_rate = pacing_gain_ * BandwidthEstimate();
  if (rate_based_recovery_ && InRecovery()) {
    //QUIC_FLAG_COUNT(quic_reloadable_flag_quic_bbr_rate_recovery);
    pacing_rate_ = pacing_gain_ * max_bandwidth_.GetThirdBest();
  }
  if (is_at_full_bandwidth_) {
    pacing_rate_ = target_rate;
    return;
  }

  // Pace at the rate of initial_window / RTT as soon as RTT measurements are
  // available.
  if (pacing_rate_.IsZero() && !rtt_stats_->min_rtt().IsZero()) {
    pacing_rate_ = QuicBandwidth::FromBytesAndTimeDelta(
        initial_congestion_window_, rtt_stats_->min_rtt());
    return;
  }
  // Slow the pacing rate in STARTUP once loss has ever been detected.
  const bool has_ever_detected_loss = end_recovery_at_.IsInitialized();
  if (slower_startup_ && has_ever_detected_loss) {
    pacing_rate_ = kStartupAfterLossGain * BandwidthEstimate();
    return;
  }

  // Do not decrease the pacing rate during the startup.
  pacing_rate_ = std::max(pacing_rate_, target_rate);
}

void BbrSenderV0::CalculateCongestionWindow(QuicByteCount bytes_acked) {
  if (mode_ == PROBE_RTT) {
    return;
  }

  QuicByteCount target_window =
      GetTargetCongestionWindow(congestion_window_gain_);

  if (rtt_variance_weight_ > 0.f && !BandwidthEstimate().IsZero()) {
    target_window += rtt_variance_weight_ * rtt_stats_->mean_deviation() *
                     BandwidthEstimate();
  } else if (max_aggregation_bytes_multiplier_ > 0 && is_at_full_bandwidth_) {
    // Subtracting only half the bytes_acked_since_queue_drained ensures sending
    // doesn't completely stop for a long period of time if the queue hasn't
    // been drained recently.
    if (max_aggregation_bytes_multiplier_ * max_ack_height_.GetBest() >
        bytes_acked_since_queue_drained_ / 2) {
      target_window +=
          max_aggregation_bytes_multiplier_ * max_ack_height_.GetBest() -
          bytes_acked_since_queue_drained_ / 2;
    }
  } else if (is_at_full_bandwidth_) {
    target_window += max_ack_height_.GetBest();
  }

  if (false/*GetQuicReloadableFlag(quic_bbr_add_tso_cwnd)*/) {
    // QUIC doesn't have TSO, but it does have similarly quantized pacing, so
    // allow extra CWND to make QUIC's BBR CWND identical to TCP's.
    QuicByteCount tso_segs_goal = 0;
    if (pacing_rate_ < QuicBandwidth::FromKBitsPerSecond(1200)) {
      tso_segs_goal = kDefaultTCPMSS;
    } else if (pacing_rate_ < QuicBandwidth::FromKBitsPerSecond(24000)) {
      tso_segs_goal = 2 * kDefaultTCPMSS;
    } else {
      tso_segs_goal =
          std::min(pacing_rate_ * TimeDelta::FromMilliseconds(1),
                   /* 64k */ static_cast<QuicByteCount>(1 << 16));
    }
    target_window += 3 * tso_segs_goal;
  }

  // Instead of immediately setting the target CWND as the new one, BBR grows
  // the CWND towards |target_window| by only increasing it |bytes_acked| at a
  // time.
  if (is_at_full_bandwidth_) {
    congestion_window_ =
        std::min(target_window, congestion_window_ + bytes_acked);
  } else if (congestion_window_ < target_window ||
             sampler_->total_bytes_acked() < initial_congestion_window_) {
    // If the connection is not yet out of startup phase, do not decrease the
    // window.
    congestion_window_ = congestion_window_ + bytes_acked;
  }

  // Enforce the limits on the congestion window.
  congestion_window_ = std::max(congestion_window_, min_congestion_window_);
  congestion_window_ = std::min(congestion_window_, max_congestion_window_);
}

void BbrSenderV0::CalculateRecoveryWindow(QuicByteCount bytes_acked,
                                        QuicByteCount bytes_lost) {
  if (rate_based_recovery_ || (rate_based_startup_ && mode_ == STARTUP)) {
    return;
  }

  if (recovery_state_ == NOT_IN_RECOVERY) {
    return;
  }

  // Set up the initial recovery window.
  if (recovery_window_ == 0) {
    recovery_window_ = unacked_packets_->bytes_in_flight() + bytes_acked;
    recovery_window_ = std::max(min_congestion_window_, recovery_window_);
    return;
  }

  // Remove losses from the recovery window, while accounting for a potential
  // integer underflow.
  recovery_window_ = recovery_window_ >= bytes_lost
                         ? recovery_window_ - bytes_lost
                         : kMaxSegmentSize;

  // In CONSERVATION mode, just subtracting losses is sufficient.  In GROWTH,
  // release additional |bytes_acked| to achieve a slow-start-like behavior.
  // In MEDIUM_GROWTH, release |bytes_acked| / 2 to split the difference.
  if (recovery_state_ == GROWTH) {
    recovery_window_ += bytes_acked;
  } else if (recovery_state_ == MEDIUM_GROWTH) {
    recovery_window_ += bytes_acked / 2;
  }

  // Sanity checks.  Ensure that we always allow to send at least
  // |bytes_acked| in response.
  recovery_window_ = std::max(
      recovery_window_, unacked_packets_->bytes_in_flight() + bytes_acked);
  recovery_window_ = std::max(min_congestion_window_, recovery_window_);
}

std::string BbrSenderV0::GetDebugState() const {
  std::ostringstream stream;
  stream << ExportDebugState();
  return stream.str();
}

void BbrSenderV0::OnApplicationLimited(QuicByteCount bytes_in_flight) {
  if (bytes_in_flight >= GetCongestionWindow()) {
    return;
  }

  app_limited_since_last_probe_rtt_ = true;
  sampler_->OnAppLimited();
  //QUIC_DVLOG(2) << "Becoming application limited. Last sent packet: "
  //              << last_sent_packet_ << ", CWND: " << GetCongestionWindow();
}

BbrSenderV0::DebugState BbrSenderV0::ExportDebugState() const {
  return DebugState(*this);
}

static std::string ModeToString(BbrSenderV0::Mode mode) {
  switch (mode) {
    case BbrSenderV0::STARTUP:
      return "STARTUP";
    case BbrSenderV0::DRAIN:
      return "DRAIN";
    case BbrSenderV0::PROBE_BW:
      return "PROBE_BW";
    case BbrSenderV0::PROBE_RTT:
      return "PROBE_RTT";
  }
  return "???";
}

std::ostream& operator<<(std::ostream& os, const BbrSenderV0::Mode& mode) {
  os << ModeToString(mode);
  return os;
}

std::ostream& operator<<(std::ostream& os, const BbrSenderV0::DebugState& state) {
  os << "Mode: " << ModeToString(state.mode) << std::endl;
  os << "Maximum bandwidth: " << state.max_bandwidth << std::endl;
  os << "Round trip counter: " << state.round_trip_count << std::endl;
  os << "Gain cycle index: " << static_cast<int>(state.gain_cycle_index)
     << std::endl;
  os << "Congestion window: " << state.congestion_window << " bytes"
     << std::endl;

  if (state.mode == BbrSenderV0::STARTUP) {
    os << "(startup) Bandwidth at last round: " << state.bandwidth_at_last_round
       << std::endl;
    os << "(startup) Rounds without gain: "
       << state.rounds_without_bandwidth_gain << std::endl;
  }

  os << "Minimum RTT: " << state.min_rtt << std::endl;
  os << "Minimum RTT timestamp: " << state.min_rtt_timestamp.ToDebuggingValue()
     << std::endl;

  os << "Last sample is app-limited: "
     << (state.last_sample_is_app_limited ? "yes" : "no");

  return os;
}

}  // namespace net
