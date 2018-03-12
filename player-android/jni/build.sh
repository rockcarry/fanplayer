#!/bin/bash
set -e

BASEDIR=$PWD

echo ""
echo "building fanplayer jni library using ndk..."
echo ""

export PATH=$PATH:$NDK_HOME
ndk-build

cp -r $BASEDIR/../libs/* $BASEDIR/../apk/app/src/main/jniLibs/
rm -rf $BASEDIR/../libs/ $BASEDIR/../obj/

echo ""
echo "build fanplayer jni library done !"
echo ""

