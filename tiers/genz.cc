#include<close_socket.hh>
#include<ErrorWords.hh>
#include<getaddrinfo_wrapper.hh>
#include<platforms.hh>
#include<poll_wrapper.hh>
#include<ReceiveBufferStack.hh>
#include<syslog_wrapper.hh>
#include"zz.frontMiddle.hh"
#include<stdio.h>
#include<stdlib.h>//exit

using namespace ::cmw;

int main (int ac,char** av){
  try{
    if(ac<3||ac>5)
      throw failure("Usage: genz account-number .mdl-file-path [node] [port]");

    windowsStart();
    getaddrinfo_wrapper res(ac<4?
#ifdef __linux__
		            "127.0.0.1"
#else
		            "::1"
#endif
                            :av[3]
                            ,ac<5?"55555":av[4],SOCK_DGRAM);
    auto rp=res.get();
    SendBufferStack<> sendbuf;
    for(;rp!=nullptr;rp=rp->ai_next){
      if((sendbuf.sock_=::socket(rp->ai_family,rp->ai_socktype,0))!=-1)goto sk;
    }
    if(-1==sendbuf.sock_)throw failure("socket call(s) ")<<GetError();

sk: ::pollfd pfd{sendbuf.sock_,POLLIN,0};
    int waitSeconds=9;
    frontMiddle::Marshal(sendbuf,marshalling_integer(av[1]),av[2]);
    for(int j=0;j<2;++j,waitSeconds*=2){
      sendbuf.Send(rp->ai_addr,rp->ai_addrlen);
#ifdef __linux__
      set_nonblocking(pfd.fd);
#endif
      if(poll_wrapper(&pfd,1,waitSeconds*1000)>0){
        ReceiveBufferStack<SameFormat> buf(pfd.fd);
        if(buf.GiveBool())::exit(EXIT_SUCCESS);
        throw failure("cmwA:")<<buf.GiveString_view();
      }
    }
    throw failure("No reply received.  Is the cmwA running?");
  }catch(::std::exception const& e){
    ::printf("%s: %s\n",av[0],e.what());
#ifndef CMW_WINDOWS
    ::openlog(av[0],LOG_NDELAY,LOG_USER);
#endif
    syslog_wrapper(LOG_ERR,"%s",e.what());
  }
  return EXIT_FAILURE;
}
