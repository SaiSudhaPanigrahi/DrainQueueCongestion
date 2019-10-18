#pragma once
#include <stdint.h>
/*
*https://blog.csdn.net/tiandijun/article/details/72469471
*https://zhuanlan.zhihu.com/p/24312995
*https://cs.chromium.org/chromium/src/third_party/webrtc/modules/remote_bitrate_estimator/overuse_estimator.cc
* x_k=x_{k-1}+w_{k-1}
* y_k=x_k+v_k
* In essence, this is a dynamic exponential filter.
* reference from:
* Analysis and Design of the Google Congestion Control for Web Real-time Communication
*/

namespace dqc{
class KalmanFilter{
public:
    KalmanFilter();
    int Update(int64_t now,int measure);
    void UpdateNoiseEstaimte(double residual);
    void Reset();
private:
    bool first_{true};
    double avg_noise_{0.0};
    double var_noise_{0.0};
    double p_{0.0};
    double q_{0.0};
    double x_hat_{0.0};
};
}
