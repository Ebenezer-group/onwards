//  This program receives messages sent by sendExample.
#include<cmw_BufferImpl.hh>
#include"messageIDs.hh"

#include<array>
#include<iostream>
#include<set>
#include<string>
#include<vector>
#include"example.mdl.hh"

int main ()try{
  ::cmw::winStart();
  ::cmw::BufferStack<::cmw::SameFormat> buf(::cmw::udpServer("12345"));

  for(;;){
    buf.getPacket();
    auto const msgid=::cmw::give<messageID>(buf);
    ::std::cout<<"Message id: "<<static_cast<unsigned>(msgid)<<'\n';
    switch(msgid){
      case messageID::id1:
      {
        ::std::vector<::int32_t> vec;
        ::std::string str;
	exampleMessages::give(buf,vec,str);
        for(auto v:vec){::std::cout<<v<<' ';}
        ::std::cout<<'\n'<<str;
      }
      break;

      case messageID::id2:
      {
        ::std::set<::int32_t> iset;
	exampleMessages::give(buf,iset);
        for(auto v:iset){::std::cout<<v<<' ';}
      }
      break;

      case messageID::id3:
      {
        ::std::array<::std::array<float,2>,3> a;
	exampleMessages::give(buf,a);
        for(auto subarray:a){
          for(auto v:subarray){::std::cout<<v<<' ';}
          ::std::cout<<'\n';
        }
      }
      break;

      default:
        ::std::cout<<"Unexpected message id: "<<msgid;
    }
    ::std::cout<<::std::endl;
  }
}catch(std::exception& e){
  ::std::cout<<"failure: "<<e.what()<<'\n';
}
