#pragma once
#include"ErrorWords.hh"
#include"platforms.hh"
#ifndef CMW_WINDOWS
#include<unistd.h>
#endif

inline void setDirectory (char const* d){
#ifdef CMW_WINDOWS
  if(!SetCurrentDirectory(d))
#else
  if(::chdir(d)==-1)
#endif
    throw ::cmw::failure("setDirectory ")<<d<<" "<<GetError();
}
