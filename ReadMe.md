The C++ Middleware Writer (CMW) is an on-line code generator.
The CMW and the software found in this repository provide
support for messaging and serialization.

## License
The software is under the BSD license.  Two of the files,
quicklz.h and quicklz.c, are from another developer and
should only be used in conjunction with the CMW.

## Building
If you are on \*nix, you can use either Make or CMake.
On Windows, I suggest using CMake.  If you're using a
big-endian machine, add -DCMW_ENDIAN_BIG to the CXXFLAGS
in the makefile.

Four programs are built.  Two of the programs are the middle
and front tiers of the C++ Middleware Writer.  The middle tier
is called "cmwA", which is short for CMW Ambassador.  The
front tier is called "genz" and is a command line interface.
The middle tier isn't built on Windows.  The middle tier is
still needed for Windows users so it has to be running on a
\*nix system.

A compiler with 2017 C++ support and an implementation of
std::span are required to use The CMW.  (Build flags that
dictate 2020 C++ are only used to ensure a std::span
implementation.)

## Accounts
An account is needed to use the CMW.  Before running the
middle tier, you have to get an account and modify your
cmwA.cfg file to include your account number(s) and
password(s).  To get an account send an email to
support@webEbenezer.net with "CMW account" as the subject.
After receiving an account number, update your cmwA.cfg
file accordingly.


Running the cmwA (after installing):

cmwA cmwA.cfg

After starting the cmwA, run genz like this:

genz 14 /home/brian/onwards/example/example.mdl

or on Windows:

genz.exe 14 /Users/brian/onwards/example/example.mdl

14 is an account number.  Substitute your account number there.

The path for a Middle file (.mdl) is next.  Zero or more header
files are listed in a Middle file to specify a request.  There's
more info on Middle files here:
https://github.com/Ebenezer-group/onwards/blob/master/doc/middleFiles
.

Once you have built the software, I suggest copying example.mdl
to one of your directories and modifying the copy according to
your needs.


## Troubleshooting
The middle tier has to be running for the front tier to work.
If genz fails with "No reply received.  Is the cmwA running?"
make sure the middle tier is running.

Another possible problem could be due to "breaking changes"
between the back and middle tiers.  We may have changed the
protocol between these tiers and now your version of the
middle tier no longer works.  The thing to do in this case
is to check our website for an announcement/email that you
may have missed.  Probably you will have to clone/download
the new version of the repo and then rebuild and reinstall
in order to fix the problem.

If you have only moved/renamed header files listed in your
.mdl files, you will need to touch those files (update
timestamps) so the changes will be "noticed" by the cmwA.


Thank you for using the software.
