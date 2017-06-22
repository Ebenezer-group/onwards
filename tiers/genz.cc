#include<close_socket.hh>
#include<ErrorWords.hh>
#include<getaddrinfo_wrapper.hh>
#include<platforms.hh>
#include<poll_wrapper.hh>
#include<ReceiveBufferStack.hh>
#include<SendBufferStack.hh>
#include<syslog_wrapper.hh>
#include"zz.front_middle.hh"
#include<stdio.h>
#include<stdlib.h> //exit

using namespace ::cmw;

int main (int argc,char** argv){
  try{
    if(argc<3||argc>5)
      throw failure("Usage: genz account-number .req-file-path [node] [port]");

    windows_start();
    getaddrinfo_wrapper res(argc<4?"::1"/*"127.0.0.1"*/:argv[3]
                            ,argc<5?"55555":argv[4],SOCK_DGRAM);
    auto rp=res.get();
    SendBufferStack<> sendbuf;
    for(;rp!=nullptr;rp=rp->ai_next){
      if((sendbuf.sock_=::socket(rp->ai_family,rp->ai_socktype,0))!=-1)goto sk;
    }
    if(-1==sendbuf.sock_)throw failure("socket call(s) ")<<GetError();

sk: ::pollfd pfd{sendbuf.sock_,POLLIN,0};
    int waitMillisecs=9000;
    front_middle::Marshal(sendbuf,marshalling_integer(argv[1]),argv[2]);
    for(int j=0;j<2;++j,waitMillisecs*=2){
      sendbuf.Send(rp->ai_addr,rp->ai_addrlen);
#ifdef __linux__
      set_nonblocking(pfd.fd);
#endif
      if(poll_wrapper(&pfd,1,waitMillisecs)>0){
        ReceiveBufferStack<SameFormat> buf(pfd.fd);
        if(buf.GiveBool())::exit(EXIT_SUCCESS);
        throw failure("CMWA:")<<buf.GiveString_view();
      }
    }
    throw failure("No reply received.  Is the cmwAmbassador running?");
  }catch(::std::exception const& ex){
    ::printf("%s: %s\n",argv[0],ex.what());
#ifndef CMW_WINDOWS
    ::openlog(argv[0],LOG_NDELAY,LOG_USER);
#endif
    syslog_wrapper(LOG_ERR,"%s",ex.what());
  }
  return EXIT_FAILURE;
}
