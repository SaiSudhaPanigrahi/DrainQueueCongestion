#include <iostream>
#include <utility>
#include <vector>
#include <string.h>
#include <stdint.h>
#include <algorithm>
#include "logging.h"
#include "send_packet_manager.h"
#include "fun_test.h"
#include "proto_types.h"
#include "interval.h"
#include "flag_impl.h"
#include "flag_util_impl.h"
using namespace dqc;
template<int N>
class MedianFilter{
public:
    MedianFilter(){}
    ~MedianFilter(){}
    void NewValue(double &sample){
        samples_[offset_]=sample;
        if(!full_rank_){
            if(offset_==(N-1)){
                full_rank_=true;
            }
        }
        offset_=(offset_+1)%N;
    }
    double Average(){
        double average=0.0;
        double sum=0.0;
        if(!full_rank_){
            for(int i=0;i<offset_;i++){
                sum+=samples_[i];
            }
            if(offset_==0){
                average=0;
            }else{
                average=sum/(offset_);
            }

        }else{
            for(int i=0;i<N;i++){
                sum+=samples_[i];
            }
            average=sum/(N);
        }
        return average;
    }
private:
    double samples_[N];
    int offset_{0};
    bool full_rank_{false};
};
int main(){
    su_platform_init();
    //SendPacketManager manager(nullptr);
    //manager.Test();
    //manager.Test2();
    //test_test();
    su_platform_uninit();
    //QuicBandwidth bw(QuicBandwidth::FromBitsPerSecond(1000));
    //TimeDelta wait=bw.TransferTime(1000);
    //std::cout<<std::to_string(wait.ToMilliseconds());
    /*IntervalSet<int> stream;
    stream.Add(0,100);
    stream.Add(200,500);
    if(stream.IsDisjoint(100,200)){
        stream.Add(100,200);
        DLOG(INFO)<<stream.Size();
    }
    MedianFilter<5> filter;
    double sample=5.0;
    for(int i=0;i<10;i++){
        filter.NewValue(sample);
        std::cout<<filter.Average()<<std::endl;
    }*/
}


