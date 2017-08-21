#include"marshalling_integer.hh"
#include"SendBuffer.hh"

// Encode integer into variable-length format.
void ::cmw::marshalling_integer::Marshal (SendBuffer& b,bool)const{
  uint32_t n=value;
  for(;;){
    uint8_t abyte=n&127;
    n>>=7;
    if(0==n){b.Receive(abyte);return;}
    abyte|=128;
    b.Receive(abyte);
    --n;
  }
}
