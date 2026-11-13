ARG MANYLINUX_ARCH=x86_64
FROM quay.io/pypa/manylinux_2_28_${MANYLINUX_ARCH} AS build
RUN dnf install -y perl-IPC-Cmd perl-Time-Piece perl-Data-Dumper patch xz bzip2 diffutils which clang llvm rsync \
    && /opt/python/cp312-cp312/bin/python -m pip install --no-cache-dir meson==1.6.1 ninja==1.11.1.3 \
    && ln -s /opt/python/cp312-cp312/bin/meson /usr/local/bin/meson \
    && ln -s /opt/python/cp312-cp312/bin/ninja /usr/local/bin/ninja
COPY --from=source . /src
COPY optional_system_libraries.c /loader/optional_system_libraries.c
COPY build.sh /build.sh
ARG MAKEJ=4
RUN MAKEJ=${MAKEJ} bash /build.sh

FROM scratch
COPY --from=build /out/ /
