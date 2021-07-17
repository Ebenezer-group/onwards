#include<cmw_BufferImpl.hh>
#include"account.hh"
#include"messageIDs.hh"

#include<vector>
#include<cassert>
#include<ctime>
#ifdef __linux__
#include<linux/sctp.h>//sockaddr_in6,socklen_t
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
    if('.'==name[0]||name[0]=='/')receive(buf,::std::strrchr(name,'/')+1,1);
    else receive(buf,name,1);

    buf.receiveFile(FileWrapper{name,O_RDONLY,0}.d,sb.st_size);
    return true;
  }

 public:
  template<class R>
  cmwRequest (Socky const& ft,ReceiveBuffer<R>& buf):frnt{ft}
     ,bday{static_cast<::int32_t>(::std::time(nullptr))},acctNbr{buf},path{buf}{
    if(path.bytesAvailable()<3)raise("No room for file suffix");
    char* const pos=::std::strrchr(path(),'/');
    if(nullptr==pos)raise("cmwRequest didn't find /");
    *pos=0;
    setDirectory(path());
    *pos='/';
    mdlFile=pos+1;
    char last[60];
    ::std::snprintf(last,sizeof last,".%s.last",mdlFile);
    ::new(&fl)FileWrapper(last,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP);
    switch(::pread(fl.d,&prevTime,sizeof prevTime,0)){
      case 0:prevTime=0;break;
      case -1:raise("pread",errno);
    }
  }

  void marshal (SendBuffer& buf)const{
    acctNbr.marshal(buf);
    if(auto ind=buf.reserveBytes(1);!buf.receive(ind,marshalFile(mdlFile,buf)))
      receive(buf,mdlFile,1);

    ::int8_t updatedFiles=0;
    auto const idx=buf.reserveBytes(sizeof updatedFiles);
    FileBuffer f{mdlFile,O_RDONLY};
    while(auto line=f.getline()){
      char const *tok=::std::strtok(line,"\n \r");
      if(!::std::strncmp(tok,"//",2))continue;
      if(!::std::strcmp(tok,"--"))break;
      if(marshalFile(tok,buf))++updatedFiles;
    }
    buf.receive(idx,updatedFiles);
  }

  auto getFileName (){
    Write(fl.d,&bday,sizeof bday);
    path.append(".hh");
    return path.data();
  }
};
#include"cmwA.mdl.hh"

auto checkField (char const *fld,char const *actl){
  if(::std::strcmp(fld,actl))bail("Expected %s",fld);
  return ::std::strtok(nullptr,"\n \r");
}

namespace{
GetaddrinfoWrapper res("75.23.62.38","56789",SOCK_STREAM);
::pollfd fds[2];
int loginPause;
::std::vector<cmwAccount> accounts;
::std::vector<cmwRequest*> pendingRequests;
BufferCompressed<SameFormat> cmwBuf{1101000};
BufferStack<SameFormat> frntBuf;
}

void setup (int ac,char **av){
  ::openlog(av[0],LOG_PID|LOG_NDELAY,LOG_USER);
  if(ac!=2)bail("Usage: cmwA config-file");
  FileBuffer cfg{av[1],O_RDONLY};
  char const *tok;
  while((tok=::std::strtok(cfg.getline()," "))&&!::std::strcmp("Account-number",tok)){
    auto num=fromChars(::std::strtok(nullptr,"\n \r"));
    tok=checkField("Password",::std::strtok(cfg.getline()," "));
    accounts.emplace_back(num,tok);
  }
  if(accounts.empty())bail("An account number is required.");
  fds[1].fd=frntBuf.sock_=udpServer(checkField("UDP-port-number",tok));
  fds[1].events=POLLIN;

  tok=checkField("Login-interval-in-seconds",::std::strtok(cfg.getline()," "));
  loginPause=fromChars(tok);
}

void login (){
  back::marshal<messageID::login>(cmwBuf,accounts,cmwBuf.getSize());
  for(;;){
    cmwBuf.sock_=::socket(AF_INET,SOCK_STREAM,IPPROTO_SCTP);
    if(0==::connect(cmwBuf.sock_,res().ai_addr,res().ai_addrlen))break;
    ::std::printf("connect %d\n",errno);
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
  ::front::marshal<false>(frntBuf,{context," ",detail});
  for(auto r:pendingRequests){
    frntBuf.send((::sockaddr*)&r->frnt.addr,r->frnt.len);
    delete r;
  }
  pendingRequests.clear();
  cmwBuf.compressedReset();
  login();
}

bool sendData ()try{return cmwBuf.flush();}
catch(::std::exception& e){reset("sendData",e.what());return true;}

template<bool res,class...T>
void outFront (Socky const& s,T...t){
  frntBuf.reset();
  ::front::marshal<res>(frntBuf,{t...});
  frntBuf.send((::sockaddr*)&s.addr,s.len);
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
          auto it=::std::cbegin(pendingRequests);
          if(giveBool(cmwBuf)){
            getFile((*it)->getFileName(),cmwBuf);
            outFront<true>((*it)->frnt);
          }else outFront<false>((*it)->frnt,"CMW:",cmwBuf.giveStringView());
          delete *it;
          pendingRequests.erase(it);
        }while(cmwBuf.nextMessage());
      }
    }catch(Fiasco& e){
      reset("Fiasco",e.what());
      continue;
    }catch(::std::exception& e){
      ::syslog(LOG_ERR,"Reply from CMW %s",e.what());
      assert(!pendingRequests.empty());
      auto it=::std::cbegin(pendingRequests);
      outFront<false>((*it)->frnt,e.what());
      delete *it;
      pendingRequests.erase(it);
    }

    if(fds[0].revents&POLLOUT&&sendData())fds[0].events=POLLIN;
    if(fds[0].revents&POLLERR)reset("Lost contact");

    if(fds[1].revents&POLLIN){
      Socky frnt;
      bool gotAddr=false;
      cmwRequest *req=nullptr;
      try{
        gotAddr=frntBuf.getPacket((::sockaddr*)&frnt.addr,&frnt.len);
        req=new cmwRequest(frnt,frntBuf);
        back::marshal<messageID::generate>(cmwBuf,*req);
        pendingRequests.push_back(req);
      }catch(::std::exception& e){
        ::syslog(LOG_ERR,"Accept request:%s",e.what());
        if(gotAddr)outFront<false>(frnt,e.what());
        delete req;
        continue;
      }
      if(!sendData())fds[0].events|=POLLOUT;
    }
  }
}catch(::std::exception& e){bail("Oops:%s",e.what());}
