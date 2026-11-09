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
