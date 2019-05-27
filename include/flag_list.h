PROTO_FLAG(uint64_t,FLAG_max_send_buf_size,256u)
PROTO_FLAG(uint32_t,FLAG_packet_payload,1400u)
// Congestion window gain for QUIC BBR during PROBE_BW phase.
PROTO_FLAG(double, FLAG_quic_bbr_cwnd_gain, 2.0f)
// If true, in BbrSender, always get a bandwidth sample when a packet is acked,
// even if packet.bytes_acked is zero.
PROTO_FLAG(bool,
          FLAG_quic_reloadable_flag_quic_always_get_bw_sample_when_acked,
          true)

// When you\'re app-limited entering recovery, stay app-limited until you exit
// recovery in QUIC BBR.
PROTO_FLAG(bool, FLAG_quic_reloadable_flag_quic_bbr_app_limited_recovery, false)


// When true, ensure BBR allows at least one MSS to be sent in response to an
// ACK in packet conservation.
PROTO_FLAG(bool, FLAG_quic_reloadable_flag_quic_bbr_one_mss_conservation, false)

// Add 3 connection options to decrease the pacing and CWND gain in QUIC BBR
// STARTUP.
PROTO_FLAG(bool, FLAG_quic_reloadable_flag_quic_bbr_slower_startup3, true)

// When true, the LOSS connection option allows for 1/8 RTT of reording instead
// of the current 1/8th threshold which has been found to be too large for fast
// loss recovery.
PROTO_FLAG(bool,
          FLAG_quic_reloadable_flag_quic_eighth_rtt_loss_detection,
          false)

// Enables the BBQ5 connection option, which forces saved aggregation values to
// expire when the bandwidth increases more than 25% in QUIC BBR STARTUP.
PROTO_FLAG(bool, FLAG_quic_reloadable_flag_quic_bbr_slower_startup4, false)

// When true and the BBR9 connection option is present, BBR only considers
// bandwidth samples app-limited if they're not filling the pipe.
PROTO_FLAG(bool, FLAG_quic_reloadable_flag_quic_bbr_flexible_app_limited, false)

// When in STARTUP and recovery, do not add bytes_acked to QUIC BBR's CWND in
// CalculateCongestionWindow()
PROTO_FLAG(
    bool,
    FLAG_quic_reloadable_flag_quic_bbr_no_bytes_acked_in_startup_recovery,
    false)
