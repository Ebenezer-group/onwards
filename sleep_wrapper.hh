#include"platforms.hh"

#ifndef CMW_WINDOWS
#include<unistd.h>
#endif

inline void sleep_wrapper(int seconds)
{
#ifdef CMW_WINDOWS
  ::Sleep(seconds*1000);
#else
  ::sleep(seconds);
#endif
}
