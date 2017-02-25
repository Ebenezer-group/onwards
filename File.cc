#include "File.hh"
#include "SendBuffer.hh"
#include <stdint.h>

extern int32_t previous_updatedtime;
extern int32_t current_updatedtime;

bool cmw::File::Marshal (SendBuffer& buf,bool) const
{
  struct stat sb;
  if(::stat(name.c_str(),&sb)<0)throw failure("File::Marshal stat ")<<name;
  if(sb.st_mtime>previous_updatedtime){
    if((fd=::open(name.c_str(),O_RDONLY))<0)
      throw failure("File::Marshal open ")<<name<<" "<<errno;
    buf.Receive(name);
    buf.ReceiveFile(fd,sb.st_size);
    if(sb.st_mtime>current_updatedtime)current_updatedtime=sb.st_mtime;
    return true;
  }
  return false;
}
