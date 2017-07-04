#include"account_info.hh"
#include<close_socket.hh>
#include<connect_wrapper.hh>
#include<ErrorWords.hh>
#include<File.hh>
#include<FILE_wrapper.hh>
#include<IO.hh>
#include<marshalling_integer.hh>
#include"message_ids.hh"
#include<poll_wrapper.hh>
#include<ReceiveBuffer.hh>
#include<ReceiveBufferCompressed.hh>
#include<ReceiveBufferStack.hh>
#include<setDirectory.hh>
#include<syslog_wrapper.hh>
#include<SendBufferCompressed.hh>
#include<SendBufferStack.hh>
#include<udp_stuff.hh>
#include"zz.middle_front.hh"

#include<memory> //unique_ptr
#include<vector>
#include<assert.h>
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h> //strtol
#include<string.h>
#include<errno.h>
#include<netinet/in.h> //sockaddr_in6,socklen_t
#include<unistd.h> //pread

using namespace ::cmw;
class request_generator{
  // hand_written_marshalling_code
  char const* fname;
  FILE_wrapper Fl;

public:
  explicit request_generator (char const* file):fname(file),Fl{file,"r"}{}

  void Marshal (SendBuffer& buf,bool=false)const{
    auto index=buf.ReserveBytes(1);
    if(MarshalFile(fname,buf))buf.Receive(index,true);
    else{
      buf.Receive(index,false);
      buf.Receive(fname);
    }

    char lineBuf[100];
    int8_t updatedFiles=0;
    index=buf.ReserveBytes(sizeof(updatedFiles));
    while(::fgets(lineBuf,sizeof(lineBuf),Fl.Hndl)){
      if('/'==lineBuf[0]&&'/'==lineBuf[1])continue;
      char const* token=::strtok(lineBuf,"\n ");
      if(!::strcmp("message-lengths",token))break;
      if(MarshalFile(token,buf))++updatedFiles;
    }
    buf.Receive(index,updatedFiles);
  }
};

#include"zz.middle_back.hh"

int32_t previous_updatedtime;
int32_t current_updatedtime;

struct cmw_request{
  marshalling_integer const accountNbr;
  char path[120];
  char const* filename;
  ::sockaddr_in6 front_tier;
  int32_t latest_update;
  int fd;

  template<class R>
  explicit cmw_request (ReceiveBuffer<R>& buf):accountNbr(buf){
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
      if(::pread(fd,&previous_updatedtime,sizeof(previous_updatedtime),0)==-1)
        throw failure("pread ")<<errno;
    }else{
      fd=::open(lastrunFile,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
      if(fd<0)throw failure("open ")<<errno;
    }
    current_updatedtime=previous_updatedtime;
  }

  void save_lastruntime ()const
  {Write(fd,&latest_update,sizeof(latest_update));}

  ~cmw_request (){::close(fd);}
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
  ::std::vector<cmw_account> accounts;
  ::std::vector<::std::unique_ptr<cmw_request>> pendingTransactions;
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
  for(;;){
    fds[0].fd=cmwSendbuf.sock_=cmwBuf.sock_=
       connect_wrapper("174.20.7.204",
#ifdef CMW_ENDIAN_BIG
                       "56790");
#else
                       "56789");
#endif
    if(fds[0].fd!=-1)break;
    ::printf("connect_wrapper %d\n",errno);
    poll_wrapper(nullptr,0,loginPause);
  }

  middle_back::Marshal(cmwSendbuf,Login,accounts);
  while(!cmwSendbuf.Flush());
  while(!cmwBuf.GotPacket());
  if(cmwBuf.GiveBool())set_nonblocking(fds[0].fd);
  else throw failure("Login:")<<cmwBuf.GiveString_view();
}

