#define NS_PROTOCOL "tcp_bbr2.c"
#include "ns-linux-c.h"
#include "ns-linux-util.h"
#include <stdio.h>
#include "private-const.h"
/* Scale factor for rate in pkt/uSec unit to avoid truncation in bandwidth
 * estimation. The rate unit ~= (1500 bytes / 1 usec / 2^24) ~= 715 bps.
 * This handles bandwidths from 0.06pps (715bps) to 256Mpps (3Tbps) in a u32.
 * Since the minimum window is >=4 packets, the lower bound isn't
 * an issue. The upper bound isn't an issue with existing technologies.
 */
#define BW_SCALE 24
#define BW_UNIT (1 << BW_SCALE)

#define BBR_SCALE 8	/* scaling factor for fractions in BBR (e.g. gains) */
#define BBR_UNIT (1 << BBR_SCALE)

#define FLAG_DEBUG_VERBOSE	0x1	/* Verbose debugging messages */
#define FLAG_DEBUG_LOOPBACK	0x2	/* Do NOT skip loopback addr */

#define CYCLE_LEN		8	/* number of phases in a pacing gain cycle */

/* BBR has the following modes for deciding how fast to send: */
enum bbr_mode {
	BBR_STARTUP,	/* ramp up sending rate rapidly to fill pipe */
	BBR_DRAIN,	/* drain any queue created during startup */
	BBR_PROBE_BW,	/* discover, share bw: pace around estimated bw */
	BBR_PROBE_RTT,	/* cut inflight to min to probe min_rtt */
};

/* How does the incoming ACK stream relate to our bandwidth probing? */
enum bbr_ack_phase {
	BBR_ACKS_INIT,		  /* not probing; not getting probe feedback */
	BBR_ACKS_REFILLING,	  /* sending at est. bw to fill pipe */
	BBR_ACKS_PROBE_STARTING,  /* inflight rising to probe bw */
	BBR_ACKS_PROBE_FEEDBACK,  /* getting feedback from bw probing */
	BBR_ACKS_PROBE_STOPPING,  /* stopped probing; still getting feedback */
};

/* BBR congestion control block */
struct bbr {
	u32	min_rtt_us;	        /* min RTT in min_rtt_win_sec window */
	u32	min_rtt_stamp;	        /* timestamp of min_rtt_us */
	u32	probe_rtt_done_stamp;   /* end time for BBR_PROBE_RTT mode */
	u32	probe_rtt_min_us;	/* min RTT in bbr_probe_rtt_win_ms window */
	u32	probe_rtt_min_stamp;	/* timestamp of probe_rtt_min_us*/
	u32     next_rtt_delivered; /* scb->tx.delivered at end of round */
	u32	prior_rcv_nxt;	/* tp->rcv_nxt when CE state last changed */
	u64	cycle_mstamp;	     /* time of this cycle phase start */
	u32     mode:3,		     /* current bbr_mode in state machine */
		prev_ca_state:3,     /* CA state on previous ACK */
		packet_conservation:1,  /* use packet conservation? */
		round_start:1,	     /* start of packet-timed tx->ack round? */
		ce_state:1,          /* If most recent data has CE bit set */
		bw_probe_up_rounds:5,   /* cwnd-limited rounds in PROBE_UP */
		try_fast_path:1, 	/* can we take fast path? */
		unused2:11,
		idle_restart:1,	     /* restarting after idle? */
		probe_rtt_round_done:1,  /* a BBR_PROBE_RTT round at 4 pkts? */
		cycle_idx:3,	/* current index in pacing_gain cycle array */
		has_seen_rtt:1;	     /* have we seen an RTT sample yet? */
	u32	pacing_gain:11,	/* current gain for setting pacing rate */
		cwnd_gain:11,	/* current gain for setting cwnd */
		full_bw_reached:1,   /* reached full bw in Startup? */
		full_bw_cnt:2,	/* number of rounds without large bw gains */
		init_cwnd:7;	/* initial cwnd */
	u32	prior_cwnd;	/* prior cwnd upon entering loss recovery */
	u32	full_bw;	/* recent bw, to estimate if pipe is full */

