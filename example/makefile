CXXFLAGS= -std=c++17 -O2 -s -W -Wall -I. -I .. 

CXX=g++7
AR=ar
LIBS=-L./ -L/usr/lib

TARGETS = send_example receive_example 
all: $(TARGETS)


send_example: send_example.cc
	$(CXX) -o $@ $(CXXFLAGS) send_example.cc ../libhome.a
	ls -l $@

receive_example: receive_example.cc
	$(CXX) -o $@ $(CXXFLAGS) receive_example.cc ../libhome.a 
	ls -l $@

clean:
	rm -f $(TARGETS)

