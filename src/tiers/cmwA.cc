#include<cmw_Buffer.hh>
#define CMW_MIDDLE
#include"credentials.hh"
#include"messageIDs.hh"

#include<deque>
#include<cassert>
#include<ctime>
#include<liburing.h>
#include<poll.h>
#include<linux/sctp.h>
#include<signal.h>
#include<syslog.h>

using namespace ::cmw;

void bail (char const *fmt,auto... t){
  ::syslog(LOG_ERR,fmt,t...);
  exitFailure();
}

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
      if(!::std::strncmp(tok,"//",2)||!::std::strncmp(tok,"define",6))continue;
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

GetaddrinfoWrapper gai("127.0.0.1","56789",SOCK_STREAM);
cmwCredentials cred;
BufferCompressed<SameFormat,1101000> cmwBuf{};

void login (){
  ::back::marshal<::messageID::login>(cmwBuf,cred,cmwBuf.getSize());
  for(;;){
    cmwBuf.sock_=::socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);
    if(0==::connect(cmwBuf.sock_,gai().ai_addr,gai().ai_addrlen))break;
    ::std::printf("connect %d\n",errno);
    ::close(cmwBuf.sock_);
    ::sleep(30);
  }

  while(!cmwBuf.flush());
  ::sctp_paddrparams pad{};
  pad.spp_address.ss_family=AF_INET;
  pad.spp_hbinterval=240000;
  pad.spp_flags=SPP_HB_ENABLE;
  if(::setsockopt(cmwBuf.sock_,IPPROTO_SCTP,SCTP_PEER_ADDR_PARAMS
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

::uint64_t const reedTag=1;
class ioUring{
  ::io_uring rng;

  auto getSqe (){
    if(auto e=::io_uring_get_sqe(&rng);e!=0)return e;
    raise("getSqe");
  }

 public:
  ioUring (int sock){
    if(int const rc=::io_uring_queue_init(16,&rng,0);rc<0)raise("ioUring",rc);
    auto e=getSqe();
    ::io_uring_prep_poll_multishot(e,sock,POLLIN);
    ::io_uring_sqe_set_data(e,nullptr);
    reed();
  }

  auto submit (){
    ::io_uring_cqe *cq;
a:  if(int rc=::io_uring_submit_and_wait_timeout(&rng,&cq,1,nullptr,nullptr);rc<0){
      if(-EINTR==rc)goto a;
      raise("waitCqe",rc);
    }
    auto pr=::std::make_pair(::io_uring_cqe_get_data(cq),cq->res);
    ::io_uring_cqe_seen(&rng,cq);
    return pr;
  }

  void reed (){
    auto e=getSqe();
    auto sp=cmwBuf.getDuo();
    ::io_uring_prep_recv(e,cmwBuf.sock_,sp.data(),sp.size(),0);
    ::io_uring_sqe_set_data64(e,reedTag);
  }

  void writ (){
    auto e=getSqe();
    ::io_uring_prep_send(e,cmwBuf.sock_,cmwBuf.compBuf,cmwBuf.compIndex,0);
    ::io_uring_sqe_set_data64(e,~reedTag);
  }
};

int main (int ac,char **av)try{
  ::openlog(av[0],LOG_PID|LOG_NDELAY,LOG_USER);
  if(ac!=2)bail("Usage: cmwA config-file");
  FileBuffer cfg{av[1],O_RDONLY};
  checkField("UserID",cfg.getline(' '));
  cred.userID=cfg.getline();
  if(cred.userID.size()>20)bail("UserID is too long");
  checkField("Password",cfg.getline(' '));
  cred.password=cfg.getline();
  ::signal(SIGPIPE,SIG_IGN);
  login();

  checkField("UDP-port-number",cfg.getline(' '));
  ioUring ring{frntBuf.sock_=udpServer(cfg.getline())};

  for(;;){
    auto const[buf,rc]=ring.submit();
    if(rc<=0){
      if(-EPIPE==rc||0==rc){
        reset("Back tier vanished");
        ring.reed();
        continue;
      }
      bail("op failed: %d",rc);
    }

    if(nullptr==buf){
      Socky frnt;
      bool gotAddr=false;
      cmwRequest *req=nullptr;
      try{
        gotAddr=frntBuf.getPacket((::sockaddr*)&frnt.addr,&frnt.len);
        req=&pendingRequests.emplace_back(frnt,frntBuf);
        ::back::marshal<::messageID::generate,700000>(cmwBuf,*req);
        cmwBuf.compress();
        ring.writ();
      }catch(::std::exception& e){
        ::syslog(LOG_ERR,"Accept request:%s",e.what());
        if(gotAddr)toFront<false>(frnt,e.what());
        if(req!=nullptr)pendingRequests.pop_back();
      }
      continue;
    }
    try{
      if((::uint64_t)buf&reedTag){
        if(cmwBuf.gotIt(rc)){
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
        ring.reed();
      }else
        if(!cmwBuf.all(rc))ring.writ();
    }catch(::std::exception& e){
      ::syslog(LOG_ERR,"Reply from CMW %s",e.what());
      assert(!pendingRequests.empty());
      toFront<false>(pendingRequests.front().frnt,e.what());
      pendingRequests.pop_front();
    }
  }
}catch(::std::exception& e){bail("Oops:%s",e.what());}