	/* For tracking ACK aggregation: */
	u64	ack_epoch_mstamp;	/* start of ACK sampling epoch */
	u16	extra_acked[2];		/* max excess data ACKed in epoch */
	u32	ack_epoch_acked:20,	/* packets (S)ACKed in sampling epoch */
		extra_acked_win_rtts:5,	/* age of extra_acked, in round trips */
		extra_acked_win_idx:1,	/* current index in extra_acked array */
	/* BBR v2 state: */
		unused1:2,
		startup_ecn_rounds:2,	/* consecutive hi ECN STARTUP rounds */
		loss_in_cycle:1,	/* packet loss in this cycle? */
		ecn_in_cycle:1;		/* ECN in this cycle? */
	u32	loss_round_delivered; /* scb->tx.delivered ending loss round */
	u32	undo_bw_lo;	     /* bw_lo before latest losses */
	u32	undo_inflight_lo;    /* inflight_lo before latest losses */
	u32	undo_inflight_hi;    /* inflight_hi before latest losses */
	u32	bw_latest;	 /* max delivered bw in last round trip */
	u32	bw_lo;		 /* lower bound on sending bandwidth */
	u32	bw_hi[2];	 /* upper bound of sending bandwidth range*/
	u32	inflight_latest; /* max delivered data in last round trip */
	u32	inflight_lo;	 /* lower bound of inflight data range */
	u32	inflight_hi;	 /* upper bound of inflight data range */
	u32	bw_probe_up_cnt; /* packets delivered per inflight_hi incr */
	u32	bw_probe_up_acks;  /* packets (S)ACKed since inflight_hi incr */
	u32	probe_wait_us;	 /* PROBE_DOWN until next clock-driven probe */
	u32	ecn_eligible:1,	/* sender can use ECN (RTT, handshake)? */
		ecn_alpha:9,	/* EWMA delivered_ce/delivered; 0..256 */
		bw_probe_samples:1,    /* rate samples reflect bw probing? */
		prev_probe_too_high:1, /* did last PROBE_UP go too high? */
		stopped_risky_probe:1, /* last PROBE_UP stopped due to risk? */
		rounds_since_probe:8,  /* packet-timed rounds since probed bw */
		loss_round_start:1,    /* loss_round_delivered round trip? */
		loss_in_round:1,       /* loss marked in this round trip? */
		ecn_in_round:1,	       /* ECN marked in this round trip? */
		ack_phase:3,	       /* bbr_ack_phase: meaning of ACKs */
		loss_events_in_round:4,/* losses in STARTUP round */
		initialized:1;	       /* has bbr_init() been called? */
	u32	alpha_last_delivered;	 /* tp->delivered    at alpha update */
	u32	alpha_last_delivered_ce; /* tp->delivered_ce at alpha update */