void cmwAmbassador::reset (char const* explanation){
  middle_front::Marshal(localsendbuf,false,{explanation});
  for(auto& t:pendingTransactions){
    if(t.get()){
      localsendbuf.Send((::sockaddr*)&t->front_tier,sizeof(t->front_tier));
    }
  }
  pendingTransactions.clear();
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

#define CHECK_FIELD_NAME(fieldname)             \
  ::fgets(lineBuf,sizeof(lineBuf),Fl.Hndl);     \
  if(::strcmp(fieldname,::strtok(lineBuf," "))) \
    throw failure("Expected ")<<fieldname;

cmwAmbassador::cmwAmbassador (char const* configfile):cmwBuf(1100000)
  ,cmwSendbuf(1000000)
{
  char lineBuf[140];
  FILE_wrapper Fl(configfile,"r");
  while(::fgets(lineBuf,sizeof(lineBuf),Fl.Hndl)){
    char const* token=::strtok(lineBuf," ");
    if(!::strcmp("Account-number",token)){
      auto num=::strtol(::strtok(nullptr,"\n "),0,10);
      CHECK_FIELD_NAME("Password");
      accounts.emplace_back(num,::strtok(nullptr,"\n "));
    }else{
      if(accounts.empty())throw failure("An account number is required.");
      if(!::strcmp("UDP-port-number",token))break;
      else throw failure("UDP-port-number is required.");
    }
  }
  fds[1].fd=localsendbuf.sock_=udp_server(::strtok(nullptr,"\n "));
#ifdef __linux__
  set_nonblocking(fds[1].fd);
#endif

  CHECK_FIELD_NAME("Login-attempts-interval-in-milliseconds");
  loginPause=::strtol(::strtok(nullptr,"\n "),0,10);
  CHECK_FIELD_NAME("Keepalive-interval-in-milliseconds");
  int keepaliveInterval=::strtol(::strtok(nullptr,"\n "),0,10);

  fds[0].events=fds[1].events=POLLIN;
  login();

  for(;;){
    if(0==poll_wrapper(fds,2,keepaliveInterval)){
      try{
        if(++unrepliedKeepalives>1)throw failure("No reply from CMW");
        middle_back::Marshal(cmwSendbuf,Keepalive);
        fds[0].events|=POLLOUT;
        pendingTransactions.push_back(nullptr);
      }catch(::std::exception const& ex){
        syslog_wrapper(LOG_ERR,"Keepalive: %s",ex.what());
        reset("Keepalive problem");
      }
      continue;
    }

    localsendbuf.Reset();
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
                middle_front::Marshal(localsendbuf,true);
              }else middle_front::Marshal(localsendbuf,false,
                                 {"CMW:",cmwBuf.GiveString_view()});
              localsendbuf.Send((::sockaddr*)&request.front_tier
                                ,sizeof(request.front_tier));
              localsendbuf.Reset();
            }else --unrepliedKeepalives;
            pendingTransactions.erase(::std::begin(pendingTransactions));
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
          middle_front::Marshal(localsendbuf,false,{ex.what()});
          localsendbuf.Send((::sockaddr*)&request.front_tier
                            ,sizeof(request.front_tier));
        }else --unrepliedKeepalives;
        pendingTransactions.erase(::std::begin(pendingTransactions));
      }
    }

    if(fds[0].revents&POLLOUT&&sendData())fds[0].events=POLLIN;

    if(fds[1].revents&POLLIN){
      bool gotAddress=false;
      ::sockaddr_in6 front;
      ::socklen_t frontlen=sizeof(front);
      try{
        ReceiveBufferStack<SameFormat> recbuf(fds[1].fd,(::sockaddr*)&front,&frontlen);
        gotAddress=true;
        auto request=::std::make_unique<cmw_request>(recbuf);
        //::std::unique_ptr request{new cmw_request(recbuf)};
        middle_back::Marshal(cmwSendbuf,Generate,request->accountNbr
                                      ,request_generator(request->filename),500000);
        request->latest_update=current_updatedtime;
        request->front_tier=front;
        pendingTransactions.push_back(static_cast<::std::unique_ptr<cmw_request>&&>(request));
      }catch(::std::exception const& ex){
        syslog_wrapper(LOG_ERR,"Mediate request: %s",ex.what());
        if(gotAddress){
          middle_front::Marshal(localsendbuf,false,{ex.what()});
          localsendbuf.Send((::sockaddr*)&front,frontlen);
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
    syslog_wrapper(LOG_ERR,"Program ending: %s",ex.what());
  }catch(...){
    syslog_wrapper(LOG_ERR,"Unknown exception!");
  }
  return EXIT_FAILURE;
}
