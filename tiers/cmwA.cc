#include"account.hh"
#include<Buffer.hh>
#include<ErrorWords.hh>
#include"messageIDs.hh"
#include<wrappers.hh>

#include<memory>//unique_ptr
#include<vector>
#include<assert.h>
#include<errno.h>
#include<fcntl.h>//open
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>//strtol
#include<string.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<time.h>
#include<netinet/in.h>//sockaddr_in6,socklen_t
#include<unistd.h>//pread,close

::int32_t previousTime;
using namespace ::cmw;

bool MarshalFile (char const* name,SendBuffer& buf){
  struct ::stat sb;
  if(::stat(name,&sb)<0)throw failure("MarshalFile stat ")<<name;
  if(sb.st_mtime>previousTime){
    if('.'==name[0]||name[0]=='/')Receive(buf,::strrchr(name,'/')+1);
    else Receive(buf,name);
    InsertNull(buf);

    fileWrapper fl(name,O_RDONLY);
    buf.ReceiveFile(fl.d,sb.st_size);
    return true;
  }
  return false;
}

struct cmwRequest{
  ::sockaddr_in6 front;
  ::socklen_t frontLen=sizeof(front);
  marshallingInt const accountNbr;
  fixedString120 path;
  ::int32_t now;
  char const* middleFile;
  int fd;

  cmwRequest (){}

  template<class R>
  explicit cmwRequest (ReceiveBuffer<R>& buf):accountNbr(buf),path(buf){
    now=::time(nullptr);
    char* const pos=::strrchr(path(),'/');
    if(nullptr==pos)throw failure("cmwRequest didn't find a /");
    *pos='\0';
    middleFile=pos+1;
    setDirectory(path());
    char lastrun[60];
    ::snprintf(lastrun,sizeof(lastrun),"%s.lastrun",middleFile);
    fd=::open(lastrun,O_RDWR);
    previousTime=0;
    if(fd>=0){
      if(::pread(fd,&previousTime,sizeof(previousTime),0)==-1){
        auto err=errno;
        ::close(fd);
        throw failure("pread ")<<err;
      }
    }else{
      fd=::open(lastrun,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
      if(fd<0)throw failure("open ")<<lastrun<<" "<<errno;
    }
  }

  void saveLastruntime ()const{Write(fd,&now,sizeof(now));}

  void Marshal (SendBuffer& buf)const{
    accountNbr.Marshal(buf);
    auto ind=buf.ReserveBytes(1);
    if(MarshalFile(middleFile,buf))buf.Receive(ind,true);
    else{
      buf.Receive(ind,false);
      Receive(buf,middleFile);
      InsertNull(buf);
    }

    int8_t updatedFiles=0;
    ind=buf.ReserveBytes(sizeof(updatedFiles));
    FILE_wrapper f{middleFile,"r"};
    while(auto line=f.fgets()){
      auto tok=::strtok(line,"\n ");
      if(!::strncmp(tok,"//",2)||!::strcmp(tok,"fixedMessageLengths")||
         !::strcmp(tok,"splitOutput"))continue;
      if(!::strcmp(tok,"--"))break;
      if(MarshalFile(tok,buf))++updatedFiles;
    }
    buf.Receive(ind,updatedFiles);
  }

  ~cmwRequest (){::close(fd);}
};
#include"zz.middleBack.hh"

class cmwAmbassador{
  BufferCompressed<
#ifdef CMW_ENDIAN_BIG
    LeastSignificantFirst
#else
    SameFormat
#endif
  > cmwBuf;

  BufferStack<SameFormat> localbuf;
  ::std::vector<cmwAccount> accounts;
  ::std::vector<::std::unique_ptr<cmwRequest>> pendingRequests;
  ::pollfd fds[2];
  int loginPause;

  void login ();
  void reset (char const*);
  bool sendData ();
public:
  cmwAmbassador (char*);
};

void cmwAmbassador::login (){
  for(;;){
    fds[0].fd=cmwBuf.sock_=connectWrapper("70.56.166.91",
#ifdef CMW_ENDIAN_BIG
                      "56790");
#else
                      "56789");
#endif
    if(fds[0].fd!=-1)break;
    ::printf("connectWrapper %d\n",errno);
    pollWrapper(nullptr,0,loginPause);
  }

  ::middleBack::Marshal(cmwBuf,Login,accounts);
  while(!cmwBuf.Flush());
  while(!cmwBuf.GotPacket());
  if(GiveBool(cmwBuf))setNonblocking(fds[0].fd);
  else throw failure("Login:")<<GiveStringView(cmwBuf);
}

void cmwAmbassador::reset (char const* explanation){
  ::middleFront::Marshal(localbuf,false,{explanation});
  for(auto& r:pendingRequests){
    if(r.get())localbuf.Send((::sockaddr*)&r->front,r->frontLen);
  }
  pendingRequests.clear();
  closeSocket(cmwBuf.sock_);
  cmwBuf.CompressedReset();
  login();
}

