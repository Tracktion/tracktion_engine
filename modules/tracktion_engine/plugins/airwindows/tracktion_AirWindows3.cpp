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
const char* AirWindowsPurestAir::xmlTypeName = "airwindows_purestair";
const char* AirWindowsPurestConsoleBuss::xmlTypeName = "airwindows_purestconsolebuss";
const char* AirWindowsPurestConsoleChannel::xmlTypeName = "airwindows_purestconsolechannel";
const char* AirWindowsPurestDrive::xmlTypeName = "airwindows_purestdrive";
const char* AirWindowsPurestEcho::xmlTypeName = "airwindows_purestecho";
const char* AirWindowsPurestGain::xmlTypeName = "airwindows_purestgain";
const char* AirWindowsPurestSquish::xmlTypeName = "airwindows_purestsquish";
const char* AirWindowsPurestWarm::xmlTypeName = "airwindows_purestwarm";
const char* AirWindowsPyewacket::xmlTypeName = "airwindows_pyewacket";
const char* AirWindowsRawGlitters::xmlTypeName = "airwindows_rawglitters";
const char* AirWindowsRawTimbers::xmlTypeName = "airwindows_rawtimbers";
const char* AirWindowsRecurve::xmlTypeName = "airwindows_recurve";
const char* AirWindowsRemap::xmlTypeName = "airwindows_remap";
const char* AirWindowsResEQ::xmlTypeName = "airwindows_reseq";
const char* AirWindowsRighteous4::xmlTypeName = "airwindows_righteous4";
const char* AirWindowsRightoMono::xmlTypeName = "airwindows_rightomono";
const char* AirWindowsSideDull::xmlTypeName = "airwindows_sidedull";
const char* AirWindowsSidepass::xmlTypeName = "airwindows_sidepass";
const char* AirWindowsSingleEndedTriode::xmlTypeName = "airwindows_singeendedtriode";
const char* AirWindowsSlew::xmlTypeName = "airwindows_slew";
const char* AirWindowsSlew2::xmlTypeName = "airwindows_slew2";
const char* AirWindowsSlewOnly::xmlTypeName = "airwindows_slewonly";
const char* AirWindowsSmooth::xmlTypeName = "airwindows_smooth";
const char* AirWindowsSoftGate::xmlTypeName = "airwindows_softgate";
const char* AirWindowsSpatializeDither::xmlTypeName = "airwindows_spatializedither";
const char* AirWindowsSpiral::xmlTypeName = "airwindows_spiral";
const char* AirWindowsSpiral2::xmlTypeName = "airwindows_spiral2";
const char* AirWindowsStarChild::xmlTypeName = "airwindows_starchild";
const char* AirWindowsStereoFX::xmlTypeName = "airwindows_stereofx";
const char* AirWindowsStudioTan::xmlTypeName = "airwindows_studiotan";
const char* AirWindowsSubsOnly::xmlTypeName = "airwindows_subsonly";
const char* AirWindowsSurge::xmlTypeName = "airwindows_surge";
const char* AirWindowsSurgeTide::xmlTypeName = "airwindows_surgetide";
const char* AirWindowsSwell::xmlTypeName = "airwindows_swell";
const char* AirWindowsTPDFDither::xmlTypeName = "airwindows_tpdfdither";
const char* AirWindowsTapeDelay::xmlTypeName = "airwindows_tapedelay";
const char* AirWindowsTapeDither::xmlTypeName = "airwindows_tapedither";
const char* AirWindowsTapeDust::xmlTypeName = "airwindows_tapedust";
const char* AirWindowsTapeFat::xmlTypeName = "airwindows_tapefat";
const char* AirWindowsThunder::xmlTypeName = "airwindows_thunder";
const char* AirWindowsToTape5::xmlTypeName = "airwindows_totape5";
const char* AirWindowsToVinyl4::xmlTypeName = "airwindows_tovinyl4";
const char* AirWindowsToneSlant::xmlTypeName = "airwindows_toneslant";
const char* AirWindowsTransDesk::xmlTypeName = "airwindows_transdesk";
const char* AirWindowsTremolo::xmlTypeName = "airwindows_tremolo";
const char* AirWindowsTubeDesk::xmlTypeName = "airwindows_tubedesk";
const char* AirWindowsUnBox::xmlTypeName = "airwindows_unbox";
const char* AirWindowsVariMu::xmlTypeName = "airwindows_varimu";
const char* AirWindowsVibrato::xmlTypeName = "airwindows_vibrato";
const char* AirWindowsVinylDither::xmlTypeName = "airwindows_vinyldither";
const char* AirWindowsVoiceOfTheStarship::xmlTypeName = "airwindows_voiceofthestarship";
const char* AirWindowsVoiceTrick::xmlTypeName = "airwindows_voicetrick";
const char* AirWindowsWider::xmlTypeName = "airwindows_wider";
const char* AirWindowscurve::xmlTypeName = "airwindows_curve";
const char* AirWindowsuLawDecode::xmlTypeName = "airwindows_ulawdecode";
const char* AirWindowsuLawEncode::xmlTypeName = "airwindows_ulawencode";

