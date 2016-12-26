#pragma once

#include <complex>

namespace cmw {
template <class T,class B>
void complexMarshal (B& buf,::std::complex<T> const& cmplx)
{
  buf.Receive(cmplx.real());
  buf.Receive(cmplx.imag());
}

template <class T,class B>
::std::complex<T> complexGive (B& buf)
{
  // The following suffers from an order of evaluation problem.
  //return ::std::complex<T>(buf.template Give<T>(), buf.template Give<T>());
  T rl=buf.template Give<T>();
  return ::std::complex<T>(rl,buf.template Give<T>());
}

#ifdef CMW_VALARRAY_MARSHALLING
#include <valarray>
template <class B, class T>
void valarrayMarshal (B& buf,::std::valarray<T> const& valarr)
{
  int32_t headCount=valarr.size();
  buf.Receive(headCount);
  buf->Receive(&valarr[0],headCount*sizeof(T));
}

template <class B,class T>
void valarrayReceive (B& buf,::std::valarray<T>& valarr)
{
  int32_t headCount=buf.template Give<uint32_t>();
  valarr.resize(headCount);
  buf.Give(&valarr,headCount*sizeof(T));
}
#endif
}
