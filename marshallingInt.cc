#include"Buffer.hh"

//Encode integer into variable-length format.
void ::cmw::marshallingInt::Marshal (SendBuffer& b,bool)const{
  uint32_t n=val;
  for(;;){
    uint8_t a=n&127;
    n>>=7;
    if(0==n){b.Receive(a);return;}
    b.Receive(a|=128);
    --n;
  }
}
