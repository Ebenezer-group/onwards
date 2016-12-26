#include "SendBuffer.hh"
#include "IO.hh"

bool ::cmw::SendBuffer::Flush (::sockaddr* toAddr,::socklen_t toLen)
{
  int const bytes=sockWrite(sock_,buf,index,toAddr,toLen);
  if(bytes==index){
    Reset();
    return true;
  }

  index-=bytes;
  saved_size=index;
  ::memmove(buf,buf+bytes,index);
  return false;
}
