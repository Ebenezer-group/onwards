CXX=g++
CXXFLAGS=$(CXX) -std=c++17 -Isrc -Os -flto -Wundef -W -Wall -Wextra -Wpedantic -Wreorder -o $@ $@.cc

BASE:=src/tiers/
TIERS:=$(BASE)genz $(BASE)cmwA
EXAMPLES:=example/sendExample example/receiveExample
all:$(TIERS) $(EXAMPLES)
.PHONY:all clean

#cmwA.mdl.hh: account.hh $(BASE)cmwA.mdl
#	genz 2 $(BASE)cmwA.mdl

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

INSTALL_DIR=/usr/local/
install:
	cp -f src/cmw/*.hh src/cmw/quicklz.h $(INSTALL_DIR)include
	cp -f $(TIERS) $(INSTALL_DIR)bin

uninstall:
	cd $(INSTALL_DIR)include && rm -f $(includes)

