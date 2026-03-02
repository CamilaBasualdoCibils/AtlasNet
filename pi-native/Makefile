#
# Native arm64 (Pi) build helper for AtlasNet
#
# Usage on the Pi:
#   cd pi-native
#   make deps           # (optional) install build deps via apt
#   make vcpkg          # clone + bootstrap vcpkg into /opt/vcpkg
#   make configure      # configure CMake for arm64 with vcpkg
#   make build-core     # build core AtlasNet runtimes
#   make build-docker   # (optional) build local Docker images via CopyDockerfile
#

.PHONY: deps vcpkg configure build-core build-docker all

# Adjust if you prefer a different install location on the Pi.
VCPKG_ROOT ?= /opt/vcpkg

# AtlasNet repo root is the parent of this directory.
ATLASNET_ROOT := $(abspath ..)

# Separate build dir for Pi native builds (keeps devcontainer build tree separate).
BUILD_DIR ?= $(ATLASNET_ROOT)/build-pi

all: build-core

deps:
	@echo "==> Installing system build dependencies (requires sudo)..."
	sudo apt-get update
	sudo apt-get install -y \
	  build-essential clang clangd \
	  cmake ninja-build pkg-config \
	  git curl wget unzip zip \
	  python3 python3-pip \
	  uuid-dev libuv1-dev libssl-dev zlib1g-dev \
	  protobuf-compiler libprotobuf-dev \
	  swig nodejs npm
	@echo "==> System dependencies installed (or already present)."

vcpkg:
	@echo "==> Ensuring vcpkg exists at $(VCPKG_ROOT)..."
	if [ ! -d "$(VCPKG_ROOT)" ]; then \
	  sudo mkdir -p "$(VCPKG_ROOT)"; \
	  sudo chown "$$USER":"$$USER" "$(VCPKG_ROOT)"; \
	fi
	if [ ! -d "$(VCPKG_ROOT)/.git" ]; then \
	  git clone https://github.com/microsoft/vcpkg.git "$(VCPKG_ROOT)"; \
	fi
	cd "$(VCPKG_ROOT)" && ./bootstrap-vcpkg.sh
	@echo "==> vcpkg bootstrapped at $(VCPKG_ROOT)."

configure: vcpkg
	@echo "==> Configuring CMake for native arm64 build in $(BUILD_DIR)..."
	mkdir -p "$(BUILD_DIR)"
	cd "$(BUILD_DIR)" && cmake \
	  -G Ninja \
	  -DCMAKE_TOOLCHAIN_FILE="$(VCPKG_ROOT)/scripts/buildsystems/vcpkg.cmake" \
	  -DVCPKG_MANIFEST_DIR="$(ATLASNET_ROOT)/AtlasNet" \
	  -DVCPKG_TARGET_TRIPLET=arm64-linux \
	  -DSANDBOX_ADD_CLIENT=ON \
	  -DCMAKE_C_COMPILER=clang \
	  -DCMAKE_CXX_COMPILER=clang++ \
	  "$(ATLASNET_ROOT)"
	@echo "==> CMake configure completed for arm64."

build-core: configure
	@echo "==> Building core AtlasNet runtimes for arm64 (this may take a while on a Pi 5)..."
	cmake --build "$(BUILD_DIR)" --target watchdog proxy shard cartograph -- -j2
	@echo "==> Core AtlasNet targets built."

build-docker: build-core
	@echo "==> Building local Docker images (arm64) via CopyDockerfile..."
	cd "$(ATLASNET_ROOT)/AtlasNet" && ./docker/BuildDockerImages.sh -f ./docker/dockerfiles/docker-bake.copy.json
	@echo "==> Docker images built using CopyDockerfile (arm64 local images: watchdog, proxy, shard, cartograph)."