	/* Params configurable using setsockopt. Refer to correspoding
	 * module param for detailed description of params.
	 */
	struct bbr_params {
		u32	high_gain:11,		/* max allowed value: 2047 */
			drain_gain:10,		/* max allowed value: 1023 */
			cwnd_gain:11;		/* max allowed value: 2047 */
		u32	cwnd_min_target:4,	/* max allowed value: 15 */
			min_rtt_win_sec:5,	/* max allowed value: 31 */
			probe_rtt_mode_ms:9,	/* max allowed value: 511 */
			full_bw_cnt:3,		/* max allowed value: 7 */
			bw_rtts:5,		/* max allowed value: 31 */
			cwnd_tso_budget:1,	/* allowed values: {0, 1} */
			unused3:1,
			drain_to_target:1,	/* boolean */
			precise_ece_ack:1,	/* boolean */
			extra_acked_in_startup:1, /* allowed values: {0, 1} */
			fast_path:1;		/* boolean */
		u32	full_bw_thresh:10,	/* max allowed value: 1023 */
			startup_cwnd_gain:11,	/* max allowed value: 2047 */
			bw_probe_pif_gain:9,	/* max allowed value: 511 */
			usage_based_cwnd:1, 	/* boolean */
			unused2:1;
		u16	probe_rtt_win_ms:14,	/* max allowed value: 16383 */
			refill_add_inc:2;	/* max allowed value: 3 */
		u16	extra_acked_gain:11,	/* max allowed value: 2047 */
			extra_acked_win_rtts:5; /* max allowed value: 31*/
		u16	pacing_gain[CYCLE_LEN]; /* max allowed value: 1023 */
		/* Mostly BBR v2 parameters below here: */
		u32	ecn_alpha_gain:8,	/* max allowed value: 255 */
			ecn_factor:8,		/* max allowed value: 255 */
			ecn_thresh:8,		/* max allowed value: 255 */
			beta:8;			/* max allowed value: 255 */
		u32	ecn_max_rtt_us:19,	/* max allowed value: 524287 */
			bw_probe_reno_gain:9,	/* max allowed value: 511 */
			full_loss_cnt:4;	/* max allowed value: 15 */
		u32	probe_rtt_cwnd_gain:8,	/* max allowed value: 255 */
			inflight_headroom:8,	/* max allowed value: 255 */
			loss_thresh:8,		/* max allowed value: 255 */
			bw_probe_max_rounds:8;	/* max allowed value: 255 */
		u32	bw_probe_rand_rounds:4, /* max allowed value: 15 */
			bw_probe_base_us:26,	/* usecs: 0..2^26-1 (67 secs) */
			full_ecn_cnt:2;		/* max allowed value: 3 */
		u32	bw_probe_rand_us:26,	/* usecs: 0..2^26-1 (67 secs) */
			undo:1,			/* boolean */
			tso_rtt_shift:4,	/* max allowed value: 15 */
			unused5:1;
		u32	ecn_reprobe_gain:9,	/* max allowed value: 511 */
			unused1:14,
			ecn_alpha_init:9;	/* max allowed value: 256 */
	} params;

	struct {
		u32	snd_isn; /* Initial sequence number */
		u32	rs_bw; 	 /* last valid rate sample bw */
		u32	target_cwnd; /* target cwnd, based on BDP */
		u8	undo:1,  /* Undo even happened but not yet logged */
			unused:7;
		char	event;	 /* single-letter event debug codes */
		u16	unused2;
	} debug;
};

struct bbr_context {
	u32 sample_bw;
	u32 target_cwnd;
	u32 log:1;
};

/* Window length of bw filter (in rounds). Max allowed value is 31 (0x1F) */
static int bbr_bw_rtts = CYCLE_LEN + 2;
/* Window length of min_rtt filter (in sec). Max allowed value is 31 (0x1F) */
static u32 bbr_min_rtt_win_sec = 10;
/* Minimum time (in ms) spent at bbr_cwnd_min_target in BBR_PROBE_RTT mode.
 * Max allowed value is 511 (0x1FF).
 */
static u32 bbr_probe_rtt_mode_ms = 200;
/* Window length of probe_rtt_min_us filter (in ms), and consequently the
 * typical interval between PROBE_RTT mode entries.
 * Note that bbr_probe_rtt_win_ms must be <= bbr_min_rtt_win_sec * MSEC_PER_SEC
 */
static u32 bbr_probe_rtt_win_ms = 5000;
/* Skip TSO below the following bandwidth (bits/sec): */
static int bbr_min_tso_rate = 1200000;

/* Use min_rtt to help adapt TSO burst size, with smaller min_rtt resulting
 * in bigger TSO bursts. By default we cut the RTT-based allowance in half
 * for every 2^9 usec (aka 512 us) of RTT, so that the RTT-based allowance
 * is below 1500 bytes after 6 * ~500 usec = 3ms.
 */
static u32 bbr_tso_rtt_shift = 9;  /* halve allowance per 2^9 usecs, 512us */

/* Select cwnd TSO budget approach:
 *  0: padding
 *  1: flooring
 */
static uint bbr_cwnd_tso_budget = 1;

/* Pace at ~1% below estimated bw, on average, to reduce queue at bottleneck.
 * In order to help drive the network toward lower queues and low latency while
 * maintaining high utilization, the average pacing rate aims to be slightly
 * lower than the estimated bandwidth. This is an important aspect of the
 * design.
 */
static const int bbr_pacing_margin_percent = 1;

/* We use a high_gain value of 2/ln(2) because it's the smallest pacing gain
 * that will allow a smoothly increasing pacing rate that will double each RTT
 * and send the same number of packets per RTT that an un-paced, slow-starting
 * Reno or CUBIC flow would. Max allowed value is 2047 (0x7FF).
 */
