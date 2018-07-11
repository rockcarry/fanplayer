#!/bin/bash
set -e


#++ build x264 ++#
if false; then
if [ ! -d x264 ]; then
  git clone git://git.videolan.org/x264.git
fi
cd x264
./configure \
--enable-strip \
--enable-static
make -j8 && make install
cd -
fi
#-- build x264 --#


#++ build ffmpeg ++#
if [ ! -d ffmpeg ]; then
  git clone -b fanplayer-n3.3.x https://github.com/rockcarry/ffmpeg
fi
cd ffmpeg
./configure \
--pkg-config=pkg-config \
--prefix=$PWD/../ffmpeg-win32-sdk \
--enable-static \
--enable-shared \
--enable-small \
--disable-symver \
--disable-debug \
--disable-doc \
--disable-postproc \
--disable-encoders \
--disable-muxers   \
--disable-filters  \
--disable-swscale-alpha \
--disable-outdev=sdl2 \
--enable-encoder=mjpeg \
--enable-encoder=apng \
--enable-encoder=libx264 \
--enable-encoder=aac \
--enable-muxer=mjpeg \
--enable-muxer=apng \
--enable-muxer=mp4 \
--enable-muxer=flv \
--enable-filter=yadif \
--enable-filter=rotate \
--enable-filter=scale \
--enable-filter=movie \
--enable-filter=overlay \
--enable-filter=hflip \
--enable-filter=vflip \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--enable-dxva2 \
--enable-d3d11va \
--enable-libx264
make -j8 && make install
cd -
#++ build ffmpeg ++#

echo done

