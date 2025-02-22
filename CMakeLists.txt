cmake_minimum_required(VERSION 3.20)
project(UsARMirror)

set(CMAKE_CXX_STANDARD 17) # OpenPose refuses to compile on >17
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(imgui CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GFlags)
find_package(Glog)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenCV)
find_package(OpenPose REQUIRED)

include_directories(
        ${OpenPose_INCLUDE_DIRS}
        ${GFLAGS_INCLUDE_DIR}
        ${GLOG_INCLUDE_DIR}
        ${OpenCV_INCLUDE_DIRS}
)

file(GLOB SOURCES
        "src/main.cpp"
        "src/pipeline.cpp"
)

file(GLOB HEADERS
        "src/common.hpp"
        "src/pipeline.hpp"
)

add_executable(UsARMirror
        ${SOURCES}
        ${HEADERS}
)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        imgui::imgui
        glad::glad
        glfw
        spdlog::spdlog
        ${OpenPose_LIBS}
        ${GFLAGS_LIBRARY}
        ${GLOG_LIBRARY}
        ${OpenCV_LIBS}
)

# Transfer openpose/models dir into binary dir
set(SOURCE_MODELS_DIR "${CMAKE_SOURCE_DIR}/deps/openpose/models")
file(COPY ${SOURCE_MODELS_DIR} DESTINATION ${CMAKE_BINARY_DIR})