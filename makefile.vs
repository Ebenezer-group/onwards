CC=cl
CFLAGS=-Isrc -EHsc -O2 -nologo -D"NOMINMAX"

LIBS:="\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib" "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"
EXAMPLES:=example\sendExample.exe example\receiveExample.exe
BASE:=src\cmw\tiers
GENZ:=$(BASE)\genz.exe
all:$(GENZ) $(EXAMPLES)

$(GENZ):$(BASE)\genz.cc
	$(CC) -Fe:$@ $(CFLAGS) -std:c++latest $(BASE)\genz.cc $(LIBS)

example\sendExample.exe: example\sendExample.cc
	$(CC) -Fe:$@ $(CFLAGS) -std:c++latest example\sendExample.cc $(LIBS)

example\receiveExample.exe: example\receiveExample.cc
	$(CC) -Fe:$@ $(CFLAGS) example\receiveExample.cc $(LIBS)

clean:
	del $(GENZ) $(EXAMPLES)

INSTALL_DIR=\Users\Store
install:
	copy src\cmw\*.hh src\cmw\quicklz.h  $(INSTALL_DIR)\include
	copy $(GENZ) $(INSTALL_DIR)\bin

uninstall:
	cd $(INSTALL_DIR)\include && del *.hh

