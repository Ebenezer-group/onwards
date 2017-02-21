CXXFLAGS= -std=c++17 -O2 -s -W -Wall -I. -I .. 

CXX=g++7
AR=ar
LIBS=-L./ -L/usr/lib

all: send_example receive_example 


send_example: send_example.cc
	$(CXX) -o $@ $(CXXFLAGS) send_example.cc /usr/local/lib/libhome.a
	ls -l $@

receive_example: receive_example.cc
	$(CXX) -o $@ $(CXXFLAGS) receive_example.cc /usr/local/lib/libhome.a 
	ls -l $@

clean:
	rm -f send_example receive_example *.o

