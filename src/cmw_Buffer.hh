#ifndef CMW_BUFFER_HH
#define CMW_BUFFER_HH
#include"quicklz.c"
#include<charconv>
#include<cstdint>
#include<cstdio>//snprintf
#include<cstdlib>//exit
#include<cstring>//memcpy,memmove
#include<exception>
#include<initializer_list>
#include<limits>
#include<span>
#include<string>
#include<string_view>
#include<type_traits>
#include<utility>
static_assert(::std::numeric_limits<float>::is_iec559,"IEEE754");

#if defined(_MSC_VER)||defined(WIN32)||defined(_WIN32)||defined(__WIN32__)||defined(__CYGWIN__)
#include<ws2tcpip.h>
#define CMW_WINDOWS
#else
#include<errno.h>
#include<fcntl.h>//open
#include<netdb.h>
#include<sys/socket.h>
#include<sys/stat.h>//open
#include<sys/types.h>
#include<unistd.h>//chdir
#endif

template<class T>concept arithmetic=::std::is_arithmetic_v<T>;

namespace cmw{
class Failure:public ::std::exception{
  ::std::string st;
 public:
  explicit Failure (auto s):st(s){}
  void operator<< (::std::string_view v){(st+=" ")+=v;}

  void operator<< (::int64_t i){
    char b[32];
    *this<<::std::string_view(b,::std::snprintf(b,sizeof b,"%ld",i));
  }

  char const* what ()const noexcept{return st.data();}
};

void apps (auto&){}
void apps (auto& e,auto t,auto...ts){
  e<<t;
  apps(e,ts...);
}

template<class E=Failure>[[noreturn]]void raise (char const *s,auto...t){
  E e{s};
  apps(e,t...);
  throw ::std::move(e);
}

inline void winStart (){
#ifdef CMW_WINDOWS
  WSADATA w;
  if(auto r=::WSAStartup(MAKEWORD(2,2),&w);0!=r)raise("WSAStartup",r);
#endif
}

inline int getError (){
#ifdef CMW_WINDOWS
  return WSAGetLastError();
#else
  return errno;
#endif
}

inline int fromChars (::std::string_view s){
  int n;
  ::std::from_chars(s.data(),s.data()+s.size(),n);
  return n;
}


class SendBuffer;
template<class>class ReceiveBuffer;
class MarshallingInt{
  ::int32_t val=0;
 public:
  MarshallingInt ()=default;
  explicit MarshallingInt (::int32_t v):val{v}{}
  explicit MarshallingInt (::std::string_view v):val{fromChars(v)}{}

  //Reads a sequence of bytes in variable-length format and
  //builds a 32 bit integer.
  template<class R>explicit MarshallingInt (ReceiveBuffer<R>& b){
    ::uint32_t shift=1;
    for(;;){
      ::uint8_t a=b.giveOne();
      val+=(a&127)*shift;
      if((a&128)==0)return;
      shift<<=7;
      val+=shift;
    }
  }

  void operator= (::int32_t r){val=r;}
  void operator+= (::int32_t r){val+=r;}
  auto operator() ()const{return val;}
  void marshal (SendBuffer& b)const;
};
inline bool operator== (MarshallingInt l,MarshallingInt r){return l()==r();}
inline bool operator== (MarshallingInt l,::int32_t r){return l()==r;}

inline void exitFailure (){::std::exit(EXIT_FAILURE);}
#ifdef CMW_WINDOWS
using sockType=SOCKET;
#else
using sockType=int;
inline void setDirectory (char const *d){
  if(::chdir(d)==-1)raise("setDirectory",d,errno);
}

inline int Write (int fd,void const *data,int len){
  if(int r=::write(fd,data,len);r>=0)return r;
  raise("Write",errno);
}

inline int Read (int fd,void *data,int len){
  int r=::read(fd,data,len);
  if(r>0)return r;
  if(r==0)raise("Read eof",len);
  if(EAGAIN==errno||EWOULDBLOCK==errno)return 0;
  raise("Read",len,errno);
}

class FileWrapper{
  int d=-2;
 public:
  FileWrapper (){}
  FileWrapper (char const *name,int flags,mode_t mode):
        d{::open(name,flags,mode)} {if(d<0)raise("FileWrapper",name,errno);}

