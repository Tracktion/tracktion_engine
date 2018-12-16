/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if ! JUCE_PROJUCER_LIVE_BUILD

#if TRACKTION_AIR_WINDOWS

#include <atomic>
#include <numeric>
#include <set>
#include <string>
#include <math.h>

#define __audioeffect__

#include "tracktion_engine.h"

using namespace juce;

#include "plugins/airwindows/tracktion_AirWindowsBase.cpp"

#if JUCE_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#pragma clang diagnostic ignored "-Wreorder"
#pragma clang diagnostic ignored "-Wunsequenced"
#pragma clang diagnostic ignored "-Wint-to-pointer-cast"
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#pragma clang diagnostic ignored "-Wshadow"
#pragma clang diagnostic ignored "-Wdeprecated-register"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wdelete-non-virtual-dtor"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wextra-semi"
#pragma clang diagnostic ignored "-Wunused-value"
#pragma clang diagnostic ignored "-Wunused-private-field"
#endif

#if JUCE_WINDOWS
#pragma warning (push)
#pragma warning (disable : 4244 4100 4305)
#endif

namespace tracktion_engine
{
namespace airwindows
{
namespace deess
{
#include "3rd_party/airwindows/plugins/WinVST/DeEss/DeEss.h"
#include "3rd_party/airwindows/plugins/WinVST/DeEss/DeEss.cpp"
#include "3rd_party/airwindows/plugins/WinVST/DeEss/DeEssProc.cpp"
}
namespace drive
{
#include "3rd_party/airwindows/plugins/WinVST/Drive/Drive.h"
#include "3rd_party/airwindows/plugins/WinVST/Drive/Drive.cpp"
#include "3rd_party/airwindows/plugins/WinVST/Drive/DriveProc.cpp"
}
namespace hardvacuum
{
#include "3rd_party/airwindows/plugins/WinVST/HardVacuum/HardVacuum.h"
#include "3rd_party/airwindows/plugins/WinVST/HardVacuum/HardVacuum.cpp"
#include "3rd_party/airwindows/plugins/WinVST/HardVacuum/HardVacuumProc.cpp"
}
namespace nonlinearspace
{
#include "3rd_party/airwindows/plugins/WinVST/NonlinearSpace/NonlinearSpace.h"
#include "3rd_party/airwindows/plugins/WinVST/NonlinearSpace/NonlinearSpace.cpp"
#include "3rd_party/airwindows/plugins/WinVST/NonlinearSpace/NonlinearSpaceProc.cpp"
}
namespace purestdrive
{
#include "3rd_party/airwindows/plugins/WinVST/PurestDrive/PurestDrive.h"
#include "3rd_party/airwindows/plugins/WinVST/PurestDrive/PurestDrive.cpp"
#include "3rd_party/airwindows/plugins/WinVST/PurestDrive/PurestDriveProc.cpp"
}
namespace tubedesk
{
#include "3rd_party/airwindows/plugins/WinVST/TubeDesk/TubeDesk.h"
#include "3rd_party/airwindows/plugins/WinVST/TubeDesk/TubeDesk.cpp"
#include "3rd_party/airwindows/plugins/WinVST/TubeDesk/TubeDeskProc.cpp"
}
}
}

#if JUCE_CLANG
#pragma clang diagnostic pop
#endif

#if JUCE_WINDOWS
#pragma warning (pop)
#endif

#include "plugins/airwindows/tracktion_AirWindows.cpp"

#endif
#endif

