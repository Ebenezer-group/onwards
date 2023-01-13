CXXFLAGS=$(CXX) -std=c++20 -Isrc -Oz -flto -Wundef -W -Wall -Wextra -Wpedantic -Wreorder -o $@ $@.cc

EXAMPLES:=example/sendExample example/receiveExample
FRONT:=src/tiers/front/genz
MIDDLE:=src/tiers/posix.cmwA
NMIDDLE:=src/tiers/cmwA
BINARIES:=$(EXAMPLES) $(FRONT) $(MIDDLE)
all:$(BINARIES)
.PHONY:all clean

#cmwA.mdl.h: account.h $(BASE)cmwA.mdl
#	genz 2 $(BASE)cmwA.mdl

example/sendExample:example/sendExample.cc
	$(CXXFLAGS)
	size $@
example/receiveExample:example/receiveExample.cc
	$(CXXFLAGS)
	size $@

$(FRONT):$(FRONT).cc
	$(CXXFLAGS)
	size $@

$(MIDDLE):$(MIDDLE).cc
	$(CXXFLAGS)
	size $@

$(NMIDDLE):$(NMIDDLE).cc
	$(CXXFLAGS) -luring
	size $@

clean:
	rm -f $(BINARIES)

INSTALL_DIR=/usr/local/
install:
	cp -f src/*.hh src/quicklz.* $(INSTALL_DIR)include
	cp -f $(FRONT) $(MIDDLE) $(INSTALL_DIR)bin
	cp -f $(BASE)cmwA.cfg $(INSTALL_DIR)etc

uninstall:
	cd $(INSTALL_DIR)include && rm -f cmw_*.hh
	cd $(INSTALL_DIR)bin && rm -f cmwA genz

