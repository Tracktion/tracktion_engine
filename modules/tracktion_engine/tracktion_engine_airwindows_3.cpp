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

#include "plugins/airwindows/tracktion_AirWindowsBase.h"

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

namespace tracktion { inline namespace engine
{
namespace airwindows
{
namespace purestair
{
 #include "3rd_party/airwindows/PurestAir/PurestAir.cpp"
 #include "3rd_party/airwindows/PurestAir/PurestAirProc.cpp"
}
namespace purestconsolebuss
{
 #include "3rd_party/airwindows/PurestConsoleBuss/PurestConsoleBuss.cpp"
 #include "3rd_party/airwindows/PurestConsoleBuss/PurestConsoleBussProc.cpp"
}
namespace purestconsolechannel
{
 #include "3rd_party/airwindows/PurestConsoleChannel/PurestConsoleChannel.cpp"
 #include "3rd_party/airwindows/PurestConsoleChannel/PurestConsoleChannelProc.cpp"
}
namespace purestdrive
{
 #include "3rd_party/airwindows/PurestDrive/PurestDrive.cpp"
 #include "3rd_party/airwindows/PurestDrive/PurestDriveProc.cpp"
}
namespace purestecho
{
 #include "3rd_party/airwindows/PurestEcho/PurestEcho.cpp"
 #include "3rd_party/airwindows/PurestEcho/PurestEchoProc.cpp"
}
namespace purestgain
{
 #include "3rd_party/airwindows/PurestGain/PurestGain.cpp"
 #include "3rd_party/airwindows/PurestGain/PurestGainProc.cpp"
}
namespace purestsquish
{
 #include "3rd_party/airwindows/PurestSquish/PurestSquish.cpp"
 #include "3rd_party/airwindows/PurestSquish/PurestSquishProc.cpp"
}
namespace purestwarm
{
 #include "3rd_party/airwindows/PurestWarm/PurestWarm.cpp"
 #include "3rd_party/airwindows/PurestWarm/PurestWarmProc.cpp"
}
namespace pyewacket
{
 #include "3rd_party/airwindows/Pyewacket/Pyewacket.cpp"
 #include "3rd_party/airwindows/Pyewacket/PyewacketProc.cpp"
}
namespace rawglitters
{
 #include "3rd_party/airwindows/RawGlitters/RawGlitters.cpp"
 #include "3rd_party/airwindows/RawGlitters/RawGlittersProc.cpp"
}
namespace rawtimbers
{
 #include "3rd_party/airwindows/RawTimbers/RawTimbers.cpp"
 #include "3rd_party/airwindows/RawTimbers/RawTimbersProc.cpp"
}
namespace recurve
{
 #include "3rd_party/airwindows/Recurve/Recurve.cpp"
 #include "3rd_party/airwindows/Recurve/RecurveProc.cpp"
}
namespace remap
{
 #include "3rd_party/airwindows/Remap/Remap.cpp"
 #include "3rd_party/airwindows/Remap/RemapProc.cpp"
}
namespace reseq
{
 #include "3rd_party/airwindows/ResEQ/ResEQ.cpp"
 #include "3rd_party/airwindows/ResEQ/ResEQProc.cpp"
}
namespace righteous4
{
 #include "3rd_party/airwindows/Righteous4/Righteous4.cpp"
 #include "3rd_party/airwindows/Righteous4/Righteous4Proc.cpp"
}
namespace rightomono
{
 #include "3rd_party/airwindows/RightoMono/RightoMono.cpp"
 #include "3rd_party/airwindows/RightoMono/RightoMonoProc.cpp"
}
namespace sidedull
{
 #include "3rd_party/airwindows/SideDull/SideDull.cpp"
 #include "3rd_party/airwindows/SideDull/SideDullProc.cpp"
}
namespace sidepass
{
 #include "3rd_party/airwindows/Sidepass/Sidepass.cpp"
 #include "3rd_party/airwindows/Sidepass/SidepassProc.cpp"
}
namespace singleendedtriode
{
 #include "3rd_party/airwindows/SingleEndedTriode/SingleEndedTriode.cpp"
 #include "3rd_party/airwindows/SingleEndedTriode/SingleEndedTriodeProc.cpp"
}
namespace slew
{
 #include "3rd_party/airwindows/Slew/Slew.cpp"
 #include "3rd_party/airwindows/Slew/SlewProc.cpp"
}
namespace slew2
{
 #include "3rd_party/airwindows/Slew2/Slew2.cpp"
 #include "3rd_party/airwindows/Slew2/Slew2Proc.cpp"
}
namespace slewonly
{
 #include "3rd_party/airwindows/SlewOnly/SlewOnly.cpp"
 #include "3rd_party/airwindows/SlewOnly/SlewOnlyProc.cpp"
}
namespace smooth
{
 #include "3rd_party/airwindows/Smooth/Smooth.cpp"
 #include "3rd_party/airwindows/Smooth/SmoothProc.cpp"
}
namespace softgate
{
 #include "3rd_party/airwindows/SoftGate/SoftGate.cpp"
 #include "3rd_party/airwindows/SoftGate/SoftGateProc.cpp"
}
namespace spatializedither
{
 #include "3rd_party/airwindows/SpatializeDither/SpatializeDither.cpp"
 #include "3rd_party/airwindows/SpatializeDither/SpatializeDitherProc.cpp"
}
namespace spiral
{
 #include "3rd_party/airwindows/Spiral/Spiral.cpp"
 #include "3rd_party/airwindows/Spiral/SpiralProc.cpp"
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
namespace studiotan
{
 #include "3rd_party/airwindows/StudioTan/StudioTan.cpp"
 #include "3rd_party/airwindows/StudioTan/StudioTanProc.cpp"
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
namespace surgetide
{
 #include "3rd_party/airwindows/SurgeTide/SurgeTide.cpp"
 #include "3rd_party/airwindows/SurgeTide/SurgeTideProc.cpp"
}
namespace swell
{
 #include "3rd_party/airwindows/Swell/Swell.cpp"
 #include "3rd_party/airwindows/Swell/SwellProc.cpp"
}
namespace tpdfdither
{
 #include "3rd_party/airwindows/TPDFDither/TPDFDither.cpp"
 #include "3rd_party/airwindows/TPDFDither/TPDFDitherProc.cpp"
}
namespace tapedelay
{
 #include "3rd_party/airwindows/TapeDelay/TapeDelay.cpp"
 #include "3rd_party/airwindows/TapeDelay/TapeDelayProc.cpp"
}
namespace tapedither
{
 #include "3rd_party/airwindows/TapeDither/TapeDither.cpp"
 #include "3rd_party/airwindows/TapeDither/TapeDitherProc.cpp"
}
namespace tapedust
{
 #include "3rd_party/airwindows/TapeDust/TapeDust.cpp"
 #include "3rd_party/airwindows/TapeDust/TapeDustProc.cpp"
}
namespace tapefat
{
 #include "3rd_party/airwindows/TapeFat/TapeFat.cpp"
 #include "3rd_party/airwindows/TapeFat/TapeFatProc.cpp"
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
namespace toneslant
{
 #include "3rd_party/airwindows/ToneSlant/ToneSlant.cpp"
 #include "3rd_party/airwindows/ToneSlant/ToneSlantProc.cpp"
}
namespace transdesk
{
 #include "3rd_party/airwindows/TransDesk/TransDesk.cpp"
 #include "3rd_party/airwindows/TransDesk/TransDeskProc.cpp"
}
namespace tremolo
{
 #include "3rd_party/airwindows/Tremolo/Tremolo.cpp"
 #include "3rd_party/airwindows/Tremolo/TremoloProc.cpp"
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
namespace vibrato
{
 #include "3rd_party/airwindows/Vibrato/Vibrato.cpp"
 #include "3rd_party/airwindows/Vibrato/VibratoProc.cpp"
}
namespace vinyldither
{
 #include "3rd_party/airwindows/VinylDither/VinylDither.cpp"
 #include "3rd_party/airwindows/VinylDither/VinylDitherProc.cpp"
}
namespace voiceofthestarship
{
 #include "3rd_party/airwindows/VoiceOfTheStarship/VoiceOfTheStarship.cpp"
 #include "3rd_party/airwindows/VoiceOfTheStarship/VoiceOfTheStarshipProc.cpp"
}
namespace voicetrick
{
 #include "3rd_party/airwindows/VoiceTrick/VoiceTrick.cpp"
 #include "3rd_party/airwindows/VoiceTrick/VoiceTrickProc.cpp"
}
namespace wider
{
 #include "3rd_party/airwindows/Wider/Wider.cpp"
 #include "3rd_party/airwindows/Wider/WiderProc.cpp"
}
namespace curve
{
 #include "3rd_party/airwindows/curve/curve.cpp"
 #include "3rd_party/airwindows/curve/curveProc.cpp"
}
namespace ulawdecode
{
 #include "3rd_party/airwindows/uLawDecode/uLawDecode.cpp"
 #include "3rd_party/airwindows/uLawDecode/uLawDecodeProc.cpp"
}
namespace ulawencode
{
 #include "3rd_party/airwindows/uLawEncode/uLawEncode.cpp"
 #include "3rd_party/airwindows/uLawEncode/uLawEncodeProc.cpp"
}

}
}} // namespace tracktion { inline namespace engine


#if JUCE_CLANG
 #pragma clang diagnostic pop
#endif

#if JUCE_WINDOWS
 #pragma warning (pop)
#endif

#include "plugins/airwindows/tracktion_AirWindows3.cpp"

#endif
#endif
