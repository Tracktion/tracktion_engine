/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if defined (__clang__) || defined (__GNUC__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Wzero-as-null-pointer-constant"
#endif

extern "C"
{
    #include "../../3rd_party/rpmalloc/rpmalloc.c"
}

#if defined (__clang__) || defined (__GNUC__)
 #pragma clang diagnostic pop
#endif
