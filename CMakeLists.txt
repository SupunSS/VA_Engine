cmake_minimum_required(VERSION 3.20)
project(GTAEngine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(engine 
    src/main.cpp
    src/core/Log.cpp
)
target_link_libraries(engine PRIVATE glfw glm::glm)