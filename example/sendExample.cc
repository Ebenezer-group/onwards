//   The following Middle code was used as input to the C++
//   Middleware Writer.
//
//   sendMessages
//      -out (messageID_8, ::std::vector<int32_t>, ::std::string)
//      -out (messageID_8, ::std::set<int32_t>)
//      -out (messageID_8, ::std::array<::std::array<float,2>, 3>)
//   }
//
//   receiveMessages
//      -in                (::std::vector<int32_t>, ::std::string)
//      -in                (::std::set<int32_t>)
//      -in                (::std::array<::std::array<float,2>, 3>)
//   }

#include<cmw/Buffer.hh>
#include"messageIDs.hh"
#include"plf_colony.h"

#include<array>
#include<iostream>
#include<set>
#include<string>
#include<vector>
#include"zz.sendMessages.hh"
using namespace ::cmw;
using namespace ::sendMessages;

int main ()try{
  winStart();
  getaddrinfoWrapper ai(
#ifdef __linux__
                    "127.0.0.1"
#else
                    "::1"
#endif
                    ,"12345",SOCK_DGRAM);
  BufferStack<SameFormat> buf(ai.getSock());

  ::std::cout<<"Enter the ID of the message to send: 1,2,3 or 4.\n";
  int messageID;
  ::std::cin>>messageID;
  switch(messageID){
    case messageid1:
    {
      ::std::vector<int32_t> vec {100,97,94,91,88,85};
      Marshal(buf,messageid1,vec,"Proverbs 24:27");
    }
    break;

    case messageid2:
    {
      ::std::set<int32_t> iset {100,97,94,91,88,85};
      Marshal(buf,messageid2,iset);
    }
    break;

    case messageid3:
    {
      ::std::array<::std::array<float,2>, 3> ar {{ {{1.1f,2.2}}
                                                  ,{{3.3,4.4}}
                                                  ,{{5.5,6.6}}
                                                }};
      Marshal(buf,messageid3,ar);
    }
    break;

    case messageid4:
    {
      ::plf::colony<::std::string> clny{"Beautiful words ","wonderful words ","of life"};
      Marshal(buf,messageid4,clny);
    }
    break;

    default:
      return 0;
  }
  buf.Send(ai()->ai_addr,ai()->ai_addrlen);
  return 1;
}catch(::std::exception const& e){::std::cout<<"failure: "<<e.what()<<"\n";}
