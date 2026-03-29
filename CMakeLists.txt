cmake_minimum_required(VERSION 3.20)
project(DawnMRE CXX C)

set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "Disable Dawn samples" FORCE)
set(DAWN_BUILD_TESTS OFF CACHE BOOL "Disable Dawn tests" FORCE)
set(TINT_BUILD_TESTS OFF CACHE BOOL "Disable Tint tests" FORCE)

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dawn/CMakeLists.txt")
    add_subdirectory(dawn EXCLUDE_FROM_ALL)
else()
    message(FATAL_ERROR "Dawn directory not found! Please clone dawn into ./dawn")
endif()

add_executable(mre main.cpp)
target_compile_features(mre PRIVATE cxx_std_17)

target_link_libraries(mre PRIVATE webgpu_cpp dawn_native dawn_proc)