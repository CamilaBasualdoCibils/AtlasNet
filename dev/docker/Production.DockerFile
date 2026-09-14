FROM debian:stable-slim AS builder
ENV DEBIAN_FRONTEND=noninteractive

# Install core build tools and dependencies required by vcpkg
RUN apt-get update && apt-get install -y --no-install-recommends \
build-essential \
cmake \
git \
pkg-config \
clang \
libssl-dev \
libuv1-dev \
uuid-dev \
zlib1g-dev \
libjsoncpp-dev \
ca-certificates \
curl \
zip \
unzip \
tar \
autoconf \
autoconf-archive \
automake \
libtool 

#GLFW dependencies
RUN apt-get install -y --no-install-recommends \
    libxinerama-dev \
    libxcursor-dev \
    xorg-dev \
    libglu1-mesa-dev \
    pkg-config
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg
WORKDIR /opt/vcpkg
RUN ./bootstrap-vcpkg.sh

# Set environment variables so system tools can find vcpkg
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"
# Copy only src/include/cmake directories
WORKDIR /atlasnet
COPY vcpkg-triplets/ vcpkg-triplets/
COPY vcpkg.json vcpkg.json

COPY CMakePresets.json CMakePresets.json
COPY CMakeLists.txt CMakeLists.txt
RUN cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake || true

COPY cmake/ cmake/
COPY dev/ dev/
COPY include/ include/
COPY src/ src/
RUN cmake --preset linux-vcpkg -DATLASNET_EXAMPLES=OFF -DATLASNET_TESTS=OFF -DATLASNET_CARTOGRAPH=ON
RUN cmake --build --preset linux-vcpkg --target AtlasNetNode AtlasNetDB

FROM debian:stable-slim AS runner
WORKDIR /atlasnet


FROM runner AS atlasnet-node
COPY --from=builder /atlasnet/build/AtlasNetNode /atlasnet/
ENTRYPOINT ["/atlasnet/AtlasNetNode"]

FROM valkey/valkey-bundle AS atlasnet-db
RUN apt-get update && \
    apt-get install -y \
    libc6 \
    libgcc-s1 \
    zlib1g \
    libssl3 \
    libc-ares2 \
    libuuid1 \
    libbrotli1 \
    libsqlite3-0 \
    libzstd1 \
    libatomic1 \
    binutils && \
    rm -rf /var/lib/apt/lists/*
COPY --from=builder /atlasnet/build/libAtlasNetDB.so /atlasnet/libAtlasNetDB.so
CMD ["valkey-server", "--loadmodule", "/atlasnet/libAtlasNetDB.so"]
FROM runner AS atlasnet-cartograph
#COPY --from=builder /atlasnet/build/AtlasNetCartographBackend /atlasnet/
#COPY web/ /atlasnet/frontend/
#WORKDIR /atlasnet/frontend
#RUN npm install
#RUN npm run build
#ENTRYPOINT ["/atlasnet/AtlasNetCartographBackend"]

