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

# ---- core library (shared by device binaries and host tests) ----
set(M12CORE_SOURCES
  src/core/latest_frame.cpp
  src/core/line_queue.cpp
  src/core/fps_meter.cpp
  src/image/yuyv.cpp
  src/camera/capture_loop.cpp
  src/camera/v4l2_camera.cpp
  src/vision/retinaface.cpp
  src/vision/align.cpp
  src/vision/age_gender.cpp
  src/vision/genderage.cpp
  src/vision/face_tracker.cpp
  src/vision/person_det.cpp
  src/vision/person_tracker.cpp
  src/vision/det_schedule.cpp
  src/vision/pairing.cpp
  src/proto/det_json.cpp
  src/io/line_splitter.cpp
  src/io/serial_port.cpp
  src/io/tcp_listener.cpp
  src/io/io_loop.cpp
  src/proto/flat_json.cpp
  src/proto/commands.cpp
  src/sys/thermal.cpp
  src/sys/sysinfo.cpp
  src/app/options.cpp
  src/sys/fileio.cpp
)
add_library(m12core STATIC ${M12CORE_SOURCES})
target_include_directories(m12core PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(m12core PUBLIC ncnn_lib)
if(HOST)
  target_link_libraries(m12core PUBLIC Threads::Threads)
endif()

# ---- host unit tests ----
if(HOST)
  enable_testing()
  set(M12TEST_SOURCES
    tests/test_main.cpp
    tests/test_latest_frame.cpp
    tests/test_line_queue.cpp
    tests/test_yuyv.cpp
    tests/test_capture_loop.cpp
    tests/test_retinaface.cpp
    tests/test_det_json.cpp
    tests/test_line_splitter.cpp
    tests/test_io_loop.cpp
    tests/test_flat_json.cpp
    tests/test_commands.cpp
    tests/test_thermal.cpp
    tests/test_options.cpp
    tests/test_fileio.cpp
    tests/test_fps_meter.cpp
    tests/test_attributes.cpp
    tests/test_genderage.cpp
    tests/test_face_tracker.cpp
    tests/test_person_det.cpp
    tests/test_person_tracker.cpp
    tests/test_det_schedule.cpp
    tests/test_pairing.cpp
  )
  add_executable(m12tests ${M12TEST_SOURCES})
  target_include_directories(m12tests PRIVATE ${CMAKE_SOURCE_DIR}/third_party/doctest)
  target_link_libraries(m12tests PRIVATE m12core)
  add_test(NAME m12tests COMMAND m12tests WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
endif()

# ---- tools ----
m12_exe(m12bench tools/m12bench.cpp)
target_link_libraries(m12bench PRIVATE ncnn_lib)
m12_exe(m12cap tools/m12cap.cpp)
target_link_libraries(m12cap PRIVATE m12core)
m12_exe(detect_file tools/detect_file.cpp)
target_link_libraries(detect_file PRIVATE m12core)

# ---- host-only stock reference oracle (B-layer parity), links host OpenCV ----
if(HOST)
  # Only libopencv-{core,imgproc}-dev are installed (no OpenCVConfig.cmake), so locate directly.
  find_path(OPENCV_INC opencv2/core.hpp PATH_SUFFIXES opencv4 REQUIRED)
  find_library(OPENCV_CORE opencv_core REQUIRED)
  find_library(OPENCV_IMGPROC opencv_imgproc REQUIRED)
  add_executable(ref_stock_fd tools/ref/ref_stock_fd.cpp)
  target_include_directories(ref_stock_fd PRIVATE ${OPENCV_INC})
  target_link_libraries(ref_stock_fd PRIVATE ncnn_lib ${OPENCV_IMGPROC} ${OPENCV_CORE})
  # experiment: detection quality vs detector input size on the fixed frame set
  add_executable(attr_file tools/attr_file.cpp)
  target_link_libraries(attr_file PRIVATE m12core)
  add_executable(genderage_file tools/genderage_file.cpp)
  target_link_libraries(genderage_file PRIVATE m12core)
  add_executable(scale_sweep tools/scale_sweep.cpp)
  target_link_libraries(scale_sweep PRIVATE m12core)
endif()
# experiment (phase 1b): Yolo-Fastest person detection vs RetinaFace on captured frames (host + device timing)
m12_exe(person_file tools/person_file.cpp)
target_link_libraries(person_file PRIVATE m12core)

# ---- m12d main program ----
execute_process(COMMAND git -c safe.directory=* rev-parse --short HEAD
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE M12D_GIT
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
if(NOT M12D_GIT)
  set(M12D_GIT unknown)
endif()
m12_exe(m12d src/app/m12d.cpp)
target_link_libraries(m12d PRIVATE m12core)
target_compile_definitions(m12d PRIVATE M12D_GIT="${M12D_GIT}")
