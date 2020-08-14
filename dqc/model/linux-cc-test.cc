//#include "ns-linux-c.h"
#include "linux-cc-test.h"
#include "tcp_bbr.h"
#include "tcp_congestion_manager.h"
#include "rtt_stats.h"
#include "proto_constants.h"
#include "ns3-func.h"
#include <memory.h>
#include <iostream>
#include <iostream>
using namespace dqc;
namespace ns3{
void init_sock(tcp_sock *tcp){
    memset(tcp,0,sizeof(tcp_sock));
    tcp->snd_cwnd=TCP_INIT_CWND;
    tcp->snd_cwnd_clamp=2000;
    tcp->mss_cache=kDefaultTCPMSS;
    tcp->icsk_ca_state=TCP_CA_Open;
    tcp->sk_max_pacing_rate=~0U;
}
void linux_congestion_test(){
    update_tcp_time();
    CongestionControlManager cong_ops_manager; 
    struct tcp_congestion_ops *e=nullptr;
    std::cout<<"num "<<cong_ops_manager.num()<<std::endl;
    e=cong_ops_manager.get_ops("bbr");
    tcp_sock linux;
    init_sock(&linux);
    dqc::RttStats rtt_stats;
    TimeDelta srtt=rtt_stats.SmoothedOrInitialRtt();
    linux.srtt_us=srtt.ToMicroseconds()<<3;
    tcp_update_rtt_min(&linux,rtt_stats.initial_rtt().ToMicroseconds());
    //tcp_time_stamp=1234;
    std::cout<<"jiffes "<<tcp_jiffies32<<std::endl;
    if(e){
    std::cout<<e->name<<std::endl;
    e->init(&linux);
    //struct sock *sk=tcp_sk(&linux);
    //struct bbr *bbr_con=inet_csk_ca(sk);
    std::cout<<"pacing rate "<<linux.sk_pacing_rate<<std::endl;
    std::cout<<linux.snd_cwnd<<std::endl;
    }
    if(e&&e->bw_est){
	uint32_t bw=e->bw_est(&linux);
	std::cout<<"bw es "<<bw<<std::endl;
    }
}    
}
