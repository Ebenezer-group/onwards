//  This program sends a message and then exits.

#include<cmwBuffer.hh>
#include"messageIDs.hh"

#include<array>
#include<iostream>
#include<set>
#include<string>
#include<vector>
#include"example.mdl.hh"

int main ()try{
  ::cmw::winStart();
  ::cmw::SockaddrWrapper sa("127.0.0.1", 12345);
  ::cmw::BufferStack<::cmw::SameFormat> buf(::socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP));

  ::std::cout<<"Enter the ID of the message to send: 0,1 or 2.\n";
  int msgID;
  ::std::cin>>msgID;
  switch(msgID){
  case static_cast<::std::underlying_type_t<messageID>>(messageID::id1):
    {
      ::std::vector<int32_t> vec {100,97,94,91,88,85};
      exampleMessages::marshal<messageID::id1>(buf,vec,"Proverbs 24:27");
    }
    break;

  case static_cast<::std::underlying_type_t<messageID>>(messageID::id2):
    {
      ::std::set<int32_t> iset {100,97,94,91,88,85};
      exampleMessages::marshal<messageID::id2>(buf,iset);
    }
    break;

  case static_cast<::std::underlying_type_t<messageID>>(messageID::id3):
    {
      ::std::array<float,6> ar {{ 1.1f, 2.2f ,3.3f, 4.4f,5.5f, 6.6f }};
      exampleMessages::marshal<messageID::id3>(buf,ar);
    }
    break;

  default:
    std::cout<<"unknown message id\n";
    return 0;
  }
  buf.send((sockaddr*)&sa,sizeof(sa));
}catch(::std::exception& e){::std::cout<<"failure: "<<e.what()<<"\n";}
