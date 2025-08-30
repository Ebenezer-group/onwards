#include<cmwBuffer.hh>
#include"credentials.hh"

#include<array>
#include<deque>
#include<cassert>
#include<liburing.h>
#include<linux/sctp.h>
#include<signal.h>
#include<stdio.h>
#include<syslog.h>
#include<time.h>
#include"ring_setup.hh"

using namespace ::cmw;
class FileBuffer{
  char buf[4096];
  char line[120];
  int ind=0;
  int bytes=0;
  int fd;

 public:
  FileBuffer (char const* nam,int flags):fd(Open(nam,flags)){}

  char getc (){
    if(ind>=bytes){
      bytes=Read(fd,buf,sizeof buf);
      ind=0;
    }
    return buf[ind++];
  }

  auto getline (char delim='\n'){
    ::std::size_t idx=0;
    while((line[idx]=getc())!=delim){
      if(line[idx]=='\r')raise("getline cr");
      if(++idx>=sizeof line)raise("getline size");
    }
    line[idx]=0;
    return ::std::string_view{line,idx};
  }

  auto operator() (){return fd;}
  void release (){fd=-1;}
  ~FileBuffer (){if(fd>0)::close(fd);}
};

struct Socky{
  ::sockaddr_in addr;
  ::socklen_t len=sizeof addr;
};

constexpr ::int32_t BufSize=1101000;
BufferCompressed<SameFormat,::int32_t,BufSize> cmwBuf;

auto roundUp (::uint64_t val,::uint64_t multiple){
  return ((val+multiple-1)/multiple)*multiple;
}

class ioUring{
  static constexpr int SubQ=512,MaxBatch=10,NumBufs=4;
  ::io_uring rng{};
  ::iovec iov;
  ::msghdr mhdr{0,sizeof(::sockaddr_in),&iov,0,0,0,0};
  int sentBytes=0,bufsUsed=NumBufs;
  int const pageSize,chunk1,chunk2,chunk3;
  char* const bufBase;
  ::io_uring_buf_ring* const bufRing;

 public:
  ioUring (int udpSock):
    pageSize(::sysconf(_SC_PAGESIZE))
    ,chunk1(roundUp(NumBufs*udpPacketMax,pageSize))
    ,chunk2(roundUp(NumBufs*sizeof(::io_uring_buf),pageSize))
    ,chunk3(roundUp(SubQ*(sizeof(::io_uring_sqe)+2*sizeof(::io_uring_cqe))+pageSize,pageSize))
    ,bufBase{mmapWrapper(chunk1+chunk2+chunk3)}
    ,bufRing{reinterpret_cast<::io_uring_buf_ring*>(bufBase+chunk1)}{
    //bufRing->tail=0;

    ::io_uring_params ps{};
    ps.flags=IORING_SETUP_SINGLE_ISSUER|IORING_SETUP_DEFER_TASKRUN;
    ps.flags|=IORING_SETUP_NO_MMAP|IORING_SETUP_NO_SQARRAY|IORING_SETUP_REGISTERED_FD_ONLY;

    if(int rc=uring_alloc_huge(SubQ,ps,&rng.sq,&rng.cq,bufBase+chunk1+chunk2
                               ,chunk3);rc<0)raise("alloc_huge",rc);
    int fd=::io_uring_setup(SubQ,&ps);
    if(fd<0)raise("ioUring",fd);
    uring_setup_ring(ps,rng);
    rng.enter_ring_fd=fd;
    rng.ring_fd=-1;
    rng.int_flags|=INT_FLAG_REG_RING|INT_FLAG_REG_REG_RING|INT_FLAG_APP_MEM;

    ::io_uring_buf_reg reg{};
    reg.ring_addr=(unsigned long) (uintptr_t)bufRing;
    reg.ring_entries=NumBufs;
    reg.bgid=0;
    if(int rc=::io_uring_register(fd,IORING_REGISTER_PBUF_RING|IORING_REGISTER_USE_REGISTERED_RING
                           ,&reg,1);rc<0)raise("reg buf ring",rc);

    int mask=NumBufs-1;
    for(int i=0;i<NumBufs;++i){
      ::io_uring_buf* buf=&bufRing->bufs[(bufRing->tail + i)&mask];
      buf->addr=(unsigned long) (uintptr_t)(bufBase+i*udpPacketMax);
      buf->len=udpPacketMax;
      buf->bid=i;
    }
    ::std::array regfds={udpSock,0};
    if(int rc=::io_uring_register(fd,IORING_REGISTER_FILES|IORING_REGISTER_USE_REGISTERED_RING,
                           regfds.data(),regfds.size());rc<0)raise("reg files",rc);
  }

