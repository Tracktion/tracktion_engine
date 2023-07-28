cmake_minimum_required(VERSION 3.16)

project(choc_tests)

add_executable(choc_tests main.cpp main2.cpp)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID MATCHES "GNU")

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wshadow -Wno-missing-field-initializers -Wstrict-aliasing -Wuninitialized -Wunused-parameter -Wconversion -Wsign-compare -Wreorder -Wsign-conversion -Wno-ignored-qualifiers -Wunreachable-code")
    set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS} -Wno-unused-result")

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -latomic")
    endif ()

    # Enable additional Clang warnings
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined -Wint-conversion -Woverloaded-virtual -Wshorten-64-to-32 -Wconditional-uninitialized -Wconstant-conversion -Wunused-private-field -Wbool-conversion -Wextra-semi -Wshadow-uncaptured-local")
    endif()

    # Disable warnings about ABI changes
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi")
    endif()

    if (WARNINGS_AS_ERRORS)
        message ("Treating warnings as errors")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif ()

    if (PROFILE)
        message ("Enabling profile output")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
    endif ()

    target_compile_features(choc_tests PRIVATE cxx_std_17)

elseif (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

    set(CompilerFlags
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_C_FLAGS
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELEASE
            )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc /W4 /WX -DWIN32 /bigobj ")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")

    # MSVC seems to throw up more problems with C++20 mode than C++17
    target_compile_features(choc_tests PRIVATE cxx_std_20)
endif()

# On OSX, need to add the WebKit framework for WebView functionality
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(choc_tests "-framework WebKit -framework CoreServices")
endif ()

# This is an example of how to link the packages required for WebView on Linux
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules (gtk3 REQUIRED gtk+-3.0 IMPORTED_TARGET)
    pkg_check_modules (webkit2 REQUIRED webkit2gtk-4.0 IMPORTED_TARGET)
    target_link_libraries (choc_tests PUBLIC pthread PkgConfig::gtk3 PkgConfig::webkit2)
endif ()

install(TARGETS choc_tests)

enable_testing()
add_test(NAME choc_tests COMMAND choc_tests)
