#pragma once

#include "ErrorWords.hh"
#include "platforms.hh"
#include <stdio.h>

namespace cmw {

class FILEWrapper
{
public:
  FILE* Hndl;

  inline FILEWrapper (char const* fn,char const* mode)
  {
    if((Hndl=::fopen(fn,mode))==nullptr)
      throw failure("FILEWrapper ctor ")<<fn<<" "<<mode<<
            " "<<GetError();
  }

  inline ~FILEWrapper () {::fclose(Hndl);}
};
}
