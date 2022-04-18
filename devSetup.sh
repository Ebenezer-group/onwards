#!/usr/bin/bash

set -ex
cd ~/Downloads
git clone https://github.com/axboe/liburing.git
cd liburing && ./configure && make -j4
#
cd ~/Downloads && tar -xvjf boost*.tar.bz2

