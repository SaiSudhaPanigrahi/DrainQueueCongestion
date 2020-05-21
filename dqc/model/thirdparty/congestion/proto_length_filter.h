#pragma once
#include <stdint.h>
#include <deque>
#include "logging.h"
namespace dqc{
// Compares two values and returns true if the first is less than or equal
// to the second.
typedef uint64_t TimeRoundType;
template <class T, class Compare>
class LengthFilter {
 public:
  // |window_length| is the period after which a best estimate expires.
  // |zero_value| is used as the uninitialized value for objects of T.
  // Importantly, |zero_value| should be an invalid value for a true sample.
  LengthFilter(size_t window_length,T zero_value)
      : window_length_(window_length),
        zero_value_(zero_value),
        first_index_(0),
        max_index_(0){
        samples_.emplace_back(Sample(zero_value_,0,false));
}
  // Updates best estimates with |sample|, and expires and updates best
  // estimates as necessary.
  void Update(T new_sample, TimeRoundType new_time) {
    size_t len=samples_.size();
    if(len>0&&(new_time<samples_.back().time)){
        return ;
    }
    if(new_time-samples_.back().time>window_length_){
        Reset(new_sample,new_time);
    }
    size_t i=samples_.size();
    while(first_index_+samples_.size()<=new_time){
        samples_.emplace_back(Sample(zero_value_,first_index_+i,false));
        //LOG(INFO)<<"add "<<first_index_+i;
        i++;
    }
    if(!samples_.back().is_useful){
        samples_.back().sample=new_sample;
        samples_.back().is_useful=true;
    }else{
        T old_sample=samples_.back().sample;
        if(Compare()(new_sample,old_sample)){
            samples_.back().sample=new_sample;
        }
    }
    DeleteOldSamples();
    T max_sample=samples_[max_index_-first_index_].sample;
    if(Compare()(new_sample,max_sample)){
        max_index_=samples_.back().time;
    }

  }

  // Resets all estimates to new sample.
  void Reset(T new_sample, TimeRoundType new_time) {
    samples_.clear();
    first_index_=new_time;
    max_index_=new_time;
    samples_.emplace_back(Sample(new_sample, new_time,true));
  }


  T GetBest() const { return samples_[max_index_-first_index_].sample; }
  void DeleteOldSamples(){
    size_t len=samples_.size();
    bool find_new_max=false;
    if(len>window_length_){
        size_t delete_eles=len-window_length_;
        for(size_t i=0;i<delete_eles;i++){
            TimeRoundType index=samples_.front().time;
            samples_.pop_front();
            if(!find_new_max&&(index==max_index_)){
                find_new_max=true;
            }
            index=samples_.front().time;
        }
        first_index_=samples_.front().time;
    }
    if(find_new_max){
        max_index_=samples_.front().time;
        T max_sample=samples_[max_index_-first_index_].sample;
        for(auto it=samples_.begin();it!=samples_.end();it++){
            if(!it->is_useful){continue;}
            if(Compare()(it->sample,max_sample)){
                max_sample=it->sample;
                max_index_=it->time;
            }
        }
    }
  }
 private:
  struct Sample {
    T sample;
    TimeRoundType time;
    bool is_useful;
    Sample(T init_sample, TimeRoundType init_time,bool validate)
        : sample(init_sample), time(init_time), is_useful(validate){}
  };

  size_t window_length_;  // Time length of window.
  T zero_value_;              // Uninitialized value of T.
  TimeRoundType first_index_;
  TimeRoundType max_index_;
  std::deque<Sample> samples_;
};
}
