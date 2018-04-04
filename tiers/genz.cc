#include<Buffer.hh>
#include<ErrorWords.hh>
#include<wrappers.hh>
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

    ::pollfd pd{buf.sock_,POLLIN,0};
    ::frontMiddle::Marshal(buf,marshallingInt(av[1]),av[2]);
    for(int j=0,waitTime=8000;j<2;++j,waitTime*=2){
      buf.Send(res()->ai_addr,res()->ai_addrlen);
#ifdef __linux__
      setNonblocking(pd.fd);
#endif
      if(pollWrapper(&pd,1,waitTime)>0){
        buf.GetPacket();
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
  }
  ::exit(EXIT_FAILURE);
}
