//   This program receives messages sent by sendexample.
//   The Middle code is the same as in the send example.
//

#include <array>
#include <iostream>
#include <ReceiveBufferStack.hh>
#include <set>
#include <string>
#include <udp_stuff.hh>
#include <vector>
#include <zz.receive_example_messages.hh>

int main()
{
  try {
    auto sd = cmw::udp_server("12345");

    for (;;) {
      cmw::ReceiveBufferStack<SameFormat> buffer(sd); 
      auto msgid = buffer.Give<messageid_t>();
      switch (msgid) {
      case messageid1:
      {
        std::vector<int32_t> vec;
        std::string str;
        receive_example_messages::Give(buffer, vec, str);  
        for (auto val : vec) { std::cout << val << " "; }
        std::cout << "\n" << str;
      }
      break;
      
      case messageid2:
      {
        std::set<int32_t> iset;
        receive_example_messages::Give(buffer, iset);   
        for (auto val : iset) { std::cout << val << " "; }
      }
      break;

      case messageid3:
      {
        std::array<std::array<float, 2>, 3> a;
        receive_example_messages::Give(buffer, a);   
        for (auto subarray : a) {
          for (auto val : subarray) { std::cout << val << " "; }
          std::cout << std::endl;
        }
      }
      break;

      default:
        std::cout << "Unexpected message id: " << msgid;
      }
      std::cout << std::endl;            
    }
    return 1;
  } catch (std::exception const& ex) {
    std::cout << "failure: " << ex.what() << std::endl;
  }
}

