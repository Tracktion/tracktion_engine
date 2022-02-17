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

namespace tracktion { inline namespace engine
{
namespace airwindows
{
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
namespace aquickvoiceclip
{
 #include "3rd_party/airwindows/AQuickVoiceClip/AQuickVoiceClip.cpp"
 #include "3rd_party/airwindows/AQuickVoiceClip/AQuickVoiceClipProc.cpp"
}
namespace acceleration
{
 #include "3rd_party/airwindows/Acceleration/Acceleration.cpp"
 #include "3rd_party/airwindows/Acceleration/AccelerationProc.cpp"
}
namespace air
{
 #include "3rd_party/airwindows/Air/Air.cpp"
 #include "3rd_party/airwindows/Air/AirProc.cpp"
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
namespace average
{
 #include "3rd_party/airwindows/Average/Average.cpp"
 #include "3rd_party/airwindows/Average/AverageProc.cpp"
}
namespace bassdrive
{
 #include "3rd_party/airwindows/BassDrive/BassDrive.cpp"
 #include "3rd_party/airwindows/BassDrive/BassDriveProc.cpp"
}
namespace basskit
{
 #include "3rd_party/airwindows/BassKit/BassKit.cpp"
 #include "3rd_party/airwindows/BassKit/BassKitProc.cpp"
}
namespace biquad
{
 #include "3rd_party/airwindows/Biquad/Biquad.cpp"
 #include "3rd_party/airwindows/Biquad/BiquadProc.cpp"
}
namespace biquad2
{
 #include "3rd_party/airwindows/Biquad2/Biquad2.cpp"
 #include "3rd_party/airwindows/Biquad2/Biquad2Proc.cpp"
}
namespace bitglitter
{
 #include "3rd_party/airwindows/BitGlitter/BitGlitter.cpp"
 #include "3rd_party/airwindows/BitGlitter/BitGlitterProc.cpp"
}
namespace bitshiftgain
{
 #include "3rd_party/airwindows/BitShiftGain/BitShiftGain.cpp"
 #include "3rd_party/airwindows/BitShiftGain/BitShiftGainProc.cpp"
}
namespace bite
{
 #include "3rd_party/airwindows/Bite/Bite.cpp"
 #include "3rd_party/airwindows/Bite/BiteProc.cpp"
}
namespace blockparty
{
 #include "3rd_party/airwindows/BlockParty/BlockParty.cpp"
 #include "3rd_party/airwindows/BlockParty/BlockPartyProc.cpp"
}
namespace brassrider
{
 #include "3rd_party/airwindows/BrassRider/BrassRider.cpp"
 #include "3rd_party/airwindows/BrassRider/BrassRiderProc.cpp"
}
namespace buildatpdf
{
 #include "3rd_party/airwindows/BuildATPDF/BuildATPDF.cpp"
 #include "3rd_party/airwindows/BuildATPDF/BuildATPDFProc.cpp"
}
namespace busscolors4
{
 #include "3rd_party/airwindows/BussColors4/BussColors4.cpp"
 #include "3rd_party/airwindows/BussColors4/BussColors4Proc.cpp"
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
namespace c5rawbuss
{
 #include "3rd_party/airwindows/C5RawBuss/C5RawBuss.cpp"
 #include "3rd_party/airwindows/C5RawBuss/C5RawBussProc.cpp"
}
namespace c5rawchannel
{
 #include "3rd_party/airwindows/C5RawChannel/C5RawChannel.cpp"
 #include "3rd_party/airwindows/C5RawChannel/C5RawChannelProc.cpp"
}
namespace cstrip
{
 #include "3rd_party/airwindows/CStrip/CStrip.cpp"
 #include "3rd_party/airwindows/CStrip/CStripProc.cpp"
}
namespace capacitor
{
 #include "3rd_party/airwindows/Capacitor/Capacitor.cpp"
 #include "3rd_party/airwindows/Capacitor/CapacitorProc.cpp"
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
namespace channel6
{
 #include "3rd_party/airwindows/Channel6/Channel6.cpp"
 #include "3rd_party/airwindows/Channel6/Channel6Proc.cpp"
}
namespace channel7
{
 #include "3rd_party/airwindows/Channel7/Channel7.cpp"
 #include "3rd_party/airwindows/Channel7/Channel7Proc.cpp"
}
namespace chorus
{
 #include "3rd_party/airwindows/Chorus/Chorus.cpp"
 #include "3rd_party/airwindows/Chorus/ChorusProc.cpp"
}
namespace chorusensemble
{
 #include "3rd_party/airwindows/ChorusEnsemble/ChorusEnsemble.cpp"
 #include "3rd_party/airwindows/ChorusEnsemble/ChorusEnsembleProc.cpp"
}
namespace cliponly
{
 #include "3rd_party/airwindows/ClipOnly/ClipOnly.cpp"
 #include "3rd_party/airwindows/ClipOnly/ClipOnlyProc.cpp"
}
namespace coils
{
 #include "3rd_party/airwindows/Coils/Coils.cpp"
 #include "3rd_party/airwindows/Coils/CoilsProc.cpp"
}
namespace cojones
{
 #include "3rd_party/airwindows/Cojones/Cojones.cpp"
 #include "3rd_party/airwindows/Cojones/CojonesProc.cpp"
}
namespace compresaturator
{
 #include "3rd_party/airwindows/Compresaturator/Compresaturator.cpp"
 #include "3rd_party/airwindows/Compresaturator/CompresaturatorProc.cpp"
}
namespace console4buss
{
 #include "3rd_party/airwindows/Console4Buss/Console4Buss.cpp"
 #include "3rd_party/airwindows/Console4Buss/Console4BussProc.cpp"
}
namespace console4channel
{
 #include "3rd_party/airwindows/Console4Channel/Console4Channel.cpp"
 #include "3rd_party/airwindows/Console4Channel/Console4ChannelProc.cpp"
}
namespace console5buss
{
 #include "3rd_party/airwindows/Console5Buss/Console5Buss.cpp"
 #include "3rd_party/airwindows/Console5Buss/Console5BussProc.cpp"
}
namespace console5channel
{
 #include "3rd_party/airwindows/Console5Channel/Console5Channel.cpp"
 #include "3rd_party/airwindows/Console5Channel/Console5ChannelProc.cpp"
}
namespace console5darkch
{
 #include "3rd_party/airwindows/Console5DarkCh/Console5DarkCh.cpp"
 #include "3rd_party/airwindows/Console5DarkCh/Console5DarkChProc.cpp"
}
namespace console6buss
{
 #include "3rd_party/airwindows/Console6Buss/Console6Buss.cpp"
 #include "3rd_party/airwindows/Console6Buss/Console6BussProc.cpp"
}
namespace console6channel
{
 #include "3rd_party/airwindows/Console6Channel/Console6Channel.cpp"
 #include "3rd_party/airwindows/Console6Channel/Console6ChannelProc.cpp"
}
namespace crunchygroovewear
{
 #include "3rd_party/airwindows/CrunchyGrooveWear/CrunchyGrooveWear.cpp"
 #include "3rd_party/airwindows/CrunchyGrooveWear/CrunchyGrooveWearProc.cpp"
}
namespace crystal
{
 #include "3rd_party/airwindows/Crystal/Crystal.cpp"
 #include "3rd_party/airwindows/Crystal/CrystalProc.cpp"
}
namespace dcvoltage
{
 #include "3rd_party/airwindows/DCVoltage/DCVoltage.cpp"
 #include "3rd_party/airwindows/DCVoltage/DCVoltageProc.cpp"
}
namespace debess
{
 #include "3rd_party/airwindows/DeBess/DeBess.cpp"
 #include "3rd_party/airwindows/DeBess/DeBessProc.cpp"
}
namespace deess
{
 #include "3rd_party/airwindows/DeEss/DeEss.cpp"
 #include "3rd_party/airwindows/DeEss/DeEssProc.cpp"
}
namespace dehiss
{
 #include "3rd_party/airwindows/DeHiss/DeHiss.cpp"
 #include "3rd_party/airwindows/DeHiss/DeHissProc.cpp"
}
namespace derez
{
 #include "3rd_party/airwindows/DeRez/DeRez.cpp"
 #include "3rd_party/airwindows/DeRez/DeRezProc.cpp"
}
namespace derez2
{
 #include "3rd_party/airwindows/DeRez2/DeRez2.cpp"
 #include "3rd_party/airwindows/DeRez2/DeRez2Proc.cpp"
}
namespace deckwrecka
{
 #include "3rd_party/airwindows/Deckwrecka/Deckwrecka.cpp"
 #include "3rd_party/airwindows/Deckwrecka/DeckwreckaProc.cpp"
}
namespace density
{
 #include "3rd_party/airwindows/Density/Density.cpp"
 #include "3rd_party/airwindows/Density/DensityProc.cpp"
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
namespace distance
{
 #include "3rd_party/airwindows/Distance/Distance.cpp"
 #include "3rd_party/airwindows/Distance/DistanceProc.cpp"
}
namespace distance2
{
 #include "3rd_party/airwindows/Distance2/Distance2.cpp"
 #include "3rd_party/airwindows/Distance2/Distance2Proc.cpp"
}
namespace ditherfloat
{
 #include "3rd_party/airwindows/DitherFloat/DitherFloat.cpp"
 #include "3rd_party/airwindows/DitherFloat/DitherFloatProc.cpp"
}
namespace dithermediskers
{
 #include "3rd_party/airwindows/DitherMeDiskers/DitherMeDiskers.cpp"
 #include "3rd_party/airwindows/DitherMeDiskers/DitherMeDiskersProc.cpp"
}
namespace dithermetimbers
{
 #include "3rd_party/airwindows/DitherMeTimbers/DitherMeTimbers.cpp"
 #include "3rd_party/airwindows/DitherMeTimbers/DitherMeTimbersProc.cpp"
}
namespace ditherbox
{
 #include "3rd_party/airwindows/Ditherbox/Ditherbox.cpp"
 #include "3rd_party/airwindows/Ditherbox/DitherboxProc.cpp"
}
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

#include "plugins/airwindows/tracktion_AirWindows.cpp"

#endif
#endif
