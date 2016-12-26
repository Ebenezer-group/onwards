#include "File.hh"

#if !defined(CMW_WINDOWS)
#include <unistd.h>  // close
#endif

cmw::File::~File ()
{
  if(0!=fd)
#ifdef CMW_WINDOWS
    ::CloseHandle(fd);
#else
    ::close(fd);
#endif
}

