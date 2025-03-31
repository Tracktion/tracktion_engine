cmake_minimum_required(VERSION 3.16)

project(choc_tests)

add_executable(choc_tests main.cpp main2.cpp)

# To test V8, there needs to be a suitable folder somewhere with a build
# set(V8_LOCATION "/Users/jules/code/v8")
# set(V8_BUILD_NAME "arm64.release")

# To test the HTTP server, you'll need boost beast and its dependencies, in a
# folder at this location:
# set(BOOST_LOCATION "/Users/jules/code/cmajor-dev/cmajor/3rdParty/boost")

target_compile_features(choc_tests PRIVATE cxx_std_20)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR
    CMAKE_CXX_COMPILER_ID MATCHES "GNU")

    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
    set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG} -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} -Werror -Wall -Wextra -Wshadow -Wno-missing-field-initializers -Wstrict-aliasing -Wuninitialized -Wunused-parameter -Wconversion -Wsign-compare -Wreorder -Wsign-conversion -Wno-ignored-qualifiers -Wunreachable-code -Wdeprecated-copy -Wdeprecated-copy-dtor -Wpedantic ")
    set(CMAKE_C_FLAGS           "${CMAKE_C_FLAGS} -Wno-unused-result")

    if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++ -latomic")
    endif()

    # Enable additional Clang warnings
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wint-conversion -Woverloaded-virtual -Wshorten-64-to-32 -Wconditional-uninitialized -Wconstant-conversion -Wunused-private-field -Wbool-conversion -Wextra-semi -Wshadow-uncaptured-local")

        # V8 screws up the sanitizer...
        if (DEFINED V8_LOCATION)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DCHOC_V8_AVAILABLE=1 -DV8_COMPRESS_POINTERS=1 -DV8_ENABLE_SANDBOX=1")
        else()
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=undefined")
        endif()
    endif()

    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # Disable warnings about ABI changes
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-psabi -flarge-source-files")
        set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -static-libstdc++")
    endif()

    if (WARNINGS_AS_ERRORS)
        message ("Treating warnings as errors")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif()

    if (PROFILE)
        message ("Enabling profile output")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
    endif()

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
endif()

# On OSX, need to add the WebKit framework for WebView functionality
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(choc_tests "-framework WebKit -framework CoreServices")

    if (DEFINED V8_LOCATION)
        target_include_directories(choc_tests PRIVATE "${V8_LOCATION}/v8/include")
        target_link_directories(choc_tests PRIVATE "${V8_LOCATION}/v8/out.gn/${V8_BUILD_NAME}/obj")
        target_link_libraries(choc_tests "-lv8_monolith -lv8_libbase -lv8_libplatform")
    endif()
endif()

# This is an example of how to link the packages required for WebView on Linux
if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules (gtk3 REQUIRED gtk+-3.0 IMPORTED_TARGET)
    pkg_check_modules (webkit2 REQUIRED webkit2gtk-4.1 IMPORTED_TARGET)
    target_link_libraries (choc_tests PUBLIC pthread PkgConfig::gtk3 PkgConfig::webkit2)
endif()

if (DEFINED BOOST_LOCATION)
    add_compile_definitions (choc_tests
        CHOC_ENABLE_HTTP_SERVER_TEST=1
    )

    target_include_directories(choc_tests
        SYSTEM PRIVATE
        ${BOOST_LOCATION}/align/include/
        ${BOOST_LOCATION}/asio/include/
        ${BOOST_LOCATION}/assert/include/
        ${BOOST_LOCATION}/beast/include/
        ${BOOST_LOCATION}/bind/include/
        ${BOOST_LOCATION}/config/include/
        ${BOOST_LOCATION}/core/include/
        ${BOOST_LOCATION}/date_time/include/
        ${BOOST_LOCATION}/endian/include/
        ${BOOST_LOCATION}/intrusive/include/
        ${BOOST_LOCATION}/is_placeholder/include/
        ${BOOST_LOCATION}/io/include/
        ${BOOST_LOCATION}/logic/include/
        ${BOOST_LOCATION}/optional/include/
        ${BOOST_LOCATION}/static_assert/include/
        ${BOOST_LOCATION}/smart_ptr/include/
        ${BOOST_LOCATION}/system/include/
        ${BOOST_LOCATION}/move/include/
        ${BOOST_LOCATION}/mp11/include/
        ${BOOST_LOCATION}/mpl/include/
        ${BOOST_LOCATION}/numeric_conversion/include/
        ${BOOST_LOCATION}/preprocessor/include/
        ${BOOST_LOCATION}/predef/include/
        ${BOOST_LOCATION}/regex/include/
        ${BOOST_LOCATION}/throw_exception/include/
        ${BOOST_LOCATION}/type_traits/include/
        ${BOOST_LOCATION}/utility/include/
        ${BOOST_LOCATION}/winapi/include/
        ${BOOST_LOCATION}/static_string/include/
    )
endif()

install(TARGETS choc_tests)

enable_testing()
add_test(NAME choc_tests COMMAND choc_tests)
