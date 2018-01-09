CXXFLAGS=-Os -Wundef -W -Wall -Wextra -Wreorder

CXX=g++-7 -std=c++17
#CXX=clang++ -std=c++1z -stdlib=libc++

LIBS=-L./ -L/usr/lib

FRONT:=tiers/genz
MIDDLE:=tiers/cmwA
TIERS:=$(FRONT) $(MIDDLE)
all:$(TIERS)

#zz.middleBack.hh: account.hh middleBack.mdl
#	genz 2 /usr/home/brian/onwards/tiers/middleBack.mdl

$(FRONT): $(FRONT).cc
	$(CXX) $(CXXFLAGS) -I. -o $@ $@.cc
	size $@

$(MIDDLE): $(MIDDLE).cc
	$(CXX) $(CXXFLAGS) -I. -o $@ $@.cc
	size $@

EXAMPLES:=example/send_example example/receive_example
example:$(EXAMPLES)

example/send_example: example/send_example.cc
	$(CXX) -o $@ $(CXXFLAGS) -I. -I./example $@.cc
	size $@
example/receive_example: example/receive_example.cc
	$(CXX) -o $@ $(CXXFLAGS) -I. -I./example $@.cc
	size $@

clean:
	rm -f $(TIERS) $(EXAMPLES)

INSTALL_DIR=/usr/local
includes=ErrorWords.hh File.hh *Buffer*.hh Complex.hh wrappers.hh quicklz.h

install:
	cp -f $(includes) $(INSTALL_DIR)/include
	cp -f $(TIERS) $(INSTALL_DIR)/bin

uninstall:
	cd $(INSTALL_DIR)/include && rm -f $(includes)

