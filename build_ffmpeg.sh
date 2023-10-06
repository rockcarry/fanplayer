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
--disable-decoders \
--disable-encoders \
--disable-demuxers \
--disable-muxers   \
--disable-hwaccels \
--enable-decoder=h264 \
--enable-decoder=hevc \
--enable-decoder=mjpeg \
--enable-decoder=png \
--enable-decoder=mp3 \
--enable-decoder=aac \
--enable-decoder=pcm_alaw \
--enable-decoder=pcm_mulaw \
--enable-demuxer=aac \
--enable-demuxer=mjpeg \
--enable-demuxer=mp3 \
--enable-demuxer=avi \
--enable-demuxer=flv \
--enable-demuxer=mov \
--enable-demuxer=rtp \
--enable-demuxer=rtsp \
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
