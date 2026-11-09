cmake_minimum_required(VERSION 3.16)
project(m12d CXX C)

option(HOST "Build host unit tests instead of device binaries" OFF)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

if(NOT HOST)
  # Static glibc + std::thread needs the whole libpthread archive.
  set(M12_STATIC_LINK -static -Wl,--whole-archive -lpthread -Wl,--no-whole-archive)
else()
  set(M12_STATIC_LINK)
  find_package(Threads REQUIRED)
endif()

function(m12_exe name)
  add_executable(${name} ${ARGN})
  if(HOST)
    target_link_libraries(${name} PRIVATE Threads::Threads)
  else()
    target_link_options(${name} PRIVATE ${M12_STATIC_LINK})
  endif()
endfunction()

m12_exe(hello tools/hello.cpp)

# ---- ncnn (pinned 6afc736, static, built by scripts/build-ncnn.sh) ----
if(HOST)
  set(NCNN_ARCH host)
else()
  set(NCNN_ARCH armhf)
endif()
set(NCNN_PREFIX ${CMAKE_SOURCE_DIR}/third_party/ncnn-install/${NCNN_ARCH})
add_library(ncnn_lib STATIC IMPORTED GLOBAL)
set_target_properties(ncnn_lib PROPERTIES
  IMPORTED_LOCATION ${NCNN_PREFIX}/lib/libncnn.a
  INTERFACE_INCLUDE_DIRECTORIES ${NCNN_PREFIX}/include/ncnn)
if(HOST)
  find_package(OpenMP REQUIRED)
  target_link_libraries(ncnn_lib INTERFACE OpenMP::OpenMP_CXX)
else()
  # FindOpenMP resolves to libgomp.so; with -static let gcc pick libgomp.a itself.
  target_link_options(ncnn_lib INTERFACE -fopenmp)
endif()

m12_exe(m12bench tools/m12bench.cpp)
target_link_libraries(m12bench PRIVATE ncnn_lib)
