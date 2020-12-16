#!/bin/bash
set -e

PREFIX_DIR=$PWD/ffmpeg-android-sdk
SYSROOT=$NDK_HOME/platforms/android-21/arch-x86/
CROSS_COMPILE=$NDK_HOME/toolchains/x86-4.9/prebuilt/windows/bin/i686-linux-android-
EXTRA_CFLAGS="-I$PREFIX_DIR/include -DANDROID -DNDEBUG -Os -ffast-math"
EXTRA_LDFLAGS="-L$PREFIX_DIR/lib"

#++ build ffmpeg ++#
if [ ! -d ffmpeg ]; then
  git clone -b fanplayer-n3.3.x https://github.com/rockcarry/ffmpeg
fi
cd ffmpeg
./configure \
--pkg-config=pkg-config \
--arch=x86 \
--cpu=i686 \
--target-os=android \
--enable-cross-compile \
--cross-prefix=$CROSS_COMPILE \
--sysroot=$SYSROOT \
--prefix=$PREFIX_DIR \
--enable-thumb \
--enable-static \
--enable-small \
--disable-shared \
--disable-symver \
--disable-debug \
--disable-programs \
--disable-doc \
--disable-postproc \
--disable-encoders \
--disable-muxers   \
--disable-filters  \
--disable-swscale-alpha \
--enable-encoder=mjpeg \
--enable-encoder=apng \
--enable-encoder=aac \
--enable-muxer=mjpeg \
--enable-muxer=apng \
--enable-muxer=mp4 \
--enable-muxer=flv \
--enable-filter=yadif \
--enable-filter=rotate \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--enable-jni \
--enable-mediacodec \
--enable-decoder=h264_mediacodec \
--enable-decoder=hevc_mediacodec \
--enable-decoder=mpeg2_mediacodec \
--enable-decoder=mpeg4_mediacodec \
--enable-decoder=vp8_mediacodec \
--enable-decoder=vp9_mediacodec \
--extra-cflags="$EXTRA_CFLAGS" \
--extra-ldflags="$EXTRA_LDFLAGS"
make -j8 && make install
cd -
#++ build ffmpeg ++#

echo done

