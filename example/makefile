CXXFLAGS= -std=c++17 -O2 -s -W -Wall -I. -I ..  -DMAKEFILES 

CXX=g++7
AR=ar
LIBS=-L./ -L/usr/lib

all: send_sample receive_sample 


send_sample: send_sample.cc
	$(CXX) -o $@ $(CXXFLAGS) send_sample.cc /usr/local/lib/libhome.a
	ls -l $@

receive_sample: receive_sample.cc
	$(CXX) -o $@ $(CXXFLAGS) receive_sample.cc /usr/local/lib/libhome.a 
	ls -l $@

clean:
	rm -f send_sample receive_sample *.o

