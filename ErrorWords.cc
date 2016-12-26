#include "ErrorWords.hh"
#include <stdio.h>

cmw::failure& cmw::failure::operator<< (char const* s)
{
  whatStr.append(s);
  return *this;
}

cmw::failure& cmw::failure::operator<< (int i)
{
  char buf[40];
  ::sprintf(buf,"%d",i);
  return *this<<buf;
}

