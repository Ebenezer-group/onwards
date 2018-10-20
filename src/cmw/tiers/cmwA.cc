#include"account.hh"
#include<cmw/Buffer.hh>
#include<cmw/ErrorWords.hh>
#include"messageIDs.hh"
#include<cmw/wrappers.hh>

#include<memory>//unique_ptr
#include<vector>
#include<assert.h>
#include<errno.h>
#include<fcntl.h>//open
#include<stdint.h>
#include<stdio.h>
#include<stdlib.h>
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
  ::socklen_t frontLen=sizeof front;
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
    ::snprintf(lastrun,sizeof lastrun,"%s.lastrun",middleFile);
    fd=::open(lastrun,O_RDWR);
    previousTime=0;
    if(fd>=0){
      if(::pread(fd,&previousTime,sizeof previousTime,0)==-1){
        auto err=errno;
        ::close(fd);
        throw failure("pread ")<<err;
      }
    }else{
      fd=::open(lastrun,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
      if(fd<0)throw failure("open ")<<lastrun<<" "<<errno;
    }
  }

  void saveRuntime ()const{Write(fd,&now,sizeof now);}

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
    ind=buf.ReserveBytes(sizeof updatedFiles);
    FILE_wrapper f{middleFile,"r"};
    while(auto line=f.fgets()){
      char const* tok=::strtok(line,"\n ");
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

void bail (char const* a,char const* b=""){
  syslogWrapper(LOG_ERR,"%s%s",a,b);
  ::exit(EXIT_FAILURE);
}

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

  void login (){
    ::middleBack::Marshal(cmwBuf,Login,accounts,cmwBuf.GetSize());
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

    while(!cmwBuf.Flush());
    while(!cmwBuf.GotPacket());
    if(GiveBool(cmwBuf)){
      setNonblocking(fds[0].fd);
      fds[0].events=POLLIN;
    }else{
      auto v=GiveStringView(cmwBuf);
      *(const_cast<char*>(v.data())+v.length())='\0';
      bail("Login:",v.data());
    }
  }

  void reset (char const* context,char const* detail){
    syslogWrapper(LOG_ERR,"%s:%s",context,detail);
    ::middleFront::Marshal(localbuf,false,{context," ",detail});
    for(auto& r:pendingRequests)
      if(r.get())localbuf.Send((::sockaddr*)&r->front,r->frontLen);
    pendingRequests.clear();
    closeSocket(fds[0].fd);
    cmwBuf.CompressedReset();
    login();
  }

  bool sendData (){
    try{return cmwBuf.Flush();}catch(::std::exception const& e){
      reset("sendData",e.what());
      return true;
    }
  }

public:
  cmwAmbassador (char*);
};

void checkField (char const* fld,FILE_wrapper& f){
  if(::strcmp(fld,::strtok(f.fgets()," ")))bail("Expected ",fld);
}

cmwAmbassador::cmwAmbassador (char* configfile):cmwBuf(1100000){
  FILE_wrapper cfg(configfile,"r");
  while(char const* tok=::strtok(cfg.fgets()," ")){
    if(!::strcmp("Account-number",tok)){
      auto num=fromChars(::strtok(nullptr,"\n "));
      checkField("Password",cfg);
      accounts.emplace_back(num,::strtok(nullptr,"\n "));
    }else{
      if(accounts.empty())bail("An account number is required.");
      if(!::strcmp("UDP-port-number",tok))break;
      else bail("Expected UDP-port-number");
    }
  }
  fds[1].fd=localbuf.sock_=udpServer(::strtok(nullptr,"\n "));
  fds[1].events=POLLIN;
#ifdef __linux__
  setNonblocking(fds[1].fd);
#endif

  checkField("Login-attempts-interval-in-milliseconds",cfg);
  loginPause=fromChars(::strtok(nullptr,"\n "));
  checkField("Keepalive-interval-in-milliseconds",cfg);
  int const keepaliveInterval=fromChars(::strtok(nullptr,"\n "));

  login();
  for(;;){
    if(0==pollWrapper(fds,2,keepaliveInterval)){
      if(pendingRequests.empty())
        try{
          ::middleBack::Marshal(cmwBuf,Keepalive);
          fds[0].events|=POLLOUT;
          pendingRequests.push_back(nullptr);
        }catch(::std::exception const& e){reset("Keepalive",e.what());}
      else reset("Keepalive","No reply from CMW");
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
              req.saveRuntime();
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
    }catch(fiasco const& e){
      reset("fiasco",e.what());
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
      bool gotAddr=false;
      cmwRequest* req=nullptr;
      try{
        req=&*pendingRequests.emplace_back(::std::make_unique<cmwRequest>());
        localbuf.GetPacket((::sockaddr*)&req->front,&req->frontLen);
        gotAddr=true;
        new(req)cmwRequest(localbuf);
        ::middleBack::Marshal(cmwBuf,Generate,*req);
      }catch(::std::exception const& e){
        syslogWrapper(LOG_ERR,"Accept request:%s",e.what());
        if(gotAddr){
          ::middleFront::Marshal(localbuf,false,{e.what()});
          localbuf.Send((::sockaddr*)&req->front,req->frontLen);
        }
        if(req)pendingRequests.pop_back();
        continue;
      }
      if(!sendData())fds[0].events|=POLLOUT;
    }
  }
}

int main (int ac,char** av){
  try{
    ::openlog(av[0],LOG_PID|LOG_NDELAY,LOG_USER);
    if(ac!=2)bail("Usage: cmwA config-file-name");
    cmwAmbassador{av[1]};
  }catch(::std::exception const& e){bail("Oops:",e.what());
  }catch(...){bail("Unknown exception!");}
}
