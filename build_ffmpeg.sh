#!/bin/bash

set -e

TOPDIR=$PWD

cd ffmpeg-4.3.6

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
