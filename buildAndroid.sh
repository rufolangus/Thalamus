#!/bin/bash

NDK_PATH="/Users/blurryrobot/Library/Android/sdk/ndk/27.2.12479018"  # Replace with your NDK path

ANDROID_ABI="arm64-v8a" 

ANDROID_PLATFORM="android-21"

BUILD_TYPE="Release"

CMAKE_GENERATOR="Ninja"

PROJECT_ROOT="$(pwd)"

BUILD_DIR="${PROJECT_ROOT}/build/android/${ANDROID_ABI}/${BUILD_TYPE}"

mkdir -p "${BUILD_DIR}"

CMAKE_TOOLCHAIN_FILE="${NDK_PATH}/build/cmake/android.toolchain.cmake"

cmake -S "${PROJECT_ROOT}" \
      -B "${BUILD_DIR}" \
      -G "${CMAKE_GENERATOR}" \
      -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
      -DCMAKE_TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE}" \
      -DANDROID_ABI="${ANDROID_ABI}" \
      -DANDROID_PLATFORM="${ANDROID_PLATFORM}" \
      -DCMAKE_SYSTEM_NAME=Android \
      -DENABLE_SWIG=OFF \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" --parallel "4"

OUTPUT_LIB_DIR="${PROJECT_ROOT}/android_libs/${ANDROID_ABI}"
mkdir -p "${OUTPUT_LIB_DIR}"
find "${BUILD_DIR}" -name "*.so" -exec cp {} "${OUTPUT_LIB_DIR}" \;

echo "Build completed. Libraries are located in ${OUTPUT_LIB_DIR}"
