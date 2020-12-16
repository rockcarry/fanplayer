#!/bin/sh

set -e

export ANDROID_NDK_HOME=$PWD/android-ndk-r13b
export PATH=$ANDROID_NDK_HOME/toolchains/arm-linux-androideabi-4.9/prebuilt/windows/bin:$PATH

./Configure no-shared android-arm -D__ANDROID_API__=24 --prefix=$PWD/_install
make
make install_sw
