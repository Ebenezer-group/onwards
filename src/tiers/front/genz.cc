#include<cmwBuffer.hh>
#include"genz.mdl.hh"
#include<stdio.h>
using namespace ::cmw;

void leave (char const* fmt,auto...t)noexcept{
  if constexpr(sizeof...(t)==0)::fputs(fmt,stderr);
  else ::fprintf(stderr,fmt,t...);
  exitFailure();
}

int main (int ac,char** av)try{
  if(ac<3||ac>5)
    leave("Usage: genz account-num mdl-file-path [node] [port]\n");
  winStart();
  SockaddrWrapper sa(ac<4?"127.0.0.1":av[3],ac<5?55555:fromChars(av[4]));
  BufferStack<SameFormat> buf(::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP));

  ::middle::marshal<udpPacketMax>(buf,MarshallingInt(av[1]),av[2]);
  for(int tm=8;tm<13;tm+=4){
    buf.send(sa(),sizeof sa);
    setRcvTimeout(buf.sock,tm);
    if(buf.getPacket()){
      if(giveBool(buf))::std::exit(EXIT_SUCCESS);
      leave("cmwA:%s\n",buf.giveStringView().data());
    }
  }
  leave("No reply received.  Is the cmwA running?\n");
}catch(::std::exception& e){leave("%s\n",e.what());}
