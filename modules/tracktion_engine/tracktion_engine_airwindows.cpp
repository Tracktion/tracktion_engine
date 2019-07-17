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

#include "tracktion_engine.h"

#if TRACKTION_AIR_WINDOWS

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
 #pragma clang diagnostic ignored "-Wuninitialized"
 #pragma clang diagnostic ignored "-Widiomatic-parentheses"
 #pragma clang diagnostic ignored "-Wconditional-uninitialized"
 #pragma clang diagnostic ignored "-Wparentheses"
#endif

#if JUCE_WINDOWS
 #pragma warning (push)
 #pragma warning (disable : 4244 4100 4305 4065 4701 4706 4723)
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
namespace adclip7
{
 #include "3rd_party/airwindows/ADClip7/ADClip7.cpp"
 #include "3rd_party/airwindows/ADClip7/ADClip7Proc.cpp"
}
namespace adt
{
 #include "3rd_party/airwindows/ADT/ADT.cpp"
 #include "3rd_party/airwindows/ADT/ADTProc.cpp"
}
namespace atmospherebuss
{
 #include "3rd_party/airwindows/AtmosphereBuss/AtmosphereBuss.cpp"
 #include "3rd_party/airwindows/AtmosphereBuss/AtmosphereBussProc.cpp"
}
namespace atmospherechannel
{
 #include "3rd_party/airwindows/AtmosphereChannel/AtmosphereChannel.cpp"
 #include "3rd_party/airwindows/AtmosphereChannel/AtmosphereChannelProc.cpp"
}
namespace aura
{
 #include "3rd_party/airwindows/Aura/Aura.cpp"
 #include "3rd_party/airwindows/Aura/AuraProc.cpp"
}
namespace basskit
{
 #include "3rd_party/airwindows/BassKit/BassKit.cpp"
 #include "3rd_party/airwindows/BassKit/BassKitProc.cpp"
}
namespace bitglitter
{
 #include "3rd_party/airwindows/BitGlitter/BitGlitter.cpp"
 #include "3rd_party/airwindows/BitGlitter/BitGlitterProc.cpp"
}
namespace buttercomp
{
 #include "3rd_party/airwindows/ButterComp/ButterComp.cpp"
 #include "3rd_party/airwindows/ButterComp/ButterCompProc.cpp"
}
namespace buttercomp2
{
 #include "3rd_party/airwindows/ButterComp2/ButterComp2.cpp"
 #include "3rd_party/airwindows/ButterComp2/ButterComp2Proc.cpp"
}
namespace channel4
{
 #include "3rd_party/airwindows/Channel4/Channel4.cpp"
 #include "3rd_party/airwindows/Channel4/Channel4Proc.cpp"
}
namespace channel5
{
 #include "3rd_party/airwindows/Channel5/Channel5.cpp"
 #include "3rd_party/airwindows/Channel5/Channel5Proc.cpp"
}
namespace chorusensemble
{
 #include "3rd_party/airwindows/ChorusEnsemble/ChorusEnsemble.cpp"
 #include "3rd_party/airwindows/ChorusEnsemble/ChorusEnsembleProc.cpp"
}
namespace crunchygroovewear
{
 #include "3rd_party/airwindows/CrunchyGrooveWear/CrunchyGrooveWear.cpp"
 #include "3rd_party/airwindows/CrunchyGrooveWear/CrunchyGrooveWearProc.cpp"
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
namespace derez
{
 #include "3rd_party/airwindows/DeRez/DeRez.cpp"
 #include "3rd_party/airwindows/DeRez/DeRezProc.cpp"
}
namespace desk
{
 #include "3rd_party/airwindows/Desk/Desk.cpp"
 #include "3rd_party/airwindows/Desk/DeskProc.cpp"
}
namespace desk4
{
 #include "3rd_party/airwindows/Desk4/Desk4.cpp"
 #include "3rd_party/airwindows/Desk4/Desk4Proc.cpp"
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
namespace drumslam
{
 #include "3rd_party/airwindows/DrumSlam/DrumSlam.cpp"
 #include "3rd_party/airwindows/DrumSlam/DrumSlamProc.cpp"
}
namespace dubsub
{
 #include "3rd_party/airwindows/DubSub/DubSub.cpp"
 #include "3rd_party/airwindows/DubSub/DubSubProc.cpp"
}
namespace edisdim
{
 #include "3rd_party/airwindows/EdIsDim/EdIsDim.cpp"
 #include "3rd_party/airwindows/EdIsDim/EdIsDimProc.cpp"
}
namespace electrohat
{
 #include "3rd_party/airwindows/ElectroHat/ElectroHat.cpp"
 #include "3rd_party/airwindows/ElectroHat/ElectroHatProc.cpp"
}
namespace energy
{
 #include "3rd_party/airwindows/Energy/Energy.cpp"
 #include "3rd_party/airwindows/Energy/EnergyProc.cpp"
}
namespace ensemble
{
 #include "3rd_party/airwindows/Ensemble/Ensemble.cpp"
 #include "3rd_party/airwindows/Ensemble/EnsembleProc.cpp"
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
namespace fromtape
{
 #include "3rd_party/airwindows/FromTape/FromTape.cpp"
 #include "3rd_party/airwindows/FromTape/FromTapeProc.cpp"
}
namespace gatelope
{
 #include "3rd_party/airwindows/Gatelope/Gatelope.cpp"
 #include "3rd_party/airwindows/Gatelope/GatelopeProc.cpp"
}
namespace golem
{
 #include "3rd_party/airwindows/Golem/Golem.cpp"
 #include "3rd_party/airwindows/Golem/GolemProc.cpp"
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
namespace melt
{
 #include "3rd_party/airwindows/Melt/Melt.cpp"
 #include "3rd_party/airwindows/Melt/MeltProc.cpp"
}
namespace midside
{
 #include "3rd_party/airwindows/MidSide/MidSide.cpp"
 #include "3rd_party/airwindows/MidSide/MidSideProc.cpp"
}
namespace nc17
{
 #include "3rd_party/airwindows/NCSeventeen/NCSeventeen.cpp"
 #include "3rd_party/airwindows/NCSeventeen/NCSeventeenProc.cpp"
}
namespace noise
{
 #include "3rd_party/airwindows/Noise/Noise.cpp"
 #include "3rd_party/airwindows/Noise/NoiseProc.cpp"
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
namespace pop
{
 #include "3rd_party/airwindows/Pop/Pop.cpp"
 #include "3rd_party/airwindows/Pop/PopProc.cpp"
}
namespace pressure4
{
 #include "3rd_party/airwindows/Pressure4/Pressure4.cpp"
 #include "3rd_party/airwindows/Pressure4/Pressure4Proc.cpp"
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
namespace righteous4
{
 #include "3rd_party/airwindows/Righteous4/Righteous4.cpp"
 #include "3rd_party/airwindows/Righteous4/Righteous4Proc.cpp"
}
namespace singleendedtriode
{
 #include "3rd_party/airwindows/SingleEndedTriode/SingleEndedTriode.cpp"
 #include "3rd_party/airwindows/SingleEndedTriode/SingleEndedTriodeProc.cpp"
}
namespace slewonly
{
 #include "3rd_party/airwindows/SlewOnly/SlewOnly.cpp"
 #include "3rd_party/airwindows/SlewOnly/SlewOnlyProc.cpp"
}
namespace spiral2
{
 #include "3rd_party/airwindows/Spiral2/Spiral2.cpp"
 #include "3rd_party/airwindows/Spiral2/Spiral2Proc.cpp"
}
namespace starchild
{
 #include "3rd_party/airwindows/StarChild/StarChild.cpp"
 #include "3rd_party/airwindows/StarChild/StarChildProc.cpp"
}
namespace stereofx
{
 #include "3rd_party/airwindows/StereoFX/StereoFX.cpp"
 #include "3rd_party/airwindows/StereoFX/StereoFXProc.cpp"
}
namespace subsonly
{
 #include "3rd_party/airwindows/SubsOnly/SubsOnly.cpp"
 #include "3rd_party/airwindows/SubsOnly/SubsOnlyProc.cpp"
}
namespace surge
{
 #include "3rd_party/airwindows/Surge/Surge.cpp"
 #include "3rd_party/airwindows/Surge/SurgeProc.cpp"
}
namespace swell
{
 #include "3rd_party/airwindows/Swell/Swell.cpp"
 #include "3rd_party/airwindows/Swell/SwellProc.cpp"
}
namespace tapedust
{
 #include "3rd_party/airwindows/TapeDust/TapeDust.cpp"
 #include "3rd_party/airwindows/TapeDust/TapeDustProc.cpp"
}
namespace thunder
{
 #include "3rd_party/airwindows/Thunder/Thunder.cpp"
 #include "3rd_party/airwindows/Thunder/ThunderProc.cpp"
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
namespace varimu
{
 #include "3rd_party/airwindows/VariMu/VariMu.cpp"
 #include "3rd_party/airwindows/VariMu/VariMuProc.cpp"
}
namespace voiceofthestarship
{
 #include "3rd_party/airwindows/VoiceOfTheStarship/VoiceOfTheStarship.cpp"
 #include "3rd_party/airwindows/VoiceOfTheStarship/VoiceOfTheStarshipProc.cpp"
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