static int bbr_high_gain  = BBR_UNIT * 2885 / 1000 + 1;
/* The gain for deriving startup cwnd. Max allowed value is 2047 (0x7FF). */
static int bbr_startup_cwnd_gain  = BBR_UNIT * 2885 / 1000 + 1;
/* The pacing gain of 1/high_gain in BBR_DRAIN is calculated to typically drain
 * the queue created in BBR_STARTUP in a single round. Max allowed value
 * is 1023 (0x3FF).
 */
static int bbr_drain_gain = BBR_UNIT * 1000 / 2885;
/* The gain for deriving steady-state cwnd tolerates delayed/stretched ACKs.
 * Max allowed value is 2047 (0x7FF).
 */
static int bbr_cwnd_gain  = BBR_UNIT * 2;
/* The pacing_gain values for the PROBE_BW gain cycle, to discover/share bw.
 * Max allowed value for each element is 1023 (0x3FF).
 */
enum bbr_pacing_gain_phase {
	BBR_BW_PROBE_UP		= 0,  /* push up inflight to probe for bw/vol */
	BBR_BW_PROBE_DOWN	= 1,  /* drain excess inflight from the queue */
	BBR_BW_PROBE_CRUISE	= 2,  /* use pipe, w/ headroom in queue/pipe */
	BBR_BW_PROBE_REFILL	= 3,  /* v2: refill the pipe again to 100% */
};
static int bbr_pacing_gain[] = {
	BBR_UNIT * 5 / 4,	/* probe for more available bw */
	BBR_UNIT * 3 / 4,	/* drain queue and/or yield bw to other flows */
	BBR_UNIT, BBR_UNIT, BBR_UNIT,	/* cruise at 1.0*bw to utilize pipe, */
	BBR_UNIT, BBR_UNIT, BBR_UNIT	/* without creating excess queue... */
};
/* Randomize the starting gain cycling phase over N phases: */
static u32 bbr_cycle_rand = 7;

/* Try to keep at least this many packets in flight, if things go smoothly. For
 * smooth functioning, a sliding window protocol ACKing every other packet
 * needs at least 4 packets in flight. Max allowed value is 15 (0xF).
 */
static u32 bbr_cwnd_min_target = 4;

/* Cwnd to BDP proportion in PROBE_RTT mode scaled by BBR_UNIT. Default: 50%.
 * Use 0 to disable. Max allowed value is 255.
 */
static u32 bbr_probe_rtt_cwnd_gain = BBR_UNIT * 1 / 2;

/* To estimate if BBR_STARTUP mode (i.e. high_gain) has filled pipe... */
/* If bw has increased significantly (1.25x), there may be more bw available.
 * Max allowed value is 1023 (0x3FF).
 */
static u32 bbr_full_bw_thresh = BBR_UNIT * 5 / 4;
/* But after 3 rounds w/o significant bw growth, estimate pipe is full.
 * Max allowed value is 7 (0x7).
 */
static u32 bbr_full_bw_cnt = 3;

static u32 bbr_flags;		/* Debugging related stuff */

/* Whether to debug using printk.
 */
static bool bbr_debug_with_printk;

/* Whether to debug using ftrace event tcp:tcp_bbr_event.
 * Ignored when bbr_debug_with_printk is set.
 */
static bool bbr_debug_ftrace;

/* Experiment: each cycle, try to hold sub-unity gain until inflight <= BDP. */
static bool bbr_drain_to_target = true;		/* default: enabled */

/* Experiment: Flags to control BBR with ECN behavior.
 */
static bool bbr_precise_ece_ack = true;		/* default: enabled */

/* The max rwin scaling shift factor is 14 (RFC 1323), so the max sane rwin is
 * (2^(16+14) B)/(1024 B/packet) = 1M packets.
 */
static u32 bbr_cwnd_warn_val	= 1U << 20;

static u16 bbr_debug_port_mask;

/* BBR module parameters. These are module parameters only in Google prod.
 * Upstream these are intentionally not module parameters.
 */
static int bbr_pacing_gain_size = CYCLE_LEN;

/* Gain factor for adding extra_acked to target cwnd: */
static int bbr_extra_acked_gain = 256;

