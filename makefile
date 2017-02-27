CFLAGS= -O2 -Wshadow -Wundef -W -Wall -Wextra
CXXFLAGS= -std=c++17 -O2 -Wshadow -Wundef -Wreorder -W -Wall -Wextra

CXX=g++7
#CXX=clang++ -stdlib=libc++

AR=ar
LIBS=-L./ -L/usr/lib

all: libhome.a project_code

.PHONY: project_code
project_code:
	$(MAKE) -C tiers
	cd example && make

objects = close_socket.o marshalling_integer.o quicklz.o ErrorWords.o File.o IO.o SendBuffer.o SendBufferCompressed.o SendBufferStdString.o SendBufferFile.o SendBufferFlush.o

libhome.a: $(objects)
	$(AR) r $@ $(objects)


INSTALL_DIR=/usr/local
install:
	cp -f libhome.a  $(INSTALL_DIR)/lib
	cp -f close_socket.hh connect_wrapper.hh getaddrinfo_wrapper.hh tcp_server.hh  udp_stuff.hh ErrorWords.hh File.hh IO*.hh marshalling_integer.hh poll_wrapper.hh SendBuffer*.hh Formatting.hh ReceiveBuffer*.hh cmw_complex.hh platforms.hh empty_container.hh quicklz.h $(INSTALL_DIR)/include

uninstall:
	rm -f $(INSTALL_DIR)/lib/libhome.a

clean:
	rm -f $(objects) libhome.*
	cd tiers && make clean
	cd example && make clean

