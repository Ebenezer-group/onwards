#ifndef CMW_Complex_hh
#define CMW_Complex_hh 1
#include<cmw/Buffer.hh>
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

template<class C>int32_t marshalSegments (C&,SendBuffer&,uint8_t){return 0;}

template<class T,class... Ts,class C>
int32_t marshalSegments (C& c,SendBuffer& buf,uint8_t& segs){
  int32_t n;
  if(c.template is_registered<T>()){
    n=c.template size<T>();
    if(n>0){
      ++segs;
      buf.Receive(T::typeNum);
      buf.Receive(n);
      for(T const& t:c.template segment<T>()){t.marshal(buf);}
    }
  }else n=0;
  return n+marshalSegments<Ts...>(c,buf,segs);
}

template<class...Ts,class C>void marshalCollection (C& c,SendBuffer& buf){
  auto const ind=buf.reserveBytes(1);
  uint8_t segs=0;
  if(c.size()!=marshalSegments<Ts...>(c,buf,segs))raise("marshalCollection");
  buf.Receive(ind,segs);
}

template<class T,class C,class R>
void BuildSegment (C& c,ReceiveBuffer<R>& buf){
  int32_t n=Give<uint32_t>(buf);
  c.template reserve<T>(n);
  for(;n>0;--n){c.template emplace<T>(buf);}
}

template<class C,class R,class F>
void BuildCollection (C& c,ReceiveBuffer<R>& buf,F f){
  for(auto segs=Give<uint8_t>(buf);segs>0;--segs){f(c,buf);}
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
#endif
