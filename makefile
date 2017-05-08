CFLAGS=-Os -Wundef -W -Wall -Wextra
CXXFLAGS=$(CFLAGS) -Wreorder

CC=gcc7
CXX=g++7 -std=c++17
#CXX=clang++ -std=c++1z -stdlib=libc++

LIBS=-L./ -L/usr/lib

MIDDLE:= tiers/cmwAmbassador
FRONT:= tiers/genz
TIERS:= $(MIDDLE) $(FRONT)
TARGETS:= libhome.a $(TIERS)
all: $(TARGETS)

objects=quicklz.o marshalling_integer.o
libhome.a: $(objects)
	ar rc $@ $(objects)

#zz.middle_messages_back.hh: account_info.hh remote.mdl cmw.req
#	genz 2 /usr/home/brian/onwards/tiers/cmw.req

$(MIDDLE): $(MIDDLE).cc libhome.a
	$(CXX) -o $@ $(CXXFLAGS) -I. $@.cc libhome.a
	size $@

$(FRONT): $(FRONT).cc libhome.a
	$(CXX) -o $@ $(CXXFLAGS) -I. $@.cc libhome.a
	size $@

EXAMPLES:= example/send_example example/receive_example
example: $(EXAMPLES)

example/send_example: example/send_example.cc libhome.a
	$(CXX) -o $@ $(CXXFLAGS) -I. $@.cc libhome.a
example/receive_example: example/receive_example.cc libhome.a
	$(CXX) -o $@ $(CXXFLAGS) -I. $@.cc libhome.a

clean:
	rm -f $(objects) $(TARGETS) $(EXAMPLES)

INSTALL_DIR=/usr/local
includes=close_socket.hh connect_wrapper.hh getaddrinfo_wrapper.hh tcp_server.hh udp_stuff.hh ErrorWords.hh File.hh IO*.hh marshalling_integer.hh poll_wrapper.hh SendBuffer*.hh Formatting.hh ReceiveBuffer*.hh cmw_complex.hh platforms.hh empty_container.hh quicklz.h

install:
	cp -f $(includes) $(INSTALL_DIR)/include
	cp -f libhome.a $(INSTALL_DIR)/lib
	cp -f $(TIERS) $(INSTALL_DIR)/bin

uninstall:
	cd $(INSTALL_DIR)/include && rm -f $(includes)
	rm -f $(INSTALL_DIR)/lib/libhome.a

