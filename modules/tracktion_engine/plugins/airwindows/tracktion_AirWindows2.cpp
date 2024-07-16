/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion { inline namespace engine
{

const char* AirWindowsDoublePaul::xmlTypeName = "airwindows_doublepaul";
const char* AirWindowsDrive::xmlTypeName = "airwindows_drive";
const char* AirWindowsDrumSlam::xmlTypeName = "airwindows_drumslam";
const char* AirWindowsDubCenter::xmlTypeName = "airwindows_dubcenter";
const char* AirWindowsDubSub::xmlTypeName = "airwindows_dubsub";
const char* AirWindowsDustBunny::xmlTypeName = "airwindows_dustbunny";
const char* AirWindowsDyno::xmlTypeName = "airwindows_dyno";
const char* AirWindowsEQ::xmlTypeName = "airwindows_eq";
const char* AirWindowsEdIsDim::xmlTypeName = "airwindows_edisdim";
const char* AirWindowsElectroHat::xmlTypeName = "airwindows_electrohat";
const char* AirWindowsEnergy::xmlTypeName = "airwindows_energy";
const char* AirWindowsEnsemble::xmlTypeName = "airwindows_ensemble";
const char* AirWindowsEveryTrim::xmlTypeName = "airwindows_everytrim";
const char* AirWindowsFacet::xmlTypeName = "airwindows_facet";
const char* AirWindowsFathomFive::xmlTypeName = "airwindows_fathomfive";
const char* AirWindowsFloor::xmlTypeName = "airwindows_floor";
const char* AirWindowsFocus::xmlTypeName = "airwindows_focus";
const char* AirWindowsFracture::xmlTypeName = "airwindows_fracture";
const char* AirWindowsFromTape::xmlTypeName = "airwindows_fromtape";
const char* AirWindowsGatelope::xmlTypeName = "airwindows_gatelope";
const char* AirWindowsGolem::xmlTypeName = "airwindows_golem";
const char* AirWindowsGringer::xmlTypeName = "airwindows_gringer";
const char* AirWindowsGrooveWear::xmlTypeName = "airwindows_groovewear";
const char* AirWindowsGuitarConditioner::xmlTypeName = "airwindows_guitarconditioner";
const char* AirWindowsHardVacuum::xmlTypeName = "airwindows_hardvacuum";
const char* AirWindowsHermeTrim::xmlTypeName = "airwindows_hermetrim";
const char* AirWindowsHermepass::xmlTypeName = "airwindows_hermepass";
const char* AirWindowsHighGlossDither::xmlTypeName = "airwindows_highglossdither";
const char* AirWindowsHighImpact::xmlTypeName = "airwindows_highimpact";
const char* AirWindowsHighpass::xmlTypeName = "airwindows_highpass";
const char* AirWindowsHighpass2::xmlTypeName = "airwindows_highpass2";
const char* AirWindowsHolt::xmlTypeName = "airwindows_holt";
const char* AirWindowsHombre::xmlTypeName = "airwindows_hombre";
const char* AirWindowsInterstage::xmlTypeName = "airwindows_interstage";
const char* AirWindowsIronOxide5::xmlTypeName = "airwindows_ironoxide5";
const char* AirWindowsIronOxideClassic::xmlTypeName = "airwindows_ironoxideclassic";
const char* AirWindowsLeftoMono::xmlTypeName = "airwindows_leftomono";
const char* AirWindowsLogical4::xmlTypeName = "airwindows_logical4";
const char* AirWindowsLoud::xmlTypeName = "airwindows_loud";
const char* AirWindowsLowpass::xmlTypeName = "airwindows_lowpass";
const char* AirWindowsLowpass2::xmlTypeName = "airwindows_lowpass2";
const char* AirWindowsMV::xmlTypeName = "airwindows_mv";
const char* AirWindowsMelt::xmlTypeName = "airwindows_melt";
const char* AirWindowsMidSide::xmlTypeName = "airwindows_midside";
const char* AirWindowsMoNoam::xmlTypeName = "airwindows_monoam";
const char* AirWindowsMojo::xmlTypeName = "airwindows_mojo";
const char* AirWindowsMonitoring::xmlTypeName = "airwindows_monitoring";
const char* AirWindowsNCSeventeen::xmlTypeName = "airwindows_nc17";
const char* AirWindowsNaturalizeDither::xmlTypeName = "airwindows_naturalizedither";
const char* AirWindowsNodeDither::xmlTypeName = "airwindows_nodedither";
const char* AirWindowsNoise::xmlTypeName = "airwindows_noise";
const char* AirWindowsNonlinearSpace::xmlTypeName = "airwindows_nonlinearspace";
const char* AirWindowsNotJustAnotherCD::xmlTypeName = "airwindows_notjustanothercd";
const char* AirWindowsNotJustAnotherDither::xmlTypeName = "airwindows_notjustanotherdither";
const char* AirWindowsOneCornerClip::xmlTypeName = "airwindows_onecornerclip";
const char* AirWindowsPDBuss::xmlTypeName = "airwindows_pdbuss";
const char* AirWindowsPDChannel::xmlTypeName = "airwindows_pdchannel";
const char* AirWindowsPafnuty::xmlTypeName = "airwindows_pafnuty";
const char* AirWindowsPaulDither::xmlTypeName = "airwindows_pauldither";
const char* AirWindowsPeaksOnly::xmlTypeName = "airwindows_peaksonly";
const char* AirWindowsPhaseNudge::xmlTypeName = "airwindows_phasenudge";
const char* AirWindowsPocketVerbs::xmlTypeName = "airwindows_pocketverbs";
const char* AirWindowsPodcast::xmlTypeName = "airwindows_podcast";
const char* AirWindowsPodcastDeluxe::xmlTypeName = "airwindows_podcastdeluxe";
const char* AirWindowsPoint::xmlTypeName = "airwindows_point";
const char* AirWindowsPop::xmlTypeName = "airwindows_pop";
const char* AirWindowsPowerSag::xmlTypeName = "airwindows_powersag";
const char* AirWindowsPowerSag2::xmlTypeName = "airwindows_powersag2";
const char* AirWindowsPressure4::xmlTypeName = "airwindows_pressure4";

AirWindowsDoublePaul::Type AirWindowsDoublePaul::pluginType = AirWindowsPlugin::dither;
AirWindowsDrive::Type AirWindowsDrive::pluginType = AirWindowsPlugin::distortion;
AirWindowsDrumSlam::Type AirWindowsDrumSlam::pluginType = AirWindowsPlugin::emulation;
AirWindowsDubCenter::Type AirWindowsDubCenter::pluginType = AirWindowsPlugin::filter;
AirWindowsDubSub::Type AirWindowsDubSub::pluginType = AirWindowsPlugin::filter;
AirWindowsDustBunny::Type AirWindowsDustBunny::pluginType = AirWindowsPlugin::distortion;
AirWindowsDyno::Type AirWindowsDyno::pluginType = AirWindowsPlugin::utility;
AirWindowsEQ::Type AirWindowsEQ::pluginType = AirWindowsPlugin::eq;
AirWindowsEdIsDim::Type AirWindowsEdIsDim::pluginType = AirWindowsPlugin::utility;
AirWindowsElectroHat::Type AirWindowsElectroHat::pluginType = AirWindowsPlugin::utility;
AirWindowsEnergy::Type AirWindowsEnergy::pluginType = AirWindowsPlugin::filter;
AirWindowsEnsemble::Type AirWindowsEnsemble::pluginType = AirWindowsPlugin::modulation;
AirWindowsEveryTrim::Type AirWindowsEveryTrim::pluginType = AirWindowsPlugin::utility;
AirWindowsFacet::Type AirWindowsFacet::pluginType = AirWindowsPlugin::dynamics;
AirWindowsFathomFive::Type AirWindowsFathomFive::pluginType = AirWindowsPlugin::eq;
AirWindowsFloor::Type AirWindowsFloor::pluginType = AirWindowsPlugin::utility;
AirWindowsFocus::Type AirWindowsFocus::pluginType = AirWindowsPlugin::distortion;
AirWindowsFracture::Type AirWindowsFracture::pluginType = AirWindowsPlugin::distortion;
AirWindowsFromTape::Type AirWindowsFromTape::pluginType = AirWindowsPlugin::emulation;
AirWindowsGatelope::Type AirWindowsGatelope::pluginType = AirWindowsPlugin::dynamics;
AirWindowsGolem::Type AirWindowsGolem::pluginType = AirWindowsPlugin::imaging;
AirWindowsGringer::Type AirWindowsGringer::pluginType = AirWindowsPlugin::distortion;
AirWindowsGrooveWear::Type AirWindowsGrooveWear::pluginType = AirWindowsPlugin::emulation;
AirWindowsGuitarConditioner::Type AirWindowsGuitarConditioner::pluginType = AirWindowsPlugin::distortion;
AirWindowsHardVacuum::Type AirWindowsHardVacuum::pluginType = AirWindowsPlugin::emulation;
AirWindowsHermeTrim::Type AirWindowsHermeTrim::pluginType = AirWindowsPlugin::utility;
AirWindowsHermepass::Type AirWindowsHermepass::pluginType = AirWindowsPlugin::filter;
AirWindowsHighGlossDither::Type AirWindowsHighGlossDither::pluginType = AirWindowsPlugin::dither;
AirWindowsHighImpact::Type AirWindowsHighImpact::pluginType = AirWindowsPlugin::distortion;
AirWindowsHighpass2::Type AirWindowsHighpass2::pluginType = AirWindowsPlugin::filter;
AirWindowsHighpass::Type AirWindowsHighpass::pluginType = AirWindowsPlugin::filter;
AirWindowsHolt::Type AirWindowsHolt::pluginType = AirWindowsPlugin::filter;
AirWindowsHombre::Type AirWindowsHombre::pluginType = AirWindowsPlugin::delay;
AirWindowsInterstage::Type AirWindowsInterstage::pluginType = AirWindowsPlugin::filter;
AirWindowsIronOxide5::Type AirWindowsIronOxide5::pluginType = AirWindowsPlugin::emulation;
AirWindowsIronOxideClassic::Type AirWindowsIronOxideClassic::pluginType = AirWindowsPlugin::emulation;
AirWindowsLeftoMono::Type AirWindowsLeftoMono::pluginType = AirWindowsPlugin::utility;
AirWindowsLogical4::Type AirWindowsLogical4::pluginType = AirWindowsPlugin::dynamics;
AirWindowsLoud::Type AirWindowsLoud::pluginType = AirWindowsPlugin::distortion;
AirWindowsLowpass2::Type AirWindowsLowpass2::pluginType = AirWindowsPlugin::filter;
AirWindowsLowpass::Type AirWindowsLowpass::pluginType = AirWindowsPlugin::filter;
AirWindowsMV::Type AirWindowsMV::pluginType = AirWindowsPlugin::reverb;
AirWindowsMelt::Type AirWindowsMelt::pluginType = AirWindowsPlugin::modulation;
AirWindowsMidSide::Type AirWindowsMidSide::pluginType = AirWindowsPlugin::utility;
AirWindowsMoNoam::Type AirWindowsMoNoam::pluginType = AirWindowsPlugin::utility;
AirWindowsMojo::Type AirWindowsMojo::pluginType = AirWindowsPlugin::utility;
AirWindowsMonitoring::Type AirWindowsMonitoring::pluginType = AirWindowsPlugin::utility;
AirWindowsNCSeventeen::Type AirWindowsNCSeventeen::pluginType = AirWindowsPlugin::distortion;
AirWindowsNaturalizeDither::Type AirWindowsNaturalizeDither::pluginType = AirWindowsPlugin::dither;
AirWindowsNodeDither::Type AirWindowsNodeDither::pluginType = AirWindowsPlugin::dither;
AirWindowsNoise::Type AirWindowsNoise::pluginType = AirWindowsPlugin::utility;
AirWindowsNonlinearSpace::Type AirWindowsNonlinearSpace::pluginType = AirWindowsPlugin::reverb;
AirWindowsNotJustAnotherCD::Type AirWindowsNotJustAnotherCD::pluginType = AirWindowsPlugin::dither;
AirWindowsNotJustAnotherDither::Type AirWindowsNotJustAnotherDither::pluginType = AirWindowsPlugin::dither;
AirWindowsOneCornerClip::Type AirWindowsOneCornerClip::pluginType = AirWindowsPlugin::dynamics;
AirWindowsPDBuss::Type AirWindowsPDBuss::pluginType = AirWindowsPlugin::emulation;
AirWindowsPDChannel::Type AirWindowsPDChannel::pluginType = AirWindowsPlugin::emulation;
AirWindowsPafnuty::Type AirWindowsPafnuty::pluginType = AirWindowsPlugin::filter;
AirWindowsPaulDither::Type AirWindowsPaulDither::pluginType = AirWindowsPlugin::dither;
AirWindowsPeaksOnly::Type AirWindowsPeaksOnly::pluginType = AirWindowsPlugin::utility;
AirWindowsPhaseNudge::Type AirWindowsPhaseNudge::pluginType = AirWindowsPlugin::filter;
AirWindowsPocketVerbs::Type AirWindowsPocketVerbs::pluginType = AirWindowsPlugin::reverb;
AirWindowsPodcast::Type AirWindowsPodcast::pluginType = AirWindowsPlugin::dynamics;
AirWindowsPodcastDeluxe::Type AirWindowsPodcastDeluxe::pluginType = AirWindowsPlugin::dynamics;
AirWindowsPoint::Type AirWindowsPoint::pluginType = AirWindowsPlugin::dynamics;
AirWindowsPop::Type AirWindowsPop::pluginType = AirWindowsPlugin::dynamics;
AirWindowsPowerSag2::Type AirWindowsPowerSag2::pluginType = AirWindowsPlugin::emulation;
AirWindowsPowerSag::Type AirWindowsPowerSag::pluginType = AirWindowsPlugin::emulation;
AirWindowsPressure4::Type AirWindowsPressure4::pluginType = AirWindowsPlugin::dynamics;

AirWindowsDoublePaul::AirWindowsDoublePaul (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::doublepaul::DoublePaul> (&callback)) {}
AirWindowsDrive::AirWindowsDrive (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::drive::Drive> (&callback)) {}
AirWindowsDrumSlam::AirWindowsDrumSlam (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::drumslam::DrumSlam> (&callback)) {}
AirWindowsDubCenter::AirWindowsDubCenter (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::dubcenter::DubCenter> (&callback)) {}
AirWindowsDubSub::AirWindowsDubSub (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::dubsub::DubSub> (&callback)) {}
AirWindowsDustBunny::AirWindowsDustBunny (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::dustbunny::DustBunny> (&callback)) {}
AirWindowsDyno::AirWindowsDyno (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::dyno::Dyno> (&callback)) {}
AirWindowsEQ::AirWindowsEQ (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::eq::EQ> (&callback)) {}
AirWindowsEdIsDim::AirWindowsEdIsDim (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::edisdim::EdIsDim> (&callback)) {}
AirWindowsElectroHat::AirWindowsElectroHat (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::electrohat::ElectroHat> (&callback)) {}
AirWindowsEnergy::AirWindowsEnergy (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::energy::Energy> (&callback)) {}
AirWindowsEnsemble::AirWindowsEnsemble (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::ensemble::Ensemble> (&callback)) {}
AirWindowsEveryTrim::AirWindowsEveryTrim (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::everytrim::EveryTrim> (&callback)) {}
AirWindowsFacet::AirWindowsFacet (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::facet::Facet> (&callback)) {}
AirWindowsFathomFive::AirWindowsFathomFive (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::fathomfive::FathomFive> (&callback)) {}
AirWindowsFloor::AirWindowsFloor (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::awfloor::Floor> (&callback)) {}
AirWindowsFocus::AirWindowsFocus (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::focus::Focus> (&callback)) {}
AirWindowsFracture::AirWindowsFracture (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::fracture::Fracture> (&callback)) {}
AirWindowsFromTape::AirWindowsFromTape (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::fromtape::FromTape> (&callback)) {}
AirWindowsGatelope::AirWindowsGatelope (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::gatelope::Gatelope> (&callback)) {}
AirWindowsGolem::AirWindowsGolem (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::golem::Golem> (&callback)) {}
AirWindowsGringer::AirWindowsGringer (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::gringer::Gringer> (&callback)) {}
AirWindowsGrooveWear::AirWindowsGrooveWear (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::groovewear::GrooveWear> (&callback)) {}
AirWindowsGuitarConditioner::AirWindowsGuitarConditioner (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::guitarconditioner::GuitarConditioner> (&callback)) {}
AirWindowsHardVacuum::AirWindowsHardVacuum (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::hardvacuum::HardVacuum> (&callback)) {}
AirWindowsHermeTrim::AirWindowsHermeTrim (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::hermetrim::HermeTrim> (&callback)) {}
AirWindowsHermepass::AirWindowsHermepass (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::hermepass::Hermepass> (&callback)) {}
AirWindowsHighGlossDither::AirWindowsHighGlossDither (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::highglossdither::HighGlossDither> (&callback)) {}
AirWindowsHighImpact::AirWindowsHighImpact (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::highimpact::HighImpact> (&callback)) {}
AirWindowsHighpass::AirWindowsHighpass (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::highpass::Highpass> (&callback)) {}
AirWindowsHighpass2::AirWindowsHighpass2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::highpass2::Highpass2> (&callback)) {}
AirWindowsHolt::AirWindowsHolt (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::holt::Holt> (&callback)) {}
AirWindowsHombre::AirWindowsHombre (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::hombre::Hombre> (&callback)) {}
AirWindowsInterstage::AirWindowsInterstage (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::interstage::Interstage> (&callback)) {}
AirWindowsIronOxide5::AirWindowsIronOxide5 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::ironoxide5::IronOxide5> (&callback)) {}
AirWindowsIronOxideClassic::AirWindowsIronOxideClassic (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::ironoxideclassic::IronOxideClassic> (&callback)) {}
AirWindowsLeftoMono::AirWindowsLeftoMono (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::leftomono::LeftoMono> (&callback)) {}
AirWindowsLogical4::AirWindowsLogical4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::logical4::Logical4> (&callback)) {}
AirWindowsLoud::AirWindowsLoud (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::loud::Loud> (&callback)) {}
AirWindowsLowpass::AirWindowsLowpass (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::lowpass::Lowpass> (&callback)) {}
AirWindowsLowpass2::AirWindowsLowpass2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::lowpass2::Lowpass2> (&callback)) {}
AirWindowsMV::AirWindowsMV (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::mv::MV> (&callback)) {}
AirWindowsMelt::AirWindowsMelt (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::melt::Melt> (&callback)) {}
AirWindowsMidSide::AirWindowsMidSide (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::midside::MidSide> (&callback)) {}
AirWindowsMoNoam::AirWindowsMoNoam (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::monoam::MoNoam> (&callback)) {}
AirWindowsMojo::AirWindowsMojo (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::mojo::Mojo> (&callback)) {}
AirWindowsMonitoring::AirWindowsMonitoring (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::monitoring::Monitoring> (&callback)) {}
AirWindowsNCSeventeen::AirWindowsNCSeventeen (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::ncseventeen::NCSeventeen> (&callback)) {}
AirWindowsNaturalizeDither::AirWindowsNaturalizeDither (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::naturalizedither::NaturalizeDither> (&callback)) {}
AirWindowsNodeDither::AirWindowsNodeDither (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::nodedither::NodeDither> (&callback)) {}
AirWindowsNoise::AirWindowsNoise (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::noise::Noise> (&callback)) {}
AirWindowsNonlinearSpace::AirWindowsNonlinearSpace (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::nonlinearspace::NonlinearSpace> (&callback)) {}
AirWindowsNotJustAnotherCD::AirWindowsNotJustAnotherCD (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::notjustanothercd::NotJustAnotherCD> (&callback)) {}
AirWindowsNotJustAnotherDither::AirWindowsNotJustAnotherDither (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::notjustanotherdither::NotJustAnotherDither> (&callback)) {}
AirWindowsOneCornerClip::AirWindowsOneCornerClip (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::onecornerclip::OneCornerClip> (&callback)) {}
AirWindowsPDBuss::AirWindowsPDBuss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::pdbuss::PDBuss> (&callback)) {}
AirWindowsPDChannel::AirWindowsPDChannel (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::pdchannel::PDChannel> (&callback)) {}
AirWindowsPafnuty::AirWindowsPafnuty (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::pafnuty::Pafnuty> (&callback)) {}
AirWindowsPaulDither::AirWindowsPaulDither (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::pauldither::PaulDither> (&callback)) {}
AirWindowsPeaksOnly::AirWindowsPeaksOnly (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::peaksonly::PeaksOnly> (&callback)) {}
AirWindowsPhaseNudge::AirWindowsPhaseNudge (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::phasenudge::PhaseNudge> (&callback)) {}
AirWindowsPocketVerbs::AirWindowsPocketVerbs (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::pocketverbs::PocketVerbs> (&callback)) {}
AirWindowsPodcast::AirWindowsPodcast (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::podcast::Podcast> (&callback)) {}
AirWindowsPodcastDeluxe::AirWindowsPodcastDeluxe (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::podcastdeluxe::PodcastDeluxe> (&callback)) {}
AirWindowsPoint::AirWindowsPoint (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::point::Point> (&callback)) {}
AirWindowsPop::AirWindowsPop (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::pop::Pop> (&callback)) {}
AirWindowsPowerSag::AirWindowsPowerSag (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::powersag::PowerSag> (&callback)) {}
AirWindowsPowerSag2::AirWindowsPowerSag2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::powersag2::PowerSag2> (&callback)) {}
AirWindowsPressure4::AirWindowsPressure4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::pressure4::Pressure4> (&callback)) {}


}} // namespace tracktion { inline namespace engine
