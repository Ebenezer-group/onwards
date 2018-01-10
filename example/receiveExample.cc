//  This program receives messages sent by sendExample.

#include <Buffer.hh>
#include "messageIDs.hh"
#include <plf_colony.h>
#include <wrappers.hh>
#include "zz.receiveMessages.hh"

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
      auto const msgid=Give<message_id_8>(buffer);
      ::std::cout<<"Message id: " << static_cast<unsigned>(msgid)<<'\n';
      switch(msgid){
      case messageid1:
      {
        ::std::vector<int32_t> vec;
        ::std::string str;
        receiveMessages::Give(buffer,vec,str);
        for(auto val:vec){::std::cout<<val<<" ";}
        ::std::cout<<'\n'<<str;
      }
      break;

      case messageid2:
      {
        ::std::set<int32_t> iset;
        receiveMessages::Give(buffer,iset);
        for(auto val:iset){::std::cout<<val<<" ";}
      }
      break;

      case messageid3:
      {
        ::std::array<std::array<float,2>, 3> a;
        receiveMessages::Give(buffer,a);
        for(auto subarray:a){
          for(auto val:subarray){::std::cout<<val<<" ";}
          ::std::cout<<::std::endl;
        }
      }
      break;

      case messageid4:
      {
        ::plf::colony<::std::string> clny;
        receiveMessages::Give(buffer,clny);
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
