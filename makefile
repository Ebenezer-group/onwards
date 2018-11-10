CXX=g++
#CXX=clang++ -stdlib=libc++

CXXFLAGS=-Isrc -Os -Wundef -W -Wall -Wextra -Wpedantic -Wreorder -o $@ $@.cc

BASE:=src/cmw/tiers
TIERS:=$(BASE)/genz $(BASE)/cmwA
EXAMPLES:=example/sendExample example/receiveExample
all:$(TIERS) $(EXAMPLES)
PHONY:all

#zz.middleBack.hh: account.hh middleBack.mdl
#	genz 2 /home/brian/onwards/src/cmw/tiers/middleBack.mdl

$(BASE)/genz:$(BASE)/genz.cc
	$(CXX) -std=c++17 $(CXXFLAGS)
	size $@

$(BASE)/cmwA:$(BASE)/cmwA.cc
	$(CXX) -std=c++17 $(CXXFLAGS)
	size $@

EXFLAGS:=-std=c++11 -Iexample
example/sendExample:example/sendExample.cc
	$(CXX) $(EXFLAGS) $(CXXFLAGS)
	size $@
example/receiveExample:example/receiveExample.cc
	$(CXX) $(EXFLAGS) $(CXXFLAGS)
	size $@

clean:
	rm -f $(TIERS) $(EXAMPLES)

INSTALL_DIR=/usr/local
install:
	cp -f src/cmw/*.hh src/cmw/quicklz.h $(INSTALL_DIR)/include
	cp -f $(TIERS) $(INSTALL_DIR)/bin

uninstall:
	cd $(INSTALL_DIR)/include && rm -f $(includes)

