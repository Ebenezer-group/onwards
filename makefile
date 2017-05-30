CXXFLAGS=-Os -Wundef -W -Wall -Wextra -Wreorder

CXX=g++7 -std=c++17
#CXX=clang++ -std=c++1z -stdlib=libc++

LIBS=-L./ -L/usr/lib

MIDDLE:= tiers/cmwAmbassador
FRONT:= tiers/genz
TIERS:= $(MIDDLE) $(FRONT)
objects:=marshalling_integer.o
TARGETS:= $(objects) $(TIERS)
all: $(TARGETS)

#zz.middle_messages_back.hh: account_info.hh remote.mdl cmw.req
#	genz 2 /usr/home/brian/onwards/tiers/cmw.req

$(MIDDLE): $(MIDDLE).cc $(objects)
	$(CXX) $(CXXFLAGS) -I. $@.cc $(objects) -o $@
	size $@

$(FRONT): $(FRONT).cc $(objects)
	$(CXX) $(CXXFLAGS) -I. $@.cc $(objects) -o $@
	size $@

EXAMPLES:= example/send_example example/receive_example
example: $(EXAMPLES)

example/send_example: example/send_example.cc $(objects)
	$(CXX) -o $@ $(CXXFLAGS) -I. $@.cc $(objects)
example/receive_example: example/receive_example.cc $(objects)
	$(CXX) -o $@ $(CXXFLAGS) -I. $@.cc $(objects)

clean:
	rm -f $(TARGETS) $(EXAMPLES)

INSTALL_DIR=/usr/local
includes=close_socket.hh connect_wrapper.hh getaddrinfo_wrapper.hh tcp_server.hh udp_stuff.hh ErrorWords.hh File.hh IO*.hh marshalling_integer.hh poll_wrapper.hh SendBuffer*.hh ReceiveBuffer*.hh cmw_complex.hh platforms.hh quicklz.h

install:
	cp -f $(includes) $(INSTALL_DIR)/include
	cp -f libhome.a $(INSTALL_DIR)/lib
	cp -f $(TIERS) $(INSTALL_DIR)/bin

uninstall:
	cd $(INSTALL_DIR)/include && rm -f $(includes)
	rm -f $(INSTALL_DIR)/lib/libhome.a

