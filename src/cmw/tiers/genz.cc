#include<cmw/Buffer.hh>
#include<cmw/ErrorWords.hh>
#include<cmw/wrappers.hh>
#include"zz.frontMiddle.hh"
#include<stdio.h>
#include<stdlib.h>//exit

using namespace ::cmw;

int main (int ac,char** av){
  try{
    if(ac<3||ac>5)
      throw failure("Usage: genz account-number .mdl-file-path [node] [port]");

    windowsStart();
    getaddrinfoWrapper res(ac<4?
#ifdef __linux__
                           "127.0.0.1":av[3]
#else
                           "::1":av[3]
#endif
                           ,ac<5?"55555":av[4],SOCK_DGRAM);
    BufferStack<SameFormat> buf;
    buf.sock_=res.getSock();

    ::frontMiddle::Marshal(buf,marshallingInt(av[1]),av[2]);
    for(int waitTime=8;waitTime<17;waitTime*=2){
      buf.Send(res()->ai_addr,res()->ai_addrlen);
      setRcvTimeout(buf.sock_,waitTime);
      if(buf.GetPacket()){
        if(GiveBool(buf))::exit(EXIT_SUCCESS);
        throw failure("cmwA:")<<GiveStringView(buf);
      }
    }
    throw failure("No reply received.  Is the cmwA running?");
  }catch(::std::exception const& e){
    ::printf("%s: %s\n",av[0],e.what());
#ifndef CMW_WINDOWS
    ::openlog(av[0],LOG_NDELAY,LOG_USER);
#endif
    syslogWrapper(LOG_ERR,"%s",e.what());
    ::exit(EXIT_FAILURE);
  }
}
