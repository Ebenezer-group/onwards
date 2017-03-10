#include "SendBuffer.hh"

void ::cmw::SendBuffer::Receive (void const* data,int size)
{
  if(size>bufsize-index)
    throw failure("Size of marshalled data exceeds space available: ")
      <<bufsize<<" "<<saved_size;
  ::memcpy(buf+index,data,size);
  index+=size;
}

void ::cmw::SendBuffer::FillInSize (int32_t max)
{
  int32_t marshalled_bytes=index-saved_size;
  if(marshalled_bytes>max){
    throw failure("Size of marshalled data exceeds max of: ")<<max;
  }
  ::memcpy(buf+saved_size,&marshalled_bytes,sizeof(marshalled_bytes));
  saved_size=index;
}
