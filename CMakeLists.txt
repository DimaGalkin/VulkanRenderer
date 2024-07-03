cmake_minimum_required(VERSION 3.27)
project(ThreeDL)

set(CMAKE_CXX_STANDARD 20)

add_executable(ThreeDL main.cpp
        engine/vulkan/vulkan-utils.hpp
        engine/threedl.cpp
        engine/threedl.hpp
        engine/vulkan/buffers.hpp
        engine/objects.hpp
        engine/objects.cpp
        engine/vulkan/vulkan-utils.cpp
        engine/vulkan/buffers.cpp)

target_link_libraries(ThreeDL glfw)
target_link_libraries(ThreeDL vulkan)
