# syntax=docker/dockerfile:1.10

FROM ubuntu:24.04 AS builder
WORKDIR /sandbox
RUN apt-get update && apt-get install cmake tree -y

RUN apt-get update && apt-get install -y --no-install-recommends \
    # Init / quality-of-life
    tini less coreutils tree curl zip unzip tar  \
    # Networking / server debugging
    curl libcurl4-openssl-dev wget jq openssh-client libssl-dev \
    # Build toolchain (general)
    g++ binutils \
    clang clangd git\
    ninja-build pkg-config ccache \
    autoconf automake libtool m4 \
    # Dev headers / misc
    uuid-dev \
    protobuf-compiler \
    libprotobuf-dev libuv1-dev \
    # DinD
    docker.io iptables uidmap \
    # Needed for Kitware CMake install
    ca-certificates gnupg lsb-release \
    && rm -rf /var/lib/apt/lists/*

# Install latest CMake from Kitware (suitable for vcpkg)
RUN wget -qO - https://apt.kitware.com/keys/kitware-archive-latest.asc \
        | gpg --dearmor -o /usr/share/keyrings/kitware-archive-keyring.gpg \
    && echo "deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ $(lsb_release -cs) main" \
        > /etc/apt/sources.list.d/kitware.list \
    && apt-get update \
    && apt-get install -y --no-install-recommends cmake \
    && rm -rf /var/lib/apt/lists/*

COPY --from=atlasnetsdk ./vcpkg.json ./vcpkg.json

ENV VCPKG_ROOT=/opt/vcpkg/
# Create the directory
RUN mkdir -p $VCPKG_ROOT
RUN curl -L https://github.com/microsoft/vcpkg/archive/refs/tags/2026.01.16.tar.gz \
| tar -xz --strip-components=1 -C ${VCPKG_ROOT}
RUN  /opt/vcpkg/bootstrap-vcpkg.sh 
RUN apt-get update && apt-get install binutils build-essential -y
RUN ${VCPKG_ROOT}/vcpkg install \
       --x-install-root=/sandbox/build/vcpkg_installed


COPY --from=atlasnetsdk . sandbox/.AtlasNet

COPY ./CMakeLists.txt /sandbox/CMakeLists.txt
COPY ./client /sandbox/client
COPY ./lib /sandbox/lib
COPY ./server /sandbox/server
#
RUN  cmake -S . -B build \
-DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_INSTALLED_DIR=/sandbox/build/vcpkg_installed 
  
RUN  cmake --build build --parallel --target SandboxServer

${GAME_SERVER_ENTRYPOINT}