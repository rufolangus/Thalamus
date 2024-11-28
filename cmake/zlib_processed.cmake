file(GLOB ZLIB_HEADERS ${grpc_SOURCE_DIR}/third_party/zlib/*.h)
list(APPEND ZLIB_HEADERS ${grpc_BINARY_DIR}/third_party/zlib/zconf.h)

set(ZLIB_HEADER_FILENAMES)
foreach(HEADER ${ZLIB_HEADERS})
  get_filename_component(FILENAME ${HEADER} NAME)
  list(APPEND ZLIB_HEADER_FILENAMES "${FILENAME}")
endforeach()

set(ZLIB_PROCESSED_HEADER_DIR "${CMAKE_CURRENT_BINARY_DIR}/opencv_zlib_headers")
set(ZLIB_PKG_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/opencv_zlib_headers")
set(ZLIB_PROCESSED_HEADER_PATHS)
foreach(HEADER ${ZLIB_HEADER_FILENAMES})
  list(APPEND ZLIB_PROCESSED_HEADER_PATHS "${ZLIB_PROCESSED_HEADER_DIR}/${HEADER}")
endforeach()

if(NOT ANDROID)
  add_custom_command(DEPENDS ZLIB::ZLIB
                     OUTPUT ${ZLIB_PROCESSED_HEADER_PATHS} "${ZLIB_PROCESSED_HEADER_DIR}/zlib.pc"
                     COMMAND cmake -E make_directory "${ZLIB_PROCESSED_HEADER_DIR}"
                     && cmake -E copy ${ZLIB_HEADERS} "${ZLIB_PROCESSED_HEADER_DIR}"
                     && cmake "-DZLIB_LIBRARY=${ZLIB_LIBRARY}" "-DOUTPUT_DIR=${ZLIB_PROCESSED_HEADER_DIR}" -P ${CMAKE_SOURCE_DIR}/generate_zlib_pc.cmake)
else()
  # On Android, use the NDK's zlib directly; no need for custom commands
  set(ZLIB_ROOT "${CMAKE_ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/sysroot/usr")
  set(ZLIB_INCLUDE_DIR "${ZLIB_ROOT}/include")
  set(ZLIB_LIBRARY "${ZLIB_ROOT}/lib/${CMAKE_ANDROID_ARCH_ABI}/libz.so")
  set(ZLIB_PROCESSED_HEADER_PATHS ${ZLIB_HEADERS})
  if(TARGET ZLIB::ZLIB)
    remove_library(ZLIB::ZLIB)
  endif()
  add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
  set_target_properties(ZLIB::ZLIB PROPERTIES
    IMPORTED_LOCATION "${ZLIB_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
  )
  if(NOT TARGET zlib_processed)
    add_library(zlib_processed INTERFACE)
  endif()
  target_include_directories(zlib_processed INTERFACE "${ZLIB_INCLUDE_DIR}")
  target_link_libraries(zlib_processed INTERFACE "${ZLIB_LIBRARY}")
endif()

if(NOT TARGET zlib_processed)
  add_library(zlib_processed INTERFACE ${ZLIB_PROCESSED_HEADER_PATHS})
  if(WIN32)
    target_link_libraries(zlib_processed INTERFACE ZLIB::ZLIB)
  elseif(APPLE)
    target_link_options(zlib_processed INTERFACE -Wl,-force_load ZLIB::ZLIB)
  else()
    target_link_options(zlib_processed INTERFACE -Wl,--whole-archive ZLIB::ZLIB -Wl,--no-whole-archive)
  endif()
  target_include_directories(zlib_processed INTERFACE "${ZLIB_PROCESSED_HEADER_DIR}")
endif()

target_include_directories(zlib_processed INTERFACE "${ZLIB_PROCESSED_HEADER_DIR}")

