#pragma once
// Code generated by the C++ Middleware Writer version 1.14.
#include"SendBuffer.hh"
#include"message_ids.hh"
#include<array>
#include<set>
#include<string>
#include<vector>
#include"plf_colony.h"

namespace send_example_messages{
inline void Marshal (::cmw::SendBuffer& buf
         ,messageid_t const& az1
         ,::std::vector<int32_t> const& az2
         ,::std::string const& az3
         ,int32_t max_length=10000){
  try{
    buf.ReserveBytes(4);
    buf.Receive(az1);
    buf.ReceiveBlock(az2);
    buf.Receive(az3);
    buf.FillInSize(max_length);
  }catch(...){buf.Rollback();throw;}
}

inline void Marshal (::cmw::SendBuffer& buf
         ,messageid_t const& az1
         ,::std::set<int32_t> const& az2
         ,int32_t max_length=10000){
  try{
    buf.ReserveBytes(4);
    buf.Receive(az1);
    buf.Receive(static_cast<int32_t>(az2.size()));
    for(auto const& it2:az2){
      buf.Receive(it2);
    }
    buf.FillInSize(max_length);
  }catch(...){buf.Rollback();throw;}
}

inline void Marshal (::cmw::SendBuffer& buf
         ,messageid_t const& az1
         ,::std::array<::std::array<float, 2>, 3> const& az2
         ,int32_t max_length=10000){
  try{
    buf.ReserveBytes(4);
    buf.Receive(az1);
    buf.Receive(&az2, sizeof az2);
    buf.FillInSize(max_length);
  }catch(...){buf.Rollback();throw;}
}

inline void Marshal (::cmw::SendBuffer& buf
         ,messageid_t const& az1
         ,::plf::colony<::std::string> const& az2
         ,int32_t max_length=10000){
  try{
    buf.ReserveBytes(4);
    buf.Receive(az1);
    buf.Receive(static_cast<int32_t>(az2.size()));
    for(auto const& it3:az2){
      buf.Receive(it3);
    }
    buf.FillInSize(max_length);
  }catch(...){buf.Rollback();throw;}
}
}
