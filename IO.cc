#include "IO.hh"
#include "ErrorWords.hh"

int cmw::sockWrite (sock_type sock,void const* data,int len
                    ,sockaddr* toAddr,socklen_t toLen)
{
  int rc=::sendto(sock,static_cast<char const*>(data),len,0
                  ,toAddr,toLen);
  if(rc>0)return rc;
  auto err=GetError();
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockWrite sock==")<<sock<<" "<<err;
}

int cmw::sockRead (sock_type sock,char* data,int len
                   ,sockaddr* fromAddr,socklen_t* fromLen)
{
  int rc=::recvfrom(sock,data,len,0,fromAddr,fromLen);
  if(rc>0)return rc;
  if(rc==0)throw connection_lost("sockRead eof sock==")<<sock<<" len=="<<len;
  auto err=GetError();
  if(ECONNRESET==err)throw connection_lost("sockRead-ECONNRESET");
  if(EAGAIN==err||EWOULDBLOCK==err)return 0;
  throw failure("sockRead sock==")<<sock<<" len=="<<len<<" "<<err;
}
