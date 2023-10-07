#!/bin/bash

set -e

TOPDIR=$PWD

cd ffmpeg-3.4.13

./configure \
--pkg-config=pkg-config \
--prefix=$TOPDIR/_install \
--enable-static \
--disable-shared \
--enable-small \
--disable-symver \
--disable-debug \
--disable-swscale-alpha \
--disable-programs \
--disable-doc \
--disable-avdevice \
--disable-postproc \
--disable-avfilter \
--disable-encoders \
--disable-muxers   \
--disable-hwaccels \
--enable-muxer=mp4 \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree  \
--enable-dxva2    \
--enable-d3d11va  \
--disable-openssl \
--disable-iconv   \
--disable-bzlib   \
--disable-lzma    \
--disable-sdl2    \
--disable-xlib    \
--extra-cflags="-I$TOPDIR/_install/include" \
--extra-ldflags="-L$TOPDIR/_install/lib"

make -j2 && make install
cd -

echo done
