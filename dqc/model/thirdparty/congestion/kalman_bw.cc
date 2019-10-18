#include <iostream>
#include "kalman_bw.h"
//https://github.com/bachagas/Kalman/blob/master/Kalman.h
namespace dqc{
namespace{
    const double kInitialP=1023;
    const double kQ=0.125;
    const double kAverageNoise=0;
    const double kVarNoise=0.1;
    const double kBeta=0.95;
}
KalmanFilter::KalmanFilter(){
    Reset();
}
int KalmanFilter::Update(int64_t now,int measure){
    if(first_){
        x_hat_=measure;
        first_=false;
        return (int)x_hat_;
    }
    double x=measure;
    double last_x_hat=x_hat_;
    double residual=x-last_x_hat;
    //UpdateNoiseEstaimte(residual);
    double k=0.0;
    double R=32;
    k=(p_+q_)/(p_+q_+R);
    x_hat_=(1-k)*last_x_hat+k*x;
    p_=(1-k)*(p_+q_);
    return (int)x_hat_;
}
void KalmanFilter::UpdateNoiseEstaimte(double residual){
    std::cout<<"res "<<residual<<std::endl;
    avg_noise_ = kBeta * avg_noise_ + (1 - kBeta) * residual;
    var_noise_ = kBeta * var_noise_ +
               (1 - kBeta) * (avg_noise_ - residual) * (avg_noise_ - residual);
}
void KalmanFilter::Reset(){
    first_=true;
    avg_noise_=0.0;
    var_noise_=kVarNoise;
    p_=kInitialP;
    q_=kQ;
}
}
