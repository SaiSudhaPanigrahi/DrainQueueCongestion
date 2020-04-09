/*test quic bbr on ns3 platform*/
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"
#include "ns3/dqc-module.h"
#include "couple_cc_manager.h"
#include "couple_cc_source.h"
#include <string>
#include <iostream>
#include <memory>
using namespace ns3;
using namespace dqc;
const uint32_t TOPO_DEFAULT_BW     = 3000000;    // in bps: 3Mbps
const uint32_t TOPO_DEFAULT_PDELAY =      100;    // in ms:   100ms
const uint32_t TOPO_DEFAULT_QDELAY =     300;    // in ms:  300ms
const uint32_t DEFAULT_PACKET_SIZE = 1000;
static int ip=1;
static NodeContainer BuildExampleTopo (uint64_t bps,
                                       uint32_t msDelay,
                                       uint32_t msQdelay,bool enable_random_loss)
{
    NodeContainer nodes;
    nodes.Create (2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", DataRateValue  (DataRate (bps)));
    pointToPoint.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (msDelay)));
    auto bufSize = std::max<uint32_t> (DEFAULT_PACKET_SIZE, bps * msQdelay / 8000);
    pointToPoint.SetQueue ("ns3::DropTailQueue",
                           "Mode", StringValue ("QUEUE_MODE_BYTES"),
                           "MaxBytes", UintegerValue (bufSize));
    NetDeviceContainer devices = pointToPoint.Install (nodes);

    InternetStackHelper stack;
    stack.Install (nodes);
    Ipv4AddressHelper address;
	std::string nodeip="10.1."+std::to_string(ip)+".0";
    ip++;
    address.SetBase (nodeip.c_str(), "255.255.255.0");
    address.Assign (devices);
    // disable tc for now, some bug in ns3 causes extra delay
    TrafficControlHelper tch;
    tch.Uninstall (devices);
    if(enable_random_loss){
	std::string errorModelType = "ns3::RateErrorModel";
  	ObjectFactory factory;
  	factory.SetTypeId (errorModelType);
  	Ptr<ErrorModel> em = factory.Create<ErrorModel> ();
	devices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));		
	}
    return nodes;
}
static void InstallDqc( dqc::CongestionControlType cc_type,
                        Ptr<Node> sender,
                        Ptr<Node> receiver,
						uint16_t send_port,
                        uint16_t recv_port,
                        float startTime,
                        float stopTime,
						DqcTrace *trace,
                        uint32_t max_bps=0,uint32_t cid=0,uint32_t emucons=1
)
{
    Ptr<DqcSender> sendApp = CreateObject<DqcSender> (cc_type);
	Ptr<DqcReceiver> recvApp = CreateObject<DqcReceiver>();
   	sender->AddApplication (sendApp);
	if(cid){
		sendApp->SetCongestionId(cid);
	}
	sendApp->SetNumEmulatedConnections(emucons);
    receiver->AddApplication (recvApp);
    Ptr<Ipv4> ipv4 = receiver->GetObject<Ipv4> ();
	Ipv4Address receiverIp = ipv4->GetAddress (1, 0).GetLocal ();
	recvApp->Bind(recv_port);
	sendApp->Bind(send_port);
	sendApp->ConfigurePeer(receiverIp,recv_port);
    sendApp->SetStartTime (Seconds (startTime));
    sendApp->SetStopTime (Seconds (stopTime));
    recvApp->SetStartTime (Seconds (startTime));
    recvApp->SetStopTime (Seconds (stopTime));
    if(max_bps>0){
        sendApp->SetMaxBandwidth(max_bps);
    }
	if(trace){
		sendApp->SetBwTraceFuc(MakeCallback(&DqcTrace::OnBw,trace));
		recvApp->SetOwdTraceFuc(MakeCallback(&DqcTrace::OnOwd,trace));
	}	
}
static double simDuration=300;
float appStart=0.0;
float appStop=simDuration;
int main(int argc, char *argv[]){
	CommandLine cmd;
    std::string instance=std::string("3");
    std::string cc_tmp("liaplus");
	std::string loss_str("0");
    cmd.AddValue ("it", "instacne", instance);
	cmd.AddValue ("cc", "cctype", cc_tmp);
	cmd.AddValue ("lo", "loss",loss_str);
    cmd.Parse (argc, argv);
    int loss_integer=std::stoi(loss_str);
    double loss_rate=loss_integer*1.0/100;
    std::cout<<"l "<<loss_integer<<std::endl;
	if(loss_integer>0){
	Config::SetDefault ("ns3::RateErrorModel::ErrorRate", DoubleValue (loss_rate));
	Config::SetDefault ("ns3::RateErrorModel::ErrorUnit", StringValue ("ERROR_UNIT_PACKET"));
	Config::SetDefault ("ns3::BurstErrorModel::ErrorRate", DoubleValue (loss_rate));
	Config::SetDefault ("ns3::BurstErrorModel::BurstSize", StringValue ("ns3::UniformRandomVariable[Min=1|Max=3]"));
	}
	uint16_t sendPort=6000;
	uint16_t recvPort=5000;
	uint64_t linkBw1   = TOPO_DEFAULT_BW;
    uint32_t msDelay1 = TOPO_DEFAULT_PDELAY;
    uint32_t msQDelay1 = TOPO_DEFAULT_QDELAY;

	uint64_t linkBw2   = TOPO_DEFAULT_BW;
    uint32_t msDelay2 = TOPO_DEFAULT_PDELAY;
    uint32_t msQDelay2 = TOPO_DEFAULT_QDELAY;

    uint32_t max_bps=0;
	std::string cc_name;
	if(loss_integer>0){
		cc_name="_"+cc_tmp+"_l"+std::to_string(loss_integer)+"_";
	}else{
		cc_name="_"+cc_tmp+"_";
	}
	
    if(instance==std::string("1")){
        linkBw1=6000000;
        msDelay1=50;
        msQDelay1=150;
        linkBw2=1500000;
        msDelay2=100;
        msQDelay2=200;
    }else if(instance==std::string("2")){
        linkBw1=4000000;
        msDelay1=50;
        msQDelay1=200;
        linkBw2=3000000;
        msDelay2=50;
        msQDelay2=200;     
    }
    dqc::CongestionControlType cc=kLiaBytes;
	if(cc_tmp==std::string("lia")){
		cc=kLiaBytes;
		std::cout<<cc_tmp<<std::endl;
	}else if(cc_tmp==std::string("liaplus")){
		cc=kLiaPlus;
		std::cout<<cc_tmp<<std::endl;
	}else if(cc_tmp==std::string("reno")){
		cc=kRenoBytes;
		std::cout<<cc_tmp<<std::endl;
	}else if(cc_tmp==std::string("bbrreno")){
		cc=kBBRReno;
		std::cout<<cc_tmp<<std::endl;
	}else if(cc_tmp==std::string("bbrcubic")){
		cc=kBBRCubic;
		std::cout<<cc_tmp<<std::endl;
	}
	bool enable_random_loss=false;
	if(loss_integer>0){
		enable_random_loss=true;
	}
	NodeContainer nodes1 = BuildExampleTopo (linkBw1, msDelay1, msQDelay1,enable_random_loss);
    NodeContainer nodes2 = BuildExampleTopo (linkBw2, msDelay2, msQDelay2,enable_random_loss);
	dqc::CoupleSource *source=new dqc::CoupleSource();
	source->RegsterMonitorCongestionId(1);
	source->RegsterMonitorCongestionId(4);
	dqc::CoupleManager *manager=dqc::CoupleManager::Instance();
	manager->RegisterSource(source);
	std::string prefix=instance+cc_name;
    uint32_t cc_id=1;
	int test_pair=1;
	DqcTrace trace1;
	std::string log=prefix+std::to_string(test_pair);
	trace1.Log(log,DqcTraceEnable::E_DQC_OWD|DqcTraceEnable::E_DQC_BW);
	test_pair++;
	InstallDqc(cc,nodes1.Get(0),nodes1.Get(1),sendPort,recvPort,appStart,appStop,&trace1,max_bps,cc_id);
	sendPort++;
	recvPort++;
    cc_id++;
	DqcTrace trace2;
	log=prefix+std::to_string(test_pair);
	trace2.Log(log,DqcTraceEnable::E_DQC_OWD|DqcTraceEnable::E_DQC_BW);
	test_pair++;
	InstallDqc(cc,nodes1.Get(0),nodes1.Get(1),sendPort,recvPort,appStart+40,appStop,&trace2,max_bps,cc_id,1);
    sendPort++;
	recvPort++;
	cc_id++;
    
	DqcTrace trace3;
	log=prefix+std::to_string(test_pair);
	trace3.Log(log,DqcTraceEnable::E_DQC_OWD|DqcTraceEnable::E_DQC_BW);
	test_pair++;
	InstallDqc(cc,nodes1.Get(0),nodes1.Get(1),sendPort,recvPort,appStart,appStop,&trace3,max_bps,cc_id,1);
    sendPort++;
	recvPort++;
    cc_id++;

	sendPort=6000;
	recvPort=5000;
	DqcTrace trace4;
	log=prefix+std::to_string(test_pair);
	trace4.Log(log,DqcTraceEnable::E_DQC_OWD|DqcTraceEnable::E_DQC_BW);
	test_pair++;
	InstallDqc(cc,nodes2.Get(0),nodes2.Get(1),sendPort,recvPort,appStart,appStop,&trace4,max_bps,cc_id,1);
    cc_id++;
    Simulator::Stop (Seconds(simDuration));
    Simulator::Run ();
    Simulator::Destroy();
	manager->Destructor();
	delete source;	
    return 0;
}
