#!/bin/bash

set -e

TOPDIR=$PWD

cd libavdev

./autogen.sh

./configure \
--prefix=$TOPDIR/_install \
--enable-static \
--disable-shared

make -j2 && make install
cd -

echo done