  void submitWrapper (int wait=0){
    int rc;
    while((rc=::io_uring_submit_and_wait(&rng,wait))<0){
      if(-EINTR!=rc)raise("waitCqe",rc);
    }
  }

  auto submit (){
    static int seen;
    ::io_uring_cq_advance(&rng,seen);
    ::io_uring_buf_ring_advance(bufRing,bufsUsed);
    submitWrapper(1);
    if(sentBytes)cmwBuf.adjustFrame(sentBytes);
    bufsUsed=sentBytes=0;
    static ::std::array<::io_uring_cqe*,MaxBatch> cqes;
    seen=::uring_peek_batch_cqe(&rng,cqes.data(),cqes.size());
    return ::std::span(cqes.data(),seen);
  }

  void tallyBytes (int sent){sentBytes+=sent;}

  auto getSqe (bool internal=false){
    if(auto e=::uring_get_sqe(&rng);e)return e;
    if(internal)raise("getSqe");
    submitWrapper();
    return getSqe(true);
  }

  static constexpr int Recvmsg=0,Recv9=1,Recv=2,Send=3,Close=4,Sendto=5,Fsync=6,Write=7,WriteOutput=8;

  void recvmsg (){
    auto e=getSqe();
    ::io_uring_prep_recvmsg(e,0,&mhdr,0);
    ::io_uring_sqe_set_data64(e,Recvmsg);
    e->ioprio=IORING_RECV_MULTISHOT|IORING_RECVSEND_POLL_FIRST;
    e->flags=IOSQE_FIXED_FILE|IOSQE_BUFFER_SELECT;
    e->buf_group=0;
  }

  auto checkMsg (auto const* cq,::Socky& s){
    ++bufsUsed;
    if(~cq->flags&IORING_CQE_F_MORE){
      ::syslog(LOG_ERR,"recvmsg was disabled");
      recvmsg();
    }
    auto idx=cq->flags>>IORING_CQE_BUFFER_SHIFT;
    auto* o=::io_uring_recvmsg_validate(bufBase+idx*udpPacketMax,cq->res,&mhdr);
    if(!o)raise("recvmsg_validate");
    ::std::memcpy(&s.addr,::io_uring_recvmsg_name(o),o->namelen);
    return ::std::span(static_cast<char*>(::io_uring_recvmsg_payload(o,&mhdr)),
                                          o->payloadlen);
  }

  void recv9 (bool stale){
    auto e=getSqe();
    ::io_uring_prep_recv(e,1,cmwBuf.getuo(),9,MSG_WAITALL);
    ::io_uring_sqe_set_data64(e,Recv9);
    e->flags=IOSQE_FIXED_FILE;
    e->ioprio|=IORING_RECVSEND_POLL_FIRST;
    if(stale&&(::io_uring_register_files_update(&rng,1,&cmwBuf.sock_,1)<1))
      raise("reg files update");
  }

  void recv (auto sp){
    auto e=getSqe();
    ::io_uring_prep_recv(e,1,sp.data(),sp.size(),MSG_WAITALL);
    ::io_uring_sqe_set_data64(e,Recv);
    e->flags=IOSQE_FIXED_FILE;
  }

  void send (){
    auto e=getSqe();
    auto sp=cmwBuf.outDuo();
    ::io_uring_prep_send(e,1,sp.data(),sp.size(),0);
    ::io_uring_sqe_set_data64(e,Send);
    e->flags=IOSQE_FIXED_FILE;
  }

