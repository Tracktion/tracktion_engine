/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

#pragma once

namespace Icons
{
    Path getFolderPath();
    Path getDicePath();
}

namespace DemoBinaryData
{
    extern const char*   kick_ogg;
    const int            kick_oggSize = 5315;

    extern const char*   snare_ogg;
    const int            snare_oggSize = 5723;

    extern const char*   closed_hat_ogg;
    const int            closed_hat_oggSize = 5626;

    extern const char*   open_hat_ogg;
    const int            open_hat_oggSize = 6124;

    extern const char*   clap_ogg;
    const int            clap_oggSize = 4840;

    extern const char*   low_tom_ogg;
    const int            low_tom_oggSize = 5465;

    extern const char*   high_tom_ogg;
    const int            high_tom_oggSize = 5225;

    extern const char*   ride_ogg;
    const int            ride_oggSize = 5621;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 8;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
