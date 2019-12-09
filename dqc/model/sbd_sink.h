#pragma once
#include "ns3/dqc_sender.h"
#include <stdio.h>
#include <vector>
#include <deque>
#include <string>
#include <fstream>
#include <map>
#include <memory>
#include <utility>
namespace ns3{
class HarmonicMean{
public:
    HarmonicMean(uint32_t window,int th=100):m_w(window),m_resumTh(th){}
    ~HarmonicMean();
    double OnNewSample(uint32_t signal);
private:
    void UpdateSum();
    uint32_t m_w{0};
    double m_sum{0.0};
    int m_resum{0};
    int m_resumTh{100};
    std::deque<double> m_samples;
};
struct Sample{
Sample(uint32_t event_time,double sample):ts(event_time),signal(sample){}
uint32_t ts;
double  signal;
};
struct TrendlineSampleGroup{
    TrendlineSampleGroup(uint32_t window):m_h(window){}
    ~TrendlineSampleGroup(){
        m_samples.clear();
    }
    void OnNewSample(uint32_t event,uint32_t signal){
        if(m_first){
            x0=event;
            m_first=false;
        }
        uint32_t abs=event-x0;
        double m=m_h.OnNewSample(signal);
        m_samples.emplace_back(abs,m);
    }
    size_t Size() const {return m_samples.size();}
    void Reset(){
        m_first=true;
        m_samples.clear();
    }
    double CalculateTrendline();
    HarmonicMean m_h;
    bool m_first{true};
    uint32_t x0{0};
    std::vector<Sample> m_samples;
};
struct TrendlineInfo{
    TrendlineInfo(uint32_t g,double k1,int c1,double k2,int c2,double e):
    group(g),k1(k1),k2(k2),c1(c1),c2(c2),error(e){}
    uint32_t group;
    double k1;
    double k2;
    int c1;
    int c2;
    double error;
};
class SSCSampleGroup{
public:
    SSCSampleGroup(){}
    ~SSCSampleGroup();
    void OnNewSample(uint32_t event,uint32_t signal);
    double CalculateAverageDelay();
    int CalculateSkewBase(double mean_delay);
    double CalculateVarBase(double owd_previous);
    size_t Size() const {return m_samples.size();}
    uint32_t GetStartTime(){return m_start;}
    int Crossing(double mean_delay);
private:
    uint32_t m_start{0};
    std::vector<double> m_samples;
    size_t m_n_contri_owd{0};
    double m_overline_owd{-1};
    size_t m_n_contri_var{0};
    double m_var_base{-1};
};
struct SSCInfo{
double skew_est;
double var_est;
double freq_est;
};
class SSCDetectionAlgorithm{
public:
    class Visitor{
        public:
        virtual ~Visitor(){}
        virtual void SSCDetectionSharedBottleneck(){}
    };
    SSCDetectionAlgorithm(){}
    ~SSCDetectionAlgorithm();
    void set_visitor(Visitor *visitor){m_visitor=visitor;}
    void RegisterID(uint32_t id);
    void OnNewDelaySample(uint32_t id,uint32_t event,uint32_t owd);
    bool IsSharedBottleneck() const{ return m_sbd;};
private:
    void InsertDataToGroup(uint32_t index,uint32_t event,uint32_t owd);
    uint32_t GetIndex(uint32_t id);
    void TriggerDetection(uint32_t event);
    void CalcualteSSC(uint32_t index);
    void CleanObsoleteGroups();
    void SBDDecision();
    bool m_first{true};
    uint32_t m_detectionTime{0};
    uint32_t m_startTime{0};
    Visitor *m_visitor{nullptr};
    bool m_sbd{false};
    std::map<uint32_t,uint32_t> m_id_index;
    std::deque<std::shared_ptr<SSCSampleGroup>> m_groups[2];
    SSCInfo m_info[2];
};
class ShareBottleneckDetectionSink:public OneWayDelaySink,public SSCDetectionAlgorithm::Visitor{
public:
    ShareBottleneckDetectionSink(std::string &prefix,bool enableTrace);
    ~ShareBottleneckDetectionSink();
    void RegisterDataSource(uint32_t id);
    bool NeedRegisterToSender(uint32_t id) override;
    void OnOneWayDelaySample(uint32_t id,uint32_t seq,uint32_t owd) override;
    void SSCDetectionSharedBottleneck() override;
private:
    bool TrendlineSBD();
    void PrintTrendLineInfo();
    std::map<uint32_t,std::shared_ptr<TrendlineSampleGroup>> m_sources;
    std::vector<TrendlineInfo> m_trendlineInfos;
    void OpenSBDTraceFile(std::string &prefix);
    void CloseSBDTraceFile();
    std::string m_prefix;
    bool m_enableTrace{false};
    uint32_t m_group{0};
    bool m_firstSample{true};
    uint32_t m_trendlineUpdateTime{0};
    uint32_t m_updataTimeLength{100}; //100 milliseconds
    uint32_t m_notCountInSlowStart{10};// avoid self inflict queue delay in slow start,about 1 second;
    double m_trendlineErrorTh{10.0};
    uint32_t m_congestionTh{5};
    uint32_t m_underErrorTh{2};
    std::fstream m_traceTrYes;
    std::fstream m_traceTrResult;
    std::fstream m_traceSscResult;
    //ssc
    SSCDetectionAlgorithm m_ssc;
};
}
