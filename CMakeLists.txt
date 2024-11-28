cmake_minimum_required(VERSION 3.16)
  cmake_policy(SET CMP0042 NEW)
  cmake_policy(SET CMP0071 NEW)
  cmake_policy(SET CMP0077 NEW)
  cmake_policy(SET CMP0091 NEW)
  if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
  endif()
  set(CMAKE_POLICY_DEFAULT_CMP0042 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
  set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
  # Set this before any project() command
if(ANDROID)
set(CMAKE_SYSTEM_NAME Android)
set(CMAKE_SYSTEM_VERSION ${ANDROID_PLATFORM})
set(CMAKE_ANDROID_ARCH_ABI ${ANDROID_ABI})
set(CMAKE_ANDROID_NDK ${ANDROID_NDK_HOME})
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_ANDROID_NDK}/build/cmake/android.toolchain.cmake)
# Exclude grpc_node_plugin, grpc_csharp_plugin, grpc_objective_c_plugin, and grpc_php_plugin for Android builds
set(EXCLUDE_PLUGINS "grpc_node_plugin;grpc_csharp_plugin;grpc_objective_c_plugin;grpc_php_plugin")
# Remove or comment out this line to avoid the linker error:
# set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--exclude-libs,ALL -Wl,--version-script=${CMAKE_SOURCE_DIR}/android_exclude_gz_intmax.map")
# Use NDK's zlib
set(ZLIB_ROOT "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr")
set(ZLIB_INCLUDE_DIR "${ZLIB_ROOT}/include")
set(ZLIB_LIBRARY "${ZLIB_ROOT}/lib/${CMAKE_ANDROID_ARCH_ABI}/libz.so")
# Disable building zlib from grpc third-party dependencies
set(gRPC_ZLIB_PROVIDER "package" CACHE STRING "Use system zlib")
set(ZLIB_USE_EXTERNAL ON CACHE BOOL "Use external zlib")
find_package(ZLIB REQUIRED)
if(TARGET ZLIB::ZLIB)
  remove_library(ZLIB::ZLIB)
endif()
add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
set_target_properties(ZLIB::ZLIB PROPERTIES
  IMPORTED_LOCATION "${ZLIB_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
)
endif()


  project(thalamus)

  message("CMAKE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR}")

  # Specify paths to host tools
# Remove the host protoc and plugin discovery
# find_program(HOST_PROTOC protoc)
# if(NOT HOST_PROTOC)
#   message(FATAL_ERROR "Could not find host 'protoc' executable")
# endif()
# ...similar for HOST_GRPC_CPP_PLUGIN...

