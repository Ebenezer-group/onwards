#include<cmw_Buffer.hh>
#include"genz.mdl.hh"
#include"jniGenz.h"

using namespace ::cmw;
void leave (char const *fmt,auto...t)noexcept{
  ::std::fprintf(stderr,fmt,t...);
  exitFailure();
}

int genzMain (int ac,char **av)try{
  if(ac<3||ac>5)
    leave("Usage: genz account-num mdl-file-path [node] [port]\n");
  winStart();
  GetaddrinfoWrapper res(ac<4?"127.0.0.1":av[3],ac<5?"55555":av[4],SOCK_DGRAM);
  BufferStack<SameFormat> buf(res.getSock());

  ::middle::marshal<udpPacketMax>(buf,MarshallingInt(av[1]),av[2]);
  for(int tm=8;tm<13;tm+=4){
    buf.send(res().ai_addr,res().ai_addrlen);
    setRcvTimeout(buf.sock_,tm);
    if(buf.getPacket()){
      if(giveBool(buf))::exit(EXIT_SUCCESS);
      leave("cmwA:%s\n",buf.giveStringView().data());
    }
  }
  leave("No reply received.  Is the cmwA running?\n");
}catch(::std::exception& e){leave("%s\n",e.what());}

JNIEXPORT void JNICALL Java_jniGenz_wrap (JNIEnv *env,jobject obj,jobjectArray args){
  auto argc=env->GetArrayLength(args);
  auto argv=(char**)malloc(sizeof(char*)*(argc+1));// +1 for program name at index 0
  argv[0]="jniGenz";

  for(int i=0;i<argc;++i){
    auto string=(jstring)env->GetObjectArrayElement(args,i);
    argv[i+1]=(char*)env->GetStringUTFChars(string,0);
  }
  genzMain(argc+1,argv);
}
