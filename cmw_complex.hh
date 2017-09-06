#pragma once
#include<ReceiveBuffer.hh>
#include<complex>

namespace cmw{
template<class T,class B>
void complexMarshal (B& buf,::std::complex<T> const& c){
  buf.Receive(c.real());
  buf.Receive(c.imag());
}

template<class T,class B>
::std::complex<T> complexGive (B& buf){
  //The following has an order of evaluation problem:
  //return ::std::complex<T>(Give<T>(buf),Give<T>(buf));
  T rl=Give<T>(buf);
  return ::std::complex<T>(rl,Give<T>(buf));
}

#ifdef CMW_VALARRAY_MARSHALLING
#include<valarray>
template<class B, class T>
void valarrayMarshal (B& buf,::std::valarray<T> const& va){
  int32_t count=va.size();
  buf.Receive(count);
  buf->Receive(&va[0],count*sizeof(T));
}

template<class B,class T>
void valarrayReceive (B& buf,::std::valarray<T>& va){
  int32_t count=Give<uint32_t>(buf);
  va.resize(count);
  buf.Give(&va,count*sizeof(T));
}
#endif
}
