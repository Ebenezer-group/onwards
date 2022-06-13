#include<cmw_Buffer.hh>
#include"credentials.hh"
#include"messageIDs.hh"

#include<deque>
#include<cassert>
#include<ctime>
#ifdef __linux__
#include<linux/sctp.h>//May need libsctp-dev
#else
#include<netinet/sctp.h>
#endif
using namespace ::cmw;

struct Socky{
  ::sockaddr_in6 addr;
  ::socklen_t len=sizeof addr;
};

struct cmwRequest{
  Socky const frnt;
 private:
  static inline ::int32_t prevTime;
  ::int32_t const bday;
  MarshallingInt const acctNbr;
  FixedString120 path;
  char *mdlFile;
  FileWrapper fl;

  bool marshalFile (char const *name,SendBuffer& buf)const{
    struct ::stat sb;
    if(::stat(name,&sb)<0)raise("stat",name,errno);
    if(sb.st_mtime<=prevTime)return false;

    receive(buf,{'.'==name[0]||name[0]=='/'?::std::strrchr(name,'/')+1:name,
                 {"\0",1}});
    buf.receiveFile(name,sb.st_size);
    return true;
  }

 public:
  cmwRequest (Socky const& ft,auto& buf):frnt{ft}
     ,bday{static_cast<::int32_t>(::std::time(nullptr))},acctNbr{buf},path{buf}{
    if(path.bytesAvailable()<3)raise("No room for file suffix");
    mdlFile=::std::strrchr(path(),'/');
    if(nullptr==mdlFile)raise("cmwRequest didn't find /");
    *mdlFile=0;
    setDirectory(path());
    *mdlFile='/';
    char last[60];
    ::std::snprintf(last,sizeof last,".%s.last",++mdlFile);
    ::new(&fl)FileWrapper(last,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP);
    switch(::pread(fl(),&prevTime,sizeof prevTime,0)){
      case 0:prevTime=0;break;
      case -1:raise("pread",errno);
    }
  }

  void marshal (SendBuffer& buf)const{
    acctNbr.marshal(buf);
    if(auto ind=buf.reserveBytes(1);!buf.receive(ind,marshalFile(mdlFile,buf)))
      receive(buf,{mdlFile,{"\0",1}});

    ::int8_t updatedFiles=0;
    auto const idx=buf.reserveBytes(sizeof updatedFiles);
    FileBuffer f{mdlFile,O_RDONLY};
    while(auto tok=f.getline()){
      if(!::std::strncmp(tok,"//",2))continue;
      if(!::std::strcmp(tok,"--"))break;
      if(marshalFile(tok,buf))++updatedFiles;
    }
    buf.receive(idx,updatedFiles);
  }

  auto getFileName (){
    Write(fl(),&bday,sizeof bday);
    return path.append(".hh");
  }
};
#include"cmwA.mdl.hh"

void checkField (char const *fld,char const *actl){
  if(::std::strcmp(fld,actl))bail("Expected %s",fld);
}

GetaddrinfoWrapper gai("75.23.62.38","56789",SOCK_STREAM);
::pollfd fds[2];
int loginPause;
cmwCredentials cred;
BufferCompressed<SameFormat> cmwBuf{1101000};

void login (){
  ::back::marshal<messageID::login>(cmwBuf,cred,cmwBuf.getSize());
  for(;;){
    cmwBuf.sock_=::socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);
    if(0==::connect(cmwBuf.sock_,gai().ai_addr,gai().ai_addrlen))break;
    ::std::printf("connect %d\n",errno);
    ::close(cmwBuf.sock_);
    ::sleep(loginPause);
  }

  while(!cmwBuf.flush());
  fds[0].fd=cmwBuf.sock_;
  fds[0].events=POLLIN|POLLRDHUP;
  ::sctp_paddrparams pad{};
  pad.spp_address.ss_family=AF_INET;
  pad.spp_hbinterval=240000;
  pad.spp_flags=SPP_HB_ENABLE;
  if(::setsockopt(fds[0].fd,IPPROTO_SCTP,SCTP_PEER_ADDR_PARAMS
                  ,&pad,sizeof pad)==-1)bail("setsockopt %d",errno);
  while(!cmwBuf.gotPacket());
  if(!giveBool(cmwBuf))bail("Login:%s",cmwBuf.giveStringView().data());
}

