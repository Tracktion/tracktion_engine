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
namespace acceleration
{
 #include "3rd_party/airwindows/Acceleration/Acceleration.cpp"
 #include "3rd_party/airwindows/Acceleration/AccelerationProc.cpp"
}
namespace aura
{
 #include "3rd_party/airwindows/Aura/Aura.cpp"
 #include "3rd_party/airwindows/Aura/AuraProc.cpp"
}
namespace adclip7
{
 #include "3rd_party/airwindows/ADClip7/ADClip7.cpp"
 #include "3rd_party/airwindows/ADClip7/ADClip7Proc.cpp"
}
namespace bitglitter
{
 #include "3rd_party/airwindows/BitGlitter/BitGlitter.cpp"
 #include "3rd_party/airwindows/BitGlitter/BitGlitterProc.cpp"
}
namespace buttercomp2
{
 #include "3rd_party/airwindows/ButterComp2/ButterComp2.cpp"
 #include "3rd_party/airwindows/ButterComp2/ButterComp2Proc.cpp"
}
namespace chorusensemble
{
 #include "3rd_party/airwindows/ChorusEnsemble/ChorusEnsemble.cpp"
 #include "3rd_party/airwindows/ChorusEnsemble/ChorusEnsembleProc.cpp"
}
namespace Density
{
 #include "3rd_party/airwindows/Density/Density.cpp"
 #include "3rd_party/airwindows/Density/DensityProc.cpp"
}
namespace derez
{
 #include "3rd_party/airwindows/DeRez/DeRez.cpp"
 #include "3rd_party/airwindows/DeRez/DeRezProc.cpp"
}
namespace deess
{
 #include "3rd_party/airwindows/DeEss/DeEss.cpp"
 #include "3rd_party/airwindows/DeEss/DeEssProc.cpp"
}
namespace density
{
 #include "3rd_party/airwindows/Density/Density.cpp"
 #include "3rd_party/airwindows/Density/DensityProc.cpp"
}
namespace distance2
{
 #include "3rd_party/airwindows/Distance2/Distance2.cpp"
 #include "3rd_party/airwindows/Distance2/Distance2Proc.cpp"
}
namespace drive
{
 #include "3rd_party/airwindows/Drive/Drive.cpp"
 #include "3rd_party/airwindows/Drive/DriveProc.cpp"
}
namespace fathomfive
{
 #include "3rd_party/airwindows/FathomFive/FathomFive.cpp"
 #include "3rd_party/airwindows/FathomFive/FathomFiveProc.cpp"
}
namespace awfloor
{
 #include "3rd_party/airwindows/Floor/Floor.cpp"
 #include "3rd_party/airwindows/Floor/FloorProc.cpp"
}
namespace gatelope
{
 #include "3rd_party/airwindows/Gatelope/Gatelope.cpp"
 #include "3rd_party/airwindows/Gatelope/GatelopeProc.cpp"
}
namespace groovewear
{
 #include "3rd_party/airwindows/GrooveWear/GrooveWear.cpp"
 #include "3rd_party/airwindows/GrooveWear/GrooveWearProc.cpp"
}
namespace guitarconditioner
{
 #include "3rd_party/airwindows/GuitarConditioner/GuitarConditioner.cpp"
 #include "3rd_party/airwindows/GuitarConditioner/GuitarConditionerProc.cpp"
}
namespace hardvacuum
{
 #include "3rd_party/airwindows/HardVacuum/HardVacuum.cpp"
 #include "3rd_party/airwindows/HardVacuum/HardVacuumProc.cpp"
}
namespace hombre
{
 #include "3rd_party/airwindows/Hombre/Hombre.cpp"
 #include "3rd_party/airwindows/Hombre/HombreProc.cpp"
}
namespace nc17
{
 #include "3rd_party/airwindows/NCSeventeen/NCSeventeen.cpp"
 #include "3rd_party/airwindows/NCSeventeen/NCSeventeenProc.cpp"
}
namespace nonlinearspace
{
 #include "3rd_party/airwindows/NonlinearSpace/NonlinearSpace.cpp"
 #include "3rd_party/airwindows/NonlinearSpace/NonlinearSpaceProc.cpp"
}
namespace point
{
 #include "3rd_party/airwindows/Point/Point.cpp"
 #include "3rd_party/airwindows/Point/PointProc.cpp"
}
namespace purestdrive
{
 #include "3rd_party/airwindows/PurestDrive/PurestDrive.cpp"
 #include "3rd_party/airwindows/PurestDrive/PurestDriveProc.cpp"
}
namespace purestwarm
{
 #include "3rd_party/airwindows/PurestWarm/PurestWarm.cpp"
 #include "3rd_party/airwindows/PurestWarm/PurestWarmProc.cpp"
}
namespace singleendedtriode
{
 #include "3rd_party/airwindows/SingleEndedTriode/SingleEndedTriode.cpp"
 #include "3rd_party/airwindows/SingleEndedTriode/SingleEndedTriodeProc.cpp"
}
namespace stereofx
{
 #include "3rd_party/airwindows/StereoFX/StereoFX.cpp"
 #include "3rd_party/airwindows/StereoFX/StereoFXProc.cpp"
}
namespace surge
{
 #include "3rd_party/airwindows/Surge/Surge.cpp"
 #include "3rd_party/airwindows/Surge/SurgeProc.cpp"
}
namespace totape5
{
 #include "3rd_party/airwindows/ToTape5/ToTape5.cpp"
 #include "3rd_party/airwindows/ToTape5/ToTape5Proc.cpp"
}
namespace tovinyl4
{
 #include "3rd_party/airwindows/ToVinyl4/ToVinyl4.cpp"
 #include "3rd_party/airwindows/ToVinyl4/ToVinyl4Proc.cpp"
}
namespace tubedesk
{
 #include "3rd_party/airwindows/TubeDesk/TubeDesk.cpp"
 #include "3rd_party/airwindows/TubeDesk/TubeDeskProc.cpp"
}
namespace unbox
{
 #include "3rd_party/airwindows/UnBox/UnBox.cpp"
 #include "3rd_party/airwindows/UnBox/UnBoxProc.cpp"
}
namespace wider
{
 #include "3rd_party/airwindows/Wider/Wider.cpp"
 #include "3rd_party/airwindows/Wider/WiderProc.cpp"
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

