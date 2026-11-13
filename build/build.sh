#!/bin/bash
set -euo pipefail

SRC=/src
LOADER=/loader
WORK=/work
P=/prefix
OUT=/out/bin
MAKEJ=${MAKEJ:-4}
ARCH=$(uname -m)

mkdir -p $WORK $P/lib $P/include $OUT
export PATH=$P/bin:$PATH
export PKG_CONFIG_PATH=$P/lib/pkgconfig:$P/lib64/pkgconfig:$P/share/pkgconfig
export PKG_CONFIG_LIBDIR=$PKG_CONFIG_PATH

if [[ $ARCH == x86_64 ]]; then
    ARCH_CFLAGS=""
    FFMPEG_CFLAGS=""
    OPENSSL_TARGET=linux-x86_64
    VPX_TARGET=x86_64-linux-gcc
    OPENH264_ARCH=x86_64
    KERNEL_ARCH=x86
else
    ARCH_CFLAGS="-march=armv8-a+crypto -mcpu=cortex-a57+crypto"
    FFMPEG_CFLAGS="-fno-aggressive-loop-optimizations"
    OPENSSL_TARGET=linux-aarch64
    VPX_TARGET=armv8-linux-gcc
    OPENH264_ARCH=arm64
    KERNEL_ARCH=arm64
fi
export CFLAGS="$ARCH_CFLAGS -I$P/include -fPIC"
export CXXFLAGS="$CFLAGS"
export LDFLAGS="-L$P/lib"

CMAKE_COMMON="-DCMAKE_INSTALL_PREFIX=$P -DCMAKE_INSTALL_LIBDIR=lib -DCMAKE_INSTALL_INCLUDEDIR=include -DCMAKE_INSTALL_BINDIR=bin -DCMAKE_BUILD_TYPE=Release -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5"