AirWindowsPurestAir::Type AirWindowsPurestAir::pluginType = AirWindowsPlugin::filter;
AirWindowsPurestConsoleBuss::Type AirWindowsPurestConsoleBuss::pluginType = AirWindowsPlugin::emulation;
AirWindowsPurestConsoleChannel::Type AirWindowsPurestConsoleChannel::pluginType = AirWindowsPlugin::emulation;
AirWindowsPurestDrive::Type AirWindowsPurestDrive::pluginType = AirWindowsPlugin::distortion;
AirWindowsPurestEcho::Type AirWindowsPurestEcho::pluginType = AirWindowsPlugin::imaging;
AirWindowsPurestGain::Type AirWindowsPurestGain::pluginType = AirWindowsPlugin::dynamics;
AirWindowsPurestSquish::Type AirWindowsPurestSquish::pluginType = AirWindowsPlugin::dynamics;
AirWindowsPurestWarm::Type AirWindowsPurestWarm::pluginType = AirWindowsPlugin::filter;
AirWindowsPyewacket::Type AirWindowsPyewacket::pluginType = AirWindowsPlugin::dynamics;
AirWindowsRawGlitters::Type AirWindowsRawGlitters::pluginType = AirWindowsPlugin::utility;
AirWindowsRawTimbers::Type AirWindowsRawTimbers::pluginType = AirWindowsPlugin::dither;
AirWindowsRecurve::Type AirWindowsRecurve::pluginType = AirWindowsPlugin::dynamics;
AirWindowsRemap::Type AirWindowsRemap::pluginType = AirWindowsPlugin::utility;
AirWindowsResEQ::Type AirWindowsResEQ::pluginType = AirWindowsPlugin::eq;
AirWindowsRighteous4::Type AirWindowsRighteous4::pluginType = AirWindowsPlugin::dynamics;
AirWindowsRightoMono::Type AirWindowsRightoMono::pluginType = AirWindowsPlugin::utility;
AirWindowsSideDull::Type AirWindowsSideDull::pluginType = AirWindowsPlugin::filter;
AirWindowsSidepass::Type AirWindowsSidepass::pluginType = AirWindowsPlugin::filter;
AirWindowsSingleEndedTriode::Type AirWindowsSingleEndedTriode::pluginType = AirWindowsPlugin::emulation;
AirWindowsSlew2::Type AirWindowsSlew2::pluginType = AirWindowsPlugin::utility;
AirWindowsSlew::Type AirWindowsSlew::pluginType = AirWindowsPlugin::utility;
AirWindowsSlewOnly::Type AirWindowsSlewOnly::pluginType = AirWindowsPlugin::utility;
AirWindowsSmooth::Type AirWindowsSmooth::pluginType = AirWindowsPlugin::utility;
AirWindowsSoftGate::Type AirWindowsSoftGate::pluginType = AirWindowsPlugin::dynamics;
AirWindowsSpatializeDither::Type AirWindowsSpatializeDither::pluginType = AirWindowsPlugin::dither;
AirWindowsSpiral2::Type AirWindowsSpiral2::pluginType = AirWindowsPlugin::distortion;
AirWindowsSpiral::Type AirWindowsSpiral::pluginType = AirWindowsPlugin::distortion;
AirWindowsStarChild::Type AirWindowsStarChild::pluginType = AirWindowsPlugin::filter;
AirWindowsStereoFX::Type AirWindowsStereoFX::pluginType = AirWindowsPlugin::imaging;
AirWindowsStudioTan::Type AirWindowsStudioTan::pluginType = AirWindowsPlugin::dither;
AirWindowsSubsOnly::Type AirWindowsSubsOnly::pluginType = AirWindowsPlugin::utility;
AirWindowsSurge::Type AirWindowsSurge::pluginType = AirWindowsPlugin::dynamics;
AirWindowsSurgeTide::Type AirWindowsSurgeTide::pluginType = AirWindowsPlugin::dynamics;
AirWindowsSwell::Type AirWindowsSwell::pluginType = AirWindowsPlugin::dynamics;
AirWindowsTPDFDither::Type AirWindowsTPDFDither::pluginType = AirWindowsPlugin::dither;
AirWindowsTapeDelay::Type AirWindowsTapeDelay::pluginType = AirWindowsPlugin::emulation;
AirWindowsTapeDither::Type AirWindowsTapeDither::pluginType = AirWindowsPlugin::dither;
AirWindowsTapeDust::Type AirWindowsTapeDust::pluginType = AirWindowsPlugin::distortion;
AirWindowsTapeFat::Type AirWindowsTapeFat::pluginType = AirWindowsPlugin::emulation;
AirWindowsThunder::Type AirWindowsThunder::pluginType = AirWindowsPlugin::dynamics;
AirWindowsToTape5::Type AirWindowsToTape5::pluginType = AirWindowsPlugin::emulation;
AirWindowsToVinyl4::Type AirWindowsToVinyl4::pluginType = AirWindowsPlugin::emulation;
AirWindowsToneSlant::Type AirWindowsToneSlant::pluginType = AirWindowsPlugin::eq;
AirWindowsTransDesk::Type AirWindowsTransDesk::pluginType = AirWindowsPlugin::emulation;
AirWindowsTremolo::Type AirWindowsTremolo::pluginType = AirWindowsPlugin::distortion;
AirWindowsTubeDesk::Type AirWindowsTubeDesk::pluginType = AirWindowsPlugin::emulation;
AirWindowsUnBox::Type AirWindowsUnBox::pluginType = AirWindowsPlugin::distortion;
AirWindowsVariMu::Type AirWindowsVariMu::pluginType = AirWindowsPlugin::dynamics;
AirWindowsVibrato::Type AirWindowsVibrato::pluginType = AirWindowsPlugin::modulation;
AirWindowsVinylDither::Type AirWindowsVinylDither::pluginType = AirWindowsPlugin::dither;
AirWindowsVoiceOfTheStarship::Type AirWindowsVoiceOfTheStarship::pluginType = AirWindowsPlugin::utility;
AirWindowsVoiceTrick::Type AirWindowsVoiceTrick::pluginType = AirWindowsPlugin::utility;
AirWindowsWider::Type AirWindowsWider::pluginType = AirWindowsPlugin::imaging;
AirWindowscurve::Type AirWindowscurve::pluginType = AirWindowsPlugin::utility;
AirWindowsuLawDecode::Type AirWindowsuLawDecode::pluginType = AirWindowsPlugin::utility;
AirWindowsuLawEncode::Type AirWindowsuLawEncode::pluginType = AirWindowsPlugin::utility;

