// This file contains implementations and should only
// be included by one source file per program.

#ifndef CMW_BUFFERIMPL_HH
#define CMW_BUFFERIMPL_HH
#include<cmw_Buffer.hh>
#include<quicklz.c>
#include<charconv>
#include<limits>
#include<cstdio>//snprintf
#include<cstdlib>//exit
static_assert(::std::numeric_limits<unsigned char>::digits==8);
static_assert(::std::numeric_limits<float>::is_iec559,"IEEE754");
#ifdef CMW_WINDOWS
#define poll WSAPoll
#else
#include<errno.h>
#include<fcntl.h>//fcntl,open
#include<netdb.h>
#include<poll.h>
#include<sys/types.h>
#include<unistd.h>//chdir
#endif

namespace cmw{
void Failure::operator<< (::std::string_view v){
  s.append(" ");
  s.append(v);
}

void Failure::operator<< (int i){
  char b[12];
  ::std::snprintf(b,sizeof b,"%d",i);
  *this<<b;
}

void winStart (){
#ifdef CMW_WINDOWS
  WSADATA w;
  if(auto r=::WSAStartup(MAKEWORD(2,2),&w);0!=r)raise("WSAStartup",r);
#endif
}

int getError (){
  return
#ifdef CMW_WINDOWS
    WSAGetLastError();
#else
    errno;
#endif
}

int fromChars (::std::string_view s){
  int n;
  ::std::from_chars(s.data(),s.data()+s.size(),n);
  return n;
}

void setDirectory (char const *d){
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

void exitFailure (){::std::exit(EXIT_FAILURE);}

#ifdef CMW_WINDOWS
DWORD Write (HANDLE h,void const *data,int len){
  DWORD bytesWritten;
  if(!WriteFile(h,static_cast<char const*>(data),len,&bytesWritten,nullptr))
    raise("Write",GetLastError());
  return bytesWritten;
}

DWORD Read (HANDLE h,void *data,int len){
  DWORD bytesRead;
  if(!ReadFile(h,static_cast<char*>(data),len,&bytesRead,nullptr))
    raise("Read",GetLastError());
  return bytesRead;
}
#else
int Write (int fd,void const *data,int len){
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

FileWrapper::FileWrapper (char const *name,int flags,mode_t mode):
        d{::open(name,flags,mode)} {if(d<0)raise("FileWrapper",name,errno);}

FileWrapper::FileWrapper (char const *name,mode_t mode):
        FileWrapper(name,O_CREAT|O_WRONLY|O_TRUNC,mode){}

FileWrapper::~FileWrapper (){::close(d);}

char FileBuffer::getc (){
  if(ind>=bytes){
    bytes=Read(fl.d,buf,sizeof buf);
    ind=0;
  }
  return buf[ind++];
}

char* FileBuffer::getline (char delim){
  ::std::size_t idx=0;
  while((line[idx]=getc())!=delim){
    if(line[idx]=='\r')raise("getline carriage return");
    if(++idx>=sizeof line)raise("getline line too long");
  }
  line[idx]=0;
  return line;
}
#endif

void setRcvTimeout (sockType s,int time){
#ifdef CMW_WINDOWS
  DWORD t=time*1000;
#else
  ::timeval t{time,0};
#endif
  if(setsockWrapper(s,SO_RCVTIMEO,t)!=0)raise("setRcvTimeout",getError());
}

int setNonblocking (sockType s){
#ifdef CMW_WINDOWS
  DWORD e=1;
  return ::ioctlsocket(s,FIONBIO,&e);
#else
  return ::fcntl(s,F_SETFL,O_NONBLOCK);
#endif
}

void closeSocket (sockType s){
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

sockType connectWrapper (char const *node,char const *port){
  GetaddrinfoWrapper ai{node,port,SOCK_STREAM};
  auto s=ai.getSock();
  if(0==::connect(s,ai().ai_addr,ai().ai_addrlen))return s;
  errno=preserveError(s);
  return -1;
}

sockType udpServer (char const *port){
  GetaddrinfoWrapper ai{nullptr,port,SOCK_DGRAM,AI_PASSIVE};
  auto s=ai.getSock();
  if(0==::bind(s,ai().ai_addr,ai().ai_addrlen))return s;
  raise("udpServer",preserveError(s));
}

sockType tcpServer (char const *port){
  GetaddrinfoWrapper ai{nullptr,port,SOCK_STREAM,AI_PASSIVE};
  auto s=ai.getSock();

  if(int on=1;setsockWrapper(s,SO_REUSEADDR,on)==0
    &&::bind(s,ai().ai_addr,ai().ai_addrlen)==0
    &&::listen(s,SOMAXCONN)==0)return s;
  raise("tcpServer",preserveError(s));
}

int acceptWrapper (sockType s){
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

int SendBuffer::reserveBytes (int n){
  if(n>bufsize-index)raise("SendBuffer checkSpace",n,index);
  auto i=index;
  index+=n;
  return i;
}

void SendBuffer::fillInSize (::int32_t max){
  ::int32_t marshalledBytes=index-savedSize;
  if(marshalledBytes>max)raise("fillInSize",max);
  receive(savedSize,marshalledBytes);
  savedSize=index;
}

#ifndef CMW_WINDOWS
void SendBuffer::receiveFile (char const* n,::int32_t sz){
  receive(sz);
  auto prev=reserveBytes(sz);
  FileWrapper fl{n,O_RDONLY,0};
  if(Read(fl.d,buf+prev,sz)!=sz)raise("SendBuffer receiveFile");
}
#endif

bool SendBuffer::flush (){
  int const bytes=sockWrite(sock_,buf,index);
  if(bytes==index){reset();return true;}

  index-=bytes;
  savedSize=index;
  ::std::memmove(buf,buf+bytes,index);
  return false;
}

void receiveBool (SendBuffer&b,bool bl){b.receive<unsigned char>(bl);}

void receive (SendBuffer& b,::std::string_view s,int a){
  MarshallingInt(s.size()+a).marshal(b);
  b.receive(s.data(),s.size()+a);
}

void receive (SendBuffer& b,stringPlus lst){
  ::int32_t t=0;
  for(auto s:lst)t+=s.size();
  MarshallingInt{t}.marshal(b);
  for(auto s:lst)b.receive(s.data(),s.size());//Use low-level receive
}
}
#endif

