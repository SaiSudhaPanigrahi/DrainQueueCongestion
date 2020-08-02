#pragma once
#include "ns-linux-util.h"
namespace ns3{
class CongestionControlManager
{

public:
	CongestionControlManager();
//	int Register(struct tcp_congestion_ops* new_ops);
	struct tcp_congestion_ops* get_ops(const char* name);
	void dump();
	void scan();
	int num() const {return num_;}
private:
	int num_{0};
	struct tcp_congestion_ops** ops_list;
};
extern CongestionControlManager cong_ops_manager;    
}
