CXX=g++7
#CXX=clang++ -stdlib=libc++

CXXFLAGS= -std=c++17 -I.. -O2 -Wshadow -Wundef -Wreorder -W -Wall -Wextra

TARGETS = cmwAmbassador direct 
all: $(TARGETS)

#zz.middle_messages_back.hh: account_info.hh remote.mdl cmw.req
#	direct 1 /usr/share/nginx/html/misc/cmw.req

cmwAmbassador: $@.cc zz.middle_messages_back.hh ../libhome.a
	$(CXX) -o $@ $(CXXFLAGS) $< ../libhome.a
	size $@

direct: $@.cc ../libhome.a
	$(CXX) -o $@ $(CXXFLAGS) $< ../libhome.a
	size $@

clean:
	rm -f $(TARGETS)

INSTALL_DIR=/usr/local
install:
	cp -f $(TARGETS) $(INSTALL_DIR)/bin

uninstall:
	cd $(INSTALL_DIR)/bin/ && rm -f $(TARGETS)