# Use protoc and plugins from the grpc submodule
set(PROTOBUF_PROTOC_EXECUTABLE ${grpc_SOURCE_DIR}/third_party/protobuf/src/protoc)
set(GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

# Ensure that protobuf includes and libraries are correctly set
include_directories(${grpc_SOURCE_DIR}/third_party/protobuf/src)

find_program(HOST_GRPC_NODE_PLUGIN grpc_node_plugin)
find_program(HOST_GRPC_CSHARP_PLUGIN grpc_csharp_plugin)
find_program(HOST_GRPC_OBJECTIVE_C_PLUGIN grpc_objective_c_plugin)
find_program(HOST_GRPC_PHP_PLUGIN grpc_php_plugin)

if(NOT ANDROID AND NOT HOST_GRPC_NODE_PLUGIN)
  message(FATAL_ERROR "Could not find host 'grpc_node_plugin' executable")
endif()

if(NOT ANDROID AND NOT HOST_GRPC_CSHARP_PLUGIN)
  message(FATAL_ERROR "Could not find host 'grpc_csharp_plugin' executable")
endif()

if(NOT ANDROID AND NOT HOST_GRPC_OBJECTIVE_C_PLUGIN)
  message(FATAL_ERROR "Could not find host 'grpc_objective_c_plugin' executable")
endif()

if(NOT ANDROID AND NOT HOST_GRPC_PHP_PLUGIN)
  message(FATAL_ERROR "Could not find host 'grpc_php_plugin' executable")
endif()



# Set variables to be used in grpc.cmake
set(PROTOBUF_PROTOC_EXECUTABLE ${HOST_PROTOC} CACHE FILEPATH "Path to the host protoc executable")
set(GRPC_CPP_PLUGIN_EXECUTABLE ${HOST_GRPC_CPP_PLUGIN} CACHE FILEPATH "Path to the host grpc_cpp_plugin executable")
set(GRPC_NODE_PLUGIN_EXECUTABLE ${HOST_GRPC_NODE_PLUGIN} CACHE FILEPATH "Path to the host grpc_node_plugin executable")
set(GRPC_CSHARP_PLUGIN_EXECUTABLE ${HOST_GRPC_CSHARP_PLUGIN} CACHE FILEPATH "Path to the host grpc_csharp_plugin executable")
set(GRPC_OBJECTIVE_C_PLUGIN_EXECUTABLE ${HOST_GRPC_OBJECTIVE_C_PLUGIN} CACHE FILEPATH "Path to the host grpc_objective_c_plugin executable")
set(GRPC_PHP_PLUGIN_EXECUTABLE ${HOST_GRPC_PHP_PLUGIN} CACHE FILEPATH "Path to the host grpc_php_plugin executable")
set(gRPC_BUILD_GRPC_CPP_PLUGIN OFF CACHE BOOL "Disable building gRPC C++ plugin")
set(gRPC_BUILD_GRPC_CSHARP_PLUGIN OFF CACHE BOOL "Disable building gRPC C# plugin")
set(gRPC_BUILD_GRPC_NODE_PLUGIN OFF CACHE BOOL "Disable building gRPC Node plugin")
set(gRPC_BUILD_GRPC_OBJECTIVE_C_PLUGIN OFF CACHE BOOL "Disable building gRPC Objective-C plugin")
set(gRPC_BUILD_GRPC_PHP_PLUGIN OFF CACHE BOOL "Disable building gRPC PHP plugin")
set(gRPC_BUILD_GRPC_PYTHON_PLUGIN OFF CACHE BOOL "Disable building gRPC Python plugin")
set(gRPC_BUILD_GRPC_RUBY_PLUGIN OFF CACHE BOOL "Disable building gRPC Ruby plugin")
set(EXCLUDE_GRPC_PLUGIN_BUILD TRUE CACHE BOOL "Exclude gRPC plugin build")

  if(WIN32)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
      execute_process(COMMAND "${CMAKE_CXX_COMPILER}" "${CMAKE_SOURCE_DIR}/stub_atomic.cpp" -c -o "${CMAKE_BINARY_DIR}/atomic.lib")
    endif()
  endif()

  if(ANDROID)
    message("Building for Android")
    set(CMAKE_SYSTEM_NAME Android)
    set(CMAKE_ANDROID_NDK "${ANDROID_NDK_PATH}")
    set(CMAKE_ANDROID_ARCH_ABI "${ANDROID_ABI}")
    set(CMAKE_ANDROID_STL_TYPE c++_shared)
    set(CMAKE_ANDROID_API "${ANDROID_PLATFORM}")
endif()

  if(APPLE)
    set(OSX_TARGET_PARAMETER -mmacosx-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET})
    list(APPEND ALL_COMPILE_OPTIONS ${OSX_TARGET_PARAMETER})
  endif()

  # Default to C99
  if(NOT CMAKE_C_STANDARD) 
    set(CMAKE_C_STANDARD 99)
  endif()

  # Default to C++17
  set(CMAKE_CXX_STANDARD 20)

  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

  set(ABSL_PROPAGATE_CXX_STD ON)

  include(FetchContent)

  include(ProcessorCount)
  ProcessorCount(CPU_COUNT)
  if(CPU_COUNT EQUAL 0)
    set(MAKE_J)
    set(CMAKE_PARALLEL)
  else()
    if(WIN32)
      set(MAKE_J /J ${CPU_COUNT})
    else()
      set(MAKE_J -j ${CPU_COUNT})
    endif()
    set(CMAKE_PARALLEL --parallel ${CPU_COUNT})
  endif()

  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    list(APPEND ALL_COMPILE_OPTIONS /Zi /MP)
    list(APPEND ALL_LINK_OPTIONS /DEBUG)
  else()
    if(NOT DEFINED ENV{GITHUB_REPOSITORY})
      message("Release debug symbols enabled")
      list(APPEND ALL_COMPILE_OPTIONS -g)
    else()
      message("Release debug symbols disabled")
    endif()
  endif()
    
  if("${SANITIZER}" STREQUAL thread)
    list(APPEND ALL_COMPILE_OPTIONS -fsanitize=thread -O1)
    list(APPEND ALL_LINK_OPTIONS -fsanitize=thread -O1)
  elseif("${SANITIZER}" STREQUAL address)
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
      list(APPEND ALL_COMPILE_OPTIONS -fsanitize=address)
    else()
      list(APPEND ALL_COMPILE_OPTIONS -fsanitize=address -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls)
      list(APPEND ALL_LINK_OPTIONS -fsanitize=address -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls)
    endif()
  elseif("${SANITIZER}" STREQUAL memory)
    list(APPEND ALL_COMPILE_OPTIONS -fsanitize=memory -fsanitize-memory-track-origins=2 -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls)
    list(APPEND ALL_LINK_OPTIONS -fsanitize=memory -fsanitize-memory-track-origins=2 -O1 -fno-omit-frame-pointer -fno-optimize-sibling-calls)
  endif()

  string(REPLACE ";" " " ALL_C_COMPILE_OPTIONS_SPACED "${ALL_COMPILE_OPTIONS}")
  string(REPLACE ";" " " ALL_C_LINK_OPTIONS_SPACED "${ALL_LINK_OPTIONS}")

  if(NOT WIN32 AND NOT APPLE AND NOT ANDROID)
    include(cmake/comedi.cmake)
  endif()
  include(cmake/sdl.cmake)
  include(cmake/ffmpeg.cmake)
  include(cmake/glib.cmake)
  include(cmake/cairo.cmake)
  if(NOT ANDROID)
  include(cmake/luajit.cmake)