  void close (int fd){
    auto e=getSqe();
    ::io_uring_prep_close(e,fd);
    ::io_uring_sqe_set_data64(e,Close);
    e->flags=IOSQE_CQE_SKIP_SUCCESS;
  }

  void writeOutput (int fd,int bday,auto nm){
    auto e=getSqe();
    ::io_uring_prep_write(e,fd,&bday,sizeof bday,0);
    ::io_uring_sqe_set_data64(e,Write);
    e->flags=IOSQE_CQE_SKIP_SUCCESS;
    e=getSqe();
    int fd2=Open(nm,O_CREAT|O_WRONLY|O_TRUNC,0644);
    auto sp=cmwBuf.giveFile();
    ::io_uring_prep_write(e,fd2,sp.data(),sp.size(),0);
    ::io_uring_sqe_set_data64(e,WriteOutput);
    e->flags=IOSQE_CQE_SKIP_SUCCESS|IOSQE_IO_LINK;
    e=getSqe();
    ::io_uring_prep_fsync(e,fd2,0);
    ::io_uring_sqe_set_data64(e,Fsync);
    e->flags=IOSQE_CQE_SKIP_SUCCESS|IOSQE_IO_HARDLINK;
    this->close(fd2);
  }

  void sendto (int&,::Socky const&,auto...);
} *ring;

struct cmwRequest{
  ::Socky const frnt;
 private:
  ::int32_t const bday;
  MarshallingInt const acctNbr;
  FixedString120 path;
  char* mdlFile;
  int fd;
  inline static ::int32_t prevTime;

  static bool marshalFile (char const* name,auto& buf){
    struct ::stat sb;
    if(::stat(name,&sb)<0)raise("stat",name,errno);
    if(sb.st_mtime<=prevTime)return false;

    receive(buf,{'.'==name[0]||name[0]=='/'?::std::strrchr(name,'/')+1:name,
                 {"\0",1}});
    ring->close(buf.receiveFile(name,sb.st_size));
    return true;
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
    fd=Open(last,O_RDWR|O_CREAT,0640);
    switch(::read(fd,&prevTime,sizeof prevTime)){
      case 0:prevTime=0;break;
      case -1:raise("read dot",errno);
    }
  }

  void marshal (auto& buf)const{
    acctNbr.marshal(buf);
    auto ind=buf.reserveBytes(1);
    bool res=marshalFile(mdlFile,buf);
    if(!res)receive(buf,{mdlFile,::std::strlen(mdlFile)+1});
    buf.receiveAt(ind,res);

    ::int8_t updatedFiles=0;
    auto const idx=buf.reserveBytes(sizeof updatedFiles);
    FileBuffer f{mdlFile,O_RDONLY};
    while(auto tok=f.getline().data()){
      if(!::std::strncmp(tok,"//",2)||!::std::strncmp(tok,"define",6))continue;
      if(!::std::strncmp(tok,"--",2))break;
      if(marshalFile(tok,buf))++updatedFiles;
    }
    buf.receiveAt(idx,updatedFiles);
    ring->close(f());
    f.release();
  }

  void saveOutput (){
    ring->writeOutput(fd,bday,path.append(".hh"));
  }

  ~cmwRequest (){
    ring->close(fd);
  }
};
#include"cmwA.mdl.hh"

