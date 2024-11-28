#!/bin/bash

# Ensure the Android NDK path is set
NDK_PATH="$ANDROID_NDK_HOME"

# Android build configurations``
ANDROID_ABI="arm64-v8a"
ANDROID_PLATFORM="android-21"
BUILD_TYPE="Release"
CMAKE_GENERATOR="Ninja"


# Project directories
PROJECT_ROOT="$(pwd)"
BUILD_DIR="${PROJECT_ROOT}/build/android/${ANDROID_ABI}/${BUILD_TYPE}"
OUTPUT_LIB_DIR="${PROJECT_ROOT}/android_libs/${ANDROID_ABI}"
CMAKE_TOOLCHAIN_FILE="${NDK_PATH}/build/cmake/android.toolchain.cmake"

# Ensure output directories exist
mkdir -p "${BUILD_DIR}"
mkdir -p "${OUTPUT_LIB_DIR}"

echo "Building the main project..."
# Find the paths to protoc and grpc_cpp_plugin
PROTOC_PATH=$(which protoc)
GRPC_CPP_PLUGIN_PATH=$(which grpc_cpp_plugin)
GRPC_NODE_PLUGIN_PATH=$(which grpc_node_plugin)
GRPC_CSHARP_PLUGIN_PATH=$(which grpc_csharp_plugin)
GRPC_OBJECTIVE_C_PLUGIN_PATH=$(which grpc_objective_c_plugin)
GRPC_PHP_PLUGIN_PATH=$(which grpc_php_plugin)

# Verify that they are found
if [ -z "$PROTOC_PATH" ] || [ -z "$GRPC_CPP_PLUGIN_PATH" ]; then
  echo "Error: protoc or grpc_cpp_plugin not found in PATH."
  exit 1
fi

# Pass these paths to CMake
cmake -S "${PROJECT_ROOT}" \
      -B "${BUILD_DIR}" \
      -G "${CMAKE_GENERATOR}" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE}" \
      -DANDROID_ABI="${ANDROID_ABI}" \
      -DANDROID_PLATFORM="${ANDROID_PLATFORM}" \
      -DCMAKE_SYSTEM_NAME=Android \
      -DPROTOBUF_PROTOC_EXECUTABLE="${PROTOC_PATH}" \
      -DGRPC_CPP_PLUGIN_EXECUTABLE="${GRPC_CPP_PLUGIN_PATH}" \
      -DGRPC_NODE_PLUGIN_EXECUTABLE="${GRPC_NODE_PLUGIN_PATH}" \
      -DGRPC_CSHARP_PLUGIN_EXECUTABLE="${GRPC_CSHARP_PLUGIN_PATH}" \
      -DGRPC_OBJECTIVE_C_PLUGIN_EXECUTABLE="${GRPC_OBJECTIVE_C_PLUGIN_PATH}" \
      -DGRPC_PHP_PLUGIN_EXECUTABLE="${GRPC_PHP_PLUGIN_PATH}" \
      -DgRPC_INSTALL=OFF \
      -DgRPC_BUILD_TESTS=OFF \
      -DgRPC_BUILD_CODEGEN=OFF \
      -DZLIB_INCLUDE_DIR="${NDK_PATH}/sources/third_party/zlib/include" \
      -DZLIB_LIBRARY="${NDK_PATH}/sources/third_party/zlib/libs/${ANDROID_ABI}/libz.a" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      -DCMAKE_MODULE_PATH="${PROJECT_ROOT}/cmake" \
      -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--exclude-libs,ALL"

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --parallel "4"

# Step 4: Copy .so libraries to the output directory
echo "Copying .so files to output directory..."
find "${BUILD_DIR}" -name "*.so" -exec cp {} "${OUTPUT_LIB_DIR}" \;
echo "Build completed. Libraries are located in ${OUTPUT_LIB_DIR}"
