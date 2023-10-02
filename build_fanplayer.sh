#!/bin/bash

set -e

TOPDIR=$PWD

./autogen.sh

./configure \
--prefix=$TOPDIR/_install \
--enable-static \
--disable-shared \
--with-libavdev

make -j2 && make install

echo done
