CC=cl
CFLAGS= -std:c++latest -EHsc -O2 -nologo -D"NOMINMAX"

objects=marshalling_integer.obj
all: $(objects) tiers\genz.exe

marshalling_integer.obj: marshalling_integer.hh
	$(CC) $(CFLAGS) -c marshalling_integer.cc

tiers\genz.exe: tiers\genz.cc $(objects)
	$(CC) -Fe:$@ $(CFLAGS) -I. tiers\genz.cc /link $(objects)  "\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib"  "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"

EXAMPLES:=example\send_example.exe example\receive_example.exe
example: $(EXAMPLES)

example\send_example.exe: example\send_example.cc $(objects)
	$(CC) -Fe:$@ $(CFLAGS) -I. -I example example\send_example.cc $(objects)  "\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib"  "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"

example\receive_example.exe: example\receive_example.cc
	$(CC) -Fe:$@ $(CFLAGS) -I. -I example example\receive_example.cc  "\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib"  "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"

clean:
	del $(objects) tiers\genz.exe $(EXAMPLES)

INSTALL_DIR=\Users\Store
includes=tcp_server.hh udp_stuff.hh ErrorWords.hh File.hh IO.hh marshalling_integer.hh SendBuffer*.hh ReceiveBuffer*.hh cmw_complex.hh platforms.hh message_id_types.hh wrappers.hh quicklz.h

install:
	copy $(includes)     $(INSTALL_DIR)\include
	copy tiers\genz.exe  $(INSTALL_DIR)\bin

uninstall:
	cd $(INSTALL_DIR)/include && del $(includes)

