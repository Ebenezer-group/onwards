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
    BufferStack<SameFormat> sbuf;
    sbuf.sock_=res.getSock();

    ::pollfd pfd{sbuf.sock_,POLLIN,0};
    frontMiddle::Marshal(sbuf,marshallingInt(av[1]),av[2]);
    for(int j=0,waitTime=8000;j<2;++j,waitTime*=2){
      sbuf.Send(res()->ai_addr,res()->ai_addrlen);
#ifdef __linux__
      setNonblocking(pfd.fd);
#endif
      if(pollWrapper(&pfd,1,waitTime)>0){
        sbuf.GetPacket();
        if(GiveBool(sbuf))::exit(EXIT_SUCCESS);
        throw failure("cmwA:")<<GiveStringView(sbuf);
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
