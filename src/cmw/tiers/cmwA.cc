#include<cmw/BufferImpl.hh>
#include"account.hh"
#include"messageIDs.hh"

#include<memory>//unique_ptr
#include<vector>
#include<assert.h>
#include<errno.h>
#include<stdint.h>
#include<stdio.h>
#include<string.h>
#include<syslog.h>
#include<sys/stat.h>
#include<time.h>
#include<unistd.h>//pread
#ifdef __linux__
#include<linux/sctp.h>//sockaddr_in6,socklen_t
#else
#include<netinet/sctp.h>
#endif
#include<arpa/inet.h>//inet_pton
::int32_t previousTime;
using namespace ::cmw;

bool marshalFile (char const* name,SendBuffer& buf){
  struct ::stat sb;
  if(::stat(name,&sb)<0)raise("stat",name,errno);
  if(sb.st_mtime<=previousTime)return false;
  if('.'==name[0]||name[0]=='/')receive(buf,::strrchr(name,'/')+1,1);
  else receive(buf,name,1);

  fileWrapper f(name,O_RDONLY);
  buf.receiveFile(f.d,sb.st_size);
  return true;
}

struct cmwRequest{
  ::sockaddr_in6 frnt;
  ::socklen_t frntLn;
  MarshallingInt const acctNbr;
  FixedString120 path;
  ::int32_t now;
  char* mdlFile;
  fileWrapper fl;

  cmwRequest ():frntLn(sizeof frnt){}

  template<class R>
  explicit cmwRequest (ReceiveBuffer<R>& buf):acctNbr(buf),path(buf)
                     ,now(::time(nullptr)){
    char* const pos=::strrchr(path(),'/');
    if(nullptr==pos)raise("cmwRequest didn't find /");
    if(path.bytesAvailable()<3)raise("No room for file suffix");
    *pos=0;
    setDirectory(path());
    *pos='/';
    mdlFile=pos+1;
    char last[60];
    ::snprintf(last,sizeof last,".%s.last",mdlFile);
    ::new(&fl)fileWrapper(last,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    switch(::pread(fl.d,&previousTime,sizeof previousTime,0)){
      default:break;
      case 0:previousTime=0;break;
      case -1:raise("pread",errno);
    }
  }

  void marshal (SendBuffer& buf)const{
    acctNbr.marshal(buf);
    if(auto ind=buf.reserveBytes(1);!buf.receive(ind,marshalFile(mdlFile,buf)))
      receive(buf,mdlFile,1);

    ::int8_t updatedFiles=0;
    auto const idx=buf.reserveBytes(sizeof updatedFiles);
    FILEwrapper f{mdlFile,"r"};
    while(auto line=f.fgets()){
      char const* tok=::strtok(line,"\n \r");
      if(!::strncmp(tok,"//",2))continue;
      if(!::strcmp(tok,"--"))break;
      if(marshalFile(tok,buf))++updatedFiles;
    }
    buf.receive(idx,updatedFiles);
  }

  auto outputFile ()const{
    Write(fl.d,&now,sizeof now);
    ::strcat(mdlFile,".hh");
    return path.c_str();
  }
};
#include"cmwA.mdl.hh"

class cmwAmbassador{
  BufferCompressed<
#ifdef CMW_ENDIAN_BIG
    LeastSignificantFirst
#else
    SameFormat
#endif
  > cmwBuf;

  BufferStack<SameFormat> frntBuf;
  ::std::vector<cmwAccount> accounts;
  ::std::vector<::std::unique_ptr<cmwRequest>> pendingRequests;
  ::pollfd fds[2];
  int loginPause;

  void login (){
    ::back::marshal<messageID::login>(cmwBuf,accounts,cmwBuf.getSize());
    for(;;){
      int s=::socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);
      ::sockaddr_in addr{};
      addr.sin_family=AF_INET;
      if(::inet_pton(AF_INET,"75.23.62.38",&addr.sin_addr)<=0)
        bail("inet_pton",errno);
      addr.sin_port=
#ifdef CMW_ENDIAN_BIG
        htons(56790);
#else
        htons(56789);
#endif
      if(0==::connect(s,(::sockaddr*)&addr,sizeof(addr))){
        fds[0].fd=cmwBuf.sock_=s;
        break;
      }
      ::printf("connect %d\n",errno);
      ::close(s);
      pollWrapper(nullptr,0,loginPause);
    }

