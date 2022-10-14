#!/bin/bash
set -e

export PKG_CONFIG_PATH=$PWD/_install/lib/pkgconfig:$PKG_CONFIG_PATH

#++ build zlib ++#
if [ ! -d zlib ]; then
  git clone https://github.com.cnpmjs.org/madler/zlib.git
fi
cd zlib
git checkout .
git checkout v1.2.11
make -j2 -f win32/Makefile.gcc
make DESTDIR=$PWD/../_install/ INCLUDE_PATH=include LIBRARY_PATH=lib BINARY_PATH=bin -f win32/Makefile.gcc install
cd -
#-- build zlib --#

#++ build openssl ++#
if [ ! -d openssl ]; then
  git clone https://github.com.cnpmjs.org/openssl/openssl.git
fi
cd openssl
git checkout .
git checkout OpenSSL_1_1_1k
./Configure --prefix=$PWD/../_install/ mingw shared
make -j2 && make install_sw
cd -
#-- build openssl --#

#++ build ffmpeg ++#
if [ ! -d ffmpeg ]; then
  git clone -b fanplayer-n4.3.x https://github.com.cnpmjs.org/rockcarry/ffmpeg
fi
cd ffmpeg

./configure \
--pkg-config=pkg-config \
--prefix=$PWD/../_install \
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
--enable-filter=scale \
--enable-filter=movie \
--enable-filter=overlay \
--enable-filter=hflip \
--enable-filter=vflip \
--enable-asm \
--enable-gpl \
--enable-version3 \
--enable-nonfree \
--enable-dxva2   \
--enable-d3d11va \
--enable-openssl \
--disable-iconv  \
--disable-bzlib  \
--disable-lzma   \
--disable-sdl2   \
--extra-cflags="-I$PWD/../_install/include" \
--extra-ldflags="-L$PWD/../_install/lib"

make -j2 && make install
cd -
#-- build ffmpeg --#

#++ copy files ++#
rm -rf $PWD/bin/* $PWD/include/lib* $PWD/include/openssl
mkdir -p $PWD/include $PWD/bin
cp -r $PWD/_install/include/lib* $PWD/_install/include/openssl $PWD/include/
cp -r $PWD/_install/bin/ffmpeg.exe $PWD/_install/bin/*.dll $PWD/_install/bin/*.lib $PWD/bin/
cp /mingw32/bin/libgcc_s_dw2-1.dll /mingw32/bin/libwinpthread-1.dll $PWD/bin/
strip $PWD/bin/*.exe $PWD/bin/*.dll
cp /mingw32/lib/libpthread.dll.a $PWD/bin/pthread.lib
cp /mingw32/include/pthread*.h   $PWD/include/
cp /mingw32/include/sched.h      $PWD/include/
cp /mingw32/include/semaphore.h  $PWD/include/
dlltool -l $PWD/bin/libcrypto.lib -d $PWD/openssl/libcrypto.def
dlltool -l $PWD/bin/libssl.lib    -d $PWD/openssl/libssl.def
#-- copy files --#

echo done

