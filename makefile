CXX=g++-7
#CXX=clang++ -stdlib=libc++

CXXFLAGS=-I. -Os -Wundef -W -Wall -Wextra -Wpedantic -Wreorder -o $@

TIERS:=tiers/genz tiers/cmwA
EXAMPLES:=example/sendExample example/receiveExample
all:$(TIERS) $(EXAMPLES)
PHONY:all

#zz.middleBack.hh: account.hh middleBack.mdl
#	genz 2 /home/brian/onwards/tiers/middleBack.mdl

tiers/genz: tiers/genz.cc
	$(CXX) $(CXXFLAGS) -std=c++17 $@.cc
	size $@

tiers/cmwA: tiers/cmwA.cc
	$(CXX) $(CXXFLAGS) -std=c++17 $@.cc
	size $@

EXFLAGS:=-std=c++11 -I./example
example/sendExample: example/sendExample.cc
	$(CXX) $(CXXFLAGS) $(EXFLAGS) $@.cc
	size $@
example/receiveExample: example/receiveExample.cc
	$(CXX) $(CXXFLAGS) $(EXFLAGS) $@.cc
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

