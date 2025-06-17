#include<cmwBuffer.hh>
#include"credentials.hh"

#include<array>
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
struct Socky{
  ::sockaddr_in addr;
  ::socklen_t len=sizeof addr;
};

constexpr ::int32_t BufSize=1101000;
BufferCompressed<SameFormat,::int32_t,BufSize> cmwBuf;

class ioUring{
  ::io_uring rng;
  ::iovec iov;
  ::msghdr mhdr{0,sizeof(::sockaddr_in),&iov,0,0,0,0};
  int s2ind;
  int const udpSock;
  ::io_uring_buf_ring* bufRing;
  char* const bufBase;

  auto getSqe (bool internal=false){
    if(auto e=::io_uring_get_sqe(&rng);e)return e;
    if(internal)raise("getSqe");
    ::io_uring_submit_and_wait(&rng,0);
    return getSqe(true);
  }

 public:
  static constexpr int MaxBatch=10,NumBufs=4;
  static constexpr int Recvmsg=0,Recv=1,Send=2,Close=3,Sendto=4,Fsync=5;

  void recvmsg (){
    auto e=getSqe();
    ::io_uring_prep_recvmsg_multishot(e,udpSock,&mhdr,MSG_TRUNC);
    e->flags|=IOSQE_BUFFER_SELECT;
    e->buf_group=0;
    ::io_uring_sqe_set_data64(e,Recvmsg);
  }

  ioUring (int sock):udpSock{sock}
          ,bufBase{static_cast<char*>(::std::aligned_alloc(4096,udpPacketMax*NumBufs))}{
    if(!bufBase)raise("aligned_alloc failed");
    auto bff=::mmap(0,103000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(MAP_FAILED==bff)raise("mmap",errno);
    ::io_uring_params ps{};
    ps.flags=IORING_SETUP_SINGLE_ISSUER|IORING_SETUP_DEFER_TASKRUN;
    ps.flags|=IORING_SETUP_NO_MMAP|IORING_SETUP_NO_SQARRAY|IORING_SETUP_REGISTERED_FD_ONLY;
    int rc=::io_uring_queue_init_mem(1024,&rng,&ps,bff,103000);
    if(rc<0)raise("ioUring",rc);

    bufRing=::io_uring_setup_buf_ring(&rng,NumBufs,0,0,&rc);
    if(!bufRing)raise("setup_buf_ring failed",rc);
    int mask=::io_uring_buf_ring_mask(NumBufs);
    for(int i=0;i<NumBufs;i++){
      ::io_uring_buf_ring_add(bufRing,bufBase+i*udpPacketMax,udpPacketMax,i,mask,i);
    }
    ::std::array regfds={sock,0};
    if(::io_uring_register_files(&rng,regfds.data(),regfds.size()))raise("io reg");
  }

  auto check (auto const* cq,::Socky& s){
    if(~cq->flags&IORING_CQE_F_MORE){
      ::syslog(LOG_ERR,"recvmsg was disabled");
      recvmsg();
    }
    auto idx=cq->flags>>IORING_CQE_BUFFER_SHIFT;
    auto* o=::io_uring_recvmsg_validate(bufBase+idx*udpPacketMax,cq->res,&mhdr);
    if(!o)raise("recvmsg_validate");
    ::std::memcpy(&s.addr,::io_uring_recvmsg_name(o),o->namelen);
    return ::std::span<char>(static_cast<char*>(::io_uring_recvmsg_payload(o,&mhdr)),o->payloadlen);
  }

  auto submit (int bufsUsed){
    static int seen;
    ::__io_uring_buf_ring_cq_advance(&rng,bufRing,seen,bufsUsed);
    int rc;
    while((rc=::io_uring_submit_and_wait(&rng,1))<0){
      if(-EINTR!=rc)raise("waitCqe",rc);
    }
    s2ind=-1;
    static ::std::array<::io_uring_cqe*,MaxBatch> cqes;
    seen=::io_uring_peek_batch_cqe(&rng,cqes.data(),cqes.size());
    return ::std::span<::io_uring_cqe*>(cqes.data(),seen);
  }

  void recv (bool stale){
    auto e=getSqe();
    auto sp=cmwBuf.getDuo();
    ::io_uring_prep_recv(e,cmwBuf.sock_,sp.data(),sp.size(),0);
    ::io_uring_sqe_set_data64(e,Recv);
    if(sp.size()<=9)e->ioprio=IORING_RECVSEND_POLL_FIRST;
    if(stale)::io_uring_register_files_update(&rng,1,&cmwBuf.sock_,1);
  }

  void send (){
    auto e=getSqe();
    auto sp=cmwBuf.outDuo();
    ::io_uring_prep_send(e,cmwBuf.sock_,sp.data(),sp.size(),0);
    ::io_uring_sqe_set_data64(e,Send);
  }

  void close (int fd){
    auto e=getSqe();
    ::io_uring_prep_close(e,fd);
    ::io_uring_sqe_set_data64(e,Close);
    ::io_uring_sqe_set_flags(e,IOSQE_CQE_SKIP_SUCCESS);
  }

  void fsync (int fd){
    auto e=getSqe();
    ::io_uring_prep_fsync(e,fd,0);
    ::io_uring_sqe_set_data64(e,Fsync);
    ::io_uring_sqe_set_flags(e,IOSQE_CQE_SKIP_SUCCESS|IOSQE_IO_HARDLINK);
    this->close(fd);
  }

  void sendto (::Socky const&,auto...);
} *ring;

struct cmwRequest{
  ::Socky const frnt;
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
  cmwRequest (auto&& buf,::Socky const& ft):frnt{ft}
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
    int res=marshalFile(mdlFile,buf);
    if(res)ring->close(res);
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
        ring->close(fd);
      }
    }
    buf.receiveAt(idx,updatedFiles);
    ring->close(f.fl());
    f.fl.release();
  }

  void saveOutput (){
    Write(fl(),(char*)&bday,sizeof bday);
    ring->fsync(cmwBuf.giveFile(path.append(".hh")));
    ring->close(fl());
    fl.release();
  }
};
#include"cmwA.mdl.hh"

