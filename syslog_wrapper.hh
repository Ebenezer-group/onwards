#pragma once

#include "platforms.hh"
#ifdef CMW_WINDOWS
#define LOG_INFO 0
#define LOG_ERR 0
#else
#include <syslog.h>
#include <stdarg.h>
#endif

namespace cmw {
template<typename... T>
void syslog_wrapper (int priority,char const* message,T... t)
{
#ifndef CMW_WINDOWS
  ::syslog(priority,message,t...);
#endif
}
}

