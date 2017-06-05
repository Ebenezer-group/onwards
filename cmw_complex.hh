#pragma once
#include<ReceiveBuffer.hh>
#include<complex>

namespace cmw{
template <class T,class B>
void complexMarshal (B& buf,::std::complex<T> const& cmplx){
  buf.Receive(cmplx.real());
  buf.Receive(cmplx.imag());
}

template <class T,class B>
::std::complex<T> complexGive (B& buf){
  //The following suffers from an order of evaluation problem.
  //return ::std::complex<T>(Give<T>(buf),Give<T>(buf));
  T rl=Give<T>(buf);
  return ::std::complex<T>(rl,Give<T>(buf));
}

#ifdef CMW_VALARRAY_MARSHALLING
#include<valarray>
template <class B, class T>
void valarrayMarshal (B& buf,::std::valarray<T> const& valarr){
  int32_t count=valarr.size();
  buf.Receive(count);
  buf->Receive(&valarr[0],count*sizeof(T));
}

template <class B,class T>
void valarrayReceive (B& buf,::std::valarray<T>& valarr){
  int32_t count=Give<uint32_t>(buf);
  valarr.resize(count);
  buf.Give(&valarr,count*sizeof(T));
}
#endif
}
