#include "ns3-func.h"
#include "thirdparty/include/random.h"
#include "ns3/simulator.h"
#include "ns-tcp.h"
#include "private-const.h"
class RandomIntegerGenerator{
public:
    static RandomIntegerGenerator* Instance();
    int NextInt( int min, int max );
private:
    RandomIntegerGenerator(){
        random_.seedTime();
    }
    ~RandomIntegerGenerator(){}
    dqc::Random random_;
};
RandomIntegerGenerator* RandomIntegerGenerator::Instance(){
    static RandomIntegerGenerator *ins=new RandomIntegerGenerator();
    return ins;
}
int RandomIntegerGenerator::NextInt( int min, int max ){
    return random_.nextInt(min,max);
}
u32 prandom_u32_max(u32 ep_ro){
    return (u32)RandomIntegerGenerator::Instance()->NextInt(0,(int)ep_ro);
}
u32 tcp_time_stamp;
long long ktime_get_real;
void update_tcp_time(){
    tcp_time_stamp=(u32)ns3::Simulator::Now().GetMilliSeconds();
    ktime_get_real=(s64)ns3::Simulator::Now().GetNanoSeconds();
}
u64 tcp_clock_ns(void){
    return (u64)ns3::Simulator::Now().GetNanoSeconds();
}
void tcp_mstamp_refresh(struct tcp_sock *tp)
{
	u64 val = tcp_clock_ns();

	tp->tcp_clock_cache = val;
	tp->tcp_mstamp = div_u64(val, NSEC_PER_USEC);
}
