#ifndef CMW_COMPLEX_HH
#define CMW_COMPLEX_HH
#include<cmwBuffer.hh>
#include<complex>

namespace cmw{
void complexMarshal (auto& buf,auto const& c){
  buf.receive(c.real());
  buf.receive(c.imag());
}

template<class T>
auto complexGive (auto& buf){
  //The following has an order of evaluation problem:
  //::std::complex<T>(give<T>(buf),give<T>(buf));
  T rl=give<T>(buf);
  return ::std::complex<T>(rl,give<T>(buf));
}

template<class T,class...Ts>
void marshalSegments (auto& c,auto& buf,auto& segs){
  if(c.template is_registered<T>()){
    if(::int32_t n=c.template size<T>();n>0){
      ++segs;
      buf.receive(T::typeNum);
      buf.receive(n);
      for(T const& t:c.template segment<T>()){t.marshal(buf);}
    }
  }
  if constexpr(sizeof...(Ts)>0)return marshalSegments<Ts...>(c,buf,segs);
  return;
}

template<class...Ts>void marshalCollection (auto& c,auto& buf){
  auto const ind=buf.reserveBytes(1);
  ::uint8_t segs=0;
  marshalSegments<Ts...>(c,buf,segs);
  buf.receiveAt(ind,segs);
}

template<class T>
void buildSegment (auto& c,auto& buf){
  ::int32_t n=give<::uint32_t>(buf);
  c.template reserve<T>(n);
  for(;n>0;--n){c.template emplace<T>(buf);}
}

void buildCollection (auto& c,auto& buf,auto f){
  for(auto segs=give<::uint8_t>(buf);segs>0;--segs){f(c,buf);}
}

#ifdef CMW_VALARRAY_MARSHALLING
#include<valarray>
void valarrayMarshal (auto& buf,auto const& varray){
  int32_t count=varray.size();
  buf.receive(count);
  buf->receive(&varray[0],count*sizeof(T));
}

void valarrayGive (auto& buf,auto& varray){
  int32_t count=give<uint32_t>(buf);
  varray.resize(count);
  buf.give(&varray,count*sizeof(T));
}
#endif
}
#endif