endif()
  
  if(NOT WIN32 AND NOT APPLE)
    if(NOT ANDROID)
      include(cmake/llvm.cmake)
    endif()
    list(APPEND ALL_COMPILE_OPTIONS ${LIBCXX_COMPILE_OPTIONS})
    list(APPEND ALL_LINK_OPTIONS ${LIBCXX_LINK_OPTIONS})
  endif()

  add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${ALL_COMPILE_OPTIONS}>")
  add_link_options("$<$<COMPILE_LANGUAGE:CXX>:${ALL_LINK_OPTIONS}>")
  string(REPLACE ";" " " ALL_COMPILE_OPTIONS_SPACED "${ALL_COMPILE_OPTIONS}")
  string(REPLACE ";" " " ALL_LINK_OPTIONS_SPACED "${ALL_LINK_OPTIONS}")

  add_definitions(-DPRINTSYNC)

  include(cmake/grpc.cmake)
  include(cmake/zlib_processed.cmake)
  include(cmake/opencv.cmake)
  include(cmake/boost.cmake)
  include(cmake/hdf5.cmake)

  include_directories(${ZLIB_ROOT})

  execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  configure_file(thalamus_config.h.in thalamus_config.h)
  
  set(PROTO_SOURCES
    proto/util.proto
    proto/task_controller.proto
    proto/ophanim.proto
    proto/thalamus.proto)
  apply_protoc_grpc(PROTO_GENERATED_SOURCES ${PROTO_SOURCES})

  add_library(protoc_generated OBJECT ${PROTO_GENERATED_SOURCES} ${PROTO_SOURCES})
  target_link_libraries(protoc_generated grpc++ grpc++_reflection)

  message("PROTO_SOURCES ${PROTO_SOURCES}")
  message("PROTO_GENERATED_SOURCES ${PROTO_GENERATED_SOURCES}")

  if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(WARNING_FLAGS -Wall -Wextra -Wpedantic -Wno-deprecated-declarations -Wno-c11-extensions -Wno-gcc-compat)
  else()
    #set(WARNING_FLAGS /Wall)
    set(WARNING_FLAGS)
  endif()

  include_directories(${CMAKE_BINARY_DIR})

  if("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    set_target_properties(boost PROPERTIES VS_GLOBAL_IntDir "boost_intermediate/x64/$(Configuration)/")
    set_target_properties(opencv PROPERTIES VS_GLOBAL_IntDir "opencv_intermediate/x64/$(Configuration)/")
    set_target_properties(zlib_processed PROPERTIES VS_GLOBAL_IntDir "zlib_processed_intermediate/x64/$(Configuration)/")
    set_target_properties(ffmpeg PROPERTIES VS_GLOBAL_IntDir "ffmpeg_intermediate/x64/$(Configuration)/")
  endif()

  FetchContent_Declare(
    webrtc_content
    GIT_REPOSITORY https://webrtc.googlesource.com/src
    GIT_TAG 7b80d4480e85cec2ca6010a4912333bf3aa30a50)
  FetchContent_MakeAvailable(webrtc_content)
  add_library(webrtc INTERFACE)
  target_link_libraries(webrtc INTERFACE absl::strings absl::str_format)
  target_include_directories(webrtc INTERFACE "${webrtc_content_SOURCE_DIR}")
  if(WIN32)
    target_compile_definitions(webrtc INTERFACE WEBRTC_WIN NOMINMAX WIN32_LEAN_AND_MEAN)
  elseif(APPLE)
    target_compile_definitions(webrtc INTERFACE WEBRTC_POSIX WEBRTC_MAC)
  else()
    target_compile_definitions(webrtc INTERFACE WEBRTC_POSIX WEBRTC_LINUX)
  endif()

  set(WEBRTC_FILES 
    "${webrtc_content_SOURCE_DIR}/rtc_base/event.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/checks.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/logging.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/time_utils.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/task_queue.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/system_time.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/string_utils.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/event_tracer.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/string_encode.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/platform_thread.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/platform_thread_types.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/strings/string_builder.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/synchronization/yield_policy.cc"
    "${webrtc_content_SOURCE_DIR}/rtc_base/synchronization/sequence_checker_internal.cc"
    "${webrtc_content_SOURCE_DIR}/api/task_queue/task_queue_base.cc")

  add_library(tracing OBJECT
    "${CMAKE_SOURCE_DIR}/src/tracing/tracing.hpp"
    "${CMAKE_SOURCE_DIR}/src/tracing/tracing.cpp"
    "${CMAKE_SOURCE_DIR}/src/tracing/iclock.hpp"
    "${CMAKE_SOURCE_DIR}/src/tracing/systemclock.hpp"
    "${CMAKE_SOURCE_DIR}/src/tracing/systemclock.cpp"
    ${WEBRTC_FILES})
  target_link_libraries(tracing PUBLIC webrtc)
  if(WIN32)
    target_link_libraries(tracing PUBLIC winmm.lib)
  elseif(APPLE)
  else()
    target_link_libraries(tracing PUBLIC stdc++fs)
    target_compile_options(tracing PRIVATE -fPIC)
  endif()
  target_include_directories(tracing PUBLIC "${CMAKE_SOURCE_DIR}/src")
  target_compile_options(tracing PRIVATE ${WARNING_FLAGS})

  add_library(hydrate "src/hydrate.cpp")
  target_link_libraries(hydrate boost grpc++ hdf5-static)
  add_dependencies(hydrate protoc_generated)
  target_compile_definitions(hydrate PRIVATE _USE_MATH_DEFINES NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS _GNU_SOURCE)
  target_include_directories(hydrate PRIVATE "${CMAKE_SOURCE_DIR}/src")
  target_compile_options(hydrate PRIVATE ${WARNING_FLAGS})
    
  add_library(thalamus "${CMAKE_SOURCE_DIR}/src/thalamus.cpp"
                      "${CMAKE_SOURCE_DIR}/src/node_graph_impl.hpp"
                      "${CMAKE_SOURCE_DIR}/src/node_graph_impl.cpp"
                      "${CMAKE_SOURCE_DIR}/src/state.hpp"
                      "${CMAKE_SOURCE_DIR}/src/state.cpp"
                      "${CMAKE_SOURCE_DIR}/src/state_manager.hpp"
                      "${CMAKE_SOURCE_DIR}/src/state_manager.cpp"
                      "${CMAKE_SOURCE_DIR}/src/base_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/base_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/alpha_omega_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/alpha_omega_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/grpc_impl.hpp"
                      "${CMAKE_SOURCE_DIR}/src/grpc_impl.cpp"
                      "${CMAKE_SOURCE_DIR}/src/util.hpp"
                      "${CMAKE_SOURCE_DIR}/src/util.cpp"
                      "${CMAKE_SOURCE_DIR}/src/h5handle.hpp"
                      "${CMAKE_SOURCE_DIR}/src/h5handle.cpp"
                      "${CMAKE_SOURCE_DIR}/src/xsens_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/xsens_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/storage_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/storage_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/run_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/run_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/remote_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/remote_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/ophanim_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/ophanim_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/task_controller_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/task_controller_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/image_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/image_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/oculomatic_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/oculomatic_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/distortion_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/distortion_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/text_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/log_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/log_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/thread_pool.hpp"
                      "${CMAKE_SOURCE_DIR}/src/thread_pool.cpp"
                      "${CMAKE_SOURCE_DIR}/src/nidaq_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/gentl.h"
                      "${CMAKE_SOURCE_DIR}/src/genicam_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/genicam_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/channel_picker_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/channel_picker_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/algebra_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/algebra_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/normalize_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/normalize_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/lua_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/lua_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/intan_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/intan_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/aruco_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/aruco_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/spikeglx_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/spikeglx_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/sync_node.hpp"
                      "${CMAKE_SOURCE_DIR}/src/sync_node.cpp"
                      #"${CMAKE_SOURCE_DIR}/src/hexascope_node.hpp"
                      #"${CMAKE_SOURCE_DIR}/src/hexascope_node.cpp"
                      "${CMAKE_SOURCE_DIR}/src/thalamus/file.hpp"
                      "${CMAKE_SOURCE_DIR}/src/thalamus/file.cpp"
                      "${CMAKE_SOURCE_DIR}/proto/thalamus.proto")
  target_sources(thalamus PRIVATE 
                "${CMAKE_SOURCE_DIR}/src/pupil_node.hpp"
                "${CMAKE_SOURCE_DIR}/src/pupil_node.cpp"
                "${CMAKE_SOURCE_DIR}/src/chessboard_node.hpp"
                "${CMAKE_SOURCE_DIR}/src/chessboard_node.cpp")

  if(WIN32)
    target_compile_definitions(thalamus PRIVATE _WIN32_WINNT=0x0A00 _CRT_SECURE_NO_WARNINGS)
    if("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
      target_compile_options(thalamus PRIVATE /bigobj)
    endif()
    target_sources(thalamus PRIVATE "${CMAKE_SOURCE_DIR}/src/nidaq_node_windows.cpp")
  elseif(APPLE)
    target_sources(thalamus PRIVATE "${CMAKE_SOURCE_DIR}/src/nidaq_node_mac.cpp")
    target_compile_definitions(thalamus PRIVATE _GNU_SOURCE)
  else()
    target_sources(thalamus PRIVATE "${CMAKE_SOURCE_DIR}/src/nidaq_node_linux.cpp"
                                    "${CMAKE_SOURCE_DIR}/src/ros2_node.hpp"
                                    "${CMAKE_SOURCE_DIR}/src/ros2_node.cpp")
    target_link_libraries(thalamus comedi m)
  endif()
  target_compile_options(thalamus PRIVATE ${WARNING_FLAGS})

  target_compile_definitions(thalamus PRIVATE _USE_MATH_DEFINES)
  target_include_directories(thalamus PRIVATE "${CMAKE_SOURCE_DIR}/src")
  target_link_libraries(thalamus opencv hdf5-static lua
    tracing webrtc boost grpc++ grpc++_reflection 
    absl::str_format_internal absl::strings absl::str_format ffmpeg cairo z)
  if(NOT APPLE AND NOT ANDROID)
    target_link_libraries(thalamus zlib_processed)
  endif()
  add_dependencies(thalamus protoc_generated)

  add_executable(native src/main.cpp)

  if(WIN32)
    target_compile_definitions(native PRIVATE _WIN32_WINNT=0x0A00)
  endif()
  target_compile_definitions(native PRIVATE _GNU_SOURCE)

  target_compile_options(native PRIVATE ${WARNING_FLAGS})
  target_include_directories(native PRIVATE "${CMAKE_SOURCE_DIR}/src")
  target_link_libraries(native PRIVATE thalamus hydrate thalamus_ffmpeg tracing webrtc ffmpeg sdl protoc_generated lua cairo)

  if(APPLE)
    target_link_options(native PRIVATE -pagezero_size 10000 -image_base 7fff04c4a000)
  endif()

  add_custom_command(TARGET native POST_BUILD COMMAND cmake -E copy "$<TARGET_FILE:native>" "${CMAKE_SOURCE_DIR}/thalamus")

# Add this before fetching grpc
include(FetchContent)

# Fetch grpc submodule
FetchContent_Declare(
  grpc
  GIT_REPOSITORY https://github.com/grpc/grpc.git
  GIT_TAG v1.55.0
)
FetchContent_MakeAvailable(grpc)

# Build host versions of protoc and grpc_cpp_plugin
add_executable(host_protoc IMPORTED)
set_target_properties(host_protoc PROPERTIES IMPORTED_LOCATION ${grpc_BINARY_DIR}/third_party/protobuf/protoc)
add_executable(host_grpc_cpp_plugin IMPORTED)
set_target_properties(host_grpc_cpp_plugin PROPERTIES IMPORTED_LOCATION ${grpc_BINARY_DIR}/grpc_cpp_plugin)

# Ensure they are built before being used
add_dependencies(host_protoc protobuf)
add_dependencies(host_grpc_cpp_plugin grpc_cpp_plugin)

# Set variables to be used in grpc.cmake
set(PROTOBUF_PROTOC_EXECUTABLE ${grpc_BINARY_DIR}/third_party/protobuf/protoc)
set(GRPC_CPP_PLUGIN_EXECUTABLE ${grpc_BINARY_DIR}/grpc_cpp_plugin)

# Ensure ZLIB is found when cross-compiling for Android
if(ANDROID)
  find_package(ZLIB REQUIRED)
endif()

