#!/bin/bash
FFDIR=`dirname $0`
NDK=$1
PREBUILT=$NDK/toolchains/arm-linux-androideabi-4.9/prebuilt/linux-x86_64
PLATFORM=$NDK/platforms/android-16/arch-arm/
GENERAL="\
--enable-small \
--enable-cross-compile \
--extra-libs="-lgcc" \
--arch=arm \
--cc=$PREBUILT/bin/arm-linux-androideabi-gcc \
--cross-prefix=$PREBUILT/bin/arm-linux-androideabi- \
--nm=$PREBUILT/bin/arm-linux-androideabi-nm \
--extra-cflags="-I$FFDIR/x264/android/arm/include" \
--extra-ldflags="-L$FFDIR/x264/android/arm/lib" "

MODULES="\
--enable-gpl \
--enable-libx264"

cd $FFDIR/ffmpeg
 ./configure \
--target-os=linux \
--prefix=./android/armeabi \
${GENERAL} \
--sysroot=$PLATFORM \
--enable-shared \
--disable-static \
--extra-cflags=" -O3 -fpic -fasm -Wno-psabi -fno-short-enums -fno-strict-aliasing -finline-limit=300 -mfloat-abi=softfp -mfpu=vfp -marm -march=armv6" \
--extra-ldflags="-lx264 -Wl,-rpath-link=$PLATFORM/usr/lib -L$PLATFORM/usr/lib -nostdlib -lc -lm -ldl -llog" \
--enable-zlib \
${MODULES} \
--disable-doc \
--enable-neon

make
make install