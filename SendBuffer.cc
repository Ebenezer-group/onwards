#include "SendBuffer.hh"

using ::cmw::SendBuffer;

void SendBuffer::Receive (void const* data,int size)
{
  if(size>bufsize-index)
    throw failure("Size of marshalled data exceeds space available: ")
      <<bufsize<<" "<<saved_size;
  ::memcpy(buf+index,data,size);
  index+=size;
}

int SendBuffer::ReserveBytes (int num)
{
  if(num>bufsize-index)throw failure("SendBuffer::ReserveBytes");
  auto copy=index;
  index+=num;
  return copy;
}

void SendBuffer::FillInSize (int32_t max)
{
  int32_t marshalled_bytes=index-saved_size;
  if(marshalled_bytes>max){
    throw failure("Size of marshalled data exceeds max of: ")<<max;
  }
  ::memcpy(buf+saved_size,&marshalled_bytes,sizeof(marshalled_bytes));
  saved_size=index;
}
