#pragma once
#include"ErrorWords.hh"
#include"platforms.hh"
#include<stdio.h>

namespace cmw{
class FILE_wrapper{
public:
  FILE* Hndl;

  inline FILE_wrapper (char const* fn,char const* mode){
    if((Hndl=::fopen(fn,mode))==nullptr)
      throw failure("FILE_wrapper ")<<fn<<" "<<mode<<" "<<GetError();
  }

  inline ~FILE_wrapper (){::fclose(Hndl);}
};
}
