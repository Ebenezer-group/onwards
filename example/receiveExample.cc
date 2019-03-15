//  This program receives messages sent by sendExample.
#include<cmw/Buffer.hh>
#include"messageIDs.hh"
#include"plf_colony.h"

#include<array>
#include<iostream>
#include<set>
#include<string>
#include<vector>
#include"zz.exampleMessages.hh"

int main ()try{
  ::cmw::winStart();
  ::cmw::BufferStack<::cmw::SameFormat> buf(::cmw::udpServer("12345"));

  using namespace ::exampleMessages;
  for(;;){
    buf.GetPacket();
    auto const msgid=Give<messageID>(buf);
    ::std::cout<<"Message id: "<<static_cast<unsigned>(msgid)<<'\n';
    switch(msgid){
      case messageID::id1:
      {
        ::std::vector<int32_t> vec;
        ::std::string str;
        Give(buf,vec,str);
        for(auto val:vec){::std::cout<<val<<' ';}
        ::std::cout<<'\n'<<str;
      }
      break;

      case messageID::id2:
      {
        ::std::set<int32_t> iset;
        Give(buf,iset);
        for(auto val:iset){::std::cout<<val<<' ';}
      }
      break;

      case messageID::id3:
      {
        ::std::array<std::array<float,2>,3> a;
        Give(buf,a);
        for(auto subarray:a){
          for(auto val:subarray){::std::cout<<val<<' ';}
          ::std::cout<<'\n';
        }
      }
      break;

      case messageID::id4:
      {
        ::plf::colony<::std::string> clny;
        Give(buf,clny);
        for(auto val:clny){::std::cout<<val<<' ';}
      }
      break;

      default:
        ::std::cout<<"Unexpected message id: "<<msgid;
    }
    ::std::cout<<::std::endl;
  }
}catch(std::exception const& e){
  ::std::cout<<"failure: "<<e.what()<<'\n';
}
