# Minimal AtlasNet runtime stage
FROM archlinux:latest AS runner

WORKDIR /atlasnet

RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm \
        bash \
        glibc \
        gcc-libs \
        zlib \
        openssl \
        c-ares \
        jsoncpp \
        util-linux-libs \
        brotli \
        sqlite \
        zstd \
        binutils \
        gdb && \
    pacman -Scc --noconfirm

FROM scratch AS atlasnetstage
COPY ./dev/.stage /atlasnet

FROM scratch AS atlasnet-src
COPY ./src /atlasnet/src
COPY ./include /atlasnet/include


FROM runner AS atlasnet-node-dev
COPY --from=atlasnetstage /atlasnet/AtlasNetNode /atlasnet/AtlasNetNode
ARG PRELAUNCH_CMD="{EXE}"
ENV PRELAUNCH_CMD="${PRELAUNCH_CMD}"
RUN chmod +x /atlasnet/AtlasNetNode
ENTRYPOINT ["bash", "-c", "EXE=/atlasnet/AtlasNetNode; CMD=$(printf '%s' \"$PRELAUNCH_CMD\" | sed \"s|{EXE}|$EXE|g\"); exec bash -c \"$CMD\""]

FROM archlinux:latest AS valkey-builder

RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm \
        base-devel \
        cmake \
        git \
        pkgconf \
        openssl && \
    pacman -Scc --noconfirm

WORKDIR /src

COPY ./external/valkey /src/valkey
WORKDIR /src/valkey

RUN make -j$(nproc)
FROM archlinux:latest as atlasnet-db-dev

COPY --from=valkey-builder /src/valkey/src/valkey-server /usr/local/bin/
RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm \
        glibc \
        gcc-libs \
        zlib \
        openssl \
        c-ares \
        util-linux-libs \
        brotli \
        sqlite \
        zstd \
        libatomic \
        binutils && \
    pacman -Scc --noconfirm
COPY --from=atlasnetstage \
    /atlasnet/libAtlasNetDB.so \
    /atlasnet/libAtlasNetDB.so

CMD ["valkey-server","--loadmodule","/atlasnet/libAtlasNetDB.so"]


FROM nginx:alpine AS atlasnet-cartograph-dev

#COPY --from=atlasnet-src /atlasnet/src/Cartograph/Frontend /atlasnet/AtlasNetCartographFrontend
#
#WORKDIR /atlasnet/AtlasNetCartographFrontend
#
#RUN apk add --no-cache nodejs npm gettext
#
#RUN npm install && npm run build
#
#COPY --from=atlasnet-src /atlasnet/src/Cartograph/nginx/nginx.conf.template /etc/nginx/nginx.conf.template
#COPY --from=atlasnet-src /atlasnet/src/Cartograph/nginx/entrypoint.sh /entrypoint.sh
#
#RUN chmod +x /entrypoint.sh
#
#ENTRYPOINT ["/entrypoint.sh"]
