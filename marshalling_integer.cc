#include"marshalling_integer.hh"
#include"SendBuffer.hh"

// Encode integer into variable-length format.
void ::cmw::marshalling_integer::Marshal (SendBuffer& buf,bool) const
{
  uint32_t N=value;
  for(;;){
    uint8_t abyte=N&127;
    N>>=7;
    if(0==N){
      buf.Receive(abyte);
      break;
    }
    abyte|=128;
    buf.Receive(abyte);
    --N;
  }
}