/* Window length of extra_acked window. Max allowed val is 31. */
static u32 bbr_extra_acked_win_rtts = 5;

/* Max allowed val for ack_epoch_acked, after which sampling epoch is reset */
static u32 bbr_ack_epoch_acked_reset_thresh = 1U << 20;

/* Time period for clamping cwnd increment due to ack aggregation */
static u32 bbr_extra_acked_max_us = 100 * 1000;

/* Use extra acked in startup ?
 * 0: disabled
 * 1: use latest extra_acked value from 1-2 rtt in startup
 */
static int bbr_extra_acked_in_startup = 1;		/* default: enabled */

/* Experiment: don't grow cwnd beyond twice of what we just probed. */
static bool bbr_usage_based_cwnd;		/* default: disabled */

/* For lab testing, researchers can enable BBRv2 ECN support with this flag,
 * when they know that any ECN marks that the connections experience will be
 * DCTCP/L4S-style ECN marks, rather than RFC3168 ECN marks.
 * TODO(ncardwell): Production use of the BBRv2 ECN functionality depends on
 * negotiation or configuration that is outside the scope of the BBRv2
 * alpha release.
 */
static bool bbr_ecn_enable = false;






/* Module parameters that are settable by TCP_CONGESTION_PARAMS are declared
 * down here, so that the algorithm functions that use the parameters must use
 * the per-socket parameters; if they accidentally use the global version
 * then there will be a compile error.
 * TODO(ncardwell): move all per-socket parameters down to this section.
 */

/* On losses, scale down inflight and pacing rate by beta scaled by BBR_SCALE.
 * No loss response when 0. Max allwed value is 255.
 */
static u32 bbr_beta = BBR_UNIT * 30 / 100;

/* Gain factor for ECN mark ratio samples, scaled by BBR_SCALE.
 * Max allowed value is 255.
 */
static u32 bbr_ecn_alpha_gain = BBR_UNIT * 1 / 16;  /* 1/16 = 6.25% */

/* The initial value for the ecn_alpha state variable. Default and max
 * BBR_UNIT (256), representing 1.0. This allows a flow to respond quickly
 * to congestion if the bottleneck is congested when the flow starts up.
 */
static u32 bbr_ecn_alpha_init = BBR_UNIT;	/* 1.0, to respond quickly */

/* On ECN, cut inflight_lo to (1 - ecn_factor * ecn_alpha) scaled by BBR_SCALE.
 * No ECN based bounding when 0. Max allwed value is 255.
 */
static u32 bbr_ecn_factor = BBR_UNIT * 1 / 3;	    /* 1/3 = 33% */

/* Estimate bw probing has gone too far if CE ratio exceeds this threshold.
 * Scaled by BBR_SCALE. Disabled when 0. Max allowed is 255.
 */
static u32 bbr_ecn_thresh = BBR_UNIT * 1 / 2;  /* 1/2 = 50% */

/* Max RTT (in usec) at which to use sender-side ECN logic.
 * Disabled when 0 (ECN allowed at any RTT).
 * Max allowed for the parameter is 524287 (0x7ffff) us, ~524 ms.
 */
static u32 bbr_ecn_max_rtt_us = 5000;

/* If non-zero, if in a cycle with no losses but some ECN marks, after ECN
 * clears then use a multiplicative increase to quickly reprobe bw by
 * starting inflight probing at the given multiple of inflight_hi.
 * Default for this experimental knob is 0 (disabled).
 * Planned value for experiments: BBR_UNIT * 1 / 2 = 128, representing 0.5.
 */
static u32 bbr_ecn_reprobe_gain;

/* Estimate bw probing has gone too far if loss rate exceeds this level. */
static u32 bbr_loss_thresh = BBR_UNIT * 2 / 100;  /* 2% loss */

/* Exit STARTUP if number of loss marking events in a Recovery round is >= N,
 * and loss rate is higher than bbr_loss_thresh.
 * Disabled if 0. Max allowed value is 15 (0xF).
 */
static u32 bbr_full_loss_cnt = 8;

/* Exit STARTUP if number of round trips with ECN mark rate above ecn_thresh
 * meets this count. Max allowed value is 3.
 */
static u32 bbr_full_ecn_cnt = 2;

