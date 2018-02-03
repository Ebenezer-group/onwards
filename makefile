CXX=g++-7
#CXX=clang++ -stdlib=libc++

CXXFLAGS=-I. -Os -Wundef -W -Wall -Wextra -Wpedantic -Wreorder -o $@

FRONT:=tiers/genz
MIDDLE:=tiers/cmwA
TIERS:=$(FRONT) $(MIDDLE)
all:$(TIERS)

#zz.middleBack.hh: account.hh middleBack.mdl
#	genz 2 /usr/home/brian/onwards/tiers/middleBack.mdl

$(FRONT): $(FRONT).cc
	$(CXX) $(CXXFLAGS) -std=c++17 $@.cc
	size $@

$(MIDDLE): $(MIDDLE).cc
	$(CXX) $(CXXFLAGS) -std=c++17 $@.cc
	size $@

EXAMPLES:=example/sendExample example/receiveExample
example:$(EXAMPLES)

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

