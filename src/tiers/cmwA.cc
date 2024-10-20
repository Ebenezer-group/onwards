#include<cmwBuffer.hh>
#include"credentials.hh"

#include<deque>
#include<cassert>
#include<liburing.h>
#include<sys/mman.h>
#include<linux/sctp.h>
#include<signal.h>
#include<stdio.h>
#include<syslog.h>
#include<time.h>

using namespace ::cmw;

constexpr ::int32_t bufSize=1101000;
BufferCompressed<SameFormat,::std::int32_t,bufSize> cmwBuf;

struct Socky{
  ::sockaddr_in6 addr;
  ::socklen_t len=sizeof addr;
};

constexpr ::uint64_t reedTag=1;
constexpr ::uint64_t closTag=2;
constexpr ::uint64_t sendtoTag=3;
class ioUring{
  ::io_uring rng;
  BufferStack<SameFormat> frntBuf;

  auto getSqe (){
    if(auto e=::io_uring_get_sqe(&rng);e)return e;
    raise("getSqe");
  }

 public:
  void recvmsg (::msghdr& msg){
    auto e=getSqe();
    ::io_uring_prep_recvmsg(e,frntBuf.sock_,&msg,0);
    ::io_uring_sqe_set_data64(e,0);
  }

  ioUring (int sock,::msghdr& msg):frntBuf{sock}{
    ::io_uring_params ps{};
    ps.flags=IORING_SETUP_SINGLE_ISSUER|IORING_SETUP_DEFER_TASKRUN;
    ps.flags|=IORING_SETUP_NO_MMAP|IORING_SETUP_NO_SQARRAY|IORING_SETUP_REGISTERED_FD_ONLY;
    auto bff=::mmap(0,108000,PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(MAP_FAILED==bff)raise("mmap",errno);
    if(int rc=::io_uring_queue_init_mem(1024,&rng,&ps,bff,108000);rc<0)
      raise("ioUring",rc);
    int regfds[]={sock,0};
    if(::io_uring_register_files(&rng,regfds,2))raise("io reg");
    recvmsg(msg);
  }

  auto submit (){
    static int seen=0;
    ::io_uring_cq_advance(&rng,seen);
    int rc;
    while((rc=::io_uring_submit_and_wait(&rng,1))<0){
      if(-EINTR!=rc)raise("waitCqe",rc);
    }
    static ::io_uring_cqe *cqes[10];
    seen=::io_uring_peek_batch_cqe(&rng,&cqes[0],10);
    return ::std::span<::io_uring_cqe*>(&cqes[0],seen);
  }

  void reed (bool stale){
    auto e=getSqe();
    auto sp=cmwBuf.getDuo();
    ::io_uring_prep_recv(e,cmwBuf.sock_,sp.data(),sp.size(),0);
    ::io_uring_sqe_set_data64(e,reedTag);
    e->ioprio=IORING_RECVSEND_POLL_FIRST;
    if(stale)::io_uring_register_files_update(&rng,1,&cmwBuf.sock_,1);
  }

  void writ (){
    auto e=getSqe();
    auto sp=cmwBuf.outDuo();
    ::io_uring_prep_send(e,cmwBuf.sock_,sp.data(),sp.size(),0);
    ::io_uring_sqe_set_data64(e,~reedTag);
  }

  void clos (int fd){
    auto e=getSqe();
    ::io_uring_prep_close(e,fd);
    ::io_uring_sqe_set_data64(e,closTag);
  }

  void sendto (Socky const&,auto...);
} *ring;

struct cmwRequest{
  Socky const frnt;
 private:
  ::int32_t const bday;
  MarshallingInt const acctNbr;
  FixedString120 path;
  char* mdlFile;
  FileWrapper fl;
  inline static ::int32_t prevTime;

