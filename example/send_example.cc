//   The following Middle code was used as input to the C++
//   Middleware Writer.
//
//   send_example_messages
//      -out (messageid_t, ::std::vector<int32_t>, ::std::string)
//      -out (messageid_t, ::std::set<int32_t>)
//      -out (messageid_t, ::std::array<::std::array<float, 2>, 3>)
//   }
//
//   receive_example_messages
//      -in (::std::vector<int32_t>, ::std::string)
//      -in (::std::set<int32_t>)
//      -in (::std::array<::std::array<float, 2>, 3>)
//   }

#include <getaddrinfo_wrapper.hh>
#include <platforms.hh>
#include "plf_colony.h"
#include <SendBufferStack.hh>
#include "zz.send_example_messages.hh"

#include <array>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using namespace cmw;

int main()
{
  try{
    windows_start();
    SendBufferStack<> buffer;
    //getaddrinfo_wrapper res("127.0.0.1", "12345", SOCK_DGRAM);
    getaddrinfo_wrapper res("::1", "12345", SOCK_DGRAM);
    auto rp = res.get();
    buffer.sock_ = ::socket(rp->ai_family, rp->ai_socktype, 0);

    ::std::cout << "Enter the ID of the message to send: 1, 2, 3 or 4." << ::std::endl;
    int messageID;
    ::std::cin >> messageID;
    if (messageID < 1 || messageID > 4) return 0;

    if (1 == messageID) {
      ::std::vector<int32_t> vec { 100, 97, 94, 91, 88, 85 };
      send_example_messages::Marshal(buffer, messageid1, vec, "Proverbs 24:27");
    } else if (2 == messageID) {
      ::std::set<int32_t> iset { 100, 97, 94, 91, 88, 85 };
      send_example_messages::Marshal(buffer, messageid2, iset);
    } else if (3 == messageID) {
      ::std::array<::std::array<float, 2>, 3> ar {{ {{1.1,2.2}}
						   ,{{3.3,4.4}}
                                                   ,{{5.5,6.6}}
                                                 }};
      send_example_messages::Marshal(buffer, messageid3, ar);
    } else {
      ::plf::colony<::std::string> clny { "Beautiful words ", "wonderful words ", "of life"};
      send_example_messages::Marshal(buffer, messageid4, clny);
    }

    buffer.Flush(rp->ai_addr, rp->ai_addrlen);
    return 1;
  } catch (::std::exception const& ex) {
    ::std::cout << "failure: " << ex.what() << ::std::endl;
  }
}

