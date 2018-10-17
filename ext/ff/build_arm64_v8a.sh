#!/bin/bash
FFDIR=`dirname $0`
NDK=$1
PREBUILT=$NDK/toolchains/aarch64-linux-android-4.9/prebuilt/linux-x86_64
PLATFORM=$NDK/platforms/android-21/arch-arm64/
GENERAL="\
--enable-small \
--enable-cross-compile \
--extra-libs="-lgcc" \
--arch=aarch64 \
--cc=$PREBUILT/bin/aarch64-linux-android-gcc \
--cross-prefix=$PREBUILT/bin/aarch64-linux-android- \
--nm=$PREBUILT/bin/aarch64-linux-android-nm \
--extra-cflags="-I../x264/android/aarch64/include" \
--extra-ldflags="-L../x264/android/aarch64/lib" "
MODULES="\
--enable-gpl \
--enable-libx264"

cd $FFDIR/ffmpeg
./configure \
--logfile=conflog.txt \
--target-os=linux \
--prefix=./android/arm64-v8a \
${GENERAL} \
--sysroot=$PLATFORM \
--extra-cflags="" \
--extra-ldflags="-lx264 -Wl,-rpath-link=$PLATFORM/usr/lib -L$PLATFORM/usr/lib -nostdlib -lc -lm -ldl -llog" \
--enable-shared \
--disable-static \
--disable-doc \
--enable-zlib \
${MODULES}

make
make install