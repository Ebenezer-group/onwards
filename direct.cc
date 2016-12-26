#include "platforms.hh"

#include "ErrorWords.hh"
#include "getaddrinfo_wrapper.hh"
#include "poll_wrapper.hh"
#include "ReceiveBufferStack.hh"
#include "SendBufferStack.hh"
#include "syslog_wrapper.hh"
#include "zz.front_messages_middle.hh"
#include <stdio.h>
#include <experimental/string_view>
#include <stdlib.h> // exit

using namespace ::cmw;

int main (int argc,char** argv)
{
  try{
    if(argc<3 || argc>5)
      throw failure("Usage: direct account-number .req-file-path [node] [port]");

    windows_start();
    getaddrinfo_wrapper res(3==argc?"::1"/*"127.0.0.1"*/:argv[3]
                            ,argc<5?"55555":argv[4],SOCK_DGRAM);
    auto rp=res.get();
    SendBufferStack<> sendbuf;
    for(;rp!=nullptr;rp=rp->ai_next){
      if((sendbuf.sock_=::socket(rp->ai_family,rp->ai_socktype,0))!=-1)break;
    }
    if(-1==sendbuf.sock_)throw failure("socket call(s) failed ")<<GetError();

    ::pollfd pfd{sendbuf.sock_,POLLIN,0};
    int waitMillisecs=16000;
    for(int j=0;j<2;++j,waitMillisecs*=2){
      front_messages_middle::Marshal(sendbuf,marshalling_integer(argv[1])
                                     ,argv[2]);
      sendbuf.Flush(rp->ai_addr,rp->ai_addrlen);
      if(poll_wrapper(&pfd,1,waitMillisecs)>0){
        ReceiveBufferStack<SameFormat> buf(pfd.fd);
        if(!buf.GiveBool())throw failure("CMWA: ")<<buf.GiveString_view();
        ::exit(EXIT_SUCCESS);
      }
    }
    throw failure("No reply received.  Is the middle tier (CMWA) running?");
  }catch(::std::exception const& ex){
    ::printf("%s: %s\n",argv[0],ex.what());
#ifndef CMW_WINDOWS
    ::openlog(argv[0],LOG_NDELAY,LOG_USER);
#endif
    syslog_wrapper(LOG_ERR,"%s",ex.what());
  }
  return EXIT_FAILURE;
}
