#include<cmwBuffer.hh>
#include"credentials.hh"
#include"messageIDs.hh"

#include<deque>
#include<cassert>
#include<cstdio>
#include<ctime>
#include<arpa/inet.h>
#include<liburing.h>
#include<poll.h>
#include<linux/sctp.h>
#include<signal.h>
#include<syslog.h>

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

  static bool marshalFile (char const *name,auto& buf){
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
      ,bday(::std::time(nullptr)),acctNbr{buf},path{buf}{
    if(path.bytesAvailable()<3)raise("No room for file suffix");
    mdlFile=::std::strrchr(path(),'/');
    if(nullptr==mdlFile)raise("cmwRequest didn't find /");
    *mdlFile=0;
    setDirectory(path());
    *mdlFile='/';
    char last[60];
    ::std::snprintf(last,sizeof last,".%s.last",++mdlFile);
    fl=FileWrapper{last,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP};
    switch(::pread(fl(),&prevTime,sizeof prevTime,0)){
      case 0:prevTime=0;break;
      case -1:raise("pread",errno);
    }
  }

  void marshal (auto& buf)const{
    acctNbr.marshal(buf);
    if(auto ind=buf.reserveBytes(1);!buf.receiveAt(ind,marshalFile(mdlFile,buf)))
      receive(buf,{mdlFile,::std::strlen(mdlFile)+1});

    ::int8_t updatedFiles=0;
    auto const idx=buf.reserveBytes(sizeof updatedFiles);
    FileBuffer f{mdlFile,O_RDONLY};
    while(auto tok=f.getline().data()){
      if(!::std::strncmp(tok,"//",2)||!::std::strncmp(tok,"define",6))continue;
      if(!::std::strncmp(tok,"--",2))break;
      if(marshalFile(tok,buf))++updatedFiles;
    }
    buf.receiveAt(idx,updatedFiles);
  }

  auto getFileName (){
    Write(fl(),&bday,sizeof bday);
    return path.append(".hh");
  }
};
#include"cmwA.mdl.hh"

void bail (char const *fmt,auto... t){
  ::syslog(LOG_ERR,fmt,t...);
  exitFailure();
}

void checkField (char const *fld,::std::string_view actl){
  if(actl!=fld)bail("Expected %s",fld);
}

BufferCompressed<SameFormat,::std::int32_t,1101000> cmwBuf;

void login (cmwCredentials const& cred,bool signUp=false){
  signUp? ::back::marshal<::messageID::signup>(cmwBuf,cred)
        : ::back::marshal<::messageID::login>(cmwBuf,cred,cmwBuf.getSize());
  ::sockaddr_in sa{AF_INET,::htons(56789),{0},{0}};
  if(int rc=::inet_pton(AF_INET,"127.0.0.1",&sa.sin_addr);rc!=1)
    bail("inet_pton %d",rc);

  cmwBuf.sock_=::socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);
  while(0!=::connect(cmwBuf.sock_,(sockaddr*)&sa,sizeof(sa))){
    ::std::printf("connect %d\n",errno);
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
  if(signUp){
    ::std::printf("Signup was successful\n");
    ::std::exit(0);
  }
}

::uint64_t const reedTag=1;
class ioUring{
  ::io_uring rng;
  ::io_uring_cqe *cq=nullptr;

  auto getSqe (){
    if(auto e=::io_uring_get_sqe(&rng);e!=0)return e;
    raise("getSqe");
  }

 public:
  void multishot (int sock){
    ::io_uring_prep_poll_multishot(getSqe(),sock,POLLIN);
  }

  ioUring (int sock){
    ::io_uring_params ps{};
    ps.flags=IORING_SETUP_SINGLE_ISSUER|IORING_SETUP_DEFER_TASKRUN;
    if(int const rc=::io_uring_queue_init_params(16,&rng,&ps);rc<0)
      raise("ioUring",rc);
    multishot(sock);
    reed();
  }

