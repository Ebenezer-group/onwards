CXX=g++7
#CXX=clang++ -stdlib=libc++

CXXFLAGS= -std=c++17 -I.. -Os -Wshadow -Wundef -Wreorder -W -Wall -Wextra

TARGETS := cmwAmbassador genz
all: $(TARGETS)

#zz.middle_messages_back.hh: account_info.hh remote.mdl cmw.req
#	genz 1 /usr/home/brian/onwards/tiers/cmw.req

cmwAmbassador: cmwAmbassador.cc zz.middle_messages_back.hh ../libhome.a
	$(CXX) -o $@ $(CXXFLAGS) $< ../libhome.a
	size $@

genz: genz.cc ../libhome.a
	$(CXX) -o $@ $(CXXFLAGS) $< ../libhome.a
	size $@

clean:
	rm -f $(TARGETS)

INSTALL_DIR=/usr/local
install:
	cp -f $(TARGETS) $(INSTALL_DIR)/bin

uninstall:
	cd $(INSTALL_DIR)/bin/ && rm -f $(TARGETS)

