//  The following Middle code was used as input to the C++
//  Middleware Writer.
//
//  exampleMessages
//     -out -in (::std::vector<int32_t>,::std::string)
//     -out -in (::std::set<int32_t>)
//     -out -in (::std::array<::std::array<float,2>, 3>)
//  }

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
  ::cmw::getaddrinfoWrapper ai(
#ifdef __linux__
                    "127.0.0.1"
#else
                    "::1"
#endif
                    ,"12345",SOCK_DGRAM);
  ::cmw::BufferStack<::cmw::SameFormat> buf(ai.getSock());

  ::std::cout<<"Enter the ID of the message to send: 0,1,2,or 3.\n";
  int msgID;
  ::std::cin>>msgID;
  using namespace ::exampleMessages;
  switch(msgID){
    case messageID::id1:
    {
      ::std::vector<int32_t> vec {100,97,94,91,88,85};
      marshal<messageID::id1>(buf,vec,"Proverbs 24:27");
    }
    break;

    case messageID::id2:
    {
      ::std::set<int32_t> iset {100,97,94,91,88,85};
      marshal<messageID::id2>(buf,iset);
    }
    break;

    case messageID::id3:
    {
      ::std::array<::std::array<float,2>, 3> ar {{ {1.1f,2.2}
                                                  ,{3.3,4.4}
                                                  ,{5.5,6.6}
                                                }};
      marshal<messageID::id3>(buf,ar);
    }
    break;

    case messageID::id4:
    {
      ::plf::colony<::std::string> clny{"Beautiful words ","wonderful words ","of life"};
      marshal<messageID::id4>(buf,clny);
    }
    break;

    default:
      std::cout<<"unknown message id\n";
      return 0;
  }
  buf.send(ai()->ai_addr,ai()->ai_addrlen);
}catch(::std::exception& e){::std::cout<<"failure: "<<e.what()<<"\n";}
