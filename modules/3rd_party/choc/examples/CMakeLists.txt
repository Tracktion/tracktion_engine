cmake_minimum_required(VERSION 3.15)
project(choc_examples CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core examples - all source files are now in the root examples directory
add_executable(value_and_json value_and_json.cpp)
add_executable(audio_operations audio_operations.cpp)

# Audio beep player - can be disabled for CI builds
option(CHOC_BUILD_AUDIO_BEEP_PLAYER "Build the audio beep player example" ON)
if (CHOC_BUILD_AUDIO_BEEP_PLAYER)
    add_executable(audio_beep_player audio_beep_player.cpp)
endif()

# add_executable(http_server http_server.cpp)  # Requires Boost libraries
add_executable(fifo_producer_consumer fifo_producer_consumer.cpp)
add_executable(xxhash_example xxhash_example.cpp)
add_executable(file_watcher_example file_watcher_example.cpp)

# Comprehensive examples
add_executable(midi_file_processing midi_file_processing.cpp)
add_executable(javascript_integration javascript_integration.cpp)
add_executable(threading_patterns threading_patterns.cpp)
add_executable(text_processing text_processing.cpp)

# Platform-specific examples
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(file_watcher_example PRIVATE "-framework CoreServices")
    if (CHOC_BUILD_AUDIO_BEEP_PLAYER)
        target_link_libraries(audio_beep_player PRIVATE "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreMIDI" "-framework CoreFoundation")
    endif()

    # WebView example requires WebKit on macOS
    add_executable(webview_desktop_app webview_desktop_app.cpp)
    target_link_libraries(webview_desktop_app PRIVATE "-framework WebKit" "-framework Cocoa")
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    if (CHOC_BUILD_AUDIO_BEEP_PLAYER)
        # Audio beep player requires ALSA on Linux
        target_link_libraries(audio_beep_player PRIVATE asound pthread)
    endif()

    # WebView example requires webkit2gtk on Linux
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK3 REQUIRED gtk+-3.0)
    pkg_check_modules(WEBKIT2 REQUIRED webkit2gtk-4.1)

    if (GTK3_FOUND AND WEBKIT2_FOUND)
        add_executable(webview_desktop_app webview_desktop_app.cpp)
        target_include_directories(webview_desktop_app PRIVATE ${GTK3_INCLUDE_DIRS} ${WEBKIT2_INCLUDE_DIRS})
        target_link_libraries(webview_desktop_app PRIVATE ${GTK3_LIBRARIES} ${WEBKIT2_LIBRARIES})
        target_compile_options(webview_desktop_app PRIVATE ${GTK3_CFLAGS_OTHER} ${WEBKIT2_CFLAGS_OTHER})
    else()
        message(WARNING "WebView example disabled: GTK3 or WebKit2GTK not found")
    endif()
elseif (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    if (CHOC_BUILD_AUDIO_BEEP_PLAYER)
        # Audio beep player requires DirectSound/WASAPI on Windows
        target_link_libraries(audio_beep_player PRIVATE dsound winmm ole32)
    endif()

    # WebView example should work on Windows without additional dependencies
    add_executable(webview_desktop_app webview_desktop_app.cpp)
endif()