    while(!cmwBuf.flush());
    fds[0].events=POLLIN;
    ::sctp_paddrparams paddr{};
    paddr.spp_address.ss_family=AF_INET;
    paddr.spp_hbinterval=240000;
    paddr.spp_flags=SPP_HB_ENABLE;
    if(::setsockopt(fds[0].fd,IPPROTO_SCTP,SCTP_PEER_ADDR_PARAMS
                    ,&paddr,sizeof paddr)==-1)bail("setsockopt",errno);
    while(!cmwBuf.gotPacket());
    if(!giveBool(cmwBuf))bail("Login:%s",cmwBuf.giveStringView().data());
    if(setNonblocking(fds[0].fd)==-1)bail("setNonb:%d",errno);
  }

  void reset (char const* context,char const* detail=""){
    ::syslog(LOG_ERR,"%s:%s",context,detail);
    frntBuf.reset();
    ::front::marshal<false>(frntBuf,{context," ",detail});
    for(auto& r:pendingRequests)
      frntBuf.send((::sockaddr*)&r->frnt,r->frntLn);
    pendingRequests.clear();
    cmwBuf.compressedReset();
    login();
  }

  bool sendData ()try{return cmwBuf.flush();}
  catch(::std::exception& e){reset("sendData",e.what());return true;}

  template<bool res,class...T>void outFront (cmwRequest const& req,T...t){
    frntBuf.reset();
    ::front::marshal<res>(frntBuf,{t...});
    frntBuf.send((::sockaddr*)&req.frnt,req.frntLn);
  }

public:
  cmwAmbassador (char*);
};

auto checkField=[](char const* fld,char const* actl){
  if(::strcmp(fld,actl))bail("Expected %s",fld);
  return ::strtok(nullptr,"\n \r");
};

cmwAmbassador::cmwAmbassador (char* config):cmwBuf(1101000){
  FILEwrapper cfg{config,"r"};
  char const* tok;
  while((tok=::strtok(cfg.fgets()," "))&&!::strcmp("Account-number",tok)){
    auto num=fromChars(::strtok(nullptr,"\n \r"));
    tok=checkField("Password",::strtok(cfg.fgets()," "));
    accounts.emplace_back(num,::strdup(tok));
  }
  if(accounts.empty())bail("An account number is required.");
  fds[1].fd=frntBuf.sock_=udpServer(checkField("UDP-port-number",tok));
  fds[1].events=POLLIN;

  tok=checkField("Login-interval-in-milliseconds",::strtok(cfg.fgets()," "));
  loginPause=fromChars(tok);

  login();
  for(;;){
    pollWrapper(fds,2);
    try{
      if(fds[0].revents&POLLIN&&cmwBuf.gotPacket()){
        do{
          assert(!pendingRequests.empty());
          auto const& req=*pendingRequests.front();
          if(giveBool(cmwBuf)){
            getFile(req.outputFile(),cmwBuf);
            outFront<true>(req);
          }else outFront<false>(req,"CMW:",cmwBuf.giveStringView());
          pendingRequests.erase(::std::begin(pendingRequests));
        }while(cmwBuf.nextMessage());
      }
    }catch(Fiasco& e){
      reset("Fiasco",e.what());
      continue;
    }catch(::std::exception& e){
      ::syslog(LOG_ERR,"Reply from CMW %s",e.what());
      assert(!pendingRequests.empty());
      auto it=::std::begin(pendingRequests);
      outFront<false>(**it,e.what());
      pendingRequests.erase(it);
    }

    if(fds[0].revents&POLLOUT&&sendData())fds[0].events=POLLIN;
    if(fds[0].revents&POLLERR)reset("Lost contact");

    if(fds[1].revents&POLLIN){
      bool gotAddr=false;
      cmwRequest* req=nullptr;
      try{
        req=&*pendingRequests.emplace_back(::new cmwRequest);
        frntBuf.getPacket((::sockaddr*)&req->frnt,&req->frntLn);
        gotAddr=true;
        ::new(req)cmwRequest(frntBuf);
        ::back::marshal<messageID::generate>(cmwBuf,*req);
      }catch(::std::exception& e){
        ::syslog(LOG_ERR,"Accept request:%s",e.what());
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
  if(ac!=2)bail("Usage: cmwA config-file");
  cmwAmbassador{av[1]};
}catch(::std::exception& e){bail("Oops:%s",e.what());}