/* Fraction of unutilized headroom to try to leave in path upon high loss. */
static u32 bbr_inflight_headroom = BBR_UNIT * 15 / 100;

/* Multiplier to get target inflight (as multiple of BDP) for PROBE_UP phase.
 * Default is 1.25x, as in BBR v1. Max allowed is 511.
 */
static u32 bbr_bw_probe_pif_gain = BBR_UNIT * 5 / 4;

/* Multiplier to get Reno-style probe epoch duration as: k * BDP round trips.
 * If zero, disables this BBR v2 Reno-style BDP-scaled coexistence mechanism.
 * Max allowed is 511.
 */
static u32 bbr_bw_probe_reno_gain = BBR_UNIT;

/* Max number of packet-timed rounds to wait before probing for bandwidth.  If
 * we want to tolerate 1% random loss per round, and not have this cut our
 * inflight too much, we must probe for bw periodically on roughly this scale.
 * If low, limits Reno/CUBIC coexistence; if high, limits loss tolerance.
 * We aim to be fair with Reno/CUBIC up to a BDP of at least:
 *  BDP = 25Mbps * .030sec /(1514bytes) = 61.9 packets
 */
static u32 bbr_bw_probe_max_rounds = 63;

/* Max amount of randomness to inject in round counting for Reno-coexistence.
 * Max value is 15.
 */
static u32 bbr_bw_probe_rand_rounds = 2;

/* Use BBR-native probe time scale starting at this many usec.
 * We aim to be fair with Reno/CUBIC up to an inter-loss time epoch of at least:
 *  BDP*RTT = 25Mbps * .030sec /(1514bytes) * 0.030sec = 1.9 secs
 */
static u32 bbr_bw_probe_base_us = 2 * USEC_PER_SEC;  /* 2 secs */

/* Use BBR-native probes spread over this many usec: */
static u32 bbr_bw_probe_rand_us = 1 * USEC_PER_SEC;  /* 1 secs */

/* Undo the model changes made in loss recovery if recovery was spurious? */
static bool bbr_undo = true;

/* Use fast path if app-limited, no loss/ECN, and target cwnd was reached? */
static bool bbr_fast_path = true;	/* default: enabled */

/* Use fast ack mode ? */
static int bbr_fast_ack_mode = 1;	/* default: rwnd check off */

