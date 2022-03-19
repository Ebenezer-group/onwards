CXXFLAGS=$(CXX) -std=c++20 -Isrc -Os -flto -Wundef -W -Wall -Wextra -Wpedantic -Wreorder -o $@ $@.cc

EXAMPLES:=example/sendExample example/receiveExample
BASE:=src/tiers/
TIERS:=$(BASE)front/genz $(BASE)cmwA
all:$(EXAMPLES) $(TIERS)
.PHONY:all clean

#cmwA.mdl.h: account.h $(BASE)cmwA.mdl
#	genz 2 $(BASE)cmwA.mdl

example/sendExample:example/sendExample.cc
	$(CXXFLAGS)
	size $@
example/receiveExample:example/receiveExample.cc
	$(CXXFLAGS)
	size $@

$(BASE)front/genz:$(BASE)front/genz.cc
	$(CXXFLAGS)
	size $@

$(BASE)cmwA:$(BASE)cmwA.cc
	$(CXXFLAGS)
	size $@

clean:
	rm -f $(TIERS) $(EXAMPLES)

INSTALL_DIR=/usr/local/
install:
	cp -f src/*.h  $(INSTALL_DIR)include
	cp -f $(TIERS) $(INSTALL_DIR)bin

uninstall:
	cd $(INSTALL_DIR)include && rm -f $(includes)

