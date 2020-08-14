/* 
 * TCP-Linux module for NS2 
 *
 * May 2006
 *
 * Author: Xiaoliang (David) Wei  (DavidWei@acm.org)
 *
 * NetLab, the California Institute of Technology 
 * http://netlab.caltech.edu
 *
 * Module: linux/ns-linux-util.h
 *      This is the header file for linkages between NS-2 source codes (in C++) and Linux source codes (in C)
 *
 * See a mini-tutorial about TCP-Linux at: http://netlab.caltech.edu/projects/ns2tcplinux/
 *
 */

#ifndef NS_LINUX_UTIL_H
#define NS_LINUX_UTIL_H
#include <stdlib.h>
#include "ns-tcp.h"
#include "ns-linux-param.h"
extern struct tcp_congestion_ops tcp_reno;
extern void tcp_cong_avoid_register(void);

#define ktime_t s64
extern ktime_t net_invalid_timestamp();
extern int ktime_equal(const ktime_t cmp1, const ktime_t cmp2);
extern s64 ktime_to_us(const ktime_t kt);
extern ktime_t net_timedelta(ktime_t t);
/* A list of parameters for different cc */
extern struct cc_list* cc_list_head;



extern unsigned char cc_list_changed;
extern struct list_head ns_tcp_cong_list;
extern struct list_head *last_added;
#define list_for_each_entry_rcu(pos, head, member) \
	for (pos=(struct tcp_congestion_ops*)ns_tcp_cong_list.next; pos!=(struct tcp_congestion_ops*)(&ns_tcp_cong_list); pos=(struct tcp_congestion_ops*)pos->member.next) 

#define list_add_rcu(a,b) {\
	(a)->next=ns_tcp_cong_list.next;\
	(a)->prev=(&ns_tcp_cong_list);\
	ns_tcp_cong_list.next->prev=(a);\
	ns_tcp_cong_list.next=(a);\
	last_added = (a);\
	cc_list_changed = 1;\
}

#define list_del_rcu(a) {(a)->prev->next=(a)->next; (a)->next->prev=(a)->prev; cc_list_changed = 1; }
#define list_move(a,b) {list_del_rcu(a); list_add_rcu(a,b);}
#define list_entry(a,b,c) ((b*)ns_tcp_cong_list.next)
#define list_add_tail_rcu(a,b) {\
	(a)->prev=ns_tcp_cong_list.prev;\
	(a)->next=(&ns_tcp_cong_list);\
	ns_tcp_cong_list.prev->next=(a);\
	ns_tcp_cong_list.prev=(a);\
	last_added = (a);\
	cc_list_changed = 1;\
}


extern struct tcp_congestion_ops tcp_init_congestion_ops;
#define FLAG_ECE 1
#define FLAG_DATA_SACKED 2
#define FLAG_DATA_ACKED 4
#define FLAG_DATA_LOST 8
#define FLAG_CA_ALERT           (FLAG_DATA_SACKED|FLAG_ECE)
#define FLAG_NOT_DUP		(FLAG_DATA_ACKED)

#define FLAG_UNSURE_TSTAMP 16

#define CONFIG_DEFAULT_TCP_CONG "reno"
#define READ_ONCE(x) (x)
#define WRITE_ONCE(x, val)						\
do {									\
    x=val;\
} while (0)


#define LL_MAX_HEADER 96
#define MAX_TCP_HEADER	(128 +LL_MAX_HEADER)
#define MAX_TCP_OPTION_SPACE 40
#define TCP_MIN_SND_MSS		48
#define TCP_MIN_GSO_SIZE	(TCP_MIN_SND_MSS - MAX_TCP_OPTION_SPACE)
#define GSO_MAX_SIZE		65536
#endif
