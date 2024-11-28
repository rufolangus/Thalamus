FetchContent_Declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG        v1.66.1
)
set(gRPC_BUILD_TESTS OFF CACHE BOOL "Disable gRPC tests")
set(gRPC_INSTALL OFF CACHE BOOL "Disable gRPC installation")
set(gRPC_BUILD_CSHARP_EXT OFF CACHE BOOL "Disable C# extension")
set(protobuf_BUILD_PROTOC_BINARIES OFF CACHE BOOL "Disable building protoc")
set(protobuf_BUILD_LIBPROTOC OFF CACHE BOOL "Disable building libprotoc")
set(BUILD_SHARED_LIBS OFF)
set(BUILD_TESTING OFF)
set(FETCHCONTENT_QUIET OFF)
set(gRPC_MSVC_STATIC_RUNTIME ON)
set(protobuf_MSVC_STATIC_RUNTIME ON CACHE BOOL "Protobuf: Link static runtime libraries")
set(ABSL_ENABLE_INSTALL ON)
#add_definitions(-DBORINGSSL_NO_CXX)
#set(gRPC_BUILD_TESTS ON)

if(ANDROID)
  # Build host versions of protoc and grpc_cpp_plugin
  set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "Build host protoc when cross-compiling")
  set(protobuf_BUILD_LIBPROTOC ON CACHE BOOL "Build host libprotoc when cross-compiling")
  set(gRPC_BUILD_GRPC_CPP_PLUGIN ON CACHE BOOL "Build host grpc_cpp_plugin when cross-compiling")

  # Set variables to use host tools
  set(PROTOBUF_PROTOC_EXECUTABLE ${protobuf_BINARY_DIR}/protoc CACHE FILEPATH "Path to the host protoc executable")
  set(GRPC_CPP_PLUGIN_EXECUTABLE ${grpc_BINARY_DIR}/grpc_cpp_plugin CACHE FILEPATH "Path to the host grpc_cpp_plugin executable")

  # Use NDK's zlib
  set(gRPC_ZLIB_PROVIDER "package" CACHE STRING "Use system zlib")
  set(ZLIB_ROOT "${CMAKE_ANDROID_NDK}/sources/third_party/zlib" CACHE STRING "Path to NDK's zlib")
  set(ZLIB_INCLUDE_DIR "${ZLIB_ROOT}/include" CACHE STRING "Zlib include directory")
  set(ZLIB_LIBRARY "${ZLIB_ROOT}/libs/${CMAKE_ANDROID_ARCH_ABI}/libz.a" CACHE STRING "Zlib library")

  # Ensure ZLIB is found
  find_package(ZLIB REQUIRED)
  add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
  set_target_properties(ZLIB::ZLIB PROPERTIES
    IMPORTED_LOCATION "${ZLIB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
  )
endif()

FetchContent_MakeAvailable(gRPC)
#file(READ "${grpc_SOURCE_DIR}/third_party/zlib/CMakeLists.txt" FILE_CONTENTS)
#string(REPLACE "cmake_minimum_required(VERSION 2.4.4)" "cmake_minimum_required(VERSION 3.12)" FILE_CONTENTS "${FILE_CONTENTS}")
#file(WRITE "${grpc_SOURCE_DIR}/third_party/zlib/CMakeLists.txt" "${FILE_CONTENTS}")
#add_subdirectory("${grpc_SOURCE_DIR}" "${grpc_BINARY_DIR}")

#target_compile_options(crypto PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:-w>")

macro(apply_protoc_grpc OUTPUT_SOURCES)
  foreach(PROTO_FILE ${ARGN})
    get_filename_component(PROTO_ABSOLUTE "${PROTO_FILE}" ABSOLUTE)
    get_filename_component(PROTO_NAME "${PROTO_FILE}" NAME_WE)
    get_filename_component(PROTO_DIRECTORY "${PROTO_ABSOLUTE}" DIRECTORY)
    set(apply_protoc_grpc_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h"
                                    "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc"
                                    "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.h"
                                    "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.grpc.pb.cc")
    add_custom_command(
      OUTPUT ${apply_protoc_grpc_GENERATED}
      COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
           --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
           --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
           -I "${PROTO_DIRECTORY}"
           ${PROTO_ABSOLUTE}
      DEPENDS "${PROTO_ABSOLUTE}"
    )
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
      set_source_files_properties(
        ${${OUTPUT_SOURCES}}
        PROPERTIES
        COMPILE_FLAGS /wd4267)
    endif()
    list(APPEND ${OUTPUT_SOURCES} ${apply_protoc_grpc_GENERATED})
  endforeach()
endmacro()


macro(apply_protoc OUTPUT_SOURCES)
  foreach(PROTO_FILE ${ARGN})
    get_filename_component(PROTO_ABSOLUTE "${PROTO_FILE}" ABSOLUTE)
    get_filename_component(PROTO_NAME "${PROTO_FILE}" NAME_WE)
    get_filename_component(PROTO_DIRECTORY "${PROTO_ABSOLUTE}" DIRECTORY)
    set(apply_protoc_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h"
                               "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
    add_custom_command(
      OUTPUT ${apply_protoc_GENERATED}
      COMMAND ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
           -I "${PROTO_DIRECTORY}"
           ${PROTO_ABSOLUTE}
      DEPENDS "${PROTO_ABSOLUTE}"
    )
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
      set_source_files_properties(
        ${${OUTPUT_SOURCES}}
        PROPERTIES
        COMPILE_FLAGS /wd4267)
    endif()
    list(APPEND ${OUTPUT_SOURCES} ${apply_protoc_GENERATED})
  endforeach()
endmacro()