::std::deque<cmwRequest> pendingRequests;
BufferStack<SameFormat> frntBuf;

void reset (char const *context,char const *detail=""){
  ::syslog(LOG_ERR,"%s:%s",context,detail);
  frntBuf.reset();
  ::front::marshal<false,udpPacketMax>(frntBuf,{context," ",detail});
  for(auto& r:pendingRequests){
    frntBuf.send((::sockaddr*)&r.frnt.addr,r.frnt.len);
  }
  pendingRequests.clear();
  cmwBuf.compressedReset();
  login();
}

template<bool res>
void toFront (Socky const& s,auto...t){
  frntBuf.reset();
  ::front::marshal<res,udpPacketMax>(frntBuf,{t...});
  frntBuf.send((::sockaddr*)&s.addr,s.len);
}

int main (int ac,char **av)try{
  ::openlog(av[0],LOG_PID|LOG_NDELAY,LOG_USER);
  if(ac!=2)bail("Usage: cmwA config-file");
  FileBuffer cfg{av[1],O_RDONLY};
  checkField("UserID",cfg.getline(' '));
  cred.userID=cfg.getline();
  checkField("Password",cfg.getline(' '));
  cred.password=cfg.getline();
  checkField("UDP-port-number",cfg.getline(' '));
  fds[1].fd=frntBuf.sock_=udpServer(cfg.getline());
  fds[1].events=POLLIN;

  checkField("Login-interval-in-seconds",cfg.getline(' '));
  loginPause=fromChars(cfg.getline());
  login();

  for(;;){
    pollWrapper(fds,2);
    if(fds[0].revents&(POLLRDHUP|POLLERR)){
      reset("Back tier vanished");
      continue;
    }
    try{
      if(fds[0].revents&POLLIN&&cmwBuf.gotPacket()){
        do{
          assert(!pendingRequests.empty());
          auto& req=pendingRequests.front();
          if(giveBool(cmwBuf)){
            getFile(req.getFileName(),cmwBuf);
            toFront<true>(req.frnt);
          }else toFront<false>(req.frnt,"CMW:",cmwBuf.giveStringView());
          pendingRequests.pop_front();
        }while(cmwBuf.nextMessage());
      }
    }catch(::std::exception& e){
      ::syslog(LOG_ERR,"Reply from CMW %s",e.what());
      assert(!pendingRequests.empty());
      toFront<false>(pendingRequests.front().frnt,e.what());
      pendingRequests.pop_front();
    }

    try{
      if(fds[0].revents&POLLOUT&&cmwBuf.flush())fds[0].events&=~POLLOUT;
    }catch(::std::exception& e){
      reset("toBack",e.what());
    }

    if(fds[1].revents&POLLIN){
      Socky frnt;
      bool gotAddr=false;
      cmwRequest *req=nullptr;
      try{
        gotAddr=frntBuf.getPacket((::sockaddr*)&frnt.addr,&frnt.len);
        req=&pendingRequests.emplace_back(frnt,frntBuf);
        back::marshal<messageID::generate,700000>(cmwBuf,*req);
        fds[0].events|=POLLOUT;
      }catch(::std::exception& e){
        ::syslog(LOG_ERR,"Accept request:%s",e.what());
        if(gotAddr)toFront<false>(frnt,e.what());
        if(req)pendingRequests.pop_back();
      }
    }
  }
}catch(::std::exception& e){bail("Oops:%s",e.what());}
