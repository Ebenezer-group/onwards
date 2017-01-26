CFLAGS= -O2 -Wshadow -Wundef -W -Wall -Wextra
CXXFLAGS= -std=c++17 -O2 -Wshadow -Wundef -Wreorder -W -Wall -Wextra

CXX=g++7
#CXX=clang++ -stdlib=libc++

AR=ar
LIBS=-L./ -L/usr/lib

all: libhome.a cmwAmbassador direct

objects = close_socket.o marshalling_integer.o quicklz.o ErrorWords.o File.o FileMarshal.o IO.o SendBuffer.o SendBufferCompressed.o SendBufferStdString.o SendBufferFile.o SendBufferFlush.o

libhome.a: $(objects)
	$(AR) r $@ $(objects)

#zz.middle_messages_back.hh: account_info.hh remote.mdl cmw.req
#	direct 1 /usr/share/nginx/html/misc/cmw.req

cmwAmbassador: cmwAmbassador.cc zz.middle_messages_back.hh libhome.a
	$(CXX) -o $@ $(CXXFLAGS) -s $< libhome.a
	size $@

direct: direct.cc libhome.a
	$(CXX) -o $@ $(CXXFLAGS) -s $< libhome.a
	size $@


INSTALL_DIR=/usr/local
install:
	cp -f libhome.a  $(INSTALL_DIR)/lib
	cp -f direct cmwAmbassador $(INSTALL_DIR)/bin
	cp -f close_socket.hh connect_wrapper.hh getaddrinfo_wrapper.hh tcp_server.hh  udp_stuff.hh ErrorWords.hh File.hh IO*.hh marshalling_integer.hh poll_wrapper.hh SendBuffer*.hh Formatting.hh ReceiveBuffer*.hh cmw_complex.hh platforms.hh empty_container.hh quicklz.h $(INSTALL_DIR)/include

uninstall:
	rm -f $(INSTALL_DIR)/lib/libhome.a
	rm -f $(INSTALL_DIR)/bin/direct $(INSTALL_DIR)/bin/cmwAmbassador

clean:
	rm -f $(objects) libhome.* cmwAmbassador direct

