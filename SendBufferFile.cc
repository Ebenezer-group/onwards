#include "SendBufferFile.hh"
#include "ErrorWords.hh"
#include "IOfile.hh"
#include "string.h"

cmw::SendBufferFile::SendBufferFile (int size) : SendBufferHeap(size)
{}

bool cmw::SendBufferFile::Flush ()
{
  int const bytes=Write(hndl,buf,index);

  if(bytes==index){
    index=0;
    return true;
  }

  index-=bytes;
  ::memmove(buf,buf+bytes,index);
  return false;
}

void cmw::SendBufferFile::ReceiveFile (int32_t)
{
  throw failure("SendBufferFile::ReceiveFile not implemented");
}