  FileWrapper (char const *name,mode_t mode):
        FileWrapper(name,O_CREAT|O_WRONLY|O_TRUNC,mode){}

  FileWrapper (FileWrapper const&)=delete;
  FileWrapper (FileWrapper&& o)noexcept:d{o.d}{o.d=-2;}

  FileWrapper& operator= (FileWrapper&& o)noexcept{
    d=o.d;
    o.d=-2;
    return *this;
  }
  auto operator() (){return d;}
  ~FileWrapper (){::close(d);}
};

void getFile (char const *n,auto& b){
  b.giveFile(FileWrapper{n,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH}());
}

struct FileBuffer{
  FileWrapper fl;
  char buf[4096];
  char line[120];
  int ind=0;
  int bytes=0;

  FileBuffer (char const* name,int flags):fl(name,flags,0){}

  char getc (){
    if(ind>=bytes){
      bytes=Read(fl(),buf,sizeof buf);
      ind=0;
    }
    return buf[ind++];
  }

  auto getline (char='\n');
};

inline auto FileBuffer::getline (char delim){
  ::std::size_t idx=0;
  while((line[idx]=getc())!=delim){
    if(line[idx]=='\r')raise("getline carriage return");
    if(++idx>=sizeof line)raise("getline line too long");
  }
  line[idx]=0;
  return ::std::string_view{line,idx};
}
#endif

auto setsockWrapper (sockType s,int opt,auto t){
  return ::setsockopt(s,SOL_SOCKET,opt,reinterpret_cast<char*>(&t),sizeof t);
}

inline void setRcvTimeout (sockType s,int time){
#ifdef CMW_WINDOWS
  DWORD t=time*1000;
#else
  ::timeval t{time,0};
#endif
  if(setsockWrapper(s,SO_RCVTIMEO,t)!=0)raise("setRcvTimeout",getError());
}

inline void closeSocket (sockType s){
#ifdef CMW_WINDOWS
  if(::closesocket(s)==SOCKET_ERROR)
#else
  if(::close(s)==-1)
#endif
    raise("closeSocket",getError());
}

inline int preserveError (sockType s){
  auto e=getError();
  closeSocket(s);
  return e;
}

class GetaddrinfoWrapper{
  ::addrinfo *head,*addr;
 public:
  GetaddrinfoWrapper (char const *node,char const *port
                      ,int type,int flags=0){
    ::addrinfo const hints{flags,AF_UNSPEC,type,0,0,0,0,0};
    if(int r=::getaddrinfo(node,port,&hints,&head);r!=0)
      raise("getaddrinfo",::gai_strerror(r));
    addr=head;
  }

  ~GetaddrinfoWrapper (){::freeaddrinfo(head);}
  auto& operator() (){return *addr;}
  void inc (){if(addr!=nullptr)addr=addr->ai_next;}

  sockType getSock (){
    for(;addr!=nullptr;addr=addr->ai_next){
      if(auto s=::socket(addr->ai_family,addr->ai_socktype,0);-1!=s)return s;
    }
    raise("getaddrinfo getSock");
  }

  GetaddrinfoWrapper (GetaddrinfoWrapper&)=delete;
  GetaddrinfoWrapper& operator= (GetaddrinfoWrapper)=delete;
};

inline sockType connectWrapper (char const *node,char const *port){
  GetaddrinfoWrapper ai{node,port,SOCK_STREAM};
  auto s=ai.getSock();
  if(0==::connect(s,ai().ai_addr,ai().ai_addrlen))return s;
  errno=preserveError(s);
  return -1;
}

inline sockType udpServer (char const *port){
  GetaddrinfoWrapper ai{nullptr,port,SOCK_DGRAM,AI_PASSIVE};
  auto s=ai.getSock();
  if(0==::bind(s,ai().ai_addr,ai().ai_addrlen))return s;
  raise("udpServer",preserveError(s));
}

inline sockType tcpServer (char const *port){
  GetaddrinfoWrapper ai{nullptr,port,SOCK_STREAM,AI_PASSIVE};
  auto s=ai.getSock();

  if(int on=1;setsockWrapper(s,SO_REUSEADDR,on)==0
    &&::bind(s,ai().ai_addr,ai().ai_addrlen)==0
    &&::listen(s,SOMAXCONN)==0)return s;
  raise("tcpServer",preserveError(s));
}

inline int sockWrite (sockType s,void const *data,int len
               ,sockaddr const *addr=nullptr,socklen_t toLen=0){
  if(int r=::sendto(s,static_cast<char const*>(data),len,0,addr,toLen);r>0)
    return r;
  raise("sockWrite",s,getError());
}

inline int sockRead (sockType s,void *data,int len,sockaddr *addr,socklen_t *fromLen){
  int r=::recvfrom(s,static_cast<char*>(data),len,0,addr,fromLen);
  if(r>=0)return r;
  auto e=getError();
  if(EAGAIN==e||EWOULDBLOCK==e
#ifdef CMW_WINDOWS
     ||WSAETIMEDOUT==e
#endif
  )return 0;
  raise("sockRead",len,e);
}

struct SameFormat{
  static void read (auto& b,auto& data){b.give(&data,sizeof(data));}

