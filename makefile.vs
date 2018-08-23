CC=cl
CFLAGS= -Isrc -std:c++latest -EHsc -O2 -nologo -D"NOMINMAX"

EXAMPLES:=example\sendExample.exe example\receiveExample.exe
GENZ:=src\cmw\tiers\genz.exe
all:$(GENZ) $(EXAMPLES)

$(GENZ): src\cmw\tiers\genz.cc
	$(CC) -Fe:$@ $(CFLAGS) src\cmw\tiers\genz.cc /link "\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib"  "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"

example\sendExample.exe: example\sendExample.cc
	$(CC) -Fe:$@ $(CFLAGS) -I example example\sendExample.cc "\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib"  "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"

example\receiveExample.exe: example\receiveExample.cc
	$(CC) -Fe:$@ $(CFLAGS) -I example example\receiveExample.cc  "\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib"  "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"

clean:
	del $(GENZ) $(EXAMPLES)

INSTALL_DIR=\Users\Store
install:
	copy src\cmw\*.hh src\cmw\quicklz.h  $(INSTALL_DIR)\include
	copy $(GENZ) $(INSTALL_DIR)\bin

uninstall:
	cd $(INSTALL_DIR)/include && del *.hh