cd $WORK
for archive in $SRC/*.tar.*; do tar -xf "$archive"; done

if [[ $ARCH == x86_64 ]]; then
    cd $WORK/nasm-3.02
    sed -i '/^\s*#\s*typedef _Bool bool/d' include/compiler.h
    sed -i 's/void pure_func/void/g' include/nasmlib.h
    ./configure --prefix=$P
    make -j $MAKEJ V=0
    make install
    export AS=nasm
fi

cd $WORK/zimg-release-3.0.6
autoreconf -iv
./configure --prefix=$P --disable-shared --with-pic
make -j $MAKEJ V=0
make install

cd $WORK/zlib-1.3.2
CC="gcc -fPIC" ./configure --prefix=$P --static
make -j $MAKEJ V=0
make install

cd $WORK/lame-3.100
patch -Np1 < $SRC/patches/lame.patch
./configure --prefix=$P --disable-frontend --disable-shared --with-pic
make -j $MAKEJ V=0
make install

cd $WORK/libxml2-2.9.12
./configure --prefix=$P --enable-static --disable-shared --without-iconv --without-python --without-lzma --with-pic
make -j $MAKEJ V=0
make install

cd $WORK/speex-1.2.1
PKG_CONFIG= ./configure --prefix=$P --disable-shared --with-pic
make -j $MAKEJ V=0
make install

cd $WORK/opus-1.3.1
./configure --prefix=$P --disable-shared --with-pic
make -j $MAKEJ V=0
make install

cd $WORK/opencore-amr-0.1.6
./configure --prefix=$P --disable-shared --with-pic
make -j $MAKEJ V=0
make install

cd $WORK/vo-amrwbenc-0.1.3
./configure --prefix=$P --disable-shared --with-pic
make -j $MAKEJ V=0
make install

cd $WORK/openssl-3.5.7
./Configure $OPENSSL_TARGET -fPIC no-shared --prefix=$P --libdir=lib $ARCH_CFLAGS
make -s -j $MAKEJ
make install_sw

cd $WORK/srt-1.5.6
cmake $CMAKE_COMMON -DENABLE_APPS:BOOL=OFF -DENABLE_ENCRYPTION:BOOL=ON -DENABLE_SHARED:BOOL=OFF -DOPENSSL_ROOT_DIR=$P -DOPENSSL_USE_STATIC_LIBS=ON .
make -j $MAKEJ V=0
make install

cd $WORK/openh264-2.6.0
make -j $MAKEJ DESTDIR=./ PREFIX=.. AR=ar OS=linux ARCH=$OPENH264_ARCH USE_ASM=No install-static
cp -r include/* $P/include/ 2>/dev/null || true
cp -r ../include/wels $P/include/ 2>/dev/null || true
cp ../lib/libopenh264.a $P/lib/ 2>/dev/null || true

cd $WORK/libvpx-1.16.0
if [[ $ARCH == x86_64 ]]; then
    ./configure --prefix=$P --enable-static --enable-pic --disable-examples --disable-unit-tests --target=$VPX_TARGET --as=nasm
else
    patch -Np1 < $SRC/patches/libvpx-linux-arm.patch
    sed -i '/neon_i8mm/d' ./configure
    ./configure --prefix=$P --enable-static --enable-pic --disable-examples --disable-unit-tests --target=$VPX_TARGET
    sed -i 's/HAS_NEON_I8MM (1 << 2)/HAS_NEON_I8MM 0/g' vpx_ports/*.h
    sed -i 's/#if HAVE_NEON_I8MM/#if 0/g' test/*.c* vpx_ports/*.c*
    sed -i 's/flags & HAS_NEON_I8MM/0/g' *.h
fi
make -j $MAKEJ
make install

cd $WORK/libwebp-1.6.0
cmake $CMAKE_COMMON -DWEBP_BUILD_ANIM_UTILS=OFF -DWEBP_BUILD_CWEBP=OFF -DWEBP_BUILD_DWEBP=OFF -DWEBP_BUILD_EXTRAS=OFF -DWEBP_BUILD_GIF2WEBP=OFF -DWEBP_BUILD_IMG2WEBP=OFF -DWEBP_BUILD_VWEBP=OFF -DWEBP_BUILD_WEBPINFO=OFF -DWEBP_BUILD_WEBPMUX=OFF -DWEBP_BUILD_WEBP_JS=OFF .
make -j $MAKEJ V=0
make install

cd $WORK/freetype-2.14.3
./configure --prefix=$P --with-bzip2=no --with-harfbuzz=no --with-png=no --with-brotli=no --enable-static --disable-shared --with-pic
make -j $MAKEJ
make install

cd $WORK/harfbuzz-14.3.0
meson setup build --prefix=$P --default-library=static -Dcairo=disabled -Dchafa=disabled -Dcoretext=disabled -Ddirectwrite=disabled -Ddocs=disabled -Dfreetype=enabled -Dglib=disabled -Dgobject=disabled -Dgraphite=disabled -Dicu=disabled -Dtests=disabled -Dutilities=disabled -Dintrospection=disabled --libdir=lib --buildtype=release
meson compile -C build -j $MAKEJ
meson install -C build

cd $WORK/libdrm-2.4.124
meson setup build --prefix=$P --libdir=lib --buildtype=release -Dintel=disabled -Dradeon=disabled -Damdgpu=disabled -Dnouveau=disabled -Dvmwgfx=disabled -Domap=disabled -Dexynos=disabled -Dfreedreno=disabled -Dtegra=disabled -Dvc4=disabled -Detnaviv=disabled -Dcairo-tests=disabled -Dman-pages=disabled -Dvalgrind=disabled -Dtests=false -Dudev=false -Dinstall-test-programs=false
meson compile -C build -j $MAKEJ
meson install -C build

cd $WORK/libva-2.22.0
meson setup build --prefix=$P --libdir=lib --buildtype=release -Ddisable_drm=false -Dwith_x11=no -Dwith_glx=no -Dwith_wayland=no -Denable_docs=false
meson compile -C build -j $MAKEJ
meson install -C build

cd $WORK/alsa-lib-1.2.16.1
./configure --prefix=$P --disable-python --disable-static --enable-shared
make -j $MAKEJ
make install

cd $WORK/xcb-proto-1.17.0
PYTHON=python3 ./configure --prefix=$P
make install

cd $WORK/libxcb-1.17.0/src
mkdir -p $P/include/xcb
for proto in xproto shm; do
    python3 c_client.py -c "libxcb 1.17.0" -l "X Version 11" -s 3 -p $(ls -d $P/lib/python3*/site-packages) $P/share/xcb/$proto.xml
    cp $proto.h $P/include/xcb/
