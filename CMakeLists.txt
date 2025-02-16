cmake_minimum_required(VERSION 3.20)
project(UsARMirror)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(imgui CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)


file(GLOB SOURCES
    "src/main.cpp"
)

file(GLOB HEADERS
    "src/constants.hpp"
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
)