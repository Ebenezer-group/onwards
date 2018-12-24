//  This program receives messages sent by sendExample.

#include<cmw/Buffer.hh>
#include"messageIDs.hh"
#include"plf_colony.h"

#include<array>
#include<iostream>
#include<set>
#include<string>
#include<vector>
#include"zz.receiveMessages.hh"
using namespace ::cmw;
using namespace ::receiveMessages;

int main ()try{
  winStart();
  BufferStack<SameFormat> buf(udpServer("12345"));

  for(;;){
    buf.GetPacket();
    auto const msgid=Give<messageID_8>(buf);
    ::std::cout<<"Message id: "<<static_cast<unsigned>(msgid)<<'\n';
    switch(msgid){
    case messageid1:
    {
      ::std::vector<int32_t> vec;
      ::std::string str;
      receiveMessages::Give(buf,vec,str);
      for(auto val:vec){::std::cout<<val<<" ";}
      ::std::cout<<'\n'<<str;
    }
    break;

    case messageid2:
    {
      ::std::set<int32_t> iset;
      Give(buf,iset);
      for(auto val:iset){::std::cout<<val<<" ";}
    }
    break;

    case messageid3:
    {
      ::std::array<std::array<float,2>, 3> a;
      Give(buf,a);
      for(auto subarray:a){
        for(auto val:subarray){::std::cout<<val<<" ";}
        ::std::cout<<::std::endl;
      }
    }
    break;

    case messageid4:
    {
      ::plf::colony<::std::string> clny;
      Give(buf,clny);
      for(auto val:clny){::std::cout<<val<<" ";}
    }
    break;

    default:
      ::std::cout<<"Unexpected message id: "<<msgid;
    }
    ::std::cout<<::std::endl;
  }
}catch(std::exception const& e){
  ::std::cout<<"failure: "<<e.what()<<::std::endl;
  return 1;
}
