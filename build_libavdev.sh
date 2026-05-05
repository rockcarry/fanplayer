#!/bin/bash

set -e

TOPDIR=$PWD

cd libavdev

./autogen.sh

./configure \
--prefix=$TOPDIR/_install \
--host=$BUILD_HOST \
--enable-static \
--disable-shared \
--enable-tests

make -j2 && make install
cd -

echo done
