CXXFLAGS= -std=c++17 -I.. -O2 -Wshadow -Wundef -Wreorder -W -Wall -Wextra

CXX=g++7
#CXX=clang++ -stdlib=libc++

all: cmwAmbassador direct 

#zz.middle_messages_back.hh: account_info.hh remote.mdl cmw.req
#	direct 1 /usr/share/nginx/html/misc/cmw.req

cmwAmbassador: cmwAmbassador.cc zz.middle_messages_back.hh ../libhome.a
	$(CXX) -o $@ $(CXXFLAGS) -s $< ../libhome.a
	size $@

direct: direct.cc ../libhome.a
	$(CXX) -o $@ $(CXXFLAGS) -s $< ../libhome.a
	size $@


INSTALL_DIR=/usr/local
install:
	cp -f direct cmwAmbassador $(INSTALL_DIR)/bin

uninstall:
	rm -f $(INSTALL_DIR)/bin/direct $(INSTALL_DIR)/bin/cmwAmbassador

clean:
	rm -f cmwAmbassador direct

