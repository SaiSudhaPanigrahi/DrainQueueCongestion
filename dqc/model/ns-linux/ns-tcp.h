#pragma once
#include <stdlib.h>
#include "ns-linux-types.h"
#include "ns-win-minmax.h"
#ifdef __cplusplus
extern "C" {
#endif
#define HZ 1000
#define JIFFY_RATIO 1000
#define US_RATIO 1000000
#define MS_RATIO 1000

#define jiffies_to_usecs(x) ((US_RATIO/JIFFY_RATIO)*(x))
#define msecs_to_jiffies(x) ((JIFFY_RATIO/MS_RATIO)*(x))


#define jiffies tcp_time_stamp

#define tcp_jiffies32 ((u32)jiffies)

/* Events passed to congestion control interface */
enum tcp_ca_event {
	CA_EVENT_TX_START,	/* first transmit when no packets in flight */
	CA_EVENT_CWND_RESTART,	/* congestion window restart */
	CA_EVENT_COMPLETE_CWR,	/* end of congestion recovery */
	CA_EVENT_LOSS,		/* loss timeout */
	CA_EVENT_ECN_NO_CE,	/* ECT set, but not CE marked */
	CA_EVENT_ECN_IS_CE,	/* received CE marked IP packet */
};

#define sock tcp_sock
#define inet_sock tcp_sock
#define inet_connection_sock tcp_sock
struct sk_buff;
struct sock;
/* Phase as reported in netlink/ss stats. */
enum tcp_bbr2_phase {
	BBR2_PHASE_INVALID		= 0,
	BBR2_PHASE_STARTUP		= 1,
	BBR2_PHASE_DRAIN		= 2,
	BBR2_PHASE_PROBE_RTT		= 3,
	BBR2_PHASE_PROBE_BW_UP		= 4,
	BBR2_PHASE_PROBE_BW_DOWN	= 5,
	BBR2_PHASE_PROBE_BW_CRUISE	= 6,
	BBR2_PHASE_PROBE_BW_REFILL	= 7
};

struct tcp_bbr2_info {
	/* u64 bw: bandwidth (app throughput) estimate in Byte per sec: */
	__u32	bbr_bw_lsb;		/* lower 32 bits of bw */
	__u32	bbr_bw_msb;		/* upper 32 bits of bw */
	__u32	bbr_min_rtt;		/* min-filtered RTT in uSec */
	__u32	bbr_pacing_gain;	/* pacing gain shifted left 8 bits */
	__u32	bbr_cwnd_gain;		/* cwnd gain shifted left 8 bits */
	__u32	bbr_bw_hi_lsb;		/* lower 32 bits of bw_hi */
	__u32	bbr_bw_hi_msb;		/* upper 32 bits of bw_hi */
	__u32	bbr_bw_lo_lsb;		/* lower 32 bits of bw_lo */
	__u32	bbr_bw_lo_msb;		/* upper 32 bits of bw_lo */
	__u8	bbr_mode;		/* current bbr_mode in state machine */
	__u8	bbr_phase;		/* current state machine phase */
	__u8	unused1;		/* alignment padding; not used yet */
	__u8	bbr_version;		/* MUST be at this offset in struct */
	__u32	bbr_inflight_lo;	/* lower/short-term data volume bound */
	__u32	bbr_inflight_hi;	/* higher/long-term data volume bound */
	__u32	bbr_extra_acked;	/* max excess packets ACKed in epoch */
};
struct tcp_bbr_info {
	/* u64 bw: max-filtered BW (app throughput) estimate in Byte per sec: */
	__u32	bbr_bw_lo;		/* lower 32 bits of bw */
	__u32	bbr_bw_hi;		/* upper 32 bits of bw */
	__u32	bbr_min_rtt;		/* min-filtered RTT in uSec */
	__u32	bbr_pacing_gain;	/* pacing gain shifted left 8 bits */
	__u32	bbr_cwnd_gain;		/* cwnd gain shifted left 8 bits */
};

union tcp_cc_info {
	struct tcp_bbr_info	bbr;
    struct tcp_bbr2_info	bbr2;
};
struct ack_sample {
	u32 pkts_acked;
	s32 rtt_us;
	u32 in_flight;
};
struct rate_sample {
	//u64  prior_mstamp; /* starting timestamp for interval */
	u32  prior_delivered;	/* tp->delivered at "prior_mstamp" */
	s32  delivered;		/* number of packets delivered over interval */
	long interval_us;	/* time for tp->delivered to incr "delivered" */
	u32 snd_interval_us;	/* snd interval for delivered packets */
	u32 rcv_interval_us;	/* rcv interval for delivered packets */
	long rtt_us;		/* RTT of last (S)ACKed packet (or -1) */
	int  losses;		/* number of packets marked lost upon ACK */
	u32  acked_sacked;	/* number of packets newly (S)ACKed upon ACK */
	u32  prior_in_flight;	/* in flight before this ACK */
	bool is_app_limited;	/* is sample from packet with bubble in pipe? */
	bool is_retrans;	/* is sample from retransmission? */
	bool is_ack_delayed;	/* is this (likely) a delayed ACK? */
};
struct cc_param_list {
	const char* name;
	const char* type;
	const char* description;
	const void* ptr;
	struct cc_param_list* next;
};

struct cc_list {
	const char* proto;
	struct cc_param_list* param_head;
	struct cc_list* next;
};
/* List Type purely for tcp_cong_list*/
struct list_head {
	struct list_head *prev;
	struct list_head *next;
	const char* file_name;
};
/*
 * Interface for adding new TCP congestion control handlers
 */
#define TCP_CA_NAME_MAX	16
#define TCP_CA_MAX	128
#define TCP_CA_BUF_MAX	(TCP_CA_NAME_MAX*TCP_CA_MAX)
#define TCP_CONG_NON_RESTRICTED 0x1
#define TCP_CONG_RTT_STAMP      0x2

/* Algorithm can be set on socket without CAP_NET_ADMIN privileges */
#define TCP_CONG_NON_RESTRICTED 0x1
/* Requires ECN/ECT set on all packets */
#define TCP_CONG_NEEDS_ECN	0x2
/* Wants notification of CE events (CA_EVENT_ECN_IS_CE, CA_EVENT_ECN_NO_CE). */
#define TCP_CONG_WANTS_CE_EVENTS	0x100000

struct tcp_congestion_ops {
	struct list_head	list;
	u32 key;
	u32 flags;

	/* initialize private data (optional) */
	void (*init)(struct sock *sk);
	/* cleanup private data  (optional) */
	void (*release)(struct sock *sk);

	/* return slow start threshold (required) */
	u32 (*ssthresh)(struct sock *sk);
	/* do new cwnd calculation (required) */
	void (*cong_avoid)(struct sock *sk, u32 ack, u32 acked);
	/* call before changing ca_state (optional) */
	void (*set_state)(struct sock *sk, u8 new_state);
	/* call when cwnd event occurs (optional) */
	void (*cwnd_event)(struct sock *sk, enum tcp_ca_event ev);
	/* call when ack arrives (optional) */
	void (*in_ack_event)(struct sock *sk, u32 flags);
	/* new value of cwnd after loss (required) */
	u32  (*undo_cwnd)(struct sock *sk);
	/* hook for packet ack accounting (optional) */
	void (*pkts_acked)(struct sock *sk, const struct ack_sample *sample);
	/* override sysctl_tcp_min_tso_segs */
	u32 (*min_tso_segs)(struct sock *sk);
	/* returns the multiplier used in tcp_sndbuf_expand (optional) */
	u32 (*sndbuf_expand)(struct sock *sk);
	/* call when packets are delivered to update cwnd and pacing rate,
	 * after all the ca_state processing. (optional)
	 */
	void (*cong_control)(struct sock *sk, const struct rate_sample *rs);
    //trace bandwidth (optional);
    unsigned long (*bw_est)(void *sk);
	/* get info for inet_diag (optional) */
	size_t (*get_info)(struct sock *sk, u32 ext, int *attr,
			   union tcp_cc_info *info);

	char 		name[TCP_CA_NAME_MAX];
	struct module 	*owner;
};

struct tcp_options_received {
/*	PAWS/RTTM data	*/
//	long	ts_recent_stamp;/* Time we stored ts_recent (for aging) */
//	__u32	ts_recent;	/* Time stamp to echo next		*/
	u32	rcv_tsval;	/* Time stamp value             	*/
	u32	rcv_tsecr;	/* Time stamp echo reply        	*/
	u16 	saw_tstamp : 1,	/* Saw TIMESTAMP on last packet		*/
		dump_xxx: 15;
//		tstamp_ok : 1,	/* TIMESTAMP seen on SYN packet		*/
//		dsack : 1,	/* D-SACK is scheduled			*/
//		wscale_ok : 1,	/* Wscale seen on SYN packet		*/
//		sack_ok : 4,	/* SACK seen on SYN packet		*/
//		snd_wscale : 4,	/* Window scaling received from sender	*/
//		rcv_wscale : 4;	/* Window scaling to send to receiver	*/
/*	SACKs data	*/
//	__u8	eff_sacks;	/* Size of SACK array to send with next packet */
//	__u8	num_sacks;	/* Number of SACK blocks		*/
///	__u16	user_mss;  	/* mss requested by user in ioctl */
//	__u16	mss_clamp;	/* Maximal mss, negotiated at connection setup */
};

struct tcp_sock {
/* inet_connection_sock has to be the first member of tcp_sock */
//	struct inet_connection_sock	inet_conn;
	int	sk_state;
// 	int	tcp_header_len;	/* Bytes of tcp header to send		*/

/*
 *	Header prediction flags
 *	0x5?10 << 16 + snd_wnd in net byte order
 */
//	__u32	pred_flags;

/*
 *	RFC793 variables by their proper names. This means you can
 *	read the code and the spec side by side (and laugh ...)
 *	See RFC793 and RFC1122. The RFC writes these in capitals.
 */
// 	__u32	rcv_nxt;	/* What we want to receive next 	*/
 //	__u32	snd_nxt;	/* Next sequence we send		*/

 	//__u32	snd_una;	/* First byte we want an ack for	*/
// 	__u32	snd_sml;	/* Last byte of the most recently transmitted small packet */
//	__u32	rcv_tstamp;	/* timestamp of last received ACK (for keepalives) */
//	__u32	lsndtime;	/* timestamp of last sent data packet (for restart window) */

	/* Data for direct copy to user */
//	struct {
//		struct sk_buff_head	prequeue;
//		struct task_struct	*task;
//		struct iovec		*iov;
//		int			memory;
//		int			len;
//	} ucopy;

//	__u32	snd_wl1;	/* Sequence for window update		*/
//	__u32	snd_wnd;	/* The window we expect to receive	*/
//	__u32	max_window;	/* Maximal window ever seen from peer	*/
//	__u32	pmtu_cookie;	/* Last pmtu seen by socket		*/
	
//	__u16	xmit_size_goal;	/* Goal for segmenting output packets	*/
//	__u16	ext_header_len;	/* Network protocol overhead (IP/IPv6 options) */
//
//	__u32	window_clamp;	/* Maximal window to advertise		*/
//	__u32	rcv_ssthresh;	/* Current window clamp			*/
//
//	__u32	frto_highmark;	/* snd_nxt when RTO occurred */
//	__u8	reordering;	/* Packet reordering metric.		*/
//	__u8	frto_counter;	/* Number of new acks after RTO */
//	__u8	nonagle;	/* Disable Nagle algorithm?             */
//	__u8	keepalive_probes; /* num of allowed keep alive probes	*/
    u32	mss_cache;	/* Cached effective mss, not including SACKS */
    u64	tcp_wstamp_ns;	/* departure time for next sent data packet */
	u64	tcp_clock_cache; /* cache last tcp_clock_ns() (see tcp_mstamp_refresh()) */
/* RTT measurement */
	u64	tcp_mstamp;	/* most recent packet received/sent micrcsecond*/  
    
    
	u32	srtt_us;	/* smoothed round trip time << 3 in usecs */
	//u32	mdev_us;	/* medium deviation			*/
	//u32	mdev_max_us;	/* maximal mdev for the last rtt period	*/
	//u32	rttvar_us;	/* smoothed mdev_max			*/
	//u32	rtt_seq;	/* sequence number to update rttvar	*/
    struct  minmax rtt_min;
    
	u32	packets_out;	/* Packets which are "in flight"	*/
	u32	retrans_out;	/* Retransmitted packets out		*/
	u32	max_packets_out;  /* max packets_out in last window */
	//u32	max_packets_seq;  /* right edge of max_packets_out flight */
/*
 *      Options received (usually on last packet, some only on SYN packets).
 */
	//struct tcp_options_received rx_opt;

/*
 *	Slow start and congestion control (see also Nagle, and Karn & Partridge)
 */
 	u32	snd_ssthresh;	/* Slow start size threshold		*/
	u32	snd_cwnd;	/* Sending congestion window		*/
 	u32	snd_cwnd_cnt;	/* Linear increase counter		*/
	u32	snd_cwnd_clamp; /* Do not allow snd_cwnd to grow above this */
    //u32	snd_cwnd_used;
	//u32	snd_cwnd_stamp;
    u32	prior_cwnd;	/* cwnd right before starting loss recovery */
	u32	bytes_acked;
//
//	struct sk_buff_head	out_of_order_queue; /* Out of order segments go here */
//
//	struct tcp_func		*af_specific;	/* Operations which are AF_INET{4,6} specific	*/
//
 //	__u32	rcv_wnd;	/* Current receiver window		*/
//	__u32	rcv_wup;	/* rcv_nxt on last window update sent	*/
//	__u32	write_seq;	/* Tail(+1) of data held in tcp send buffer */
//	__u32	pushed_seq;	/* Last pushed seq, required to talk to windows */
//	__u32	copied_seq;	/* Head of yet unread data		*/
//
/*	SACKs data	*/
//	struct tcp_sack_block duplicate_sack[1]; /* D-SACK block */
//	struct tcp_sack_block selective_acks[4]; /* The SACKS themselves*/

//	__u16	advmss;		/* Advertised MSS			*/
//	__u16	prior_ssthresh; /* ssthresh saved at recovery start	*/
//	__u32	lost_out;	/* Lost packets			*/
//	__u32	sacked_out;	/* SACK'd packets			*/
//	__u32	fackets_out;	/* FACK'd packets			*/
//	__u32	high_seq;	/* snd_nxt at onset of congestion	*/
//
//	__u32	retrans_stamp;	/* Timestamp of the last retransmit,
//				 * also used in SYN-SENT to remember stamp of
//				 * the first SYN. */
//	__u32	undo_marker;	/* tracking retrans started here. */
//	int	undo_retrans;	/* number of undoable retransmissions. */
//	__u32	urg_seq;	/* Seq of received urgent pointer */
//	__u16	urg_data;	/* Saved octet of OOB data and control flags */
//	__u8	urg_mode;	/* In urgent mode		*/
//	__u8	ecn_flags;	/* ECN status bits.			*/
//	__u32	snd_up;		/* Urgent pointer		*/
//
//	__u32	total_retrans;	/* Total retransmits for entire connection */
//
//	unsigned int		keepalive_time;	  /* time before keep alive takes place */
//	unsigned int		keepalive_intvl;  /* time interval between keep alive probes */
//	int			linger2;
//
//	unsigned long last_synq_overflow; 
//
/* Receiver side RTT estimation */
//	struct {
//		__u32	rtt;
//		__u32	seq;
//		__u32	time;
//	} rcv_rtt_est;
	unsigned long		sk_pacing_rate; /* bytes per second */
	unsigned long		sk_max_pacing_rate;
    u8 is_cwnd_limited;
    u8	fast_ack_mode:2, /* which fast ack mode ? */
    unused:6;
    
    
	u32	delivered;	/* Total data packets delivered incl. rexmits */
	u32	delivered_ce;	/* Like the above but only ECE marked packets */

	u32	lost;		/* Total data packets lost incl. rexmits */
	u32	app_limited;	/* limited until "delivered" reaches this val */
//	u64	first_tx_mstamp;  /* start of window send phase */
	u64	delivered_mstamp; /* time we reached "delivered" */
	//u32	rate_delivered;    /* saved rate sample: packets delivered */
	//u32	rate_interval_us;  /* saved rate sample: time elapsed */

	u32			sk_pacing_status; /* see enum sk_pacing */
    u8  sk_pacing_shift;  //10
    u8  icsk_ca_state;
	struct tcp_congestion_ops *icsk_ca_ops;
	
    #define ICSK_CA_PRIV_SIZE	(360)    
	u64			  icsk_ca_priv[ICSK_CA_PRIV_SIZE / sizeof(u64)];

};

struct sk_buff {

};
enum {
	TCP_ESTABLISHED = 1,
	TCP_SYN_SENT,
	TCP_SYN_RECV,
	TCP_FIN_WAIT1,
	TCP_FIN_WAIT2,
	TCP_TIME_WAIT,
	TCP_CLOSE,
	TCP_CLOSE_WAIT,
	TCP_LAST_ACK,
	TCP_LISTEN,
	TCP_CLOSING,	/* Now a valid state */
	TCP_NEW_SYN_RECV,

	TCP_MAX_STATES	/* Leave at the end! */
};
enum tcp_ca_state
{
	TCP_CA_Open = 0,
#define TCPF_CA_Open	(1<<TCP_CA_Open)
	TCP_CA_Disorder = 1,
#define TCPF_CA_Disorder (1<<TCP_CA_Disorder)
	TCP_CA_CWR = 2,
#define TCPF_CA_CWR	(1<<TCP_CA_CWR)
	TCP_CA_Recovery = 3,
#define TCPF_CA_Recovery (1<<TCP_CA_Recovery)
	TCP_CA_Loss = 4
#define TCPF_CA_Loss	(1<<TCP_CA_Loss)
};

/* TCP initial congestion window as per rfc6928 */
#define TCP_INIT_CWND		10
#define TCP_INFINITE_SSTHRESH	0x7fffffff


enum sk_pacing {
	SK_PACING_NONE		= 0,
	SK_PACING_NEEDED	= 1,
	SK_PACING_FQ		= 2,
};
enum {
    INET_DIAG_NONE,
    INET_DIAG_VEGASINFO,
	INET_DIAG_BBRINFO,	/* request as INET_DIAG_VEGASINFO */
    INET_DIAG_MAX,
};
extern u32 tcp_time_stamp;  //in milliseconds
extern long long ktime_get_real;
//from kernel.h
#define min_t(type,x,y) \
	(((type)(x)) < ((type)(y)) ? ((type)(x)): ((type)(y)))

#define max_t(type,x,y) \
	(((type)(x)) > ((type)(y)) ? ((type)(x)): ((type)(y)))

#define inet_csk(sk) (sk)
#define tcp_sk(sk) (sk)
#define inet_csk_ca(sk) (void*)((sk)->icsk_ca_priv)
void tcp_update_rtt_min(struct sock *sk, u32 rtt_us);
/* Minimum RTT in usec. ~0 means not available. */
static inline u32 tcp_min_rtt(const struct tcp_sock *tp)
{
	return minmax_get(&tp->rtt_min);
}
static inline u32 tcp_stamp_us_delta(u64 t1, u64 t0)
{
	return max_t(s64, t1 - t0, 0);
}
static inline unsigned int tcp_packets_in_flight(const struct tcp_sock *tp){
    return tp->packets_out + tp->retrans_out;
}
u64 tcp_clock_ns(void);
void tcp_mstamp_refresh(struct tcp_sock *tp);



extern int fls(int x);
extern int fls64(__u64 x);
///// For 64 bit division from include/asm-generic/div64.h ////
#define do_div(n,base) ({                                      \
        uint32_t __base = (base);                               \
        uint32_t __rem;                                         \
        __rem = ((uint64_t)(n)) % __base;                       \
        (n) = ((uint64_t)(n)) / __base;                         \
        __rem;                                                  \
 })