/* How much to additively increase inflight_hi when entering REFILL? */
static u32 bbr_refill_add_inc;		/* default: disabled */
static void bbr2_init(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct bbr *bbr = inet_csk_ca(sk);

	//bbr_init(sk);	/* run shared init code for v1 and v2 */

	/* BBR v2 parameters: */
	bbr->params.beta = min_t(u32, 0xFFU, bbr_beta);
	bbr->params.ecn_alpha_gain = min_t(u32, 0xFFU, bbr_ecn_alpha_gain);
	bbr->params.ecn_alpha_init = min_t(u32, BBR_UNIT, bbr_ecn_alpha_init);
	bbr->params.ecn_factor = min_t(u32, 0xFFU, bbr_ecn_factor);
	bbr->params.ecn_thresh = min_t(u32, 0xFFU, bbr_ecn_thresh);
	bbr->params.ecn_max_rtt_us = min_t(u32, 0x7ffffU, bbr_ecn_max_rtt_us);
	bbr->params.ecn_reprobe_gain = min_t(u32, 0x1FF, bbr_ecn_reprobe_gain);
	bbr->params.loss_thresh = min_t(u32, 0xFFU, bbr_loss_thresh);
	bbr->params.full_loss_cnt = min_t(u32, 0xFU, bbr_full_loss_cnt);
	bbr->params.full_ecn_cnt = min_t(u32, 0x3U, bbr_full_ecn_cnt);
	bbr->params.inflight_headroom =
		min_t(u32, 0xFFU, bbr_inflight_headroom);
	bbr->params.bw_probe_pif_gain =
		min_t(u32, 0x1FFU, bbr_bw_probe_pif_gain);
	bbr->params.bw_probe_reno_gain =
		min_t(u32, 0x1FFU, bbr_bw_probe_reno_gain);
	bbr->params.bw_probe_max_rounds =
		min_t(u32, 0xFFU, bbr_bw_probe_max_rounds);
	bbr->params.bw_probe_rand_rounds =
		min_t(u32, 0xFU, bbr_bw_probe_rand_rounds);
	bbr->params.bw_probe_base_us =
		min_t(u32, (1 << 26) - 1, bbr_bw_probe_base_us);
	bbr->params.bw_probe_rand_us =
		min_t(u32, (1 << 26) - 1, bbr_bw_probe_rand_us);
	bbr->params.undo = bbr_undo;
	bbr->params.fast_path = bbr_fast_path ? 1 : 0;
	bbr->params.refill_add_inc = min_t(u32, 0x3U, bbr_refill_add_inc);

	/* BBR v2 state: */
	bbr->initialized = 1;
	
	bbr->loss_round_delivered = tp->delivered + 1;
	bbr->loss_round_start = 0;
	bbr->undo_bw_lo = 0;
	bbr->undo_inflight_lo = 0;
	bbr->undo_inflight_hi = 0;
	bbr->loss_events_in_round = 0;
	bbr->startup_ecn_rounds = 0;
	//bbr2_reset_congestion_signals(sk);
	bbr->bw_lo = ~0U;
	bbr->bw_hi[0] = 0;
	bbr->bw_hi[1] = 0;
	bbr->inflight_lo = ~0U;
	bbr->inflight_hi = ~0U;
	bbr->bw_probe_up_cnt = ~0U;
	bbr->bw_probe_up_acks = 0;
	bbr->bw_probe_up_rounds = 0;
	bbr->probe_wait_us = 0;
	bbr->stopped_risky_probe = 0;
	bbr->ack_phase = BBR_ACKS_INIT;
	bbr->rounds_since_probe = 0;
	bbr->bw_probe_samples = 0;
	bbr->prev_probe_too_high = 0;
	bbr->ecn_eligible = 0;
	bbr->ecn_alpha = bbr->params.ecn_alpha_init;
	bbr->alpha_last_delivered = 0;
	bbr->alpha_last_delivered_ce = 0;

	tp->fast_ack_mode = min_t(u32, 0x2U, bbr_fast_ack_mode);
	WARN_ONCE(1,"init bbrv2  %d\n",sizeof(struct bbr)/sizeof(u64));\
}
void bbr2_main(struct sock *sk, const struct rate_sample *rs){
    
}
/* Entering loss recovery, so save state for when we undo recovery. */
static u32 bbr2_ssthresh(struct sock *sk)
{
	return tcp_sk(sk)->snd_ssthresh;
}
static u32 bbr2_undo_cwnd(struct sock *sk){
    
}
static struct tcp_congestion_ops tcp_bbr2_cong_ops __read_mostly = {
	.flags		= TCP_CONG_NON_RESTRICTED | TCP_CONG_WANTS_CE_EVENTS,
	.name		= "bbr2",
	.owner		= THIS_MODULE,
	.init		= bbr2_init,
    .cong_control	= bbr2_main,
    .undo_cwnd	= bbr2_undo_cwnd,
    .ssthresh	= bbr2_ssthresh,
};

static int __init bbr_register(void)
{
	BUILD_BUG_ON(sizeof(struct bbr) > ICSK_CA_PRIV_SIZE);
	return tcp_register_congestion_control(&tcp_bbr2_cong_ops);
}

static void __exit bbr_unregister(void)
{
	tcp_unregister_congestion_control(&tcp_bbr2_cong_ops);
}

module_init(bbr_register);
module_exit(bbr_unregister);

MODULE_AUTHOR("Van Jacobson <vanj@google.com>");
MODULE_AUTHOR("Neal Cardwell <ncardwell@google.com>");
MODULE_AUTHOR("Yuchung Cheng <ycheng@google.com>");
MODULE_AUTHOR("Soheil Hassas Yeganeh <soheil@google.com>");
MODULE_AUTHOR("Priyaranjan Jha <priyarjha@google.com>");
MODULE_AUTHOR("Yousuk Seung <ysseung@google.com>");
MODULE_AUTHOR("Kevin Yang <yyd@google.com>");
MODULE_AUTHOR("Arjun Roy <arjunroy@google.com>");

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("TCP BBR (Bottleneck Bandwidth and RTT)");

#undef NS_PROTOCOL
