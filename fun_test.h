#pragma once
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
void test_test();
