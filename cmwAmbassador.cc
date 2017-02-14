#include "platforms.hh"

#include "account_info.hh"
#include "close_socket.hh"
#include "connect_wrapper.hh"
#include "empty_container.hh"
#include "ErrorWords.hh"
#include "File.hh"
#include "FILEWrapper.hh"
#include "IO.hh"
#include "marshalling_integer.hh"
#include "message_ids.hh"
#include "poll_wrapper.hh"
#include "ReceiveBuffer.hh"
#include "ReceiveBufferCompressed.hh"
#include "ReceiveBufferStack.hh"
#include "setDirectory.hh"
#include "sleep_wrapper.hh"
#include "string_join.hh"
#include "syslog_wrapper.hh"
#include "SendBufferCompressed.hh"
#include "SendBufferStack.hh"
#include "udp_stuff.hh"
#include "zz.middle_messages_front.hh"

#include <assert.h>
#include <queue>
#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h> //strtol
#include <string.h>
#include <vector>
#include <fcntl.h>
#include <netinet/in.h> //sockaddr_in6,socklen_t
#include <unistd.h> //pread

#define CHECK_FIELD_NAME(fieldname)             \
  ::fgets(lineBuf,sizeof(lineBuf),Fl.Hndl);     \
  if(::strcmp(fieldname,::strtok(lineBuf," "))) \
    throw cmw::failure("Expected ")<<fieldname;

class request_generator{
  // hand_written_marshalling_code
  char const* fname;
  cmw::FILEWrapper Fl;

public:
  explicit request_generator (char const* file):fname(file),Fl{file,"r"}{}

  void Marshal (::cmw::SendBuffer& buf,bool=false) const{
    auto index=buf.ReserveBytes(1);
    if(::cmw::File{fname}.Marshal(buf))
      buf.Receive(index,true);
    else{
      buf.Receive(index,false);
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

    CHECK_FIELD_NAME("Message-Lengths");
    token=::strtok(nullptr,"\n ");
    int8_t msgLength;
    if(!::strcmp("variable",token))msgLength=1;
    else if(!::strcmp("fixed",token))msgLength=0;
    else throw ::cmw::failure("Invalid value for Message-Lengths.");
    buf.Receive(msgLength);
  }
};

#include "zz.middle_messages_back.hh"
using namespace ::cmw;

int32_t previous_updatedtime;
int32_t current_updatedtime;

struct cmw_request{
  marshalling_integer const accountNbr;
  char path[120];
  char const* filename;
  ::sockaddr_in6 front_tier;
  int32_t latest_update;
  file_type fd;

  template <class R>
  explicit cmw_request (ReceiveBuffer<R>& buf):accountNbr(buf)
  {
    buf.CopyString(path);
    auto const pos=::strrchr(path,'/');
    if(NULL==pos)throw failure("cmw_request didn't find a /");
    *pos='\0';
    filename=pos+1;
    setDirectory(path);
    char lastrunFile[60];
    ::snprintf(lastrunFile,sizeof(lastrunFile),"%s.lastrun",filename);
    fd=::open(lastrunFile,O_RDWR);
    previous_updatedtime=0;
    if(fd>=0){
      if(::pread(fd,&previous_updatedtime,sizeof(previous_updatedtime),0)<0)
        throw failure("pread ")<<errno;
    }else{
      fd=::open(lastrunFile,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
      if(fd<0)throw failure("open ")<<errno;
    }
    current_updatedtime=previous_updatedtime;
  }

  void save_lastruntime () const
  {Write(fd,&latest_update,sizeof(latest_update));}

  ~cmw_request () {::close(fd);}
};

class cmwAmbassador{
  ReceiveBufferCompressed<
#ifdef CMW_ENDIAN_BIG
    LeastSignificantFirst
#else
    SameFormat
#endif
  > cmwBuf;

  SendBufferCompressed cmwSendbuf;
  SendBufferStack<> localsendbuf;
  ::std::queue<::std::unique_ptr<cmw_request>> pendingTransactions;
  ::std::vector<cmw_account> accounts;
  int loginPause;
  int unrepliedKeepalives=0;
  ::pollfd fds[2];

