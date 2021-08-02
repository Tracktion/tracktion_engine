cmake_minimum_required(VERSION 3.10)
project(farbot)

set(CMAKE_CXX_STANDARD 17)
find_package(Threads REQUIRED)

# Enable testing
include(GoogleTest)
enable_testing()
set(GTEST_DIR "${CMAKE_SOURCE_DIR}/googletest/googletest")

# Set a default build type if none was specified
set(default_build_type "Release")
if(EXISTS "${CMAKE_SOURCE_DIR}/.git")
  set(default_build_type "Debug")
endif()

if(NOT CMAKE_BUILD_TYPE)
  message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
  set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
      STRING "Choose the type of build." FORCE)
  # Set the possible values of build type for cmake-gui
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
    "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
endif()

if(CMAKE_BUILD_TYPE MATCHES "Debug")
  add_definitions("-DDEBUG=1" "-D_DEBUG=1")
  if ((("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")) AND (NOT CMAKE_CROSSCOMPILING))
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=undefined,thread")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=undefined,thread")
    set(SANITIZERS_ENABLED 1)
  else()
    set(SANITIZERS_ENABLED 0)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG}")
  endif()
elseif(CMAKE_BUILD_TYPE MATCHES "Release")
  set(SANITIZERS_ENABLED 0)
  add_definitions("-DNDEBUG=1")
else()
  message(FATAL_ERROR "Unknown build configuration \"${CMAKE_BUILD_TYPE}\": must be Debug or Release")
endif()

#increase warning level
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-missing-field-initializers -Wshadow-all -Wshorten-64-to-32 -Wstrict-aliasing -Wuninitialized -Wunused-parameter -Wsign-compare -Wint-conversion -Wconditional-uninitialized -Wreorder -Wconstant-conversion -Wunused-private-field -Wbool-conversion -Wextra-semi -Wno-ignored-qualifiers -Wunreachable-code")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wno-missing-field-initializers -Wshadow -Wstrict-aliasing -Wuninitialized -Wunused-parameter -Wsign-compare -Wreorder -Wno-ignored-qualifiers -Wunreachable-code")
  string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--no-undefined -Wl,-rpath-link,${CMAKE_SYSROOT}/lib")
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
endif()

# clang needs libatomic
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
 set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} -latomic")
endif()

add_executable(gtestrunner test/test.cpp include/farbot/AsyncCaller.hpp include/farbot/RealtimeTraits.hpp include/farbot/RealtimeObject.hpp include/farbot/detail/RealtimeObject.tcc include/farbot/fifo.hpp include/farbot/detail/fifo.tcc ${GTEST_DIR}/src/gtest_main.cc ${GTEST_DIR}/src/gtest-all.cc)
target_include_directories(gtestrunner PRIVATE include ${GTEST_DIR} ${GTEST_DIR}/include)
target_link_libraries(gtestrunner Threads::Threads) 

gtest_add_tests(TARGET gtestrunner SOURCES test/test.cpp)

install(DIRECTORY include/farbot DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
