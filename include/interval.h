#ifndef INTERVAL_H_
#define INTERVAL_H_
#include "proto_types.h"
#include <iostream>
#include <utility>
#include <set>
template<typename T>
class Interval{
public:
typedef Interval<T> value_type;
Interval():min_(),max_(){}
Interval(const T &min,const T & max):min_(min),max_(max){}
void SetMin(const T &min){
    min_=min;
}
void SetMax(const T &max){
    max_=max;
}
bool Contains(const T &t) const{
    return ((min_<=t)&&(t<max_));
}
bool Contains(const Interval<T> &i) const{
    return Min()<=i.Min()&&Max()>=i.Max();
}
 const T&Max() const{
    return max_;
}
 const T&Min()const{
    return min_;
}
Interval<T>& operator=(const value_type &o){
    min_=o.Min();
    max_=o.Max();
    return (*this);
}
friend bool operator==(const value_type &a,const value_type &b){
    return (a.Min()==b.Min())&&(a.Max()==b.Max());
}
private:
    T min_;
    T max_;
};
template<class T>
class IntervalSet{
public:
    typedef Interval<T> value_type;
    IntervalSet(){}
    ~IntervalSet(){}
    void Clear(){
        intervals_.clear();
    }
    size_t Size() const{
        return intervals_.size();
    }
    bool Empty() const{
        return intervals_.empty();
    }
    void Add(const T&min,const T&max){
        Add(value_type(min,max));
    }
    void Add(const value_type&interval);
    bool Contain(const T &min,const T &max) const{
        return Contain(value_type(min,max));
    }
    bool Contain(const value_type&interval) const;
    void Test(const T &min,const T &max);
private:
    struct IntervalLess{
        bool operator()(const value_type &a,const value_type &b) const;
    };
    typedef typename std::set<Interval<T>,IntervalLess> Set;
    typedef typename Set::iterator iterator;
    void Compact(iterator begin,iterator end);
private:
    Set intervals_;
};
//Be careful. a the value waiting for insert.
//a.Max()>b.Max()  for Contain
//I found the memslice can not be free due to contain.
template<class T>
bool IntervalSet<T>::IntervalLess::operator()(const value_type &a,
                                              const value_type &b) const
{
    return a.Min()<b.Min()||(a.Min()==b.Min()&&a.Max()>b.Max());
}
template<class T>
void IntervalSet<T>::Add(const value_type&interval){
     std::pair<iterator,bool> ins=intervals_.insert(interval);
     if(!ins.second){
        return;
     }
     iterator begin=ins.first;
     if(begin!=intervals_.begin()){
        begin--;
     }
     iterator end=intervals_.end();
     Compact(begin,end);
}
template<class T>
void IntervalSet<T>::Test(const T &min,const T &max){
    //iterator it=intervals_.upper_bound(interval);
}
template<class T>
void IntervalSet<T>::Compact(iterator begin,iterator end){
    if(begin==end){
        return;
    }
    iterator it=begin;
    iterator prev=begin;
    iterator next=begin;
    ++next;
    ++it;
    while(it!=end){
        ++next;
        if(prev->Max()>=it->Min()){
            T min=prev->Min();
            T max=std::max(prev->Max(),it->Max());
            intervals_.erase(prev);
            intervals_.erase(it);
            std::pair<iterator,bool> ins=intervals_.insert(value_type(min,max));
            prev=ins.first;
        }else{
            prev=it;
        }
        it=next;
    }
}
template<class T>
bool IntervalSet<T>::Contain(const value_type&interval) const{
    iterator it=intervals_.upper_bound(interval);
    if(it==intervals_.begin()){
      return false;
    }
    it--;
    return it->Contains(interval);
}
#endif
