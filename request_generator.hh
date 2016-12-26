#pragma once

#include "ErrorWords.hh"
#include "File.hh"
#include "FILEWrapper.hh"
#include "SendBuffer.hh"
#include <string.h>

class request_generator{
  // hand_written_marshalling_code
  char const* fname;
  cmw::FILEWrapper Fl;

public:
  explicit request_generator (char const* file):fname(file),Fl{file,"r"}{}

  void Marshal (::cmw::SendBuffer& buf,bool=false) const{
    auto index=buf.ReserveBytes(1);
    if(::cmw::File{fname}.Marshal(buf))
      buf.Receive(index,static_cast<int8_t>(1));
    else{
      buf.Receive(index,static_cast<int8_t>(0));
      buf.Receive(fname);
    }

    char lineBuf[200];
    char const* token=nullptr;
    int32_t updatedFiles=0;
    index=buf.ReserveBytes(sizeof(updatedFiles));
    while(::fgets(lineBuf,sizeof(lineBuf),Fl.Hndl)){
      if('/'==lineBuf[0]&&'/'==lineBuf[1])continue;
      token=::strtok(lineBuf," ");
      if(::strcmp("Header",token))break;
      if(::cmw::File{::strtok(nullptr,"\n ")}.Marshal(buf))++updatedFiles;
    }

    if(::strcmp("Middle-File",token))
      throw ::cmw::failure("A middle file is required.");
    if(::cmw::File{::strtok(nullptr,"\n ")}.Marshal(buf))++updatedFiles;
    buf.Receive(index,updatedFiles);
  }
};
