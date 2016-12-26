
CC=cl
CFLAGS= -EHsc -O2 -nologo

all: libhome.lib cmwAmbassador direct


.c.obj:
	$(CC) $(CFLAGS) -c $*.c -o $*.obj

close_socket.obj: close_socket.hh
	$(CC) -c $(CFLAGS) close_socket.cc

quicklz.obj: quicklz.h
	$(CC) -c $(CFLAGS) quicklz.c

IO.obj: IO.hh
	$(CC) -c $(CFLAGS) IO.cc

SendBuffer.obj: SendBuffer.hh
	$(CC) -c $(CFLAGS) SendBuffer.cc

SendBufferCompressed.obj: SendBufferCompressed.hh
	$(CC) -c $(CFLAGS) SendBufferCompressed.cc

SendBufferStdString.obj: SendBuffer.hh
	$(CC) -c $(CFLAGS) SendBufferStdString.cc

SendBufferFlush.obj: SendBuffer.hh
	$(CC) -c $(CFLAGS) SendBufferFlush.cc

#SendBufferFile.obj: SendBufferFile.hh
#	$(CC) -c $(CFLAGS) SendBufferFile.cc

File.obj: File.hh
	$(CC) -c $(CFLAGS) File.cc

marshalling_integer.obj: marshalling_integer.hh
	$(CC) -c $(CFLAGS) marshalling_integer.cc

ErrorWords.obj: ErrorWords.hh
	$(CC) -c $(CFLAGS) ErrorWords.cc

FileMarshal.obj: File.hh
	$(CC) -c $(CFLAGS) FileMarshal.cc

objects = close_socket.obj quicklz.obj IO.obj SendBuffer.obj SendBufferCompressed.obj File.obj marshalling_integer.obj SendBufferStdString.obj SendBufferFlush.obj ErrorWords.obj FileMarshal.obj

libhome.lib: $(objects)
	lib /out:$@ $(objects)

direct: direct.cc libhome.lib
	$(CC) $(CFLAGS) direct.cc /link libhome.lib  "\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib"   "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"

cmwAmbassador: cmwAmbassador.cc libhome.lib
	$(CC) $(CFLAGS) cmwAmbassador.cc /link libhome.lib  "\program files\Microsoft SDKs\Windows\v7.1\Lib\wsock32.lib"   "\program files\Microsoft SDKs\Windows\v7.1\Lib\ws2_32.lib"

clean:
	del  *.obj libhome.lib direct.exe

install:
	copy *.exe  \Users\Store\bin\
