FROM ubuntu:24.04

RUN apt update && apt install -y bash git build-essential libssl-dev libuv1-dev libjsoncpp-dev clang clangd cmake uuid-dev doxygen gdb
# Install X11 development libraries
RUN apt install -y libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev mesa-utils
# Install Python development libraries
RUN apt install -y  python3-venv 
# Install Drogon dependencies
RUN apt install -y zlib1g-dev

# Install OpenGL
RUN apt install -y libgl1-mesa-dev \
    mesa-utils \
    mesa-utils-extra \
    libglu1-mesa-dev \
    libegl1-mesa-dev \
    libglx-mesa0 \
    mesa-vulkan-drivers 
ENV LIBGL_ALWAYS_SOFTWARE=1

# ETCD deps
RUN apt-get install -y libboost-all-dev libssl-dev libgrpc-dev libgrpc++-dev libprotobuf-dev protobuf-compiler-grpc libcpprest-dev

RUN apt-get install -y autoconf autoconf-archive automake libtool

#ARG USERNAME=devz
#ARG USER_UID=1000
#ARG USER_GID=$USER_UID
#
#RUN groupadd --gid $USER_GID $USERNAME \
#    && useradd \
#        --uid $USER_UID \
#        --gid $USER_GID \
#        --create-home \
#        --shell /bin/bash \
#        $USERNAME \
#    && apt install -y sudo \
#    && echo "$USERNAME ALL=(root) NOPASSWD:ALL" \
#        > /etc/sudoers.d/$USERNAME \
#    && chmod 0440 /etc/sudoers.d/$USERNAME
#
#USER $USERNAME

# ********************************************************
# * Anything else you want to do like clean up goes here *
# ********************************************************

# [Optional] Set the default user. Omit if you want to keep the default as root.
USER ubuntu
CMD ["/bin/bash"]