void ioUring::sendto (::Socky const& so,auto...t){
  if(++s2ind>=MaxBatch/2){
    ::io_uring_submit_and_wait(&rng,0);
    s2ind=0;
  }
  auto e=getSqe();
  static ::std::array<::std::pair<BufferStack<SameFormat>,::sockaddr_in>,MaxBatch/2> frnts;
  frnts[s2ind].first.reset();
  ::front::marshal<udpPacketMax>(frnts[s2ind].first,{t...});
  auto sp=frnts[s2ind].first.outDuo();
  frnts[s2ind].second=so.addr;
  ::io_uring_prep_sendto(e,udpSock,sp.data(),sp.size(),0
                         ,(sockaddr*)&frnts[s2ind].second,so.len);
  ::io_uring_sqe_set_flags(e,IOSQE_CQE_SKIP_SUCCESS);
  ::io_uring_sqe_set_data64(e,Sendto);
}

void bail (char const* fmt,auto...t)noexcept{
  ::syslog(LOG_ERR,fmt,t...);
  exitFailure();
}

void checkField (char const* fld,::std::string_view actl){
  if(actl!=fld)::bail("Expected %s",fld);
}

void login (::Credentials const& cred,auto& sa,bool signUp=false){
  signUp? ::back::marshal<::messageID::signup>(cmwBuf,cred)
        : ::back::marshal<::messageID::login>(cmwBuf,cred,BufSize);
  cmwBuf.compress();
  cmwBuf.sock_=::socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);
  while(::connect(cmwBuf.sock_,(::sockaddr*)&sa,sizeof sa)<0){
    ::perror("connect");
    ::sleep(30);
  }

  cmwBuf.flush();
  ::sctp_paddrparams pad{};
  pad.spp_address.ss_family=AF_INET;
  pad.spp_hbinterval=240000;
  pad.spp_flags=SPP_HB_ENABLE;
  if(::setsockopt(cmwBuf.sock_,IPPROTO_SCTP,SCTP_PEER_ADDR_PARAMS
                  ,&pad,sizeof pad)==-1)::bail("setsockopt %d",errno);
  ring->recv(true);
  while(!cmwBuf.gotPacket());
  if(!giveBool(cmwBuf))::bail("Login:%s",cmwBuf.giveStringView().data());
}

