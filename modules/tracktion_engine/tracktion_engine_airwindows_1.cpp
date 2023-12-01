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
}
}} // namespace tracktion { inline namespace engine


#if JUCE_CLANG
 #pragma clang diagnostic pop
#endif

#if JUCE_WINDOWS
 #pragma warning (pop)
#endif

#include "plugins/airwindows/tracktion_AirWindows1.cpp"

#endif
#endif
