#!/bin/bash

set -e

#
# howto build fanplayer_jni ?
# enviroment setup:
# 1. msys2 + android-ndk-r13b
# 2. export ANDROID_NDK_HOME
# 3. ./build_fanplayer_jni.sh arm (arm, arm64, x86 or x86_64)
#

if [ x"$1" == x"" ]; then
    ARCH=arm
else
    ARCH=$1
fi

case "$ARCH" in
arm)
    CPU=armv7-a
    TOOLCHAIN_PATH=arm-linux-androideabi-4.9
    CROSS_PREFIX=arm-linux-androideabi-
    OPENSSL_TARGET=android-arm
    JNI_DIR=armeabi-v7a
    ;;
arm64)
    CPU=armv8-a
    TOOLCHAIN_PATH=aarch64-linux-android-4.9
    CROSS_PREFIX=aarch64-linux-android-
    OPENSSL_TARGET=android-arm64
    JNI_DIR=arm64-v8a
    ;;
x86)
    CPU=i686
    TOOLCHAIN_PATH=x86-4.9
    CROSS_PREFIX=i686-linux-android-
    OPENSSL_TARGET=android-x86
    JNI_DIR=x86
    ;;
x86_64)
    CPU=x86-64
    TOOLCHAIN_PATH=x86_64-4.9
    CROSS_PREFIX=x86_64-linux-android-
    OPENSSL_TARGET=android-x86_64
    JNI_DIR=x86_64
    ;;
*)
    echo "unsupported arch type: $ARCH !"
    exit 0
    ;;
esac

export PATH=$PATH:$ANDROID_NDK_HOME/toolchains/$TOOLCHAIN_PATH/prebuilt/windows-x86_64/bin
export PKG_CONFIG_PATH=$PWD/_install/lib/pkgconfig:$PKG_CONFIG_PATH

SYSROOT=$ANDROID_NDK_HOME/platforms/android-21/arch-$ARCH
PREFIX_DIR=$PWD/_install
EXTRA_CFLAGS="-I$PREFIX_DIR/include -DANDROID -DNDEBUG -Os -ffast-math"
EXTRA_LDFLAGS="-L$PREFIX_DIR/lib"

#++ build openssl ++#
if [ ! -d openssl ]; then
    git clone https://github.com/openssl/openssl.git
fi
cd openssl
git checkout .
git checkout OpenSSL_1_1_1k
./Configure no-shared $OPENSSL_TARGET -D__ANDROID_API__=21 --prefix=$PREFIX_DIR
make -j2 && make install_sw
cd -
#-- build openssl --#

#++ build soundtouch ++#
if [ ! -d soundtouch ]; then
    git clone https://github.com/rockcarry/soundtouch.git