int main (int ac,char** av)try{
  ::openlog(av[0],LOG_PID|LOG_NDELAY,LOG_USER);
  if(ac<2||ac>3)::bail("Usage: %s config-file [-signup]",av[0]);
  FileBuffer cfg{av[1],O_RDONLY};
  ::checkField("CMW-IP",cfg.getline(' '));
  SockaddrWrapper const sa(cfg.getline().data(),56789);
  ::checkField("UDP-port-number",cfg.getline(' '));
  BufferStack<SameFormat> frntBuf{udpServer(fromChars(cfg.getline().data()))};

  ::checkField("AmbassadorID",cfg.getline(' '));
  ::Credentials cred(cfg.getline());
  ::checkField("Password",cfg.getline(' '));
  cred.password=cfg.getline();
  ::signal(SIGPIPE,SIG_IGN);
  ring=new ::ioUring{frntBuf.sock_};
  ::login(cred,sa,ac==3);
  if(ac==3){
    ::printf("Signup was successful\n");
    ::std::exit(0);
  }
  ring->recvmsg();

  ::std::deque<::cmwRequest> requests;
  for(int sentBytes=0,bufsUsed=::ioUring::NumBufs;;){
    auto cqs=ring->submit(bufsUsed);
    if(sentBytes)cmwBuf.adjustFrame(sentBytes);
    sentBytes=bufsUsed=0;
    for(auto const* cq:cqs){
      if(cq->res<=0){
        ::syslog(LOG_ERR,"Op failed %llu %d",cq->user_data,cq->res);
        if(cq->res<0&&-EPIPE!=cq->res)exitFailure();
        frntBuf.reset();
        ::front::marshal<udpPacketMax>(frntBuf,{"Back tier vanished"});
        for(auto& r:requests){frntBuf.send(&r.frnt.addr,r.frnt.len);}
        requests.clear();
        cmwBuf.compressedReset();
        ring->close(cmwBuf.sock_);
        ::login(cred,sa);
      }else if(::ioUring::Recvmsg==cq->user_data){
        ++bufsUsed;
        ::Socky frnt;
        int tracy=0;
        try{
          auto spn=ring->check(cq,frnt);
	  ++tracy;
          auto req=&requests.emplace_back(ReceiveBuffer<SameFormat,::int16_t>{spn},frnt);
	  ++tracy;
          ::back::marshal<::messageID::generate,700000>(cmwBuf,*req);
          cmwBuf.compress();
          ring->send();
        }catch(::std::exception& e){
          ::syslog(LOG_ERR,"Accept request:%s",e.what());
          if(tracy>0)ring->sendto(frnt,e.what());
          if(tracy>1)requests.pop_back();
        }
      }else if(::ioUring::Recv==cq->user_data){
        assert(!requests.empty());
        auto& req=requests.front();
        try{
          if(cmwBuf.gotIt(cq->res)){
            if(giveBool(cmwBuf)){
              req.saveOutput();
              ring->sendto(req.frnt);
            }else ring->sendto(req.frnt,"CMW:",cmwBuf.giveStringView());
            requests.pop_front();
          }
        }catch(::std::exception& e){
          ::syslog(LOG_ERR,"Reply from CMW %s",e.what());
          ring->sendto(req.frnt,e.what());
          requests.pop_front();
        }
        ring->recv(false);
      }else if(::ioUring::Send==cq->user_data)sentBytes+=cq->res;
      else ::bail("Unknown user_data %llu",cq->user_data);
    }
  }
}catch(::std::exception& e){::bail("Oops:%s",e.what());}
