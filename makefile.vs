CC=cl
CFLAGS= -I. -std:c++latest -EHsc -O2 -nologo -D"NOMINMAX"

EXAMPLES:=example\sendExample.exe example\receiveExample.exe
all:tiers\genz.exe $(EXAMPLES)

tiers\genz.exe: tiers\genz.cc
	$(CC) -Fe:$@ $(CFLAGS) tiers\genz.cc /link "\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib"  "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"

example\sendExample.exe: example\sendExample.cc
	$(CC) -Fe:$@ $(CFLAGS) -I example example\sendExample.cc "\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib"  "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"

example\receiveExample.exe: example\receiveExample.cc
	$(CC) -Fe:$@ $(CFLAGS) -I example example\receiveExample.cc  "\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib"  "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"

clean:
	del tiers\genz.exe $(EXAMPLES)

INSTALL_DIR=\Users\Store
includes=ErrorWords.hh Buffer.hh Complex.hh wrappers.hh quicklz.h

install:
	copy $(includes)     $(INSTALL_DIR)\include
	copy tiers\genz.exe  $(INSTALL_DIR)\bin

uninstall:
	cd $(INSTALL_DIR)/include && del $(includes)

