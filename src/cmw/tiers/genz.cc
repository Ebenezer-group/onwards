#include<cmw/Buffer.hh>
#include<cmw/ErrorWords.hh>
#include<cmw/wrappers.hh>
#include"zz.frontMiddle.hh"
#include<stdio.h>
#include<stdlib.h>//exit
using namespace ::cmw;

void bail (char const* a,char const* b="")noexcept{
  ::printf("%s%s\n",a,b);
#ifndef CMW_WINDOWS
  ::openlog("genz",LOG_NDELAY,LOG_USER);
#endif
  syslogWrapper(LOG_ERR,"%s%s",a,b);
  ::exit(EXIT_FAILURE);
}

int main (int ac,char** av){
  if(ac<3||ac>5)
    bail("Usage: genz account-num mdl-file-path [node] [port]");

  try{
    winStart();
    getaddrinfoWrapper res(ac<4?
#ifdef __linux__
                           "127.0.0.1":av[3]
#else
                           "::1":av[3]
#endif
                           ,ac<5?"55555":av[4],SOCK_DGRAM);
    BufferStack<SameFormat> buf(res.getSock());

    ::frontMiddle::Marshal(buf,marshallingInt(av[1]),av[2]);
    for(int tm=8;tm<17;tm+=8){
      buf.Send(res()->ai_addr,res()->ai_addrlen);
      setRcvTimeout(buf.sock_,tm);
      if(buf.GetPacket()){
        if(giveBool(buf))::exit(EXIT_SUCCESS);
        auto v=giveStringView(buf);
        *(const_cast<char*>(v.data())+v.length())='\0';
        bail("cmwA:",v.data());
      }
    }
    bail("No reply received.  Is the cmwA running?");
  }catch(::std::exception const& e){bail(e.what());}
}
