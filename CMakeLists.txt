cmake_minimum_required(VERSION 3.20)
project(UsARMirror)

set(CMAKE_CXX_STANDARD 17) # OpenPose refuses to compile on >17
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(fmt CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(Freetype CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GFlags)
find_package(Glog)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenCV)
# find_package(OpenPose REQUIRED)
find_package(realsense2 REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(Boost REQUIRED)

include_directories(
        # ${OpenPose_INCLUDE_DIRS}
        ${GFLAGS_INCLUDE_DIR}
        ${GLOG_INCLUDE_DIR}
        ${OpenCV_INCLUDE_DIRS}
        ${realsense2_INCLUDE_DIRS}
        ${PROJECT_SOURCE_DIR}/deps/tiny_gltf
        ${PROJECT_SOURCE_DIR}/deps/apriltags-cpp
        ${EIGEN3_INCLUDE_DIR}
        ${Boost_INCLUDE_DIRS}
)

add_subdirectory(${PROJECT_SOURCE_DIR}/deps/apriltags-cpp)


file(GLOB SOURCES
        "src/arduino.cpp"
        "src/camera.cpp"
        "src/depth_camera.cpp"
        "src/gesture.cpp"
        "src/main.cpp"
        "src/utils/rw_deque.cpp"
        "src/ui.cpp"
        "src/shaders.cpp"
        "src/background_shader.cpp"
        "src/model_renderer.cpp"
        "src/tiny_gltf_loader.cpp"
)

file(GLOB HEADERS
        "src/arduino.hpp"
        "src/camera.hpp"
        "src/depth_camera.hpp"
        "src/common.hpp"
        "src/event.hpp"
        "src/gesture.hpp"
        "src/kalman.hpp"
        "src/utils/rw_deque.hpp"
        "src/ui.hpp"
        "src/shaders.h"
        "src/background_shader.hpp"
        "src/model_renderer.hpp"
)

add_executable(UsARMirror
        ${SOURCES}
        ${HEADERS}
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        imgui::imgui
        Freetype::Freetype
        Fontconfig::Fontconfig
        glad::glad
        glfw
        spdlog::spdlog
        # ${OpenPose_LIBS}
        ${GFLAGS_LIBRARY}
        ${GLOG_LIBRARY}
        ${OpenCV_LIBS}
        realsense2
        apriltags
        ${Boost_LIBRARIES}
        udev
)

# Transfer openpose/models dir into binary dir
# set(SOURCE_MODELS_DIR "${CMAKE_SOURCE_DIR}/deps/openpose/models")
# file(COPY ${SOURCE_MODELS_DIR} DESTINATION ${CMAKE_BINARY_DIR})