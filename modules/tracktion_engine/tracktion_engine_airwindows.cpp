/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if ! JUCE_PROJUCER_LIVE_BUILD

#include <atomic>
#include <numeric>
#include <set>
#include <string>
#include <math.h>

#define __audioeffect__

#include "tracktion_engine.h"

using namespace juce;

#include "plugins/airwindows/tracktion_AirWindowsBase.cpp"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"
#pragma clang diagnostic ignored "-Wextra"

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
}
}

#pragma clang diagnostic pop

#include "plugins/airwindows/tracktion_AirWindows.cpp"

#endif

