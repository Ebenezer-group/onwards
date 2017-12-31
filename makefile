CXXFLAGS=-Os -Wundef -W -Wall -Wextra -Wreorder

CXX=g++7 -std=c++17
#CXX=clang++ -std=c++1z -stdlib=libc++

LIBS=-L./ -L/usr/lib

FRONT:=tiers/genz
MIDDLE:=tiers/cmwA
TIERS:=$(FRONT) $(MIDDLE)
objects:=marshallingInt.o
TARGETS:=$(objects) $(TIERS)
all:$(TARGETS)

#zz.middleBack.hh: account.hh middleBack.mdl
#	genz 2 /usr/home/brian/onwards/tiers/middleBack.mdl

$(FRONT): $(FRONT).cc $(objects)
	$(CXX) $(CXXFLAGS) -I. -o $@ $@.cc $(objects)
	size $@

$(MIDDLE): $(MIDDLE).cc $(objects)
	$(CXX) $(CXXFLAGS) -I. -o $@ $@.cc $(objects)
	size $@

EXAMPLES:=example/send_example example/receive_example
example:$(EXAMPLES)

example/send_example: example/send_example.cc $(objects)
	$(CXX) -o $@ $(CXXFLAGS) -I. -I./example $@.cc $(objects)
	size $@
example/receive_example: example/receive_example.cc
	$(CXX) -o $@ $(CXXFLAGS) -I. -I./example $@.cc
	size $@

clean:
	rm -f $(TARGETS) $(EXAMPLES)

INSTALL_DIR=/usr/local
includes=tcpServer.hh udpStuff.hh ErrorWords.hh File.hh marshallingInt.hh *Buffer*.hh cmw_complex.hh wrappers.hh quicklz.h

install:
	cp -f $(includes) $(INSTALL_DIR)/include
	cp -f marshallingInt.o $(INSTALL_DIR)/lib
	cp -f $(TIERS) $(INSTALL_DIR)/bin

uninstall:
	cd $(INSTALL_DIR)/include && rm -f $(includes)
	rm -f $(INSTALL_DIR)/lib/marshallingInt.o