  static int marshalFile (char const* name,auto& buf){
    struct ::stat sb;
    if(::stat(name,&sb)<0)raise("stat",name,errno);
    if(sb.st_mtime<=prevTime)return 0;

    receive(buf,{'.'==name[0]||name[0]=='/'?::std::strrchr(name,'/')+1:name,
                 {"\0",1}});
    return buf.receiveFile(name,sb.st_size);
  }

 public:
  cmwRequest (auto& buf,Socky const& ft):frnt{ft}
      ,bday(::time(0)),acctNbr{buf},path{buf}{
    if(path.bytesAvailable()<3)raise("No room for file suffix");
    mdlFile=::std::strrchr(path(),'/');
    if(!mdlFile)raise("cmwRequest didn't find /");
    *mdlFile=0;
    setDirectory(path());
    *mdlFile='/';
    char last[60];
    ::snprintf(last,sizeof last,".%s.last",++mdlFile);
    fl=FileWrapper{last,O_RDWR|O_CREAT,0640};
    switch(::pread(fl(),&prevTime,sizeof prevTime,0)){
      case 0:prevTime=0;break;
      case -1:raise("pread",errno);
    }
  }

  void marshal (auto& buf)const{
    acctNbr.marshal(buf);
    auto ind=buf.reserveBytes(1);
    auto res=marshalFile(mdlFile,buf);
    if(res)ring->clos(res);
    else receive(buf,{mdlFile,::std::strlen(mdlFile)+1});
    buf.receiveAt(ind,res!=0);

    ::int8_t updatedFiles=0;
    auto const idx=buf.reserveBytes(sizeof updatedFiles);
    FileBuffer f{mdlFile,O_RDONLY};
    while(auto tok=f.getline().data()){
      if(!::std::strncmp(tok,"//",2)||!::std::strncmp(tok,"define",6))continue;
      if(!::std::strncmp(tok,"--",2))break;
      if(int fd=marshalFile(tok,buf);fd>0){
	++updatedFiles;
	ring->clos(fd);
      }
    }
    buf.receiveAt(idx,updatedFiles);
    ring->clos(f.fl());
    f.fl.release();
  }

  void xyz (){
    Write(fl(),&bday,sizeof bday);
    ring->clos(cmwBuf.giveFile(path.append(".hh")));
    ring->clos(fl());
    fl.release();
  }
};
#include"cmwA.mdl.hh"

void ioUring::sendto (Socky const& s,auto...t){
  frntBuf.reset();
  ::front::marshal<udpPacketMax>(frntBuf,{t...});
  auto sp=frntBuf.outDuo();
  auto e=getSqe();
  static Socky frnt2;
  frnt2=s;
  ::io_uring_prep_sendto(e,frntBuf.sock_,sp.data(),sp.size(),0
                         ,(sockaddr*)&frnt2.addr,frnt2.len);
  ::io_uring_sqe_set_data64(e,sendtoTag);
}

void bail (char const* fmt,auto...t)noexcept{
  ::syslog(LOG_ERR,fmt,t...);
  exitFailure();
}

void checkField (char const* fld,::std::string_view actl){
  if(actl!=fld)bail("Expected %s",fld);
}

void login (cmwCredentials const& cred,SockaddrWrapper const& sa,bool signUp=false){
  signUp? ::back::marshal<::messageID::signup>(cmwBuf,cred)
        : ::back::marshal<::messageID::login>(cmwBuf,cred,bufSize);
  cmwBuf.compress();
  cmwBuf.sock_=::socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);
  while(::connect(cmwBuf.sock_,(::sockaddr*)&sa,sizeof sa)<0){
    ::perror("connect");
    ::sleep(30);
  }

