CFLAGS= -Os -Wshadow -Wundef -W -Wall -Wextra
CXXFLAGS= -std=c++17 $(CFLAGS) -Wreorder

CXX=g++7
#CXX=clang++ -stdlib=libc++

LIBS=-L./ -L/usr/lib

all: libhome.a subdirs

.PHONY: subdirs
subdirs:
	$(MAKE) -C tiers
	$(MAKE) -C example

objects = quicklz.o marshalling_integer.o File.o IO.o SendBuffer.o
libhome.a: $(objects)
	ar r $@ $(objects)

clean:
	rm -f $(objects) libhome.*
	$(MAKE) -C tiers clean
	$(MAKE) -C example clean

INSTALL_DIR=/usr/local
includes=close_socket.hh connect_wrapper.hh getaddrinfo_wrapper.hh tcp_server.hh udp_stuff.hh ErrorWords.hh File.hh IO*.hh marshalling_integer.hh poll_wrapper.hh SendBuffer*.hh Formatting.hh ReceiveBuffer*.hh cmw_complex.hh platforms.hh empty_container.hh quicklz.h

install:
	$(MAKE) -C tiers install
	cp -f libhome.a $(INSTALL_DIR)/lib
	cp -f $(includes) $(INSTALL_DIR)/include

uninstall:
	$(MAKE) -C tiers uninstall
	rm -f $(INSTALL_DIR)/lib/libhome.a
	cd $(INSTALL_DIR)/include && rm -f $(includes)

