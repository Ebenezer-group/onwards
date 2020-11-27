#ifndef CMW_BUFFERIMPL_H
#define CMW_BUFFERIMPL_H
#include<cmw_Buffer.h>
#include<quicklz.c>
#include<charconv>//from_chars
#include<limits>
static_assert(::std::numeric_limits<unsigned char>::digits==8);
static_assert(::std::numeric_limits<float>::is_iec559,"IEEE754");
#ifndef CMW_WINDOWS
#include<errno.h>
#include<netdb.h>
#include<poll.h>
#include<stdlib.h>//exit
#include<unistd.h>
#endif

namespace cmw{
int getError (){
  return
#ifdef CMW_WINDOWS
    WSAGetLastError();
#else
    errno;
#endif
}

void winStart (){
#ifdef CMW_WINDOWS
  WSADATA w;
  if(auto r=::WSAStartup(MAKEWORD(2,2),&w);0!=r)raise("WSAStartup",r);
#endif
}

int fromChars (::std::string_view s){
  int n;
  ::std::from_chars(s.data(),s.data()+s.size(),n);
  return n;
}

inline void setDirectory (char const *d){
#ifdef CMW_WINDOWS
  if(!::SetCurrentDirectory(d))
#else
  if(::chdir(d)==-1)
#endif
    raise("setDirectory",d,getError());
}

//Encode integer into variable-length format.
void MarshallingInt::marshal (SendBuffer& b)const{
  ::uint32_t n=val;
  for(;;){
    ::uint8_t a=n&127;
    n>>=7;
    if(0==n){b.receive(a);return;}
    b.receive(a|=128);
    --n;
  }
}

#ifdef CMW_WINDOWS
DWORD Write (HANDLE h,void const *data,int len){
  DWORD bytesWritten=0;
  if(!WriteFile(h,static_cast<char const*>(data),len,&bytesWritten,nullptr))
    raise("Write",GetLastError());
  return bytesWritten;
}

DWORD Read (HANDLE h,void *data,int len){
  DWORD bytesRead=0;
  if(!ReadFile(h,static_cast<char*>(data),len,&bytesRead,nullptr))
    raise("Read",GetLastError());
  return bytesRead;
}
#else
inline int Write (int fd,void const *data,int len){
  if(int r=::write(fd,data,len);r>=0)return r;
  raise("Write",errno);
}

int Read (int fd,void *data,int len){
  int r=::read(fd,data,len);
  if(r>0)return r;
  if(r==0)raise<Fiasco>("Read eof",len);
  if(EAGAIN==errno||EWOULDBLOCK==errno)return 0;
  raise("Read",len,errno);
}

void exitFailure (){::exit(EXIT_FAILURE);}

FileWrapper::FileWrapper (char const *name,int flags,mode_t mode):
        d{::open(name,flags,mode)} {if(d<0)raise("FileWrapper",name,errno);}

inline FileWrapper::~FileWrapper (){::close(d);}
#endif

void setRcvTimeout (sockType s,int time){
#ifdef CMW_WINDOWS
  DWORD t=time*1000;
#else
  ::timeval t{time,0};
#endif
  if(setsockWrapper(s,SO_RCVTIMEO,t)!=0)raise("setRcvTimeout",getError());
}

inline int setNonblocking (sockType s){
#ifndef CMW_WINDOWS
  return ::fcntl(s,F_SETFL,O_NONBLOCK);
#endif
}

inline void closeSocket (sockType s){
#ifdef CMW_WINDOWS
  if(::closesocket(s)==SOCKET_ERROR)
#else
  if(::close(s)==-1)
#endif
    raise("closeSocket",getError());
}

int preserveError (sockType s){
  auto e=getError();
  closeSocket(s);
  return e;
}

int pollWrapper (::pollfd *fds,int n,int timeout){
  if(int r=::poll(fds,n,timeout);r>=0)return r;
  raise("poll",getError());
}

GetaddrinfoWrapper::GetaddrinfoWrapper (char const *node,char const *port
                                        ,int type,int flags){
  ::addrinfo hints{flags,AF_UNSPEC,type,0,0,0,0,0};
  if(int r=::getaddrinfo(node,port,&hints,&head);r!=0)
    raise("getaddrinfo",::gai_strerror(r));
  addr=head;
}

GetaddrinfoWrapper::~GetaddrinfoWrapper (){::freeaddrinfo(head);}
void GetaddrinfoWrapper::inc (){if(addr!=nullptr)addr=addr->ai_next;}

sockType GetaddrinfoWrapper::getSock (){
  for(;addr!=nullptr;addr=addr->ai_next){
    if(auto s=::socket(addr->ai_family,addr->ai_socktype,0);-1!=s)return s;
  }
  raise("getaddrinfo getSock");
}

inline sockType connectWrapper (char const *node,char const *port){
  GetaddrinfoWrapper ai{node,port,SOCK_STREAM};
  auto s=ai.getSock();
  if(0==::connect(s,ai()->ai_addr,ai()->ai_addrlen))return s;
  errno=preserveError(s);
  return -1;
}

sockType udpServer (char const *port){
  GetaddrinfoWrapper ai{nullptr,port,SOCK_DGRAM,AI_PASSIVE};
  auto s=ai.getSock();
  if(0==::bind(s,ai()->ai_addr,ai()->ai_addrlen))return s;
  raise("udpServer",preserveError(s));
}

sockType tcpServer (char const *port){
  GetaddrinfoWrapper ai{nullptr,port,SOCK_STREAM,AI_PASSIVE};
  auto s=ai.getSock();

  if(int on=1;setsockWrapper(s,SO_REUSEADDR,on)==0
    &&::bind(s,ai()->ai_addr,ai()->ai_addrlen)==0
    &&::listen(s,SOMAXCONN)==0)return s;
  raise("tcpServer",preserveError(s));
}

inline int acceptWrapper (sockType s){
  if(int n=::accept(s,nullptr,nullptr);n>=0)return n;
  auto e=getError();
  if(ECONNABORTED==e)return 0;
  raise("acceptWrapper",e);
}

int sockWrite (sockType s,void const *data,int len
               ,sockaddr const *addr,socklen_t toLen){
  if(int r=::sendto(s,static_cast<char const*>(data),len,0,addr,toLen);r>0)
    return r;
  auto e=getError();
  if(EAGAIN==e||EWOULDBLOCK==e)return 0;
  raise("sockWrite",s,e);
}

int sockRead (sockType s,void *data,int len,sockaddr *addr,socklen_t *fromLen){
  int r=::recvfrom(s,static_cast<char*>(data),len,0,addr,fromLen);
  if(r>0)return r;
  auto e=getError();
  if(0==r||ECONNRESET==e)raise<Fiasco>("sockRead eof",s,len,e);
  if(EAGAIN==e||EWOULDBLOCK==e
#ifdef CMW_WINDOWS
     ||WSAETIMEDOUT==e
#endif
  )return 0;
  raise("sockRead",s,len,e);
}

FILEwrapper::FILEwrapper (char const *n,char const *mode):hndl{::fopen(n,mode)}
{if(nullptr==hndl)raise("FILEwrapper",n,mode,errno);}
char* FILEwrapper::fgets (){return ::fgets(line,sizeof line,hndl);}
FILEwrapper::~FILEwrapper (){::fclose(hndl);}

void receiveBool (SendBuffer&b,bool bl){b.receive<unsigned char>(bl);}

void receive (SendBuffer& b,::std::string_view s){
  MarshallingInt(s.size()).marshal(b);
  b.receive(s.data(),s.size());
}

void receiveNull (SendBuffer& b,char const *s){
  receive(b,::std::string_view(s,::strlen(s)+1));
}

void receive (SendBuffer& b,stringPlus lst){
  ::int32_t t=0;
  for(auto s:lst)t+=s.size();
  MarshallingInt{t}.marshal(b);
  for(auto s:lst)b.receive(s.data(),s.size());//Use low-level receive
}
}
#endif