  while(!cmwBuf.flush());
  ::sctp_paddrparams pad{};
  pad.spp_address.ss_family=AF_INET;
  pad.spp_hbinterval=240000;
  pad.spp_flags=SPP_HB_ENABLE;
  if(::setsockopt(cmwBuf.sock_,IPPROTO_SCTP,SCTP_PEER_ADDR_PARAMS
                  ,&pad,sizeof pad)==-1)bail("setsockopt %d",errno);
  ring->reed(true);
  while(!cmwBuf.gotPacket());
  if(!giveBool(cmwBuf))bail("Login:%s",cmwBuf.giveStringView().data());
}

int main (int ac,char** av)try{
  ::openlog(av[0],LOG_PID|LOG_NDELAY,LOG_USER);
  if(ac<2||ac>3)bail("Usage: %s config-file [-signup]",av[0]);
  FileBuffer cfg{av[1],O_RDONLY};
  checkField("CMW-IP",cfg.getline(' '));
  SockaddrWrapper const sa(cfg.getline().data(),56789);
  checkField("UDP-port-number",cfg.getline(' '));
  BufferStack<SameFormat> rfrntBuf{udpServer(fromChars(cfg.getline().data()))};
  auto sp=rfrntBuf.getDuo();
  ::iovec iov{sp.data(),sp.size()};
  Socky frnt;
  ::msghdr mhdr{&frnt.addr,frnt.len,&iov,1,0,0,0};
  ring=new ioUring{rfrntBuf.sock_,mhdr};

  checkField("AmbassadorID",cfg.getline(' '));
  cmwCredentials cred(cfg.getline());
  checkField("Password",cfg.getline(' '));
  cred.password=cfg.getline();
  ::signal(SIGPIPE,SIG_IGN);
  login(cred,sa,ac==3);
  if(ac==3){
    ::printf("Signup was successful\n");
    ::std::exit(0);
  }
  ::std::deque<cmwRequest> pendingRequests;

  for(;;){
    for(auto cq:ring->submit()){
      if(cq->res<0||(cq->res==0&&cq->user_data!=closTag)){
        if(-EPIPE!=cq->res&&0!=cq->res)bail("op failed %llu %d",cq->user_data,cq->res);
        ::syslog(LOG_ERR,"Back tier vanished %llu %d",cq->user_data,cq->res);
        rfrntBuf.reset();
        ::front::marshal<udpPacketMax>(rfrntBuf,{"Back tier vanished"});
        for(auto& r:pendingRequests){
          rfrntBuf.send(&r.frnt.addr,r.frnt.len);
        }
        pendingRequests.clear();
        cmwBuf.compressedReset();
        ring->clos(cmwBuf.sock_);
        login(cred,sa);
      }else if(0==cq->user_data){
        ring->recvmsg(mhdr);
        cmwRequest* req=0;
        try{
          rfrntBuf.update(cq->res);
          req=&pendingRequests.emplace_back(rfrntBuf,frnt);
          ::back::marshal<::messageID::generate,700000>(cmwBuf,*req);
          cmwBuf.compress();
          ring->writ();
        }catch(::std::exception& e){
          ::syslog(LOG_ERR,"Accept request:%s",e.what());
          ring->sendto(frnt,e.what());
          if(req)pendingRequests.pop_back();
        }
      }else if(closTag==cq->user_data||sendtoTag==cq->user_data){
      }else if(reedTag==cq->user_data){
        assert(!pendingRequests.empty());
        auto& req=pendingRequests.front();
        try{
          if(cmwBuf.gotIt(cq->res)){
            if(giveBool(cmwBuf)){
              req.xyz();
              ring->sendto(req.frnt);
            }else ring->sendto(req.frnt,"CMW:",cmwBuf.giveStringView());
            pendingRequests.pop_front();
          }
        }catch(::std::exception& e){
          ::syslog(LOG_ERR,"Reply from CMW %s",e.what());
          ring->sendto(req.frnt,e.what());
          pendingRequests.pop_front();
        }
        ring->reed(false);
      }else if(!cmwBuf.all(cq->res))ring->writ();
    }
  }
}catch(::std::exception& e){bail("Oops:%s",e.what());}
