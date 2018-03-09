CXX=g++-7
#CXX=clang++ -stdlib=libc++

CXXFLAGS=-I. -Os -Wundef -W -Wall -Wextra -Wpedantic -Wreorder -o $@ $@.cc

TIERS:=tiers/genz tiers/cmwA
EXAMPLES:=example/sendExample example/receiveExample
all:$(TIERS) $(EXAMPLES)
PHONY:all

#zz.middleBack.hh: account.hh middleBack.mdl
#	genz 2 /home/brian/onwards/tiers/middleBack.mdl

tiers/genz: tiers/genz.cc
	$(CXX) -std=c++17 $(CXXFLAGS)
	size $@

tiers/cmwA: tiers/cmwA.cc
	$(CXX) -std=c++17 $(CXXFLAGS)
	size $@

EXFLAGS:=-std=c++11 -I./example
example/sendExample: example/sendExample.cc
	$(CXX) $(EXFLAGS) $(CXXFLAGS)
	size $@
example/receiveExample: example/receiveExample.cc
	$(CXX) $(EXFLAGS) $(CXXFLAGS)
	size $@

clean:
	rm -f $(TIERS) $(EXAMPLES)

INSTALL_DIR=/usr/local
includes=ErrorWords.hh Buffer.hh Complex.hh wrappers.hh quicklz.h

install:
	cp -f $(includes) $(INSTALL_DIR)/include
	cp -f $(TIERS) $(INSTALL_DIR)/bin

uninstall:
	cd $(INSTALL_DIR)/include && rm -f $(includes)