  static void readBlock (auto& b,auto data,int elements)
  {b.give(data,elements*sizeof(*data));}
};

struct MixedEndian{
  static void read (auto& b,float& f){
    ::uint32_t tmp;
    read(b,tmp);
    ::std::memcpy(&f,&tmp,sizeof f);
  }

  static void read (auto& b,double& d){
    ::uint64_t tmp;
    read(b,tmp);
    ::std::memcpy(&d,&tmp,sizeof d);
  }

  static void readBlock (auto& b,auto data,int elements){
    for(;elements>0;--elements){read(b,*data++);}
  }
};

struct LeastSignificantFirst:MixedEndian{
  static void read (auto& b,auto& val)
  {for(auto c=0;c<sizeof(val);++c)val|=b.giveOne()<<8*c;}
};

struct MostSignificantFirst:MixedEndian{
  static void read (auto& b,auto& val)
  {for(auto c=sizeof(val);c>0;--c)val|=b.giveOne()<<8*(c-1);}
};

template<class R>class ReceiveBuffer{
  int msgLength=0;
  int subTotal=0;
 protected:
  char* const rbuf;
  int packetLength=0;
  int rindex=0;

 public:
  explicit ReceiveBuffer (char *addr):rbuf{addr}{}

  void checkLen (int n){
    if(n>msgLength-rindex)raise("ReceiveBuffer checkLen",n,msgLength,rindex);
  }

  void give (void *address,int len){
    checkLen(len);
    ::std::memcpy(address,rbuf+subTotal+rindex,len);
    rindex+=len;
  }

  char giveOne (){
    checkLen(1);
    return rbuf[subTotal+rindex++];
  }

  template<class T>T give (){
    T t;
    if(sizeof(T)==1)give(&t,1);
    else R::read(*this,t);
    return t;
  }

  bool nextMessage (){
    subTotal+=msgLength;
    if(subTotal<packetLength){
      rindex=0;
      msgLength=sizeof(::int32_t);
      msgLength=give<::uint32_t>();
      return true;
    }
    return false;
  }

  bool update (){
    msgLength=subTotal=0;
    return nextMessage();
  }

  void giveBlock (auto *data,unsigned int elements){
    if(sizeof(*data)==1)give(data,elements);
    else R::readBlock(*this,data,elements);
  }

#ifndef CMW_WINDOWS
  void giveFile (int d){
    int sz=give<::uint32_t>();
    checkLen(sz);
    while(sz>0){
      int r=Write(d,rbuf+subTotal+rindex,sz);
      sz-=r;
      rindex+=r;
    }
  }
#endif

  template<class T>requires arithmetic<T>
  auto giveSpan (){
    ::int32_t sz=give<::uint32_t>();
    ::int32_t serLen=sizeof(T)*sz;
    checkLen(serLen);
    ::std::span<T> s(reinterpret_cast<T*>(rbuf+subTotal+rindex),sz);
    rindex+=serLen;
    return s;
  }

  auto giveStringView (){
    MarshallingInt len{*this};
    checkLen(len());
    ::std::string_view s(rbuf+subTotal+rindex,len());
    rindex+=len();
    return s;
  }
};

template<class T>T give (auto& b){return b.template give<T>();}

bool giveBool (auto& b){
  switch(b.giveOne()){
    case 0:return false;
    case 1:return true;
    default:raise("giveBool");
  }
}

class SendBuffer{
  SendBuffer (SendBuffer const&)=delete;
  SendBuffer& operator= (SendBuffer);
  ::int32_t savedSize=0;
 protected:
  int index=0;
  int const bufsize;
  unsigned char* const buf;
 public:
  sockType sock_=-1;

