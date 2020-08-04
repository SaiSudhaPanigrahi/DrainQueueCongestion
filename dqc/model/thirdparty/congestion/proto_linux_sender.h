#pragma once
#include <ostream>
#include "proto_send_algorithm_interface.h"
#include "proto_bandwidth_sampler.h"
#include "proto_windowed_filter.h"
#include "logging.h"
#include "ns-tcp.h"
namespace dqc{
class RttStats;
typedef uint64_t QuicRoundTripCount;
class LinuxSender : public SendAlgorithmInterface {
 public:
  // Indicates how the congestion control limits the amount of bytes in flight.
  enum RecoveryState {
    // Do not limit.
    NOT_IN_RECOVERY,
    // Allow an extra outstanding byte for each byte acknowledged.
    CONSERVATION,
    // Allow two extra outstanding bytes for each byte acknowledged (slow
    // start).
    GROWTH
  };
  LinuxSender(ProtoTime now,
            const RttStats* rtt_stats,
            const UnackedPacketMapInfoInterface* unacked_packets,
            QuicPacketCount initial_tcp_congestion_window,
            QuicPacketCount max_tcp_congestion_window,
            Random* random);
  LinuxSender(const LinuxSender&) = delete;
  LinuxSender& operator=(const LinuxSender&) = delete;
  ~LinuxSender() override;

  // Start implementation of SendAlgorithmInterface.
  bool InSlowStart() const override;
  bool InRecovery() const override;
  bool ShouldSendProbingPacket() const override;


  void AdjustNetworkParameters(QuicBandwidth bandwidth,
                               TimeDelta rtt,
                               bool allow_cwnd_to_decrease) override;
  void SetNumEmulatedConnections(int num_connections) override {}
  void SetInitialCongestionWindowInPackets(
      QuicPacketCount congestion_window) override;
  void OnCongestionEvent(bool rtt_updated,
                         QuicByteCount prior_in_flight,
                         ProtoTime event_time,
                         const AckedPacketVector& acked_packets,
                         const LostPacketVector& lost_packets) override;
  void OnPacketSent(ProtoTime sent_time,
                    QuicByteCount bytes_in_flight,
                    QuicPacketNumber packet_number,
                    QuicByteCount bytes,
                    HasRetransmittableData is_retransmittable) override;
  void OnRetransmissionTimeout(bool packets_retransmitted) override {}
  void OnConnectionMigration() override {}
  bool CanSend(QuicByteCount bytes_in_flight) override;
  QuicBandwidth PacingRate(QuicByteCount bytes_in_flight) const override;
  QuicBandwidth BandwidthEstimate() const override;
  QuicByteCount GetCongestionWindow() const override;
  QuicByteCount GetSlowStartThreshold() const override;
  CongestionControlType GetCongestionControlType() const override;
  std::string GetDebugState() const override;
  void OnApplicationLimited(QuicByteCount bytes_in_flight) override;
  void OnUpdateEcnBytes(uint64_t ecn_ce_count) override {}
  // End implementation of SendAlgorithmInterface.


 private:
  // Discards the lost packets from BandwidthSampler state.
  void DiscardLostPackets(const LostPacketVector& lost_packets);
  // Updates the round-trip counter if a round-trip has passed.  Returns true if
  // the counter has been advanced.
  bool UpdateRoundTripCounter(QuicPacketNumber last_acked_packet);
  // Updates the current bandwidth and min_rtt estimate based on the samples for
  // the received acknowledgements.  Returns true if min_rtt has expired.
  void UpdateBandwidthSample(ProtoTime now,
                                const AckedPacketVector& acked_packets);
  void UpdateRecoveryState(QuicPacketNumber last_acked_packet,
                           bool has_losses,
                           bool is_round_start);
  const RttStats* rtt_stats_;
  const UnackedPacketMapInfoInterface* unacked_packets_;
  Random* random_;


  // Bandwidth sampler provides BBR with the bandwidth measurements at
  // individual points.
  BandwidthSampler sampler_;
  // The number of the round trips that have occurred during the connection.
  QuicRoundTripCount round_trip_count_;

  // The packet number of the most recently sent packet.
  QuicPacketNumber last_sent_packet_;
  // Acknowledgement of any packet after |current_round_trip_end_| will cause
  // the round trip counter to advance.
  QuicPacketNumber current_round_trip_end_;

  // The maximum allowed number of bytes in flight.
  QuicByteCount congestion_window_;

  // The initial value of the |congestion_window_|.
  QuicByteCount initial_congestion_window_;

  // The largest value the |congestion_window_| can achieve.
  QuicByteCount max_congestion_window_;

  // The smallest value the |congestion_window_| can achieve.
  QuicByteCount min_congestion_window_;
  // Set to true upon exiting quiescence.
  bool exiting_quiescence_;


  // Indicates whether the most recent bandwidth sample was marked as
  // app-limited.
  bool last_sample_is_app_limited_;
  // Indicates whether any non app-limited samples have been recorded.
  bool has_non_app_limited_sample_;
  // Indicates app-limited calls should be ignored as long as there's
  // enough data inflight to see more bandwidth when necessary.
  bool flexible_app_limited_;

  // Current state of recovery.
  RecoveryState recovery_state_;
  // Receiving acknowledgement of a packet after |end_recovery_at_| will cause
  // BBR to exit the recovery mode.  A value above zero indicates at least one
  // loss has been detected, so it must not be set back to zero.
  QuicPacketNumber end_recovery_at_;
  // A window used to limit the number of bytes in flight during loss recovery.
  QuicByteCount recovery_window_;
  // If true, consider all samples in recovery app-limited.
  bool is_app_limited_recovery_;

  QuicByteCount startup_bytes_lost_;
  const bool always_get_bw_sample_when_acked_;
  bool has_seen_rtt_{false};
  tcp_sock tcp_;
  struct tcp_congestion_ops *e_{nullptr};
  struct rate_sample rate_sample_;
};
}
