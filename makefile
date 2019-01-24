CXX=g++
CXXFLAGS=$(CXX) -std=c++17 -Isrc -Os -flto -Wundef -W -Wall -Wextra -Wpedantic -Wreorder -Wno-return-type -o $@ $@.cc

BASE:=src/cmw/tiers/
TIERS:=$(BASE)genz $(BASE)cmwA
EXAMPLES:=example/sendExample example/receiveExample
all:$(TIERS) $(EXAMPLES)
.PHONY:all clean

#zz.middleBack.hh: account.hh $(BASE)middleBack.mdl
#	genz 2 $(BASE)middleBack.mdl

$(BASE)genz:$(BASE)genz.cc
	$(CXXFLAGS)
	size $@

$(BASE)cmwA:$(BASE)cmwA.cc
	$(CXXFLAGS)
	size $@

example/sendExample:example/sendExample.cc
	$(CXXFLAGS)
	size $@
example/receiveExample:example/receiveExample.cc
	$(CXXFLAGS)
	size $@

clean:
	rm -f $(TIERS) $(EXAMPLES)

INSTALL_DIR=/usr/local
install:
	cp -f src/cmw/*.hh src/cmw/quicklz.h $(INSTALL_DIR)/include
	cp -f $(TIERS) $(INSTALL_DIR)/bin

uninstall:
	cd $(INSTALL_DIR)/include && rm -f $(includes)

