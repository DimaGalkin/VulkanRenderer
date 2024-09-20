cmake_minimum_required(VERSION 3.27)
project(ThreeDL)

set(CMAKE_CXX_STANDARD 20)

find_package( OpenCV REQUIRED )
include_directories( ${OpenCV_INCLUDE_DIRS} )

add_executable(ThreeDL main.cpp
        engine/vulkan/vulkan-utils.hpp
        engine/threedl.cpp
        engine/threedl.hpp
        engine/vulkan/buffers.hpp
        engine/objects.hpp
        engine/objects.cpp
        engine/vulkan/vulkan-utils.cpp
        engine/vulkan/buffers.cpp
        engine/camera.cpp
        engine/camera.hpp
        engine/types.hpp
        engine/lighting.hpp
        engine/lighting.cpp)

target_link_libraries( ThreeDL ${OpenCV_LIBS} )
target_link_libraries(ThreeDL glfw)
target_link_libraries(ThreeDL vulkan)
