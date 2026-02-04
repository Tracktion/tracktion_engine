/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

//==============================================================================
// TSan (ThreadSanitizer) suppression attribute.
// Use this to mark functions that TSan incorrectly flags as having data races,
// such as lock-free SPSC queue operations where TSan cannot verify that
// read/write buffer regions are disjoint.
//==============================================================================
#ifndef TRACKTION_NO_TSAN
 #if defined(__has_feature)
  #if __has_feature(thread_sanitizer)
   #define TRACKTION_NO_TSAN __attribute__((no_sanitize("thread")))
  #endif
 #endif
 #if ! defined(TRACKTION_NO_TSAN) && defined(__SANITIZE_THREAD__)
  #define TRACKTION_NO_TSAN __attribute__((no_sanitize("thread")))
 #endif
 #ifndef TRACKTION_NO_TSAN
  #define TRACKTION_NO_TSAN
 #endif
#endif