done
cp xcb.h $P/include/xcb/
printf 'prefix=%s\nincludedir=${prefix}/include\nlibdir=${prefix}/lib\n\nName: XCB\nDescription: X-protocol C Binding\nVersion: 1.17.0\nCflags: -I${includedir}\nLibs: -L${libdir} -lxcb\n' $P > $P/lib/pkgconfig/xcb.pc
printf 'prefix=%s\nincludedir=${prefix}/include\nlibdir=${prefix}/lib\n\nName: XCB Shm\nDescription: XCB Shm Extension\nVersion: 1.17.0\nRequires: xcb\nCflags: -I${includedir}\nLibs: -L${libdir} -lxcb-shm\n' $P > $P/lib/pkgconfig/xcb-shm.pc

cd $WORK/pulseaudio-17.0/src/pulse
mkdir -p $P/include/pulse
cp *.h $P/include/pulse/
sed -e 's/@PA_MAJOR@/17/g' -e 's/@PA_MINOR@/0/g' -e 's/@PA_API_VERSION@/12/g' -e 's/@PA_PROTOCOL_VERSION@/35/g' version.h.in > $P/include/pulse/version.h
printf 'prefix=%s\nincludedir=${prefix}/include\nlibdir=${prefix}/lib\n\nName: libpulse\nDescription: PulseAudio Client Interface\nVersion: 17.0\nCflags: -I${includedir} -D_REENTRANT\nLibs: -L${libdir} -lpulse\n' $P > $P/lib/pkgconfig/libpulse.pc

cp $WORK/systemd-257/src/libudev/libudev.h $P/include/
printf 'prefix=%s\nincludedir=${prefix}/include\nlibdir=${prefix}/lib\n\nName: libudev\nDescription: Library to access udev device information\nVersion: 257\nCflags: -I${includedir}\nLibs: -L${libdir} -ludev\n' $P > $P/lib/pkgconfig/libudev.pc

cd $WORK/linux-5.15
make headers_install ARCH=$KERNEL_ARCH INSTALL_HDR_PATH=$P/kernel

cd $P/lib
rm -f libva.so* libva-drm.so* libdrm.so* libasound.so* libasound.la libatopology.so* libatopology.la
gcc -shared -fPIC -O2 $ARCH_CFLAGS -I$P/include -I$P/include/libdrm -Wl,-soname,liboptional-system-libraries.so -o liboptional-system-libraries.so $LOADER/optional_system_libraries.c -ldl -lpthread
for name in va va-drm drm pulse asound xcb xcb-shm udev; do ln -s liboptional-system-libraries.so lib$name.so; done

FFMPEG_EXTRA=""
if [[ $ARCH == x86_64 ]]; then
    cd $WORK/mfx_dispatch-1.35.1
    autoreconf -fiv
    CXXFLAGS="$CXXFLAGS -include cstdint" ./configure --prefix=$P --disable-shared --enable-static --enable-fast-install --with-pic
    make -j $MAKEJ
    make install
    FFMPEG_EXTRA="--enable-libmfx"
fi

cd $WORK/nv-codec-headers-n13.1.15.0
make install PREFIX=$P

cd $WORK/Vulkan-Headers-1.4.341
cmake $CMAKE_COMMON -B build .
cmake --install build

cd $WORK/libaom-3.14.1
mkdir -p build_release && cd build_release
cmake $CMAKE_COMMON -DENABLE_TESTS:BOOL=OFF -DENABLE_TESTDATA:BOOL=OFF -DENABLE_TOOLS:BOOL=OFF -DENABLE_EXAMPLES:BOOL=OFF -DENABLE_DOCS:BOOL=OFF ..
make -j $MAKEJ
make install

cd $WORK/SVT-AV1-v4.2.0
sed -i 's/CMAKE_C_COMPILER_ID MATCHES "Clang"/FALSE/g' CMakeLists.txt
mkdir -p build_release && cd build_release
cmake $CMAKE_COMMON -DBUILD_APPS:BOOL=OFF -DBUILD_TESTING:BOOL=OFF -DBUILD_SHARED_LIBS=OFF ..
make -j $MAKEJ
make install