  SendBuffer (unsigned char *addr,int sz):bufsize(sz),buf(addr){}

  int reserveBytes (int n);

  void receive (void const *data,int size){
    ::std::memcpy(buf+reserveBytes(size),data,size);
  }

  template<class T>requires arithmetic<T>||::std::is_enum_v<T>
  void receive (T t){
    receive(&t,sizeof t);
  }

  auto receive (int where,arithmetic auto t){
    ::std::memcpy(buf+where,&t,sizeof t);
    return t;
  }

  void fillInSize (::int32_t max);

  void reset (){savedSize=index=0;}
  void rollback (){index=savedSize;}

  void receiveFile (char const*,::int32_t sz);
  bool flush ();

  //UDP-friendly alternative to flush
  void send (::sockaddr *addr=nullptr,::socklen_t len=0)
  {sockWrite(sock_,buf,index,addr,len);}

  auto data (){return buf;}
  int getIndex (){return index;}
  int getSize (){return bufsize;}
  void receiveMulti (char const*,auto...);
};

inline int SendBuffer::reserveBytes (int n){
  if(n>bufsize-index)raise("SendBuffer checkSpace",n,index);
  auto i=index;
  index+=n;
  return i;
}

inline void SendBuffer::fillInSize (::int32_t max){
  ::int32_t marshalledBytes=index-savedSize;
  if(marshalledBytes>max)raise("fillInSize",max);
  receive(savedSize,marshalledBytes);
  savedSize=index;
}

#ifndef CMW_WINDOWS
inline void SendBuffer::receiveFile (char const* n,::int32_t sz){
  receive(sz);
  auto prev=reserveBytes(sz);
  FileWrapper fl{n,O_RDONLY,0};
  if(Read(fl(),buf+prev,sz)!=sz)raise("SendBuffer receiveFile");
}
#endif

inline bool SendBuffer::flush (){
  int const bytes=sockWrite(sock_,buf,index);
  if(bytes==index){reset();return true;}

  index-=bytes;
  savedSize=index;
  ::std::memmove(buf,buf+bytes,index);
  return false;
}

inline void receiveBool (SendBuffer&b,bool bl){b.receive<unsigned char>(bl);}

inline void receive (SendBuffer& b,::std::string_view s){
  MarshallingInt(s.size()).marshal(b);
  b.receive(s.data(),s.size());
}

using stringPlus=::std::initializer_list<::std::string_view>;
inline void receive (SendBuffer& b,stringPlus lst){
  ::int32_t t=0;
  for(auto s:lst)t+=s.size();
  MarshallingInt{t}.marshal(b);
  for(auto s:lst)b.receive(s.data(),s.size());//Use low-level receive
}

template<template<class...>class C,class T>
void receiveBlock (SendBuffer& b,C<T>const& c){
  ::int32_t n=c.size();
  b.receive(n);
  if constexpr(arithmetic<T>){
    if(n>0)b.receive(c.data(),n*sizeof(T));
  }else if constexpr(::std::is_pointer_v<T>)for(auto e:c)e->marshal(b);
  else for(auto const& e:c)e.marshal(b);
}

//Encode integer into variable-length format.
inline void MarshallingInt::marshal (SendBuffer& b)const{
  ::uint32_t n=val;
  for(;;){
    ::uint8_t a=n&127;
    n>>=7;
    if(0==n){b.receive(a);break;}
    a|=128;
    b.receive(a);
    --n;
  }
}

inline constexpr auto udpPacketMax=1500;
template<class R,int N=udpPacketMax>
class BufferStack:public SendBuffer,public ReceiveBuffer<R>{
  unsigned char ar[N];
 public:
  BufferStack ():SendBuffer(ar,N),ReceiveBuffer<R>((char*)ar){}
  explicit BufferStack (int s):BufferStack(){sock_=s;}

