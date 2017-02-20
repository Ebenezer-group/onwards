#include "File.hh"
#include "SendBuffer.hh"
#include <stdint.h>

extern int32_t previous_updatedtime;
extern int32_t current_updatedtime;

bool cmw::File::Marshal (SendBuffer& buf,bool) const
{
#if defined(CMW_WINDOWS)
  fd=CreateFile(name.c_str(),GENERIC_READ,FILE_SHARE_READ,
                nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  if(fd==INVALID_HANDLE_VALUE)
    throw failure("File::Marshal CreateFile ") << name;

  FILETIME ftCreate,ftAccess,ftWrite;
  if(!GetFileTime(fd,&ftCreate,&ftAccess,&ftWrite))
    throw failure("File::Marshal -- GetFileTime ");
  // convert FILETIME to time_t
  ULARGE_INTEGER ull;
  ull.LowPart=ftWrite.dwLowDateTime;
  ull.HighPart=ftWrite.dwHighDateTime;
  int32_t mod_time=ull.QuadPart / 10000000ULL - 11644473600ULL;
  if(mod_time>previous_updatedtime){
    buf.Receive(name);
    LARGE_INTEGER li;
    GetFileSizeEx(fd,&li);
    buf.ReceiveFile(fd,li.LowPart);
    if(mod_time>current_updatedtime)current_updatedtime=mod_time;
#else
  struct stat sb;
  if(::stat(name.c_str(),&sb)<0)throw failure("File::Marshal: stat.") << name;
  if(sb.st_mtime>previous_updatedtime){
    if((fd=::open(name.c_str(),O_RDONLY))<0)
      throw failure("File::Marshal: open ")<<name<<" "<<errno;
    buf.Receive(name);
    buf.ReceiveFile(fd,sb.st_size);
    if(sb.st_mtime>current_updatedtime)current_updatedtime=sb.st_mtime;
#endif

    return true;
  }
  return false;
}
