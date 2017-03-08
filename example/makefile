CXXFLAGS= -std=c++17 -O2 -s -W -Wall -I..

CXX=g++7

TARGETS = send_example receive_example
all: $(TARGETS)


send_example: send_example.cc
	$(CXX) -o $@ $(CXXFLAGS) $@.cc ../libhome.a
	ls -l $@

receive_example: receive_example.cc
	$(CXX) -o $@ $(CXXFLAGS) $@.cc ../libhome.a
	ls -l $@

clean:
	rm -f $(TARGETS)