  void login ();
  void reset (char const*);
  bool sendData ();
public:
  cmwAmbassador (char const*);
};

void cmwAmbassador::login (){
  middle_messages_back::Marshal(cmwSendbuf,Login,accounts);
  for(;;){
    try{
      fds[0].fd=cmwSendbuf.sock_=cmwBuf.sock_=
                          connect_wrapper("www.webEbenezer.net","56789");
      break;
    }catch(::std::exception const& ex){
      ::printf("%s\n",ex.what());
      sleep_wrapper(loginPause);
    }
  }

  if(sockWrite(cmwSendbuf.sock_,
#ifdef CMW_ENDIAN_BIG
               &most_significant_first
#else
               &least_significant_first
#endif
               ,1)!=1)throw failure("Couldn't write byte order");
  while(!cmwSendbuf.Flush());

  while(!cmwBuf.GotPacket());
  if(!cmwBuf.GiveBool())
    throw failure("Login failed: ")<<cmwBuf.GiveString_view();
  if(::fcntl(cmwBuf.sock_,F_SETFL,O_NONBLOCK)<0)
    throw failure("fcntl:")<<errno;
}

void cmwAmbassador::reset (char const* explanation){
  while(!pendingTransactions.empty()){
    if(pendingTransactions.front().get()){
      auto& request=*pendingTransactions.front();
      middle_messages_front::Marshal(localsendbuf,false,explanation);
      localsendbuf.Flush((::sockaddr*)&request.front_tier
                         ,sizeof(request.front_tier));
    }
    pendingTransactions.pop();
  }
  localsendbuf.Reset();
  close_socket(cmwBuf.sock_);
  cmwBuf.Reset();
  cmwSendbuf.CompressedReset();
  unrepliedKeepalives=0;
  login();
}

bool cmwAmbassador::sendData (){
  try{
    return cmwSendbuf.Flush();
  }catch(::std::exception const& ex){
    syslog_wrapper(LOG_ERR,"Problem sending data to CMW: %s",ex.what());
    reset("Problem sending data to CMW");
  }
  return true;
}

cmwAmbassador::cmwAmbassador (char const* configfile):
  cmwBuf(1100000),cmwSendbuf(1000000)
{
  char lineBuf[140];
  FILEWrapper Fl(configfile,"r");
  while(::fgets(lineBuf,sizeof(lineBuf),Fl.Hndl)){
    char const* token=::strtok(lineBuf," ");
    if(!::strcmp("Account-Number",token)){
      auto num=::strtol(::strtok(nullptr,"\n "),0,10);
      CHECK_FIELD_NAME("Password");
      accounts.emplace_back(num,::strtok(nullptr,"\n "));
    }else{
      if(accounts.empty())
        throw failure("At least one account number is required.");
      if(!::strcmp("UDP-Port-Number",token))break;
      else throw failure("UDP-Port-Number is required.");
    }
  }
  fds[1].fd=localsendbuf.sock_=udp_server(::strtok(nullptr,"\n "));

  CHECK_FIELD_NAME("Seconds-to-Sleep-Between-Login-Attempts");
  loginPause=::strtol(::strtok(nullptr,"\n "),0,10);

  CHECK_FIELD_NAME("Keepalive-Interval-in-Milliseconds");
  int keepaliveInterval=::strtol(::strtok(nullptr,"\n "),0,10);

  fds[0].events=fds[1].events=POLLIN;
  login();

  for(;;){
    if(0==poll_wrapper(fds,2,keepaliveInterval)){
      try{
        if(++unrepliedKeepalives>1)throw failure("No reply from CMW");
        middle_messages_back::Marshal(cmwSendbuf,Keepalive);
        fds[0].events|=POLLOUT;
        pendingTransactions.push(nullptr);
      }catch(::std::exception const& ex){
        syslog_wrapper(LOG_ERR,"Keepalive: %s",ex.what());
        reset("Keepalive problem");
      }
      continue;
    }

    if(fds[0].revents&POLLIN){
      try{
        if(cmwBuf.GotPacket()){
          do{
            assert(!pendingTransactions.empty());
            if(pendingTransactions.front().get()){
              auto const& request=*pendingTransactions.front();
              if(cmwBuf.GiveBool()){
                setDirectory(request.path);
                empty_container<File>{cmwBuf};
                request.save_lastruntime();
                middle_messages_front::Marshal(localsendbuf,true);
              }else middle_messages_front::Marshal(localsendbuf,false,
                                 string_join{"CMW: ",cmwBuf.GiveString_view()});
              localsendbuf.Flush((::sockaddr*)&request.front_tier
                                 ,sizeof(request.front_tier));
            }else --unrepliedKeepalives;
            pendingTransactions.pop();
          }while(cmwBuf.NextMessage());
        }
      }catch(connection_lost const& ex){
        syslog_wrapper(LOG_ERR,"Got end of stream notice: %s",ex.what());
        reset("CMW stopped before your request was processed");
        continue;
      }catch(::std::exception const& ex){
        syslog_wrapper(LOG_ERR,"Problem handling reply from CMW %s",ex.what());
        assert(!pendingTransactions.empty());
        if(pendingTransactions.front().get()){
          auto const& request=*pendingTransactions.front();
          middle_messages_front::Marshal(localsendbuf,false,ex.what());
          localsendbuf.Flush((::sockaddr*)&request.front_tier
                             ,sizeof(request.front_tier));
        }else --unrepliedKeepalives;
        pendingTransactions.pop();
      }
    }

    if(fds[0].revents&POLLOUT&&sendData())fds[0].events=POLLIN;

    if(fds[1].revents&POLLIN){
      bool gotAddress=false;
      ::sockaddr_in6 front;
      ::socklen_t frontlen=sizeof(front);
      try{
        ReceiveBufferStack<SameFormat> localbuf(fds[1].fd,(::sockaddr*)&front,&frontlen);
        gotAddress=true;
        auto request=::std::make_unique<cmw_request>(localbuf);
        //::std::unique_ptr request{new cmw_request(localbuf)};
        middle_messages_back::Marshal(cmwSendbuf,Generate,request->accountNbr
                                      ,request_generator(request->filename),500000);
        request->latest_update=current_updatedtime;
        request->front_tier=front;
        pendingTransactions.push(static_cast<::std::unique_ptr<cmw_request>&&>(request));
      }catch(::std::exception const& ex){
        syslog_wrapper(LOG_ERR,"Mediate request: %s",ex.what());
        if(gotAddress){
          middle_messages_front::Marshal(localsendbuf,false,ex.what());
          localsendbuf.Flush((::sockaddr*)&front,frontlen);
        }
        continue;
      }
      if(!sendData())fds[0].events|=POLLOUT;
    }
  }
}

int main (int argc,char** argv){
  try{
    ::openlog(*argv,LOG_PID|LOG_NDELAY,LOG_USER);
    if(argc!=2)throw failure("Usage: ")<<*argv<<" config-file-name";
    cmwAmbassador(*(argv+1));
  }catch(::std::exception const& ex){
    syslog_wrapper(LOG_ERR,"%s - program ending.",ex.what());
  }catch(...){
    syslog_wrapper(LOG_ERR,"Unknown exception!");
  }
  return EXIT_FAILURE;
}
