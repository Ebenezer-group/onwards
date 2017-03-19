#pragma once

#include"ErrorWords.hh"
#include"platforms.hh"

#ifndef CMW_WINDOWS
#include<unistd.h>
#endif

inline void setDirectory (char const* dir)
{
#ifdef CMW_WINDOWS
  if(!SetCurrentDirectory(dir))
#else
  if(::chdir(dir)==-1)
#endif
    throw cmw::failure("setDirectory ")<<dir<<" "<<GetError();
}

