#pragma once
#include "log.h"
#include <string>
#include <fstream>
namespace ns3{
//singleton
class GlobalStream{
public:
    static GlobalStream* Create(std::string filename,std::ios::openmode filemode);
private:
    GlobalStream(std::string filename,std::ios::openmode filemode);
    ~GlobalStream();
    std::ofstream* m_os{nullptr};
};
}
