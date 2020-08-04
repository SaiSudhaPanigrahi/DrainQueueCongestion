#include "tcp_congestion_manager.h"
#include <stdio.h>
#include<string.h>
namespace ns3{
CongestionControlManager::CongestionControlManager() {
	scan();
};
void CongestionControlManager::scan() {
	struct tcp_congestion_ops *e;
	num_ = 1;
	list_for_each_entry_rcu(e, &ns_tcp_cong_list, list) {
		num_++;
	};
	ops_list = new struct tcp_congestion_ops*[num_];

	ops_list[0] = &tcp_reno;
	int i=1;
	list_for_each_entry_rcu(e, &tcp_cong_list, list) {
		ops_list[i] = e;
		i++;
	}
	cc_list_changed = 0;
	//printf("congestion control algorithm initialized\n");
};

struct tcp_congestion_ops* CongestionControlManager::get_ops(const char* name) {
	if (cc_list_changed) scan();
	for (int i=0; i< num_; i++) {
		if (strcmp(name, ops_list[i]->name)==0)
			return ops_list[i];
	}
	return 0;
};

void CongestionControlManager::dump() {
	printf("List of cc (total %d) :", num_);
	for (int i=0; i< num_; i++) {
		printf(" %s", ops_list[i]->name);
	}
	printf("\n");
};
CongestionControlManager *g_cc_manager=nullptr;
CongestionControlManager* RegisterCCManager(CongestionControlManager *manager){
    CongestionControlManager *prev=g_cc_manager;
    g_cc_manager=manager;
    return prev;
}
CongestionControlManager* GetCCManager(){
    return g_cc_manager;
}
}
