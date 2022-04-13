#!/usr/bin/bash

set -ex
git clone https://github.com/axboe/liburing.git
cd liburing && ./configure && make -j4
#
cd ~/Downloads
tar -xvjf boost*.tar.bz2


