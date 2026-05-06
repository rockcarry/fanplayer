#!/bin/bash

set -e

TOPDIR=$PWD

cd ffmpeg
git checkout .
git checkout n7.1.4

./configure \
--pkg-config=pkg-config \
--prefix=$TOPDIR/_install \
--arch=i686 \
--target-os=mingw64 \
--cross-prefix=$CROSS_COMPILE \
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
--disable-pixelutils \
--disable-encoders \
--disable-muxers   \
--enable-encoder=mjpeg \
--enable-encoder=png \
--enable-muxer=mp4 \
--enable-muxer=flv \
--enable-muxer=avi \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree  \
--disable-openssl \
--disable-iconv   \
--disable-bzlib   \
--disable-lzma    \
--disable-sdl2    \
--disable-xlib    \
--extra-cflags="-I$TOPDIR/_install/include" \
--extra-ldflags="-L$TOPDIR/_install/lib"

make -j8 && make install
cd -

echo done
