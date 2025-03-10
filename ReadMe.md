![onwards](https://github.com/Ebenezer-group/onwards/actions/workflows/main.yml/badge.svg)
The C++ Middleware Writer (CMW) is an on-line code generator.  The CMW
and the software found in this repository provide support for messaging
and serialization.  Here's an example
of the [input](https://github.com/Ebenezer-group/onwards/blob/master/example/example.mdl)
and [output](https://github.com/Ebenezer-group/onwards/blob/master/example/example.mdl.hh).
 A compiler with support for C++ 2020 or newer is required to use the CMW.

#### License
The software is under the BSD license.  One of the files, quicklz.h, is from another
developer and should only be used in conjunction with the CMW.

#### Building
The
[library](https://github.com/Ebenezer-group/onwards/blob/master/src)
is header-only.

The [middle tier](https://github.com/Ebenezer-group/onwards/blob/master/src/tiers/cmwA.cc)
of the CMW, called "cmwA", is a Linux-only program.  Non-Linux developers
can use the CMW by running the middle tier on a Linux system.
The [front tier](https://github.com/Ebenezer-group/onwards/blob/master/src/tiers/front/genz.cc)
of the CMW, called "genz", is built on all platforms.

The following can be used:

mkdir build; cd build

cmake -S .. -G "Unix Makefiles"

make

#### Configuration
Before running the middle tier, modify your cmwA.cfg file to include your
ambassador ID and password. The maximum length of an ambassador ID is 20.

#### Running the middle tier -- cmwA (after installing)
The middle tier is used for two purposes: registration/signup and
normal operation.  Before the middle tier can be run normally, you need
to run it with the -signup flag:

cmwA cmwA.cfg -signup

The program exits when run this way.  If the signup succeeds, a message
is output to the terminal indicating so.  Otherwise, an error message is
logged.  After registering, the cmwA can be run normally:

nohup cmwA cmwA.cfg &

#### Accounts
After successfully signing up, you can request one or more accounts be 
associated with your ambassador.  To get an account send an email to 
support@webEbenezer.net with "Account" as the subject, and mention the 
ambassador ID that you have chosen.


#### Running the front tier -- genz
After starting the cmwA, run genz like this:

genz 11 /home/brian/onwards/example/example.mdl

11 is an account number.  Substitute your account number there.

The path to a Middle file (.mdl) is next.  Zero or more header
files are listed in a Middle file to specify a request.  There's
more info on Middle files here:
https://github.com/Ebenezer-group/onwards/blob/master/doc/middleFiles
.

Once you have built the software, I suggest copying example.mdl
to one of your directories and modifying the copy according to
your needs.


#### Troubleshooting
The middle tier has to be running for the front tier to work.
If genz fails with "No reply received.  Is the cmwA running?"
make sure the middle tier is running.

Another possible problem could be due to a "breaking change"
between the back and middle tiers.  We may have changed the
protocol between these tiers and now your version of the
middle tier no longer works.  The thing to do in this case
is to check our website for an announcement/email that you
may have missed.  Probably you will have to clone the new
version of the repo and then rebuild and reinstall in order
to fix the problem.

If you have only moved/renamed header files listed in your
.mdl files, you will need to touch those files (update
timestamps) so the changes will be "noticed" by the cmwA.


Thank you for using the software.
