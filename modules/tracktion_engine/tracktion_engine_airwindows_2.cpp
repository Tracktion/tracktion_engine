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
namespace doublepaul
{
 #include "3rd_party/airwindows/DoublePaul/DoublePaul.cpp"
 #include "3rd_party/airwindows/DoublePaul/DoublePaulProc.cpp"
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
namespace dubcenter
{
 #include "3rd_party/airwindows/DubCenter/DubCenter.cpp"
 #include "3rd_party/airwindows/DubCenter/DubCenterProc.cpp"
}
namespace dubsub
{
 #include "3rd_party/airwindows/DubSub/DubSub.cpp"
 #include "3rd_party/airwindows/DubSub/DubSubProc.cpp"
}
namespace dustbunny
{
 #include "3rd_party/airwindows/DustBunny/DustBunny.cpp"
 #include "3rd_party/airwindows/DustBunny/DustBunnyProc.cpp"
}
namespace dyno
{
 #include "3rd_party/airwindows/Dyno/Dyno.cpp"
 #include "3rd_party/airwindows/Dyno/DynoProc.cpp"
}
namespace eq
{
 #include "3rd_party/airwindows/EQ/EQ.cpp"
 #include "3rd_party/airwindows/EQ/EQProc.cpp"
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
namespace everytrim
{
 #include "3rd_party/airwindows/EveryTrim/EveryTrim.cpp"
 #include "3rd_party/airwindows/EveryTrim/EveryTrimProc.cpp"
}
namespace facet
{
 #include "3rd_party/airwindows/Facet/Facet.cpp"
 #include "3rd_party/airwindows/Facet/FacetProc.cpp"
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
namespace focus
{
 #include "3rd_party/airwindows/Focus/Focus.cpp"
 #include "3rd_party/airwindows/Focus/FocusProc.cpp"
}
namespace fracture
{
 #include "3rd_party/airwindows/Fracture/Fracture.cpp"
 #include "3rd_party/airwindows/Fracture/FractureProc.cpp"
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
namespace gringer
{
 #include "3rd_party/airwindows/Gringer/Gringer.cpp"
 #include "3rd_party/airwindows/Gringer/GringerProc.cpp"
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
namespace hermetrim
{
 #include "3rd_party/airwindows/HermeTrim/HermeTrim.cpp"
 #include "3rd_party/airwindows/HermeTrim/HermeTrimProc.cpp"
}
namespace hermepass
{
 #include "3rd_party/airwindows/Hermepass/Hermepass.cpp"
 #include "3rd_party/airwindows/Hermepass/HermepassProc.cpp"
}
namespace highglossdither
{
 #include "3rd_party/airwindows/HighGlossDither/HighGlossDither.cpp"
 #include "3rd_party/airwindows/HighGlossDither/HighGlossDitherProc.cpp"
}
namespace highimpact
{
 #include "3rd_party/airwindows/HighImpact/HighImpact.cpp"
 #include "3rd_party/airwindows/HighImpact/HighImpactProc.cpp"
}
namespace highpass
{
 #include "3rd_party/airwindows/Highpass/Highpass.cpp"
 #include "3rd_party/airwindows/Highpass/HighpassProc.cpp"
}
namespace highpass2
{
 #include "3rd_party/airwindows/Highpass2/Highpass2.cpp"
 #include "3rd_party/airwindows/Highpass2/Highpass2Proc.cpp"
}
namespace holt
{
 #include "3rd_party/airwindows/Holt/Holt.cpp"
 #include "3rd_party/airwindows/Holt/HoltProc.cpp"
}
namespace hombre
{
 #include "3rd_party/airwindows/Hombre/Hombre.cpp"
 #include "3rd_party/airwindows/Hombre/HombreProc.cpp"
}
namespace interstage
{
 #include "3rd_party/airwindows/Interstage/Interstage.cpp"
 #include "3rd_party/airwindows/Interstage/InterstageProc.cpp"
}
namespace ironoxide5
{
 #include "3rd_party/airwindows/IronOxide5/IronOxide5.cpp"
 #include "3rd_party/airwindows/IronOxide5/IronOxide5Proc.cpp"
}
namespace ironoxideclassic
{
 #include "3rd_party/airwindows/IronOxideClassic/IronOxideClassic.cpp"
 #include "3rd_party/airwindows/IronOxideClassic/IronOxideClassicProc.cpp"
}
namespace leftomono
{
 #include "3rd_party/airwindows/LeftoMono/LeftoMono.cpp"
 #include "3rd_party/airwindows/LeftoMono/LeftoMonoProc.cpp"
}
namespace logical4
{
 #include "3rd_party/airwindows/Logical4/Logical4.cpp"
 #include "3rd_party/airwindows/Logical4/Logical4Proc.cpp"
}
namespace loud
{
 #include "3rd_party/airwindows/Loud/Loud.cpp"
 #include "3rd_party/airwindows/Loud/LoudProc.cpp"
}
namespace lowpass
{
 #include "3rd_party/airwindows/Lowpass/Lowpass.cpp"
 #include "3rd_party/airwindows/Lowpass/LowpassProc.cpp"
}
namespace lowpass2
{
 #include "3rd_party/airwindows/Lowpass2/Lowpass2.cpp"
 #include "3rd_party/airwindows/Lowpass2/Lowpass2Proc.cpp"
}
namespace mv
{
 #include "3rd_party/airwindows/MV/MV.cpp"
 #include "3rd_party/airwindows/MV/MVProc.cpp"
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
namespace monoam
{
 #include "3rd_party/airwindows/MoNoam/MoNoam.cpp"
 #include "3rd_party/airwindows/MoNoam/MoNoamProc.cpp"
}
namespace mojo
{
 #include "3rd_party/airwindows/Mojo/Mojo.cpp"
 #include "3rd_party/airwindows/Mojo/MojoProc.cpp"
}
namespace monitoring
{
 #include "3rd_party/airwindows/Monitoring/Monitoring.cpp"
 #include "3rd_party/airwindows/Monitoring/MonitoringProc.cpp"
}
namespace ncseventeen
{
 #include "3rd_party/airwindows/NCSeventeen/NCSeventeen.cpp"
 #include "3rd_party/airwindows/NCSeventeen/NCSeventeenProc.cpp"
}
namespace naturalizedither
{
 #include "3rd_party/airwindows/NaturalizeDither/NaturalizeDither.cpp"
 #include "3rd_party/airwindows/NaturalizeDither/NaturalizeDitherProc.cpp"
}
namespace nodedither
{
 #include "3rd_party/airwindows/NodeDither/NodeDither.cpp"
 #include "3rd_party/airwindows/NodeDither/NodeDitherProc.cpp"
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
namespace notjustanothercd
{
 #include "3rd_party/airwindows/NotJustAnotherCD/NotJustAnotherCD.cpp"
 #include "3rd_party/airwindows/NotJustAnotherCD/NotJustAnotherCDProc.cpp"
}
namespace notjustanotherdither
{
 #include "3rd_party/airwindows/NotJustAnotherDither/NotJustAnotherDither.cpp"
 #include "3rd_party/airwindows/NotJustAnotherDither/NotJustAnotherDitherProc.cpp"
}
namespace onecornerclip
{
 #include "3rd_party/airwindows/OneCornerClip/OneCornerClip.cpp"
 #include "3rd_party/airwindows/OneCornerClip/OneCornerClipProc.cpp"
}
namespace pdbuss
{
 #include "3rd_party/airwindows/PDBuss/PDBuss.cpp"
 #include "3rd_party/airwindows/PDBuss/PDBussProc.cpp"
}
namespace pdchannel
{
 #include "3rd_party/airwindows/PDChannel/PDChannel.cpp"
 #include "3rd_party/airwindows/PDChannel/PDChannelProc.cpp"
}
namespace pafnuty
{
 #include "3rd_party/airwindows/Pafnuty/Pafnuty.cpp"
 #include "3rd_party/airwindows/Pafnuty/PafnutyProc.cpp"
}
namespace pauldither
{
 #include "3rd_party/airwindows/PaulDither/PaulDither.cpp"
 #include "3rd_party/airwindows/PaulDither/PaulDitherProc.cpp"
}
namespace peaksonly
{
 #include "3rd_party/airwindows/PeaksOnly/PeaksOnly.cpp"
 #include "3rd_party/airwindows/PeaksOnly/PeaksOnlyProc.cpp"
}
namespace phasenudge
{
 #include "3rd_party/airwindows/PhaseNudge/PhaseNudge.cpp"
 #include "3rd_party/airwindows/PhaseNudge/PhaseNudgeProc.cpp"
}
namespace pocketverbs
{
 #include "3rd_party/airwindows/PocketVerbs/PocketVerbs.cpp"
 #include "3rd_party/airwindows/PocketVerbs/PocketVerbsProc.cpp"
}
namespace podcast
{
 #include "3rd_party/airwindows/Podcast/Podcast.cpp"
 #include "3rd_party/airwindows/Podcast/PodcastProc.cpp"
}
namespace podcastdeluxe
{
 #include "3rd_party/airwindows/PodcastDeluxe/PodcastDeluxe.cpp"
 #include "3rd_party/airwindows/PodcastDeluxe/PodcastDeluxeProc.cpp"
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
namespace powersag
{
 #include "3rd_party/airwindows/PowerSag/PowerSag.cpp"
 #include "3rd_party/airwindows/PowerSag/PowerSagProc.cpp"
}
namespace powersag2
{
 #include "3rd_party/airwindows/PowerSag2/PowerSag2.cpp"
 #include "3rd_party/airwindows/PowerSag2/PowerSag2Proc.cpp"
}
namespace pressure4
{
 #include "3rd_party/airwindows/Pressure4/Pressure4.cpp"
 #include "3rd_party/airwindows/Pressure4/Pressure4Proc.cpp"
}
}
}} // namespace tracktion { inline namespace engine


#if JUCE_CLANG
 #pragma clang diagnostic pop
#endif

#if JUCE_WINDOWS
 #pragma warning (pop)
#endif

#include "plugins/airwindows/tracktion_AirWindows2.cpp"

#endif
#endif
