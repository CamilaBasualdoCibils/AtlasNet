
FROM ubuntu:24.04 AS builder
WORKDIR /sandbox
RUN apt-get update && apt-get install cmake tree -y

COPY ./.AtlasNet/cmake/AtlasNetSetup.sh /sandbox/.AtlasNet/cmake/AtlasNetSetup.sh
RUN chmod +x ./.AtlasNet/cmake/AtlasNetSetup.sh
RUN cd ./.AtlasNet && ./cmake/AtlasNetSetup.sh

COPY ./.AtlasNet/. /sandbox/.AtlasNet/.
COPY ./client /sandbox/client
COPY ./lib /sandbox/lib
COPY ./server /sandbox/server
COPY ./CMakeLists.txt /sandbox/CMakeLists.txt

RUN cmake -S . -B build
RUN cmake --build build --parallel --target SandboxServer
${GAME_SERVER_ENTRYPOINT}