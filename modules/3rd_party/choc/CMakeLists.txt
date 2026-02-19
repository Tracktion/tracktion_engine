cmake_minimum_required(VERSION 3.15)
project(choc VERSION 1.0.0 LANGUAGES CXX)

# Create interface library for header-only CHOC
add_library(choc INTERFACE)

# Set include directories
target_include_directories(choc INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Require C++17
target_compile_features(choc INTERFACE cxx_std_17)

# Alias for easier usage
add_library(choc::choc ALIAS choc)