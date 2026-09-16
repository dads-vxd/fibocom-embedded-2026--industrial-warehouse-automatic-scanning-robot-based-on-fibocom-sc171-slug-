cmake_minimum_required(VERSION 3.15)
project(yolov11_barcode LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build the full device app and every target that depends on OpenCV / ONNX
# Runtime. Turn it OFF to configure the lightweight WS/serial tests without
# needing OpenCV or ORT installed:
#   cmake -S . -B build-test -DBUILD_DEVICE_APP=OFF
#   cmake --build build-test --target test_control
option(BUILD_DEVICE_APP "Build full device app + OpenCV/ORT-dependent targets" ON)

if(BUILD_DEVICE_APP)
    set(OPENCV_LOCAL_INSTALL "${CMAKE_SOURCE_DIR}/third_party/opencv_build/install")
    if(EXISTS "${OPENCV_LOCAL_INSTALL}")
        set(OpenCV_DIR "${OPENCV_LOCAL_INSTALL}/lib/cmake/opencv4")
        message(STATUS "Using local OpenCV: ${OPENCV_LOCAL_INSTALL}")
        message(STATUS "  OpenCV_DIR = ${OpenCV_DIR}")
    endif()

    find_package(OpenCV REQUIRED)
    message(STATUS "Found OpenCV: ${OpenCV_DIR} (version ${OpenCV_VERSION})")

    find_package(CURL REQUIRED)

    find_path(ORT_INCLUDE_DIR
        NAMES onnxruntime_cxx_api.h
        PATHS
            ${CMAKE_PREFIX_PATH}/include/onnxruntime
            ${CMAKE_PREFIX_PATH}/include
            $ENV{ONNXRUNTIME_DIR}/include
    )

    find_library(ORT_LIBRARY
        NAMES onnxruntime
        PATHS
            ${CMAKE_PREFIX_PATH}/lib
            $ENV{ONNXRUNTIME_DIR}/lib
    )

    if(NOT ORT_INCLUDE_DIR OR NOT ORT_LIBRARY)
        message(FATAL_ERROR
            "ONNX Runtime not found. Set CMAKE_PREFIX_PATH or ONNXRUNTIME_DIR.\n"
            "On NixOS: use `nix develop` or set CMAKE_PREFIX_PATH to include onnxruntime.dev")
    endif()

    message(STATUS "ORT include: ${ORT_INCLUDE_DIR}")
    message(STATUS "ORT library: ${ORT_LIBRARY}")
else()
    message(STATUS "BUILD_DEVICE_APP=OFF: skipping OpenCV / ONNX Runtime; "
                   "only lightweight WS/serial tests are available "
                   "(test_control, test_ws_fields, test_serial).")
endif()

find_package(nlohmann_json REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

set(LIB_SOURCES
    src/lib/source/barcode_data.cpp
    src/lib/source/base64.cpp
    src/lib/source/bytetrack.cpp
    src/lib/source/config.cpp
    src/lib/source/control_client.cpp
    src/lib/source/drawing.cpp
    src/lib/source/image_preprocess.cpp
    src/lib/source/iou_utils.cpp
    src/lib/source/pipeline.cpp
    src/lib/source/qr_scanner.cpp
    src/lib/source/serial_sender.cpp
    src/lib/source/upload_worker.cpp
    src/lib/source/video_recorder.cpp
    src/lib/source/video_uploader.cpp
    src/lib/source/ws_client.cpp
    src/lib/source/yolov11.cpp
)

# --- Main executable ---
if(BUILD_DEVICE_APP)
add_executable(barcode_detect
    ${LIB_SOURCES}
    src/main.cpp
)

target_include_directories(barcode_detect PRIVATE
    ${ORT_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    src/lib/include
)

target_link_libraries(barcode_detect PRIVATE
    ${ORT_LIBRARY}
    ${OpenCV_LIBS}
    opencv_wechat_qrcode
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    Threads::Threads
)

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# --- Test: ByteTrack ---
if(BUILD_DEVICE_APP)
add_executable(test_bytetrack
    src/lib/source/bytetrack.cpp
    src/lib/source/drawing.cpp
    src/lib/source/iou_utils.cpp
    src/lib/source/yolov11.cpp
    src/test/test_bytetrack.cpp
)

target_include_directories(test_bytetrack PRIVATE
    ${ORT_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    src/lib/include
)

target_link_libraries(test_bytetrack PRIVATE
    ${ORT_LIBRARY}
    ${OpenCV_LIBS}
    nlohmann_json::nlohmann_json
)
endif()

# --- Test: WebSocket get all fields ---
add_executable(test_ws_fields
    src/lib/source/base64.cpp
    src/lib/source/ws_client.cpp
    src/test/test_ws_fields.cpp
)

target_include_directories(test_ws_fields PRIVATE
    src/lib/include
)

target_link_libraries(test_ws_fields PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

# --- Test: Device control channel round-trip (command + log) ---
add_executable(test_control
    src/lib/source/base64.cpp
    src/lib/source/ws_client.cpp
    src/test/test_control.cpp
)

target_include_directories(test_control PRIVATE
    src/lib/include
)

target_link_libraries(test_control PRIVATE
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
)

# --- Test: QR ---
if(BUILD_DEVICE_APP)
add_executable(test_qr
    src/lib/source/qr_scanner.cpp
    src/test/test_qr.cpp
)

target_include_directories(test_qr PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    src/lib/include
)

target_link_libraries(test_qr PRIVATE
    ${OpenCV_LIBS}
    opencv_wechat_qrcode
)
endif()

# --- Test: Serial Sender ---
add_executable(test_serial
    src/lib/source/serial_sender.cpp
    src/test/test_serial.cpp
)

target_include_directories(test_serial PRIVATE
    src/lib/include
)

target_link_libraries(test_serial PRIVATE
    Threads::Threads
)

# --- Test: Main (no upload, no compression video saved to ./videos) ---
if(BUILD_DEVICE_APP)
add_executable(test_main
    ${LIB_SOURCES}
    src/test_main.cpp
)

target_include_directories(test_main PRIVATE
    ${ORT_INCLUDE_DIR}
    ${OpenCV_INCLUDE_DIRS}
    src/lib/include
)

target_link_libraries(test_main PRIVATE
    ${ORT_LIBRARY}
    ${OpenCV_LIBS}
    opencv_wechat_qrcode
    CURL::libcurl
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    Threads::Threads
)
endif()