void ioUring::sendto (int& ind,::Socky const& so,auto...t){
  if(++ind>=MaxBatch/2){
    submitWrapper();
    ind=0;
  }

  auto e=getSqe();
  static ::std::array<::std::pair<BufferStack<SameFormat>,::sockaddr_in>,MaxBatch/2> frnts;
  frnts[ind].first.reset();
  ::front::marshal<udpPacketMax>(frnts[ind].first,{t...});
  auto sp=frnts[ind].first.outDuo();
  frnts[ind].second=so.addr;
  ::io_uring_prep_sendto(e,0,sp.data(),sp.size(),0
                         ,cast(&frnts[ind].second),so.len);
  ::io_uring_sqe_set_data64(e,Sendto);
  e->flags=IOSQE_CQE_SKIP_SUCCESS|IOSQE_FIXED_FILE;
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
  static int sock=::socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);
  while(::connect(sock,sa(),sizeof sa)<0){
    ::fprintf(stderr,"connect %s\n",::strerror(errno));
    ::sleep(30);
  }

  cmwBuf.sock_=sock;
  cmwBuf.flush();
  ::sctp_paddrparams pad{};
  pad.spp_address.ss_family=AF_INET;
  pad.spp_hbinterval=240000;
  pad.spp_flags=SPP_HB_ENABLE;
  if(::setsockopt(sock,IPPROTO_SCTP,SCTP_PEER_ADDR_PARAMS,&pad,sizeof pad)==-1)
    ::bail("setsockopt %d",errno);
  ring->recv9(true);
  sock=::socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);
  cmwBuf.gotPacket();
  if(!giveBool(cmwBuf))::bail("Login:%s",cmwBuf.giveStringView().data());
}

int main (int pid,char** av)try{
  ::openlog(av[0],LOG_PERROR|LOG_NDELAY,LOG_USER);
  int ac=pid;
  if(ac<2||ac>3)::bail("Usage: %s config-file [-signup]",av[0]);
  pid=::getpid();
  FileBuffer cfg{av[1],O_RDONLY};
  ::checkField("CMW-IP",cfg.getline(' '));
  SockaddrWrapper const sa(cfg.getline().data(),56789);
  ::checkField("UDP-port-number",cfg.getline(' '));
  BufferStack<SameFormat> frntBuf{udpServer(fromChars(cfg.getline()))};

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
  for(;;){
    auto cqs=ring->submit();
    int s2ind=-1;
    bool writePending{};
    for(auto const* cq:cqs){
      if(cq->res<=0){
        ::syslog(LOG_ERR,"%d Op failed %llu %d",pid,cq->user_data,cq->res);
        if(cq->res<0&&-EPIPE!=cq->res)exitFailure();
        frntBuf.reset();
        ::front::marshal<udpPacketMax>(frntBuf,{"Back tier vanished"});
        for(auto& r:requests){frntBuf.send(&r.frnt.addr,r.frnt.len);}
        requests.clear();
        cmwBuf.compressedReset();
        ring->close(cmwBuf.sock_);
        ::login(cred,sa);
      }else if(::ioUring::Recvmsg==cq->user_data){
        ::Socky frnt;
        int tracy=0;
        try{
          auto spn=ring->checkMsg(cq,frnt);
          ++tracy;
          auto& req=requests.emplace_back(ReceiveBuffer<SameFormat,::int16_t>{spn},frnt);
          ++tracy;
          ::back::marshal<::messageID::generate,700000>(cmwBuf,req);
          cmwBuf.compress();
          ring->send();
        }catch(::std::exception& e){
          ::syslog(LOG_ERR,"%d Accept request:%s",pid,e.what());
          if(tracy>0)ring->sendto(s2ind,frnt,e.what());
          if(tracy>1)requests.pop_back();
        }
      }else if(::ioUring::Send==cq->user_data)ring->tallyBytes(cq->res);
      else if(::ioUring::Recv9==cq->user_data)ring->recv(cmwBuf.gothd());
      else if(::ioUring::Recv==cq->user_data){
        assert(!requests.empty());
        auto& req=requests.front();
        try{
          cmwBuf.decompress();
          if(giveBool(cmwBuf)){
	    if(writePending){
              ring->submitWrapper();
              writePending={};
	    }
            req.saveOutput();
            writePending=true;
            ring->sendto(s2ind,req.frnt);
          }else ring->sendto(s2ind,req.frnt,"CMW:",cmwBuf.giveStringView());
          requests.pop_front();
        }catch(::std::exception& e){
          ::syslog(LOG_ERR,"%d Reply from CMW %s",pid,e.what());
          ring->sendto(s2ind,req.frnt,e.what());
          requests.pop_front();
        }
        ring->recv9(false);
      }else ::bail("Unknown user_data %llu",cq->user_data);
    }
  }
}catch(::std::exception& e){::bail("%d Oops:%s",pid,e.what());}