  auto submit (){
    if(cq!=nullptr)::io_uring_cq_advance(&rng,1);
a:  if(int rc=::io_uring_submit_and_wait_timeout(&rng,&cq,1,nullptr,nullptr);rc<0){
      if(-EINTR==rc)goto a;
      raise("waitCqe",rc);
    }
    return *cq;
  }

  void reed (){
    auto e=getSqe();
    auto sp=cmwBuf.getDuo();
    ::io_uring_prep_recv(e,cmwBuf.sock_,sp.data(),sp.size(),0);
    ::io_uring_sqe_set_data64(e,reedTag);
    e->ioprio=IORING_RECVSEND_POLL_FIRST;
  }

  void writ (){
    auto e=getSqe();
    ::io_uring_prep_send(e,cmwBuf.sock_,cmwBuf.compBuf,cmwBuf.compIndex,0);
    ::io_uring_sqe_set_data64(e,~reedTag);
  }
};

BufferStack<SameFormat> frntBuf;

void toFront (Socky const& s,auto...t){
  frntBuf.reset();
  ::front::marshal<udpPacketMax>(frntBuf,{t...});
  frntBuf.send((::sockaddr*)&s.addr,s.len);
}

int main (int ac,char **av)try{
  ::openlog(av[0],LOG_PID|LOG_NDELAY,LOG_USER);
  if(ac<2||ac>3)bail("Usage: cmwA config-file [-signup]");
  FileBuffer cfg{av[1],O_RDONLY};
  checkField("AmbassadorID",cfg.getline(' '));
  cmwCredentials cred(cfg.getline());
  checkField("Password",cfg.getline(' '));
  cred.password=cfg.getline();
  ::signal(SIGPIPE,SIG_IGN);
  login(cred,ac==3);

  checkField("UDP-port-number",cfg.getline(' '));
  ioUring ring{frntBuf.sock_=udpServer(cfg.getline().data())};
  ::std::deque<cmwRequest> pendingRequests;

  for(;;){
    auto const cq=ring.submit();
    if(cq.res<=0){
      if(-EPIPE==cq.res||0==cq.res){
        ::syslog(LOG_ERR,"Back tier vanished");
        frntBuf.reset();
        ::front::marshal<udpPacketMax>(frntBuf,{"Back tier vanished"});
        for(auto& r:pendingRequests){
          frntBuf.send((::sockaddr*)&r.frnt.addr,r.frnt.len);
        }
        pendingRequests.clear();
        cmwBuf.compressedReset();
        login(cred);
        ring.reed();
        continue;
      }
      bail("op failed: %d",cq.res);
    }

    if(0==cq.user_data){
      if(!(cq.flags&IORING_CQE_F_MORE)){
        ::syslog(LOG_ERR,"Multishot");
        ring.multishot(frntBuf.sock_);
      }
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
        if(gotAddr)toFront(frnt,e.what());
        if(req!=nullptr)pendingRequests.pop_back();
      }
      continue;
    }

    if(cq.user_data&reedTag){
      try{
        if(cmwBuf.gotIt(cq.res)){
          do{
            assert(!pendingRequests.empty());
            auto& req=pendingRequests.front();
            if(giveBool(cmwBuf)){
              getFile(req.getFileName(),cmwBuf);
              toFront(req.frnt);
            }else toFront(req.frnt,"CMW:",cmwBuf.giveStringView());
            pendingRequests.pop_front();
          }while(cmwBuf.nextMessage());
        }
      }catch(::std::exception& e){
        ::syslog(LOG_ERR,"Reply from CMW %s",e.what());
        assert(!pendingRequests.empty());
        toFront(pendingRequests.front().frnt,e.what());
        pendingRequests.pop_front();
      }
      ring.reed();
    }else
      if(!cmwBuf.all(cq.res))ring.writ();
  }
}catch(::std::exception& e){bail("Oops:%s",e.what());}
