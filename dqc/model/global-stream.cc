/*#include "ns3/global-stream.h"
namespace ns3{
GlobalStream* GlobalStream::Create(std::string filename,std::ios::openmode filemode){
    static GlobalStream *ins=new GlobalStream(filename,filemode);
    return ins;
}
GlobalStream::GlobalStream(std::string filename,std::ios::openmode filemode){
    m_os = new std::ofstream ();
    m_os->open (filename.c_str (), filemode);
    std::ostream *ostream=m_os;
    RegisterGlobalStream(ostream);
}
GlobalStream::~GlobalStream(){
    if(m_os->is_open()){
        m_os->flush();
        m_os->close();
    }
    if(m_os){
        UnRegisterGlobalStream();
        delete m_os;
    }
}
}*/