static inline u64 div_u64_rem(u64 dividend, u32 divisor, u32 *remainder)
{
	union {
		u64 v64;
		u32 v32[2];
	} d = { dividend };
	u32 upper;

	upper = d.v32[1];
	d.v32[1] = 0;
	if (upper >= divisor) {
		d.v32[1] = upper / divisor;
		upper %= divisor;
	}
	asm ("divl %2" : "=a" (d.v32[0]), "=d" (*remainder) :
		"rm" (divisor), "0" (d.v32[0]), "1" (upper));
	return d.v64;
}
static inline s64 div_s64_rem(s64 dividend, s32 divisor, s32 *remainder){
	u64 quotient;

	if (dividend < 0) {
		quotient = div_u64_rem(-dividend, abs(divisor), (u32 *)remainder);
		*remainder = -*remainder;
		if (divisor > 0)
			quotient = -quotient;
	} else {
		quotient = div_u64_rem(dividend, abs(divisor), (u32 *)remainder);
		if (divisor < 0)
			quotient = -quotient;
	}
	return quotient;    
}
static inline s64 div_s64(s64 dividend, s32 divisor)
{
	s32 remainder;
	return div_s64_rem(dividend, divisor, &remainder);
}
static inline u64 div_u64(u64 dividend, u32 divisor)
{
	u32 remainder;
	return div_u64_rem(dividend, divisor, &remainder);
}

u64 div64_u64(u64 dividend, u64 divisor);
s64 div64_s64(s64 dividend, s64 divisor);

#define div64_long(x, y) div64_s64((x), (y))
#define div64_ul(x, y)   div64_u64((x), (y))



#ifdef __cplusplus
}
#endif
