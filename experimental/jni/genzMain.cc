#include<cmw/Buffer.hh>
#include"zz.frntMddl.hh"
#include<stdio.h>
#include<stdlib.h>//exit
#include"jniGenz.h"

template<class...T>void leave (char const* fmt,T...t)noexcept{
  ::fprintf(stderr,fmt,t...);
  ::exit(EXIT_FAILURE);
}

int genzMain (int ac,char** av)try{
  using namespace ::cmw;
  if(ac<3||ac>5)
    leave("Usage: genz account-num mdl-file-path [node] [port]\n");
  winStart();
  GetaddrinfoWrapper res(ac<4?"127.0.0.1":av[3],ac<5?"55555":av[4],SOCK_DGRAM);
  BufferStack<SameFormat> buf(res.getSock());

  ::frntMddl::marshal(buf,MarshallingInt(av[1]),av[2]);
  for(int tm=8;tm<13;tm+=4){
    buf.Send(res()->ai_addr,res()->ai_addrlen);
    setRcvTimeout(buf.sock_,tm);
    if(buf.getPacket()){
      if(giveBool(buf))::exit(EXIT_SUCCESS);
      leave("cmwA:%s\n",giveStringView(buf).data());
    }
  }
  leave("No reply received.  Is the cmwA running?\n");
}catch(::std::exception const& e){leave("%s\n",e.what());}

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
