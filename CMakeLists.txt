cmake_minimum_required(VERSION 3.13) #cmake 버전 선언
project(OptimizationPass LANGUAGES CXX)

set(LLVM_ROOT_DIR ${CMAKE_SOURCE_DIR}/../compiler-cpp/llvm)
# set(CMAKE_CXX_COMPILER ${LLVM_ROOT_DIR}/bin/clang++)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpic -fno-rtti -stdlib=libc++")

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/../compiler-cpp/llvm/include)
link_directories(${CMAKE_SOURCE_DIR}/../compiler-cpp/llvm/lib)

# add_subdirectory(lib)

add_library(TfliteMemoryProfiler SHARED
  TfliteMemoryProfiler.cpp
)

add_library(TfliteLogger SHARED TfliteLogger.cpp)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-headerpad,128")