AirWindowsPurestAir::AirWindowsPurestAir (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::purestair::PurestAir> (&callback)) {}
AirWindowsPurestConsoleBuss::AirWindowsPurestConsoleBuss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::purestconsolebuss::PurestConsoleBuss> (&callback)) {}
AirWindowsPurestConsoleChannel::AirWindowsPurestConsoleChannel (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::purestconsolechannel::PurestConsoleChannel> (&callback)) {}
AirWindowsPurestDrive::AirWindowsPurestDrive (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::purestdrive::PurestDrive> (&callback)) {}
AirWindowsPurestEcho::AirWindowsPurestEcho (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::purestecho::PurestEcho> (&callback)) {}
AirWindowsPurestGain::AirWindowsPurestGain (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::purestgain::PurestGain> (&callback)) {}
AirWindowsPurestSquish::AirWindowsPurestSquish (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::purestsquish::PurestSquish> (&callback)) {}
AirWindowsPurestWarm::AirWindowsPurestWarm (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::purestwarm::PurestWarm> (&callback)) {}
AirWindowsPyewacket::AirWindowsPyewacket (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::pyewacket::Pyewacket> (&callback)) {}
AirWindowsRawGlitters::AirWindowsRawGlitters (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::rawglitters::RawGlitters> (&callback)) {}
AirWindowsRawTimbers::AirWindowsRawTimbers (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::rawtimbers::RawTimbers> (&callback)) {}
AirWindowsRecurve::AirWindowsRecurve (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::recurve::Recurve> (&callback)) {}
AirWindowsRemap::AirWindowsRemap (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::remap::Remap> (&callback)) {}
AirWindowsResEQ::AirWindowsResEQ (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::reseq::ResEQ> (&callback)) {}
AirWindowsRighteous4::AirWindowsRighteous4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::righteous4::Righteous4> (&callback)) {}
AirWindowsRightoMono::AirWindowsRightoMono (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::rightomono::RightoMono> (&callback)) {}
AirWindowsSideDull::AirWindowsSideDull (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::sidedull::SideDull> (&callback)) {}
AirWindowsSidepass::AirWindowsSidepass (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::sidepass::Sidepass> (&callback)) {}
AirWindowsSingleEndedTriode::AirWindowsSingleEndedTriode (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::singleendedtriode::SingleEndedTriode> (&callback)) {}
AirWindowsSlew::AirWindowsSlew (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::slew::Slew> (&callback)) {}
AirWindowsSlew2::AirWindowsSlew2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::slew2::Slew2> (&callback)) {}
AirWindowsSlewOnly::AirWindowsSlewOnly (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::slewonly::SlewOnly> (&callback)) {}
AirWindowsSmooth::AirWindowsSmooth (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::smooth::Smooth> (&callback)) {}
AirWindowsSoftGate::AirWindowsSoftGate (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::softgate::SoftGate> (&callback)) {}
AirWindowsSpatializeDither::AirWindowsSpatializeDither (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::spatializedither::SpatializeDither> (&callback)) {}
AirWindowsSpiral::AirWindowsSpiral (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::spiral::Spiral> (&callback)) {}
AirWindowsSpiral2::AirWindowsSpiral2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::spiral2::Spiral2> (&callback)) {}
AirWindowsStarChild::AirWindowsStarChild (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::starchild::StarChild> (&callback)) {}
AirWindowsStereoFX::AirWindowsStereoFX (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::stereofx::StereoFX> (&callback)) {}
AirWindowsStudioTan::AirWindowsStudioTan (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::studiotan::StudioTan> (&callback)) {}
AirWindowsSubsOnly::AirWindowsSubsOnly (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::subsonly::SubsOnly> (&callback)) {}
AirWindowsSurge::AirWindowsSurge (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::surge::Surge> (&callback)) {}
AirWindowsSurgeTide::AirWindowsSurgeTide (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::surgetide::SurgeTide> (&callback)) {}
AirWindowsSwell::AirWindowsSwell (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::swell::Swell> (&callback)) {}
AirWindowsTPDFDither::AirWindowsTPDFDither (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::tpdfdither::TPDFDither> (&callback)) {}
AirWindowsTapeDelay::AirWindowsTapeDelay (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::tapedelay::TapeDelay> (&callback)) {}
AirWindowsTapeDither::AirWindowsTapeDither (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::tapedither::TapeDither> (&callback)) {}
AirWindowsTapeDust::AirWindowsTapeDust (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::tapedust::TapeDust> (&callback)) {}
AirWindowsTapeFat::AirWindowsTapeFat (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::tapefat::TapeFat> (&callback)) {}
AirWindowsThunder::AirWindowsThunder (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::thunder::Thunder> (&callback)) {}
AirWindowsToTape5::AirWindowsToTape5 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::totape5::ToTape5> (&callback)) {}
AirWindowsToVinyl4::AirWindowsToVinyl4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::tovinyl4::ToVinyl4> (&callback)) {}
AirWindowsToneSlant::AirWindowsToneSlant (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::toneslant::ToneSlant> (&callback)) {}
AirWindowsTransDesk::AirWindowsTransDesk (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::transdesk::TransDesk> (&callback)) {}
AirWindowsTremolo::AirWindowsTremolo (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::tremolo::Tremolo> (&callback)) {}
AirWindowsTubeDesk::AirWindowsTubeDesk (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::tubedesk::TubeDesk> (&callback)) {}
AirWindowsUnBox::AirWindowsUnBox (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::unbox::UnBox> (&callback)) {}
AirWindowsVariMu::AirWindowsVariMu (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::varimu::VariMu> (&callback)) {}
AirWindowsVibrato::AirWindowsVibrato (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::vibrato::Vibrato> (&callback)) {}
AirWindowsVinylDither::AirWindowsVinylDither (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::vinyldither::VinylDither> (&callback)) {}
AirWindowsVoiceOfTheStarship::AirWindowsVoiceOfTheStarship (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::voiceofthestarship::VoiceOfTheStarship> (&callback)) {}
AirWindowsVoiceTrick::AirWindowsVoiceTrick (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::voicetrick::VoiceTrick> (&callback)) {}
AirWindowsWider::AirWindowsWider (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::wider::Wider> (&callback)) {}
AirWindowscurve::AirWindowscurve (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::curve::curve> (&callback)) {}
AirWindowsuLawDecode::AirWindowsuLawDecode (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::ulawdecode::uLawDecode> (&callback)) {}
AirWindowsuLawEncode::AirWindowsuLawEncode (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::ulawencode::uLawEncode> (&callback)) {}


}} // namespace tracktion { inline namespace engine
