#!/bin/bash

set -e

TOPDIR=$PWD

./autogen.sh

./configure \
--prefix=$TOPDIR/_install \
--host=$BUILD_HOST \
--enable-static \
--disable-shared \
--with-libavdev

make -j2 && make install

echo done
