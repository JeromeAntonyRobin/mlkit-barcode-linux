cmake_minimum_required(VERSION 3.16)
project(mlkit_scanner VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Shared library
add_library(mlkit_scanner SHARED
    src/mlkit_scanner.cpp
)

target_link_libraries(mlkit_scanner PRIVATE ${CMAKE_DL_LIBS})

# Optional OpenCV support for test runner
find_package(OpenCV QUIET)

add_executable(mlkit_demo src/main.cpp)
target_link_libraries(mlkit_demo PRIVATE mlkit_scanner)

if(OpenCV_FOUND)
    target_compile_definitions(mlkit_demo PRIVATE HAVE_OPENCV)
    target_include_directories(mlkit_demo PRIVATE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(mlkit_demo PRIVATE ${OpenCV_LIBS})
    message(STATUS "OpenCV found: enabling OpenCV image reading in demo")
else()
    message(STATUS "OpenCV not found: demo will use raw/BMP testing")
endif()
