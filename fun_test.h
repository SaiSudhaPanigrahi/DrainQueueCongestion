#ifndef FUN_TEST_H_
#define FUN_TEST_H_
#include "proto_types.h"
#include "process_alarm_factory.h"
namespace dqc{
class AlarmTest;
class CounterDelegate:public Alarm::Delegate{
public:
    CounterDelegate(AlarmTest *test);
    ~CounterDelegate();
    virtual void OnAlarm() override;
private:
    int32_t count_{0};
    AlarmTest *test_;
};
class AlarmTest{
public:
    AlarmTest(){}
    ~AlarmTest(){}
    void OnTrigger();
    ProtoTime TimeOut(int64_t gap);
    std::shared_ptr<Alarm> alarm_;
};
}
void proto_types_test();
void ack_frame_test();
void interval_test();
void byte_order_test();
void test_proto_framer();
void test_ufloat();
void test_rtt();
void test_stream_endecode();
void test_readbuf();
void test_stop_waiting();
void test_alarm();
void test_test();
#endif