fi
cd soundtouch
echo "#define SOUNDTOUCH_FLOAT_SAMPLES 1" > $PWD/include/soundtouch_config.h
${CROSS_PREFIX}gcc --sysroot=$SYSROOT -fvisibility=hidden -fdata-sections -ffunction-sections -c \
-I$ANDROID_NDK_HOME/sources/cxx-stl/stlport/stlport -I$PWD/include -I$PWD/source/Android-lib/jni \
$PWD/source/Android-lib/jni/soundtouch-lib.cpp \
$PWD/source/SoundTouch/AAFilter.cpp \
$PWD/source/SoundTouch/FIFOSampleBuffer.cpp \
$PWD/source/SoundTouch/FIRFilter.cpp \
$PWD/source/SoundTouch/cpu_detect_x86.cpp \
$PWD/source/SoundTouch/sse_optimized.cpp \
$PWD/source/SoundStretch/WavFile.cpp \
$PWD/source/SoundTouch/RateTransposer.cpp \
$PWD/source/SoundTouch/SoundTouch.cpp \
$PWD/source/SoundTouch/InterpolateCubic.cpp \
$PWD/source/SoundTouch/InterpolateLinear.cpp \
$PWD/source/SoundTouch/InterpolateShannon.cpp \
$PWD/source/SoundTouch/TDStretch.cpp \
$PWD/source/SoundTouch/BPMDetect.cpp \
$PWD/source/SoundTouch/PeakFinder.cpp
${CROSS_PREFIX}ar rcs $PWD/libsoundtouch.a $PWD/*.o
cp $PWD/libsoundtouch.a $PREFIX_DIR/lib
cp $PWD/source/Android-lib/jni/soundtouch-lib.h $PREFIX_DIR/include/soundtouch.h
cd -
#-- build soundtouch --#

#++ build ffmpeg ++#
if [ ! -d ffmpeg ]; then
  git clone -b fanplayer-n3.3.x https://github.com/rockcarry/ffmpeg
fi
cd ffmpeg
./configure \
--pkg-config=pkg-config \
--arch=$ARCH \
--cpu=$CPU \
--target-os=android \
--enable-cross-compile \
--cross-prefix=$CROSS_PREFIX \
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
--disable-muxers \
--disable-filters \
--disable-swscale-alpha \
--enable-encoder=mjpeg \
--enable-encoder=apng \
--enable-encoder=aac \
--enable-muxer=mjpeg \
--enable-muxer=apng \
--enable-muxer=mp4 \
--enable-muxer=flv \
--enable-muxer=avi \
--enable-filter=yadif \
--enable-filter=rotate \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--enable-openssl \
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
make -j2 && make install
cd -
#-- build ffmpeg --#

#++ build fanplayer_jni ++#
${CROSS_PREFIX}gcc --sysroot=$SYSROOT -Wall -Wno-deprecated-declarations -fPIC -fno-strict-aliasing -shared -Os -o $PWD/libfanplayer_jni.so \
-I$ANDROID_NDK_HOME/sources/cxx-stl/stlport/stlport -I$PWD -I$PWD/_install/include -I$PWD/../../src -I$PWD/../../avkcpdemuxer -I$PWD/../../ffrdpdemuxer \
-L$ANDROID_NDK_HOME/sources/cxx-stl/stlport/libs/$JNI_DIR \
-DANDROID -DNDEBUG -DENABLE_AVKCP_SUPPORT -DENABLE_FFRDP_SUPPORT -DCONFIG_ENABLE_AES256 \
$PWD/fanplayer_jni.cpp \
$PWD/../../src/adev-android.cpp \
$PWD/../../src/vdev-android.cpp \
$PWD/../../src/vdev-cmn.c \
$PWD/../../src/datarate.c \
$PWD/../../src/ffplayer.c \
$PWD/../../src/ffrender.c \
$PWD/../../src/pktqueue.c \
$PWD/../../src/recorder.c \
$PWD/../../src/snapshot.c \
$PWD/../../avkcpdemuxer/ikcp.c \
$PWD/../../avkcpdemuxer/ringbuf.c \
$PWD/../../avkcpdemuxer/avkcpc.c \
$PWD/../../avkcpdemuxer/avkcpd.c \
$PWD/../../ffrdpdemuxer/ffrdp.c \
$PWD/../../ffrdpdemuxer/ffrdpc.c \
$PWD/../../ffrdpdemuxer/ffrdpd.c \
$PWD/_install/lib/libavdevice.a \
$PWD/_install/lib/libavformat.a \
$PWD/_install/lib/libavcodec.a \
$PWD/_install/lib/libavfilter.a \
$PWD/_install/lib/libswresample.a \
$PWD/_install/lib/libswscale.a \
$PWD/_install/lib/libavutil.a \
$PWD/_install/lib/libsoundtouch.a \
$PWD/_install/lib/libssl.a \
$PWD/_install/lib/libcrypto.a \
-lstlport_static -lm -lz -llog -landroid
${CROSS_PREFIX}strip $PWD/libfanplayer_jni.so
mkdir -p $PWD/../apk/app/src/main/jniLibs/$JNI_DIR
mv $PWD/libfanplayer_jni.so $PWD/../apk/app/src/main/jniLibs/$JNI_DIR
#-- build fanplayer_jni --#

echo done

