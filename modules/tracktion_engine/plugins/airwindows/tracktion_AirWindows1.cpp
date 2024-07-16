/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion { inline namespace engine
{

//==============================================================================
const char* AirWindowsADClip7::xmlTypeName = "airwindows_adclip7";
const char* AirWindowsADT::xmlTypeName = "airwindows_adt";
const char* AirWindowsAQuickVoiceClip::xmlTypeName = "airwindows_aquickvoiceclip";
const char* AirWindowsAcceleration::xmlTypeName = "airwindows_acceleration";
const char* AirWindowsAir::xmlTypeName = "airwindows_air";
const char* AirWindowsAtmosphereBuss::xmlTypeName = "airwindows_atmosphere_buss";
const char* AirWindowsAtmosphereChannel::xmlTypeName = "airwindows_atmosphere";
const char* AirWindowsAura::xmlTypeName = "airwindows_aura";
const char* AirWindowsAverage::xmlTypeName = "airwindows_average";
const char* AirWindowsBassDrive::xmlTypeName = "airwindows_bassdrive";
const char* AirWindowsBassKit::xmlTypeName = "airwindows_basskit";
const char* AirWindowsBiquad::xmlTypeName = "airwindows_biquad";
const char* AirWindowsBiquad2::xmlTypeName = "airwindows_biquad2";
const char* AirWindowsBitGlitter::xmlTypeName = "airwindows_glitter";
const char* AirWindowsBitShiftGain::xmlTypeName = "airwindows_bitshiftgain";
const char* AirWindowsBite::xmlTypeName = "airwindows_bite";
const char* AirWindowsBlockParty::xmlTypeName = "airwindows_blockparty";
const char* AirWindowsBrassRider::xmlTypeName = "airwindows_brassrider";
const char* AirWindowsBuildATPDF::xmlTypeName = "airwindows_buildatpdf";
const char* AirWindowsBussColors4::xmlTypeName = "airwindows_busscolors4";
const char* AirWindowsButterComp::xmlTypeName = "airwindows_buttercomp";
const char* AirWindowsButterComp2::xmlTypeName = "airwindows_buttercomp2";
const char* AirWindowsC5RawBuss::xmlTypeName = "airwindows_c5rawbuss";
const char* AirWindowsC5RawChannel::xmlTypeName = "airwindows_c5rawchannel";
const char* AirWindowsCStrip::xmlTypeName = "airwindows_cstrip";
const char* AirWindowsCapacitor::xmlTypeName = "airwindows_capacitor";
const char* AirWindowsChannel4::xmlTypeName = "airwindows_channel4";
const char* AirWindowsChannel5::xmlTypeName = "airwindows_channel5";
const char* AirWindowsChannel6::xmlTypeName = "airwindows_channel6";
const char* AirWindowsChannel7::xmlTypeName = "airwindows_channel7";
const char* AirWindowsChorus::xmlTypeName = "airwindows_chorus";
const char* AirWindowsChorusEnsemble::xmlTypeName = "airwindows_chorusensamble";
const char* AirWindowsClipOnly::xmlTypeName = "airwindows_cliponly";
const char* AirWindowsCoils::xmlTypeName = "airwindows_coils";
const char* AirWindowsCojones::xmlTypeName = "airwindows_cojones";
const char* AirWindowsCompresaturator::xmlTypeName = "airwindows_compresaturator";
const char* AirWindowsConsole4Buss::xmlTypeName = "airwindows_console4buss";
const char* AirWindowsConsole4Channel::xmlTypeName = "airwindows_console4channel";
const char* AirWindowsConsole5Buss::xmlTypeName = "airwindows_console5buss";
const char* AirWindowsConsole5Channel::xmlTypeName = "airwindows_console5channel";
const char* AirWindowsConsole5DarkCh::xmlTypeName = "airwindows_console5darkch";
const char* AirWindowsConsole6Buss::xmlTypeName = "airwindows_console6buss";
const char* AirWindowsConsole6Channel::xmlTypeName = "airwindows_console6channel";
const char* AirWindowsCrunchyGrooveWear::xmlTypeName = "airwindows_crunchygroovewear";
const char* AirWindowsCrystal::xmlTypeName = "airwindows_crystal";
const char* AirWindowsDCVoltage::xmlTypeName = "airwindows_dcvoltage";
const char* AirWindowsDeBess::xmlTypeName = "airwindows_debess";
const char* AirWindowsDeEss::xmlTypeName = "airwindows_deess";
const char* AirWindowsDeHiss::xmlTypeName = "airwindows_dehiss";
const char* AirWindowsDeRez::xmlTypeName = "airwindows_derez";
const char* AirWindowsDeRez2::xmlTypeName = "airwindows_derez2";
const char* AirWindowsDeckwrecka::xmlTypeName = "airwindows_deckwrecka";
const char* AirWindowsDensity::xmlTypeName = "airwindows_density";
const char* AirWindowsDesk::xmlTypeName = "airwindows_desk";
const char* AirWindowsDesk4::xmlTypeName = "airwindows_desk4";
const char* AirWindowsDistance::xmlTypeName = "airwindows_distance";
const char* AirWindowsDistance2::xmlTypeName = "airwindows_distance2";
const char* AirWindowsDitherFloat::xmlTypeName = "airwindows_ditherfloat";
const char* AirWindowsDitherMeDiskers::xmlTypeName = "airwindows_dithermediskers";
const char* AirWindowsDitherMeTimbers::xmlTypeName = "airwindows_dithermetimbers";
const char* AirWindowsDitherbox::xmlTypeName = "airwindows_ditherbox";

AirWindowsADClip7::Type AirWindowsADClip7::pluginType = AirWindowsPlugin::dynamics;
AirWindowsADT::Type AirWindowsADT::pluginType = AirWindowsPlugin::delay;
AirWindowsAQuickVoiceClip::Type AirWindowsAQuickVoiceClip::pluginType = AirWindowsPlugin::dynamics;
AirWindowsAcceleration::Type AirWindowsAcceleration::pluginType = AirWindowsPlugin::filter;
AirWindowsAir::Type AirWindowsAir::pluginType = AirWindowsPlugin::eq;
AirWindowsAtmosphereBuss::Type AirWindowsAtmosphereBuss::pluginType = AirWindowsPlugin::emulation;
AirWindowsAtmosphereChannel::Type AirWindowsAtmosphereChannel::pluginType = AirWindowsPlugin::emulation;
AirWindowsAura::Type AirWindowsAura::pluginType = AirWindowsPlugin::eq;
AirWindowsAverage::Type AirWindowsAverage::pluginType = AirWindowsPlugin::filter;
AirWindowsBassDrive::Type AirWindowsBassDrive::pluginType = AirWindowsPlugin::distortion;
AirWindowsBassKit::Type AirWindowsBassKit::pluginType = AirWindowsPlugin::utility;
AirWindowsBiquad2::Type AirWindowsBiquad2::pluginType = AirWindowsPlugin::filter;
AirWindowsBiquad::Type AirWindowsBiquad::pluginType = AirWindowsPlugin::filter;
AirWindowsBitGlitter::Type AirWindowsBitGlitter::pluginType = AirWindowsPlugin::distortion;
AirWindowsBitShiftGain::Type AirWindowsBitShiftGain::pluginType = AirWindowsPlugin::utility;
AirWindowsBite::Type AirWindowsBite::pluginType = AirWindowsPlugin::utility;
AirWindowsBlockParty::Type AirWindowsBlockParty::pluginType = AirWindowsPlugin::dynamics;
AirWindowsBrassRider::Type AirWindowsBrassRider::pluginType = AirWindowsPlugin::utility;
AirWindowsBuildATPDF::Type AirWindowsBuildATPDF::pluginType = AirWindowsPlugin::dither;
AirWindowsBussColors4::Type AirWindowsBussColors4::pluginType = AirWindowsPlugin::emulation;
AirWindowsButterComp2::Type AirWindowsButterComp2::pluginType = AirWindowsPlugin::dynamics;
AirWindowsButterComp::Type AirWindowsButterComp::pluginType = AirWindowsPlugin::dynamics;
AirWindowsC5RawBuss::Type AirWindowsC5RawBuss::pluginType = AirWindowsPlugin::emulation;
AirWindowsC5RawChannel::Type AirWindowsC5RawChannel::pluginType = AirWindowsPlugin::emulation;
AirWindowsCStrip::Type AirWindowsCStrip::pluginType = AirWindowsPlugin::utility;
AirWindowsCapacitor::Type AirWindowsCapacitor::pluginType = AirWindowsPlugin::filter;
AirWindowsChannel4::Type AirWindowsChannel4::pluginType = AirWindowsPlugin::emulation;
AirWindowsChannel5::Type AirWindowsChannel5::pluginType = AirWindowsPlugin::emulation;
AirWindowsChannel6::Type AirWindowsChannel6::pluginType = AirWindowsPlugin::emulation;
AirWindowsChannel7::Type AirWindowsChannel7::pluginType = AirWindowsPlugin::emulation;
AirWindowsChorus::Type AirWindowsChorus::pluginType = AirWindowsPlugin::modulation;
AirWindowsChorusEnsemble::Type AirWindowsChorusEnsemble::pluginType = AirWindowsPlugin::modulation;
AirWindowsClipOnly::Type AirWindowsClipOnly::pluginType = AirWindowsPlugin::dynamics;
AirWindowsCoils::Type AirWindowsCoils::pluginType = AirWindowsPlugin::distortion;
AirWindowsCojones::Type AirWindowsCojones::pluginType = AirWindowsPlugin::distortion;
AirWindowsCompresaturator::Type AirWindowsCompresaturator::pluginType = AirWindowsPlugin::dynamics;
AirWindowsConsole4Buss::Type AirWindowsConsole4Buss::pluginType = AirWindowsPlugin::emulation;
AirWindowsConsole4Channel::Type AirWindowsConsole4Channel::pluginType = AirWindowsPlugin::emulation;
AirWindowsConsole5Buss::Type AirWindowsConsole5Buss::pluginType = AirWindowsPlugin::emulation;
AirWindowsConsole5Channel::Type AirWindowsConsole5Channel::pluginType = AirWindowsPlugin::emulation;
AirWindowsConsole5DarkCh::Type AirWindowsConsole5DarkCh::pluginType = AirWindowsPlugin::emulation;
AirWindowsConsole6Buss::Type AirWindowsConsole6Buss::pluginType = AirWindowsPlugin::emulation;
AirWindowsConsole6Channel::Type AirWindowsConsole6Channel::pluginType = AirWindowsPlugin::emulation;
AirWindowsCrunchyGrooveWear::Type AirWindowsCrunchyGrooveWear::pluginType = AirWindowsPlugin::emulation;
AirWindowsCrystal::Type AirWindowsCrystal::pluginType = AirWindowsPlugin::filter;
AirWindowsDCVoltage::Type AirWindowsDCVoltage::pluginType = AirWindowsPlugin::utility;
AirWindowsDeBess::Type AirWindowsDeBess::pluginType = AirWindowsPlugin::utility;
AirWindowsDeEss::Type AirWindowsDeEss::pluginType = AirWindowsPlugin::utility;
AirWindowsDeHiss::Type AirWindowsDeHiss::pluginType = AirWindowsPlugin::utility;
AirWindowsDeRez2::Type AirWindowsDeRez2::pluginType = AirWindowsPlugin::utility;
AirWindowsDeRez::Type AirWindowsDeRez::pluginType = AirWindowsPlugin::emulation;
AirWindowsDeckwrecka::Type AirWindowsDeckwrecka::pluginType = AirWindowsPlugin::utility;
AirWindowsDensity::Type AirWindowsDensity::pluginType = AirWindowsPlugin::distortion;
AirWindowsDesk4::Type AirWindowsDesk4::pluginType = AirWindowsPlugin::emulation;
AirWindowsDesk::Type AirWindowsDesk::pluginType = AirWindowsPlugin::emulation;
AirWindowsDistance2::Type AirWindowsDistance2::pluginType = AirWindowsPlugin::imaging;
AirWindowsDistance::Type AirWindowsDistance::pluginType = AirWindowsPlugin::reverb;
AirWindowsDitherFloat::Type AirWindowsDitherFloat::pluginType = AirWindowsPlugin::dither;
AirWindowsDitherMeDiskers::Type AirWindowsDitherMeDiskers::pluginType = AirWindowsPlugin::dither;
AirWindowsDitherMeTimbers::Type AirWindowsDitherMeTimbers::pluginType = AirWindowsPlugin::dither;
AirWindowsDitherbox::Type AirWindowsDitherbox::pluginType = AirWindowsPlugin::dither;

AirWindowsADClip7::AirWindowsADClip7 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::adclip7::ADClip7> (&callback)) {}
AirWindowsADT::AirWindowsADT (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::adt::ADT> (&callback)) {}
AirWindowsAQuickVoiceClip::AirWindowsAQuickVoiceClip (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::aquickvoiceclip::AQuickVoiceClip> (&callback)) {}
AirWindowsAcceleration::AirWindowsAcceleration (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::acceleration::Acceleration> (&callback)) {}
AirWindowsAir::AirWindowsAir (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::air::Air> (&callback)) {}
AirWindowsAtmosphereBuss::AirWindowsAtmosphereBuss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::atmospherebuss::AtmosphereBuss> (&callback)) {}
AirWindowsAtmosphereChannel::AirWindowsAtmosphereChannel (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::atmospherechannel::AtmosphereChannel> (&callback)) {}
AirWindowsAura::AirWindowsAura (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::aura::Aura> (&callback)) {}
AirWindowsAverage::AirWindowsAverage (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::average::Average> (&callback)) {}
AirWindowsBassDrive::AirWindowsBassDrive (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::bassdrive::BassDrive> (&callback)) {}
AirWindowsBassKit::AirWindowsBassKit (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::basskit::BassKit> (&callback)) {}
AirWindowsBiquad::AirWindowsBiquad (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::biquad::Biquad> (&callback)) {}
AirWindowsBiquad2::AirWindowsBiquad2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::biquad2::Biquad2> (&callback)) {}
AirWindowsBitGlitter::AirWindowsBitGlitter (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::bitglitter::BitGlitter> (&callback)) {}
AirWindowsBitShiftGain::AirWindowsBitShiftGain (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::bitshiftgain::BitShiftGain> (&callback)) {}
AirWindowsBite::AirWindowsBite (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::bite::Bite> (&callback)) {}
AirWindowsBlockParty::AirWindowsBlockParty (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::blockparty::BlockParty> (&callback)) {}
AirWindowsBrassRider::AirWindowsBrassRider (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::brassrider::BrassRider> (&callback)) {}
AirWindowsBuildATPDF::AirWindowsBuildATPDF (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::buildatpdf::BuildATPDF> (&callback)) {}
AirWindowsBussColors4::AirWindowsBussColors4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::busscolors4::BussColors4> (&callback)) {}
AirWindowsButterComp::AirWindowsButterComp (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::buttercomp::ButterComp> (&callback)) {}
AirWindowsButterComp2::AirWindowsButterComp2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::buttercomp2::ButterComp2> (&callback)) {}
AirWindowsC5RawBuss::AirWindowsC5RawBuss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::c5rawbuss::C5RawBuss> (&callback)) {}
AirWindowsC5RawChannel::AirWindowsC5RawChannel (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::c5rawchannel::C5RawChannel> (&callback)) {}
AirWindowsCStrip::AirWindowsCStrip (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::cstrip::CStrip> (&callback)) {}
AirWindowsCapacitor::AirWindowsCapacitor (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::capacitor::Capacitor> (&callback)) {}
AirWindowsChannel4::AirWindowsChannel4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::channel4::Channel4> (&callback)) {}
AirWindowsChannel5::AirWindowsChannel5 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::channel5::Channel5> (&callback)) {}
AirWindowsChannel6::AirWindowsChannel6 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::channel6::Channel6> (&callback)) {}
AirWindowsChannel7::AirWindowsChannel7 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::channel7::Channel7> (&callback)) {}
AirWindowsChorus::AirWindowsChorus (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::chorus::Chorus> (&callback)) {}
AirWindowsChorusEnsemble::AirWindowsChorusEnsemble (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::chorusensemble::ChorusEnsemble> (&callback)) {}
AirWindowsClipOnly::AirWindowsClipOnly (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::cliponly::ClipOnly> (&callback)) {}
AirWindowsCoils::AirWindowsCoils (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::coils::Coils> (&callback)) {}
AirWindowsCojones::AirWindowsCojones (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::cojones::Cojones> (&callback)) {}
AirWindowsCompresaturator::AirWindowsCompresaturator (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::compresaturator::Compresaturator> (&callback)) {}
AirWindowsConsole4Buss::AirWindowsConsole4Buss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::console4buss::Console4Buss> (&callback)) {}
AirWindowsConsole4Channel::AirWindowsConsole4Channel (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::console4channel::Console4Channel> (&callback)) {}
AirWindowsConsole5Buss::AirWindowsConsole5Buss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::console5buss::Console5Buss> (&callback)) {}
AirWindowsConsole5Channel::AirWindowsConsole5Channel (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::console5channel::Console5Channel> (&callback)) {}
AirWindowsConsole5DarkCh::AirWindowsConsole5DarkCh (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::console5darkch::Console5DarkCh> (&callback)) {}
AirWindowsConsole6Buss::AirWindowsConsole6Buss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::console6buss::Console6Buss> (&callback)) {}
AirWindowsConsole6Channel::AirWindowsConsole6Channel (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::console6channel::Console6Channel> (&callback)) {}
AirWindowsCrunchyGrooveWear::AirWindowsCrunchyGrooveWear (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::crunchygroovewear::CrunchyGrooveWear> (&callback)) {}
AirWindowsCrystal::AirWindowsCrystal (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::crystal::Crystal> (&callback)) {}
AirWindowsDCVoltage::AirWindowsDCVoltage (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::dcvoltage::DCVoltage> (&callback)) {}
AirWindowsDeBess::AirWindowsDeBess (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::debess::DeBess> (&callback)) {}
AirWindowsDeEss::AirWindowsDeEss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::deess::DeEss> (&callback)) {}
AirWindowsDeHiss::AirWindowsDeHiss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::dehiss::DeHiss> (&callback)) {}
AirWindowsDeRez::AirWindowsDeRez (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::derez::DeRez> (&callback)) {}
AirWindowsDeRez2::AirWindowsDeRez2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::derez2::DeRez2> (&callback)) {}
AirWindowsDeckwrecka::AirWindowsDeckwrecka (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::deckwrecka::Deckwrecka> (&callback)) {}
AirWindowsDensity::AirWindowsDensity (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::density::Density> (&callback)) {}
AirWindowsDesk::AirWindowsDesk (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::desk::Desk> (&callback)) {}
AirWindowsDesk4::AirWindowsDesk4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::desk4::Desk4> (&callback)) {}
AirWindowsDistance::AirWindowsDistance (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::distance::Distance> (&callback)) {}
AirWindowsDistance2::AirWindowsDistance2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::distance2::Distance2> (&callback)) {}
AirWindowsDitherFloat::AirWindowsDitherFloat (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::ditherfloat::DitherFloat> (&callback)) {}
AirWindowsDitherMeDiskers::AirWindowsDitherMeDiskers (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::dithermediskers::DitherMeDiskers> (&callback)) {}
AirWindowsDitherMeTimbers::AirWindowsDitherMeTimbers (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::dithermetimbers::DitherMeTimbers> (&callback)) {}
AirWindowsDitherbox::AirWindowsDitherbox (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::ditherbox::Ditherbox> (&callback)) {}

}} // namespace tracktion { inline namespace engine