cd $WORK/ffmpeg-8.1.2
patch -Np1 < $SRC/patches/ffmpeg.patch
patch -Np1 < $SRC/patches/ffmpeg-vulkan.patch
patch -Np1 < $SRC/patches/ffmpeg-v4l2-request-20847-ffmpeg-8.1.patch
sed -i '/ANativeWindow_release/d' libavutil/hwcontext_mediacodec.c
sed -i 's/#define MAX_SLICES 32/#define MAX_SLICES 256/g' libavcodec/h264dec.h
unset CFLAGS CXXFLAGS LDFLAGS
LDEXEFLAGS='-Wl,-rpath,\$$ORIGIN/' LDSOFLAGS='-Wl,-rpath,\$$$$ORIGIN/' ./configure --prefix=$P \
    --disable-iconv --disable-opencl --disable-sdl2 --disable-bzlib --disable-lzma --disable-linux-perf --disable-xlib \
    --enable-libxcb --enable-libxcb-shm --disable-libxcb-xfixes --disable-libxcb-shape --enable-libpulse --enable-alsa --disable-sndio \
    --enable-shared --disable-static --enable-version3 --enable-runtime-cpudetect --enable-zlib --enable-libmp3lame --enable-libspeex \
    --enable-libopencore-amrnb --enable-libopencore-amrwb --enable-libvo-amrwbenc --enable-openssl --enable-libopenh264 \
    --enable-libvpx --enable-libfreetype --enable-libharfbuzz --enable-libopus --enable-libxml2 --enable-libsrt --enable-libwebp \
    --enable-libaom --enable-libsvtav1 --enable-libzimg \
    --enable-vulkan --enable-hwaccel=h264_vulkan --enable-hwaccel=hevc_vulkan --enable-hwaccel=av1_vulkan \
    --enable-libdrm --enable-vaapi --enable-v4l2-request --enable-libudev --enable-cuda --enable-cuda-llvm --enable-cuvid --enable-nvenc --enable-pthreads $FFMPEG_EXTRA \
    --extra-cflags="$ARCH_CFLAGS -I$P/kernel/include -I$P/include -I$P/include/freetype2 -I$P/include/opus -I$P/include/libxml2 -I$P/include/mfx -I$P/include/svt-av1 $FFMPEG_CFLAGS" \
    --extra-ldflags="-L$P/lib" \
    --extra-libs="-lstdc++ -lpthread -ldl -lz -lm" \
    || { tail -60 ffbuild/config.log; exit 1; }
make -j $MAKEJ
make install

cp -L $P/bin/ffmpeg $P/bin/ffprobe $OUT/
for library in avcodec avdevice avfilter avformat avutil swresample swscale; do
    soname=$(readelf -d $P/lib/lib$library.so | sed -n 's/.*(SONAME).*\[\(.*\)\]/\1/p')
    cp -L $P/lib/lib$library.so $OUT/$soname
done
cp -L $P/lib/liboptional-system-libraries.so $OUT/
strip --strip-unneeded $OUT/*
$OUT/ffmpeg -hide_banner -buildconf > /out/buildconf.txt

allowed='^(libc\.so\.6|libm\.so\.6|libdl\.so\.2|libpthread\.so\.0|librt\.so\.1|libgcc_s\.so\.1|libstdc\+\+\.so\.6|ld-linux-x86-64\.so\.2|ld-linux-aarch64\.so\.1|libavcodec\.so\.62|libavdevice\.so\.62|libavfilter\.so\.11|libavformat\.so\.62|libavutil\.so\.60|libswresample\.so\.6|libswscale\.so\.9|liboptional-system-libraries\.so)$'
failed=0
for file in $OUT/*; do
    for needed in $(readelf -d $file | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'); do
        if ! [[ $needed =~ $allowed ]]; then echo "unexpected dependency: $(basename $file) -> $needed"; failed=1; fi
    done
    newest=$(readelf -V $file | grep -oE 'GLIBC_[0-9]+\.[0-9]+(\.[0-9]+)?' | sed 's/GLIBC_//' | sort -V | tail -1)
    if [[ -n $newest ]] && [[ $(printf '%s\n2.28\n' "$newest" | sort -V | tail -1) != 2.28 ]]; then echo "glibc too new: $(basename $file) needs $newest"; failed=1; fi
done
exit $failed
