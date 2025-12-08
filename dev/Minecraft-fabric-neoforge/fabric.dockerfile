
FROM ubuntu:24.04
RUN apt update && apt install tree openjdk-21-jdk -y
#WORKDIR minecraft
#COPY src minecraft/src
#WORKDIR minecraft/src
#RUN tree  && ./gradlew --info build
FROM itzg/minecraft-server:stable AS mc
COPY src/fabric/build/libs/atlasnet-fabric-1.0.0.jar /mods

ENV EULA=TRUE \
    TYPE=FABRIC \
    VERSION=1.21\
    MAX_MEMORY=512M\
    MEMORY=512M\
    INIT_MEMORY=12M


${GAME_SERVER_ENTRYPOINT}s