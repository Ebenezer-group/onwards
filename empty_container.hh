#include"ReceiveBuffer.hh"

template <class T>
class empty_container{

public:
template <class R>
explicit empty_container (::cmw::ReceiveBuffer<R>& buf){
  int32_t count=buf.template Give<uint32_t>();
  for(;count>0;--count)T{buf};
}
};
