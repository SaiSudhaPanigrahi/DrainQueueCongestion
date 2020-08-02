#include "ns3-func.h"
#include "thirdparty/include/random.h"
class RandomIntegerGenerator{
public:
    static RandomIntegerGenerator* Instance();
    int NextInt( int min, int max );
private:
    RandomIntegerGenerator(){
        random_.seedTime();
    }
    ~RandomIntegerGenerator(){}
    dqc::Random random_;
};
RandomIntegerGenerator* RandomIntegerGenerator::Instance(){
    static RandomIntegerGenerator *ins=new RandomIntegerGenerator();
    return ins;
}
int RandomIntegerGenerator::NextInt( int min, int max ){
    return random_.nextInt(min,max);
}
u32 prandom_u32_max(u32 ep_ro){
    return (u32)RandomIntegerGenerator::Instance()->NextInt(0,(int)ep_ro);
}
