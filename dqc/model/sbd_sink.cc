#include "sbd_sink.h"
#include "ns3/core-module.h"
#include "sink_manager.h"
#include "ns3/log.h"
#include <iostream>
#include <unistd.h>
#include <math.h>
#include <algorithm>
namespace ns3{
NS_LOG_COMPONENT_DEFINE("sbd-sink");
namespace{
//https://tools.ietf.org/html/rfc8382#section-3.2.3
    const double k_p_s=0.15;
    const double k_p_mad=0.1;
    const double k_p_f=0.1;
    const double k_p_v=0.7;
    const uint32_t kGroupTimeInterval=350;//in milliseconds
    const size_t kGroups=50+1;
    const uint32_t kSSCStartDetectionTime=0;
    const uint32_t kSSCDetectionInterval=1000;//1s
}
namespace{
    const uint32_t kContinueCongestionTh=10;
}
HarmonicMean::~HarmonicMean(){
    m_samples.clear();
}
double HarmonicMean::OnNewSample(uint32_t signal){
    double mean=0.0;
    if(signal>0){
        double sample=1000.0/(double)signal;
        m_samples.push_back(sample);
        m_sum+=sample;
        if(m_samples.size()>m_w){
            double old=m_samples.front();
            m_samples.pop_front();
            m_sum-=old;
        }
        m_resum++;
        if(m_samples.size()<m_w){
            mean=(double)signal;
        }else{
            size_t n=m_samples.size();
            NS_ASSERT(n==m_w);
            mean=n*1000.0/m_sum;
        }
        if(m_resum>m_resumTh){
            UpdateSum();
            m_resum=0;
        }
    }
    return mean;
}
void HarmonicMean::UpdateSum(){
    double sum=0.0;
    for(auto it=m_samples.begin();it!=m_samples.end();it++){
        sum+=(*it);
    }
    m_sum=sum;
}
double CalculateError(double k1,double k2){
    double error=-1;
    if(k1>0&&k2>0){
        double dumerator=k1>k2?k1:k2;
        double numerator=k1>=k2?(k1-k2):(k2-k1);
        error=numerator*100/dumerator;
    }
    return error;
}
//https://www.codesansar.com/numerical-methods/linear-regression-method-using-c-programming.htm
double TrendlineSampleGroup::CalculateTrendline(){
    int n=m_samples.size();
    int i=0;
    double sumX=0.0,sumX2=0.0;
    double sumY=0.0,sumXY=0.0;
    for (i=0;i<n;i++){
        double x=(double)(m_samples[i].ts);
        double y=m_samples[i].signal;
        sumX=sumX+x;
        sumX2=sumX2+x*x;
        sumY=sumY+y;
        sumXY=sumXY+x*y;
    }
    double k=1000*(n*sumXY-sumX*sumY)/(n*sumX2-sumX*sumX);
    return k;
}
SSCSampleGroup::~SSCSampleGroup(){
    m_samples.clear();
}
void SSCSampleGroup::OnNewSample(uint32_t event,uint32_t signal){
    if(0==m_start){
        m_start=event;
    }
    double delay=(double)signal;
    m_samples.push_back(delay);
}
double SSCSampleGroup::CalculateAverageDelay(){
    size_t n=m_samples.size();
    if(n==m_n_contri_owd){
        return m_overline_owd;
    }
    size_t i=0;
    NS_ASSERT(n>0);
    double sum=0.0;
    for(i=0;i<n;i++){
        sum=sum+m_samples[i];
    }
    m_overline_owd=sum/(n*1.0);
    m_n_contri_owd=n;
    return m_overline_owd;
}
int SSCSampleGroup::CalculateSkewBase(double mean_delay){
    int skew_base=0;
    size_t i=0;
    size_t n=m_samples.size();
    for(i=0;i<n;i++){
        if(m_samples[i]<mean_delay){
            skew_base++;
        }else{
            if(m_samples[i]>mean_delay){
                skew_base--;
            }
        }
    }
    return skew_base;
}
double SSCSampleGroup::CalculateVarBase(double owd_previous){
    size_t n=m_samples.size();
    if(n==m_n_contri_var){
        return m_var_base;
    }
    size_t i=0;
    double abs_deviation=0.0;
    for(i=0;i<n;i++){
        double owd=m_samples[i];
        abs_deviation+=fabs(owd-owd_previous);
    }
    m_var_base=abs_deviation;
    m_n_contri_var=n;
    return m_var_base;
}
int SSCSampleGroup::Crossing(double mean_delay){
    CalculateAverageDelay();
    int cross=0;
    if(m_overline_owd>k_p_v*mean_delay){
        cross=1;
    }
    return cross;
}
SSCDetectionAlgorithm::~SSCDetectionAlgorithm(){
    m_groups[0].clear();
    m_groups[1].clear();
}
void SSCDetectionAlgorithm::OnNewDelaySample(uint32_t id,uint32_t event,uint32_t owd){
    if(m_first){
        m_startTime=event+kSSCStartDetectionTime;
        m_detectionTime=m_startTime;
        m_first=false;
    }
    if(event>=m_startTime){
        uint32_t index=GetIndex(id);
        if(index<2){
            InsertDataToGroup(index,event,owd);
        }else{
            NS_LOG_INFO("path not found "<<id);
        }
    }
}
void SSCDetectionAlgorithm::InsertDataToGroup(uint32_t index,uint32_t event,uint32_t owd){
    
    if(m_groups[index].empty()){
        std::shared_ptr<SSCSampleGroup> group(new SSCSampleGroup());
        group->OnNewSample(event,owd);
        m_groups[index].push_back(group);
    }else{
        auto it=m_groups[index].rbegin();
        uint32_t start=(*it)->GetStartTime();
        if(event-start>kGroupTimeInterval){
            TriggerDetection(event);
            std::shared_ptr<SSCSampleGroup> group(new SSCSampleGroup());
            group->OnNewSample(event,owd);
            m_groups[index].push_back(group);            
        }else{
            (*it)->OnNewSample(event,owd);
        }
    }
    CleanObsoleteGroups();
}
void SSCDetectionAlgorithm::TriggerDetection(uint32_t event){
    if((m_groups[0].size()>=kGroups)&&(m_groups[1].size()>=kGroups)){
        if(event-m_detectionTime>kSSCDetectionInterval){
            m_detectionTime=event;
            CalcualteSSC(0);
            CalcualteSSC(1);
            SBDDecision();
        }
    }
}
void SSCDetectionAlgorithm::CalcualteSSC(uint32_t index){
    if(m_groups[index].size()>=kGroups){
        size_t start=m_groups[index].size()-kGroups+1;
        NS_ASSERT(start>0);
        size_t i=start;
        double sum_owd=0.0;
        int samples=0;
        int sum_skew=0.0;
        double sum_var=0.0;
        int num_of_crossings=0;
        for(i=start;i<kGroups;i++){
            sum_owd+=m_groups[index].at(i)->CalculateAverageDelay();
            samples+=m_groups[index].at(i)->Size();
        }
        double mean=sum_owd/(kGroups-1);
        for(i=start;i<kGroups;i++){
            sum_skew+=m_groups[index].at(i)->CalculateSkewBase(mean);
            double previous_owd=m_groups[index].at(i-1)->CalculateAverageDelay();
            sum_var+=m_groups[index].at(i)->CalculateVarBase(previous_owd);
            num_of_crossings+=m_groups[index].at(i)->Crossing(mean);
        }
        m_info[index].skew_est=(double)sum_skew/samples;
        m_info[index].var_est=sum_var/samples;
        m_info[index].freq_est=double(num_of_crossings)/(kGroups-1);
		//NS_LOG_INFO(index<<" "<<num_of_crossings);
    }
}
void SSCDetectionAlgorithm::SBDDecision(){
    double skew=fabs(m_info[0].skew_est-m_info[1].skew_est);
    double var=fabs(m_info[0].var_est-m_info[1].var_est);
    double freq=fabs(m_info[0].freq_est-m_info[1].freq_est);
	//NS_LOG_INFO("decision "<<skew<<" "<<var<<" "<<freq);
    if(skew<k_p_s&&var<k_p_mad&&freq<k_p_f){
        m_sbd=true;
        if(m_visitor){
            m_visitor->SSCDetectionSharedBottleneck();
        }
    }else{
        m_sbd=false;
    }
}
void SSCDetectionAlgorithm::CleanObsoleteGroups(){
    int i=0;
    int old=0;
    if(m_groups[0].size()>kGroups){
        old=m_groups[0].size()-kGroups;
        for(i=0;i<old;i++){
            m_groups[0].pop_front();
        }
    }
    if(m_groups[1].size()>kGroups){
        old=m_groups[1].size()-kGroups;
        for(i=0;i<old;i++){
            m_groups[1].pop_front();
        }
    }
}
void SSCDetectionAlgorithm::RegisterID(uint32_t id){
    if(m_id_index.size()==2){
        return;
    }
    bool exist=false;
    for(auto it=m_id_index.begin();it!=m_id_index.end();it++){
        if(id==it->first){
            exist=true;
            break;
        }
    }
    if(!exist){
        uint32_t index=m_id_index.size();
        m_id_index.insert(std::make_pair(id,index));
    }
}
uint32_t SSCDetectionAlgorithm::GetIndex(uint32_t id){
    uint32_t index=5;
    for(auto it=m_id_index.begin();it!=m_id_index.end();it++){
        if(id==it->first){
            index=it->second;
            break;
        }
    }
    return index;
}
ShareBottleneckDetectionSink::ShareBottleneckDetectionSink(std::string &prefix,bool enableTrace){
    m_prefix=prefix;
    m_enableTrace=enableTrace;
    if(m_enableTrace){
        OpenSBDTraceFile(prefix);
    }
    OneWayDelaySinkManager::Instance()->RegisterSink(this);
    m_ssc.set_visitor(this);
}
ShareBottleneckDetectionSink::~ShareBottleneckDetectionSink(){
    m_sources.clear();
    CloseSBDTraceFile();
}
void ShareBottleneckDetectionSink::RegisterDataSource(uint32_t id){
    bool exist=false;
    if(m_sources.size()==2){
        return;
    }
    for(auto it=m_sources.begin();it!=m_sources.end();it++){
        if(id==it->first){
            exist=true;
            break;
        }
    }
    if(!exist){
        std::shared_ptr<TrendlineSampleGroup> tr(new TrendlineSampleGroup(20));
        m_sources.insert(std::make_pair(id,tr));
        m_ssc.RegisterID(id);
    }
}
bool ShareBottleneckDetectionSink::NeedRegisterToSender(uint32_t id){
    bool ret=false;
    for(auto it=m_sources.begin();it!=m_sources.end();it++){
        if(id==it->first){
            ret=true;
            break;
        }
    }
    return ret;
}
void ShareBottleneckDetectionSink::OnOneWayDelaySample(uint32_t id,uint32_t seq,uint32_t owd){
    uint32_t event_time=Simulator::Now().GetMilliSeconds();
    if(m_firstSample){
        m_trendlineUpdateTime=event_time;
        m_group=0;
        m_firstSample=false;
    }
    m_ssc.OnNewDelaySample(id,event_time,owd);
    int i=0;
    for(auto it=m_sources.begin();it!=m_sources.end();it++){
        if(it->first==id){
            it->second->OnNewSample(event_time,owd);
        }
        i++;
    }
    if(event_time-m_trendlineUpdateTime>m_updataTimeLength){
        m_trendlineUpdateTime=event_time;
        double k[2]={0.0};
        int c[2]={0};
        int i=0;
        double error=0.0;
        for(auto it=m_sources.begin();it!=m_sources.end();it++){
            if(it->second->Size()>1){
                k[i]=it->second->CalculateTrendline();
                c[i]=it->second->Size();
            }
            it->second->Reset();
            i++;
            if(i>=2){
                break;
            }
        }
        if(m_group>m_notCountInSlowStart){
        error=CalculateError(k[0],k[1]);
        if(error<0||m_trendlineInfos.size()>50){
            bool is_sbd=TrendlineSBD();
            if(is_sbd){
                PrintTrendLineInfo();
                if(m_traceTrResult.is_open()){
                    float now=Simulator::Now().GetSeconds();
                    m_traceTrYes<<now<<"\tyes"<<std::endl;
                    m_traceTrResult<<now<<"\tyes"<<std::endl;
                }
            }
            m_trendlineInfos.clear();
        }
        if((error>0)&&(k[0]>10.0)&&(k[1]>10.0)){
            m_trendlineInfos.emplace_back(m_group,k[0],c[0],k[1],c[1],error);
        }         
        }
        m_group++;
    }
}
void ShareBottleneckDetectionSink::SSCDetectionSharedBottleneck(){
    if(m_traceSscResult.is_open()){
        float now=Simulator::Now().GetSeconds();
        m_traceSscResult<<now<<"\tyes"<<std::endl;        
    }

}
bool TrendlineInfoCmp(TrendlineInfo &i,TrendlineInfo &j){
    return i.k1>j.k1;
}
bool ShareBottleneckDetectionSink::TrendlineSBD(){
    bool ret=false;
    if(m_trendlineInfos.size()>=kContinueCongestionTh){
        ret=true;
    }else{
        uint32_t underErrorCount=0;
        for(auto it=m_trendlineInfos.begin();it!=m_trendlineInfos.end();it++){
            if(it->error<m_trendlineErrorTh){
                underErrorCount++;
            }
            if(underErrorCount>=m_underErrorTh){
                ret=true;
                break;
            }            
        }
    }
    /*if(m_trendlineInfos.size()>=kContinueCongestionTh){
        std::sort(m_trendlineInfos.begin(),m_trendlineInfos.end(),TrendlineInfoCmp);
        uint32_t underErrorCount=0;
        size_t i=0;
        for(auto it=m_trendlineInfos.begin();it!=m_trendlineInfos.end();it++){
            if(it->error<m_trendlineErrorTh){
                underErrorCount++;
            }
            if(underErrorCount>=m_underErrorTh){
                ret=true;
                break;
            } 
            if(i>10){
                break;
            }
			i++;
        }
    }*/

    return ret;
}
void ShareBottleneckDetectionSink::PrintTrendLineInfo(){
    if(m_traceTrResult.is_open()){
        for(auto it=m_trendlineInfos.begin();it!=m_trendlineInfos.end();it++){
            m_traceTrResult<<it->group<<" "<<it->k1<<" "<<it->c1<<" "
            <<it->k2<<" "<<it->c2<<" "<<it->error<<std::endl;
        }        
    }

    
}
void ShareBottleneckDetectionSink::OpenSBDTraceFile(std::string &prefix){
    char buf[FILENAME_MAX];
    memset(buf,0,FILENAME_MAX);
    std::string common=std::string(getcwd(buf, FILENAME_MAX))+"/traces/"+prefix;
    {
        std::string path=common+"_sbd_tr_yes.txt";
        m_traceTrYes.open(path.c_str(),std::fstream::out);
    }
    {
        std::string path=common+"_sbd_tr.txt";
        m_traceTrResult.open(path.c_str(),std::fstream::out);
    }
    {
        std::string path=common+"_sbd_ssc.txt";
        m_traceSscResult.open(path.c_str(),std::fstream::out);
    }
}
void ShareBottleneckDetectionSink::CloseSBDTraceFile(){
    if(m_traceTrYes.is_open()){
        m_traceTrYes.close();
    }
    if(m_traceTrResult.is_open()){
        m_traceTrResult.close();
    }
    if(m_traceSscResult.is_open()){
        m_traceSscResult.close();
    }
}
}
