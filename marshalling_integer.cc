#include "marshalling_integer.hh"
#include "SendBuffer.hh"

#if 0
void cmw::marshalling_integer::CalculateMarshallingSize (Counter& cntr) const
{
  if(value<128){   // (2**7)
    cntr.Add(1);
  }else{
    if(value<16384){  // (2**14)
      cntr.Add(2);
    }else{
      if(value<2097152) {
        cntr.Add(3);
      }else{
        if(value<268435456){
          cntr.Add(4);
        }else{
          cntr.Add(5);
        }
      }
    }
  }
}
#endif

// Encode integer into variable-length format.
void cmw::marshalling_integer::Marshal (SendBuffer& buf,bool) const
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
