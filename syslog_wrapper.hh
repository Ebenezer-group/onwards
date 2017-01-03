#pragma once

#include "platforms.hh"
#ifdef CMW_WINDOWS
#define LOG_INFO 0
#define LOG_ERR 0
#else
#include <stdarg.h>
#include <syslog.h>
#endif

namespace cmw {
template<typename... T>
void syslog_wrapper (int priority,char const* format,T... t)
{
#ifndef CMW_WINDOWS
  ::syslog(priority,format,t...);
#endif
}
}

