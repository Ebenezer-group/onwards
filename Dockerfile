FROM gcc:11

WORKDIR /onwards
COPY . . 
RUN set -ex;\
    make;\
    make install