bool cmwAmbassador::sendData (){
  try{return cmwBuf.Flush();}catch(::std::exception const& e){
    syslogWrapper(LOG_ERR,"Problem sending data to CMW: %s",e.what());
    reset("Problem sending data to CMW");
    return true;
  }
}

void checkField (char const* fld,FILE_wrapper& f){
  if(::strcmp(fld,::strtok(f.fgets()," ")))throw failure("Expected ")<<fld;
}

cmwAmbassador::cmwAmbassador (char* configfile):cmwBuf(1100000){
  FILE_wrapper cfg(configfile,"r");
  while(auto tok=::strtok(cfg.fgets()," ")){
    if(!::strcmp("Account-number",tok)){
      auto num=::strtol(::strtok(nullptr,"\n "),0,10);
      checkField("Password",cfg);
      accounts.emplace_back(num,::strtok(nullptr,"\n "));
    }else{
      if(accounts.empty())throw failure("An account number is required.");
      if(!::strcmp("UDP-port-number",tok))break;
      else throw failure("Expected UDP-port-number");
    }
  }
  fds[1].fd=localbuf.sock_=udpServer(::strtok(nullptr,"\n "));
#ifdef __linux__
  setNonblocking(fds[1].fd);
#endif

  checkField("Login-attempts-interval-in-milliseconds",cfg);
  loginPause=::strtol(::strtok(nullptr,"\n "),0,10);
  checkField("Keepalive-interval-in-milliseconds",cfg);
  int const keepaliveInterval=::strtol(::strtok(nullptr,"\n "),0,10);

  login();
  fds[0].events=fds[1].events=POLLIN;
  for(;;){
    if(0==pollWrapper(fds,2,keepaliveInterval)){
      try{
        if(!pendingRequests.empty())throw failure("No reply from CMW");
        ::middleBack::Marshal(cmwBuf,Keepalive);
        fds[0].events|=POLLOUT;
        pendingRequests.push_back(nullptr);
      }catch(::std::exception const& e){
        syslogWrapper(LOG_ERR,"Keepalive: %s",e.what());
        reset("Keepalive problem");
      }
      continue;
    }

    localbuf.Reset();
    try{
      if(fds[0].revents&POLLIN&&cmwBuf.GotPacket()){
        do{
          assert(!pendingRequests.empty());
          if(pendingRequests.front().get()){
            auto const& req=*pendingRequests.front();
            if(GiveBool(cmwBuf)){
              req.saveLastruntime();
              setDirectory(req.path.c_str());
              GiveFiles(cmwBuf);
              ::middleFront::Marshal(localbuf,true);
            }else ::middleFront::Marshal(localbuf,false,
                               {"CMW:",GiveStringView(cmwBuf)});
            localbuf.Send((::sockaddr*)&req.front,req.frontLen);
            localbuf.Reset();
          }
          pendingRequests.erase(::std::begin(pendingRequests));
        }while(cmwBuf.NextMessage());
      }
    }catch(connectionLost const& e){
      syslogWrapper(LOG_ERR,"Got end of stream notice: %s",e.what());
      reset("CMW stopped before your request was processed");
      continue;
    }catch(::std::exception const& e){
      syslogWrapper(LOG_ERR,"Problem handling reply from CMW %s",e.what());
      assert(!pendingRequests.empty());
      if(pendingRequests.front().get()){
        auto const& req=*pendingRequests.front();
        ::middleFront::Marshal(localbuf,false,{e.what()});
        localbuf.Send((::sockaddr*)&req.front,req.frontLen);
      }
      pendingRequests.erase(::std::begin(pendingRequests));
    }

    if(fds[0].revents&POLLOUT&&sendData())fds[0].events=POLLIN;

    if(fds[1].revents&POLLIN){
      auto& req=*pendingRequests.emplace_back(::std::make_unique<cmwRequest>());
      bool gotAddr=false;
      try{
        localbuf.GetPacket((::sockaddr*)&req.front,&req.frontLen);
        gotAddr=true;
        new(&req)cmwRequest(localbuf);
        ::middleBack::Marshal(cmwBuf,Generate,req);
      }catch(::std::exception const& e){
        syslogWrapper(LOG_ERR,"Accept request: %s",e.what());
        if(gotAddr){
          ::middleFront::Marshal(localbuf,false,{e.what()});
          localbuf.Send((::sockaddr*)&req.front,req.frontLen);
        }
        pendingRequests.pop_back();
        continue;
      }
      if(!sendData())fds[0].events|=POLLOUT;
    }
  }
}

int main (int ac,char** av){
  try{
    ::openlog(*av,LOG_PID|LOG_NDELAY,LOG_USER);
    if(ac!=2)throw failure("Usage: ")<<*av<<" config-file-name";
    cmwAmbassador(*(av+1));
  }catch(::std::exception const& e){
    syslogWrapper(LOG_ERR,"Program ending: %s",e.what());
  }catch(...){
    syslogWrapper(LOG_ERR,"Unknown exception!");
  }
  return EXIT_FAILURE;
}
