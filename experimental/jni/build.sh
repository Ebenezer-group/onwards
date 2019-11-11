javac -h . jniGenz.java

g++ -shared -fPIC -std=c++17 -o libnative.so -I ../../src -I /home/brian/android-studio/jre/include/ -I /home/brian/android-studio/jre/include/linux/ genzMain.cc

java -cp . -Djava.library.path=. jniGenz
java -cp . -Djava.library.path=. jniGenz 2 ./cmwA.mdl

