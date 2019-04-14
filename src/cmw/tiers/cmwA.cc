#include<cmw/Buffer.hh>
#include"account.hh"
#include"messageIDs.hh"

#include<memory>//unique_ptr
#include<vector>
#include<assert.h>
#include<errno.h>
#include<stdint.h>
#include<stdio.h>
#include<string.h>
#include<sys/stat.h>
#include<time.h>
#include<unistd.h>//pread
#include<netinet/in.h>//sockaddr_in6,socklen_t
::int32_t previousTime;
using namespace ::cmw;

bool marshalFile (char const* name,SendBuffer& buf){
  struct ::stat sb;
  if(::stat(name,&sb)<0)raise("stat",name,errno);
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
  fileWrapper fl;

  cmwRequest (){}

  template<class R>
  explicit cmwRequest (ReceiveBuffer<R>& buf):accountNbr(buf),path(buf)
                     ,now(::time(nullptr)){
    char* const pos=::strrchr(path(),'/');
    if(nullptr==pos)raise("cmwRequest didn't find /");
    *pos='\0';
    setDirectory(path());
    middleFile=pos+1;
    char lastrun[60];
    ::snprintf(lastrun,sizeof lastrun,"%s.lastrun",middleFile);
    new(&fl)fileWrapper(lastrun,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    switch(::pread(fl.d,&previousTime,sizeof previousTime,0)){
      default:break;
      case 0:previousTime=0;break;
      case -1:raise("pread",errno);
    }
  }

  void Marshal (SendBuffer& buf)const{
    accountNbr.Marshal(buf);
    auto const ind=buf.ReserveBytes(1);
    if(marshalFile(middleFile,buf))buf.Receive(ind,true);
    else{
      buf.Receive(ind,false);
      Receive(buf,middleFile);
      InsertNull(buf);
    }

    int8_t updatedFiles=0;
    auto const ind2=buf.ReserveBytes(sizeof updatedFiles);
    FILE_wrapper f{middleFile,"r"};
    while(auto line=f.fgets()){
      char const* tok=::strtok(line,"\n \r");
      if(!::strncmp(tok,"//",2)||!::strcmp(tok,"fixedMessageLengths"))continue;
      if(!::strcmp(tok,"--"))break;
      if(marshalFile(tok,buf))++updatedFiles;
    }
    buf.Receive(ind2,updatedFiles);
  }

  void saveRuntime ()const{Write(fl.d,&now,sizeof now);}
};
#include"zz.middleBack.hh"
using namespace ::middleBack;
using namespace ::middleFront;

class cmwAmbassador{
  BufferCompressed<
#ifdef CMW_ENDIAN_BIG
    LeastSignificantFirst
#else
    SameFormat
#endif
  > cmwBuf;

  BufferStack<SameFormat> frontBuf;
  ::std::vector<cmwAccount> accounts;
  ::std::vector<::std::unique_ptr<cmwRequest>> pendingRequests;
  ::pollfd fds[2];
  int loginPause;

  void login (){
    Marshal<messageID::login>(cmwBuf,accounts,cmwBuf.GetSize());
    for(;;){
      fds[0].fd=cmwBuf.sock_=connectWrapper("97.116.189.144",
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
    fds[0].events=POLLIN;
    while(!cmwBuf.GotPacket());
    if(!giveBool(cmwBuf))
      bail("Login:%s",nullTerminate(giveStringView(cmwBuf)).data());
    if(setNonblocking(fds[0].fd)==-1)bail("setNonb:%d",errno);
  }

  void reset (char const* context,char const* detail){
    syslogWrapper(LOG_ERR,"%s:%s",context,detail);
    frontBuf.Reset();
    Marshal<false>(frontBuf,{context," ",detail});
    for(auto& r:pendingRequests)
      if(r.get())frontBuf.Send((::sockaddr*)&r->front,r->frontLen);
    pendingRequests.clear();
    closeSocket(fds[0].fd);
    cmwBuf.CompressedReset();
    login();
  }

  bool sendData ()try{return cmwBuf.Flush();}
  catch(::std::exception const& e){reset("sendData",e.what());return true;}

  template<bool res,class...T>void outFront (cmwRequest const& req,T...t){
    frontBuf.Reset();
    Marshal<res>(frontBuf,{t...});
    frontBuf.Send((::sockaddr*)&req.front,req.frontLen);
  }

public:
  cmwAmbassador (char*);
};

void checkField (char const* fld,FILE_wrapper& f){
  if(::strcmp(fld,::strtok(f.fgets()," ")))bail("Expected %s",fld);
}

cmwAmbassador::cmwAmbassador (char* configfile):cmwBuf(1100000){
  FILE_wrapper cfg{configfile,"r"};
  char const* tok;
  while((tok=::strtok(cfg.fgets()," "))&&!::strcmp("Account-number",tok)){
    auto num=fromChars(::strtok(nullptr,"\n \r"));
    checkField("Password",cfg);
    accounts.emplace_back(num,::strtok(nullptr,"\n \r"));
  }
  if(accounts.empty())bail("An account number is required.");
  if(::strcmp("UDP-port-number",tok))bail("Expected UDP-port-number");
  fds[1].fd=frontBuf.sock_=udpServer(::strtok(nullptr,"\n \r"));
  fds[1].events=POLLIN;
#ifdef __linux__
  if(setNonblocking(fds[1].fd)==-1)bail("setNonb:%d",errno);
#endif

  checkField("Login-attempts-interval-in-milliseconds",cfg);
  loginPause=fromChars(::strtok(nullptr,"\n \r"));
  checkField("Keepalive-interval-in-milliseconds",cfg);
  int const keepaliveInterval=fromChars(::strtok(nullptr,"\n \r"));

  login();
  for(;;){
    if(0==pollWrapper(fds,2,keepaliveInterval)){
      if(pendingRequests.empty())
        try{
          Marshal<messageID::keepalive>(cmwBuf);
          fds[0].events|=POLLOUT;
          pendingRequests.push_back(nullptr);
        }catch(::std::exception const& e){reset("Keepalive",e.what());}
      else reset("Keepalive","No reply from CMW");
      continue;
    }

    try{
      if(fds[0].revents&POLLIN&&cmwBuf.GotPacket()){
        do{
          assert(!pendingRequests.empty());
          if(pendingRequests.front().get()){
            auto const& req=*pendingRequests.front();
            if(giveBool(cmwBuf)){
              req.saveRuntime();
              setDirectory(req.path.c_str());
              giveFiles(cmwBuf);
              outFront<true>(req);
            }else outFront<false>(req,"CMW:",giveStringView(cmwBuf));
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
      if(pendingRequests.front().get())
        outFront<false>(*pendingRequests.front(),e.what());
      pendingRequests.erase(::std::begin(pendingRequests));
    }

    if(fds[0].revents&POLLOUT&&sendData())fds[0].events=POLLIN;

    if(fds[1].revents&POLLIN){
      bool gotAddr=false;
      cmwRequest* req=nullptr;
      try{
        req=&*pendingRequests.emplace_back(::std::make_unique<cmwRequest>());
        frontBuf.GetPacket((::sockaddr*)&req->front,&req->frontLen);
        gotAddr=true;
        new(req)cmwRequest(frontBuf);
        Marshal<messageID::generate>(cmwBuf,*req);
      }catch(::std::exception const& e){
        syslogWrapper(LOG_ERR,"Accept request:%s",e.what());
        if(gotAddr)outFront<false>(*req,e.what());
        if(req)pendingRequests.pop_back();
        continue;
      }
      if(!sendData())fds[0].events|=POLLOUT;
    }
  }
}

int main (int ac,char** av)try{
  ::openlog(av[0],LOG_PID|LOG_NDELAY,LOG_USER);
  if(ac!=2)bail("Usage: cmwA config-file-name");
  cmwAmbassador{av[1]};
}catch(::std::exception const& e){bail("Oops:%s",e.what());
}catch(...){bail("Unknown exception!");}
