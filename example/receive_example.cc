//  This program receives messages sent by sendexample.

#include "message_ids.hh"
#include <platforms.hh>
#include <plf_colony.h>
#include <ReceiveBuffer.hh>
#include <udpStuff.hh>
#include "zz.receive_messages.hh"

#include <array>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace ::cmw;

int main()
{
  try{
    windowsStart();
    auto sd=udpServer("12345");

    for(;;){
      ReceiveBufferStack<SameFormat> buffer(sd);
      auto const msgid=buffer.Give<message_id_8>();
      ::std::cout<<"Message id: " << static_cast<unsigned>(msgid)<<'\n';
      switch(msgid){
      case messageid1:
      {
        ::std::vector<int32_t> vec;
        ::std::string str;
        receive_messages::Give(buffer,vec,str);
        for(auto val:vec){::std::cout<<val<<" ";}
        ::std::cout<<'\n'<<str;
      }
      break;

      case messageid2:
      {
        ::std::set<int32_t> iset;
        receive_messages::Give(buffer,iset);
        for(auto val:iset){::std::cout<<val<<" ";}
      }
      break;

      case messageid3:
      {
        ::std::array<std::array<float,2>, 3> a;
        receive_messages::Give(buffer,a);
        for(auto subarray:a){
          for(auto val:subarray){::std::cout<<val<<" ";}
          ::std::cout<<::std::endl;
        }
      }
      break;

      case messageid4:
      {
        ::plf::colony<::std::string> clny;
        receive_messages::Give(buffer,clny);
        for(auto val:clny){::std::cout<<val<<" ";}
      }
      break;

      default:
        ::std::cout<<"Unexpected message id: "<<msgid;
      }
      ::std::cout<<::std::endl;
    }
    return 1;
  } catch(std::exception const& ex){
    ::std::cout << "failure: " << ex.what()<<::std::endl;
  }
}
