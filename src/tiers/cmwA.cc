#include<cmw_BufferImpl.hh>
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
#include<unistd.h>//pread,sleep
#ifdef __linux__
#include<linux/sctp.h>//sockaddr_in6,socklen_t
#else
#include<netinet/sctp.h>
#endif
#include<arpa/inet.h>//inet_pton
::int32_t prevTime;
using namespace ::cmw;

bool marshalFile (char const *name,SendBuffer& buf){
  struct ::stat sb;
  if(::stat(name,&sb)<0)raise("stat",name,errno);
  if(sb.st_mtime<=prevTime)return false;
  if('.'==name[0]||name[0]=='/')receiveNull(buf,::strrchr(name,'/')+1);
  else receiveNull(buf,name);

  buf.receiveFile(FileWrapper{name,O_RDONLY}.d,sb.st_size);
  return true;
}

struct cmwRequest{
  ::sockaddr_in6 frnt;
  ::socklen_t frntLn;
private:
  ::int32_t bday;
  MarshallingInt const acctNbr;
  FixedString120 path;
  char *mdlFile;
  FileWrapper fl;
public:

  cmwRequest ():frntLn{sizeof frnt}{}

  template<class R>
  explicit cmwRequest (ReceiveBuffer<R>& buf):bday{::time(nullptr)}
       ,acctNbr{buf},path{buf}{
    if(path.bytesAvailable()<3)raise("No room for file suffix");
    char* const pos=::strrchr(path(),'/');
    if(nullptr==pos)raise("cmwRequest didn't find /");
    *pos=0;
    setDirectory(path());
    *pos='/';
    mdlFile=pos+1;
    char last[60];
    ::snprintf(last,sizeof last,".%s.last",mdlFile);
    ::new(&fl)FileWrapper(last,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    switch(::pread(fl.d,&prevTime,sizeof prevTime,0)){
      case 0:prevTime=0;break;
      case -1:raise("pread",errno);
    }
  }

  void marshal (SendBuffer& buf)const{
    acctNbr.marshal(buf);
    if(auto ind=buf.reserveBytes(1);!buf.receive(ind,marshalFile(mdlFile,buf)))
      receiveNull(buf,mdlFile);

    ::int8_t updatedFiles=0;
    auto const idx=buf.reserveBytes(sizeof updatedFiles);
    FILEwrapper f{mdlFile,"r"};
    while(auto line=f.fgets()){
      char const *tok=::strtok(line,"\n \r");
      if(!::strncmp(tok,"//",2))continue;
      if(!::strcmp(tok,"--"))break;
      if(marshalFile(tok,buf))++updatedFiles;
    }
    buf.receive(idx,updatedFiles);
  }

  auto getFileName (){
    Write(fl.d,&bday,sizeof bday);
    ::strcat(mdlFile,".hh");
    return path.data();
  }
};
#include"cmwA.mdl.hh"

auto checkField (char const *fld,char const *actl){
  if(::strcmp(fld,actl))bail("Expected %s",fld);
  return ::strtok(nullptr,"\n \r");
}

::sockaddr_in addr;
::std::vector<cmwAccount> accounts;
::pollfd fds[2];
int loginPause;
BufferCompressed<SameFormat> cmwBuf{1101000};
::std::vector<::std::unique_ptr<cmwRequest>> pendingRequests;
BufferStack<SameFormat> frntBuf;

void setup (int ac,char **av){
  ::openlog(av[0],LOG_PID|LOG_NDELAY,LOG_USER);
  if(ac!=2)bail("Usage: cmwA config-file");
  FILEwrapper cfg{av[1],"r"};
  char const *tok;
  while((tok=::strtok(cfg.fgets()," "))&&!::strcmp("Account-number",tok)){
    auto num=fromChars(::strtok(nullptr,"\n \r"));
    tok=checkField("Password",::strtok(cfg.fgets()," "));
    accounts.emplace_back(num,::strdup(tok));
  }
  if(accounts.empty())bail("An account number is required.");
  fds[1].fd=frntBuf.sock_=udpServer(checkField("UDP-port-number",tok));
  fds[1].events=POLLIN;

  tok=checkField("Login-interval-in-seconds",::strtok(cfg.fgets()," "));
  loginPause=fromChars(tok);

  if(::inet_pton(AF_INET,"75.23.62.38",&addr.sin_addr)<=0)
    bail("inet_pton",errno);
  addr.sin_family=AF_INET;
  addr.sin_port=htons(56789);
}

void login (){
  back::marshal<messageID::login>(cmwBuf,accounts,cmwBuf.getSize());
  for(;;){
    cmwBuf.sock_=::socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);
    if(0==::connect(cmwBuf.sock_,(::sockaddr*)&addr,sizeof addr))break;
    ::printf("connect %d\n",errno);
    ::close(cmwBuf.sock_);
    ::sleep(loginPause);
  }

  while(!cmwBuf.flush());
  fds[0].fd=cmwBuf.sock_;
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

void reset (char const *context,char const *detail=""){
  ::syslog(LOG_ERR,"%s:%s",context,detail);
  frntBuf.reset();
  front::marshal<false>(frntBuf,{context," ",detail});
  for(auto& r:pendingRequests)frntBuf.send((::sockaddr*)&r->frnt,r->frntLn);
  pendingRequests.clear();
  cmwBuf.compressedReset();
  login();
}

bool sendData ()try{return cmwBuf.flush();}
catch(::std::exception& e){reset("sendData",e.what());return true;}

template<bool res,class...T>void outFront (cmwRequest const& req,T...t){
  frntBuf.reset();
  front::marshal<res>(frntBuf,{t...});
  frntBuf.send((::sockaddr*)&req.frnt,req.frntLn);
}

int main (int ac,char **av)try{
  setup(ac,av);
  login();

  for(;;){
    pollWrapper(fds,2);
    try{
      if(fds[0].revents&POLLIN&&cmwBuf.gotPacket()){
        do{
          assert(!pendingRequests.empty());
          auto it=::std::begin(pendingRequests);
          if(giveBool(cmwBuf)){
            getFile((**it).getFileName(),cmwBuf);
            outFront<true>(**it);
          }else outFront<false>(**it,"CMW:",cmwBuf.giveStringView());
          pendingRequests.erase(it);
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
      cmwRequest *req=nullptr;
      try{
        req=&*pendingRequests.emplace_back(::new cmwRequest);
        frntBuf.getPacket((::sockaddr*)&req->frnt,&req->frntLn);
        gotAddr=true;
        req->~cmwRequest();
        ::new(req)cmwRequest(frntBuf);
        back::marshal<messageID::generate>(cmwBuf,*req);
      }catch(::std::exception& e){
        ::syslog(LOG_ERR,"Accept request:%s",e.what());
        if(gotAddr)outFront<false>(*req,e.what());
        if(req)pendingRequests.pop_back();
        continue;
      }
      if(!sendData())fds[0].events|=POLLOUT;
    }
  }
}catch(::std::exception& e){bail("Oops:%s",e.what());}
