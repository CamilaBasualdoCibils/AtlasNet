
    FROM ubuntu:24.04 AS builder

    WORKDIR /AtlasNet
        
    RUN apt-get update && apt-get install git tini gdbserver redis-server build-essential binutils automake libtool m4 autoconf gdb curl zip less unzip ccache coreutils tar g++ cmake pkg-config uuid-dev libxmu-dev libxi-dev libgl-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev  -y 

    RUN git clone https://github.com/microsoft/vcpkg.git
    RUN ./vcpkg/bootstrap-vcpkg.sh
    ENV VCPKG_FEATURE_FLAGS=manifests
    COPY vcpkg.json /AtlasNet
    RUN --mount=type=cache,target=/root/.cache/vcpkg \
        arch=$(uname -m) && \
    case "$arch" in \
      x86_64) triplet=x64-linux ;; \
      aarch64) triplet=arm64-linux ;; \
      *) echo "Unsupported arch $arch" && exit 1 ;; \
    esac && \
    echo "Using vcpkg triplet: $triplet" && \
    ./vcpkg/vcpkg install --triplet=$triplet

    
        
RUN git clone https://github.com/premake/premake-core.git
WORKDIR /AtlasNet/premake-core
RUN make -f Bootstrap.mak linux
WORKDIR /AtlasNet
COPY docker/devcontainer.json .devcontainer/devcontainer.json
# COPY docker/UnitTestsClient/launch.json .vscode/launch.json
# COPY premake5 /AtlasNet
COPY premake5.lua /AtlasNet

COPY Tests /AtlasNet/Tests
COPY src  /AtlasNet/src
COPY srcRun /AtlasNet/srcRun
RUN  ./premake-core/bin/release/premake5 gmake
 RUN \
   --mount=type=cache,target=/AtlasNet/obj \
     make -C ./build config=debugdocker UnitTestsClient -j $(nproc)

    FROM ubuntu:24.04
    WORKDIR /AtlasNet
    RUN apt update && apt install redis-server tini -y
    COPY --from=builder /AtlasNet/bin ./bin
    COPY --from=builder /AtlasNet/vcpkg_installed/x64-linux/lib/*.so /usr/local/lib/
    ENV LD_LIBRARY_PATH=/usr/local/lib
    

    ENTRYPOINT ["bin/DebugDocker/UnitTestsClient/UnitTestsClient"]