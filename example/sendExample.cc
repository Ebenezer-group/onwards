//  This program sends a message and then exits.

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
  ::cmw::GetaddrinfoWrapper ai(
#ifdef __linux__
                    "127.0.0.1"
#else
                    "::1"
#endif
                    ,"12345",SOCK_DGRAM);
  ::cmw::BufferStack<::cmw::SameFormat> buf(ai.getSock());

  ::std::cout<<"Enter the ID of the message to send: 0,1 or 2.\n";
  int msgID;
  ::std::cin>>msgID;
  switch(msgID){
    case messageID::id1:
    {
      ::std::vector<int32_t> vec {100,97,94,91,88,85};
      exampleMessages::marshal<messageID::id1>(buf,vec,"Proverbs 24:27");
    }
    break;

    case messageID::id2:
    {
      ::std::set<int32_t> iset {100,97,94,91,88,85};
      exampleMessages::marshal<messageID::id2>(buf,iset);
    }
    break;

    case messageID::id3:
    {
      ::std::array<::std::array<float,2>, 3> ar {{ {1.1f,2.2f}
                                                  ,{3.3f,4.4f}
                                                  ,{5.5f,6.6f}
                                                }};
      exampleMessages::marshal<messageID::id3>(buf,ar);
    }
    break;

    default:
      std::cout<<"unknown message id\n";
      return 0;
  }
  buf.send(ai()->ai_addr,ai()->ai_addrlen);
}catch(::std::exception& e){::std::cout<<"failure: "<<e.what()<<"\n";}