  bool getPacket (::sockaddr *addr=nullptr,::socklen_t *len=nullptr){
    this->packetLength=sockRead(sock_,ar,N,addr,len);
    return this->update();
  }
};

#ifndef CMW_WINDOWS
auto myMin (auto a,auto b){return a<b?a:b;}
constexpr auto qlzFormula (int i){return i+(i>>3)+400;}

template<class R,int sz>class BufferCompressed:public SendBuffer,public ReceiveBuffer<R>{
 public:
  ::qlz_state_compress comp;
  ::qlz_state_decompress decomp;
  char *compressedStart;
  char compBuf[qlzFormula(sz)];
  char recBuf[sz];
  int compPacketSize;
  int compIndex=0;
  int bytesRead=0;
  bool kosher=true;

  bool all (int bytes){
    if(bytes==compIndex){
      compIndex=0;
      return true;
    }
    compIndex-=bytes;
    ::std::memmove(compBuf,compBuf+bytes,compIndex);
    return false;
  }

  bool doFlush (){
    return all(Write(sock_,compBuf,compIndex));
  }
 public:
  explicit BufferCompressed ():SendBuffer(new unsigned char[sz],sz),ReceiveBuffer<R>(recBuf){}
  ~BufferCompressed (){delete[]buf;}

  void compress (){
    if(qlzFormula(index)>(qlzFormula(sz)-compIndex))
      raise("Not enough room in compressed buf");
    compIndex+=::qlz_compress(buf,compBuf+compIndex,index,&comp);
    reset();
  }

  bool flush (){
    bool rc=true;
    if(compIndex>0)rc=doFlush();

    if(index>0){
      compress();
      if(rc)rc=doFlush();
    }
    return rc;
  }

  void compressedReset (){
    reset();
    comp={};
    decomp={};
    compIndex=bytesRead=0;
    kosher=true;
    closeSocket(sock_);
  }

  using ReceiveBuffer<R>::rbuf;
  bool gotIt (int rc){
    if((bytesRead+=rc)<9)return false;
    if(bytesRead==9){
      if((compPacketSize=::qlz_size_compressed(rbuf))>bufsize||
         (this->packetLength=::qlz_size_decompressed(rbuf))>bufsize){
        kosher=false;
        raise("gotIt too big",compPacketSize,this->packetLength,bufsize);
      }
      compressedStart=rbuf+bufsize-compPacketSize;
      ::std::memmove(compressedStart,rbuf,9);
      return false;
    }

    if(bytesRead<compPacketSize)return false;
    ::qlz_decompress(compressedStart,rbuf,&decomp);
    bytesRead=0;
    this->update();
    return true;
  }

  auto getDuo (){
    return bytesRead<9?::std::span<char>(rbuf+bytesRead,9-bytesRead):
                       ::std::span<char>(compressedStart+bytesRead,compPacketSize-bytesRead);
  }

  bool gotPacket (){
    if(kosher){
      auto sp=getDuo();
      return gotIt(Read(sock_,sp.data(),sp.size()));
    }
    bytesRead+=Read(sock_,rbuf,myMin(bufsize,compPacketSize-bytesRead));
    if(bytesRead==compPacketSize){
      kosher=true;
      bytesRead=0;
    }
    return false;
  }
};
#endif

template<int N>class FixedString{
  char str[N];
  MarshallingInt len{};

 public:
  FixedString ()=default;

  explicit FixedString (::std::string_view s):len(s.size()){
    if(len()>=N)raise("FixedString ctor");
    ::std::memcpy(str,s.data(),len());
    str[len()]=0;
  }

  template<class R>
  explicit FixedString (ReceiveBuffer<R>& b):len(b){
    if(len()>=N)raise("FixedString stream ctor");
    b.give(str,len());
    str[len()]=0;
  }

  FixedString (FixedString const& o):len(o.len()){
    ::std::memcpy(str,o.str,len());
  }

  void operator= (::std::string_view s){
    len=s.size();
    if(len()>=N)raise("FixedString operator=");
    ::std::memcpy(str,s.data(),len());
  }

  void marshal (SendBuffer& b)const{
    len.marshal(b);
    b.receive(str,len());
  }

  unsigned int bytesAvailable ()const{return N-(len()+1);}

  char const* append (::std::string_view s){
    if(bytesAvailable()>=s.size()){
      ::std::memcpy(str+len(),s.data(),s.size());
      len+=s.size();
      str[len()]=0;
    }
    return str;
  }

  char* operator() (){return str;}
  char const* data ()const{return str;}
  char operator[] (int i)const{return str[i];}
};
using FixedString60=FixedString<60>;
using FixedString120=FixedString<120>;
}
#endif
