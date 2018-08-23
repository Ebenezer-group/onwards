#pragma once
//Code generated by the C++ Middleware Writer version 1.14.
#include<cmw/Buffer.hh>
#include<array>
#include<plf_colony.h>
#include<set>
#include<string>
#include<vector>

namespace sendMessages{
inline void Marshal (::cmw::SendBuffer& buf
         ,messageID_8 a
         ,::std::vector<int32_t> const& b
         ,::std::string const& c){
  try{
    buf.ReserveBytes(4);
    buf.Receive(a);
    ReceiveBlock(buf,b);
    Receive(buf,c);
    buf.FillInSize(10000);
  }catch(...){buf.Rollback();throw;}
}

inline void Marshal (::cmw::SendBuffer& buf
         ,messageID_8 a
         ,::std::set<int32_t> const& b){
  try{
    buf.ReserveBytes(4);
    buf.Receive(a);
    buf.Receive(static_cast<int32_t>(b.size()));
    for(auto const& i1:b){
      buf.Receive(i1);
    }
    buf.FillInSize(10000);
  }catch(...){buf.Rollback();throw;}
}

inline void Marshal (::cmw::SendBuffer& buf
         ,messageID_8 a
         ,::std::array<::std::array<float, 2>, 3> const& b){
  try{
    buf.ReserveBytes(4);
    buf.Receive(a);
    buf.Receive(&b,sizeof b);
    buf.FillInSize(10000);
  }catch(...){buf.Rollback();throw;}
}

inline void Marshal (::cmw::SendBuffer& buf
         ,messageID_8 a
         ,::plf::colony<::std::string> const& b){
  try{
    buf.ReserveBytes(4);
    buf.Receive(a);
    buf.Receive(static_cast<int32_t>(b.size()));
    for(auto const& i2:b){
      Receive(buf,i2);
    }
    buf.FillInSize(10000);
  }catch(...){buf.Rollback();throw;}
}
}
