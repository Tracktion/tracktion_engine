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
AirWindowsCallback::AirWindowsCallback (AirWindowsPlugin& o)
    : owner (o)
{
}

double AirWindowsCallback::getSampleRate()
{
    return owner.sampleRate;
}

//==============================================================================
class AirWindowsAutomatableParameter   : public AutomatableParameter
{
public:
    AirWindowsAutomatableParameter (AirWindowsPlugin& p, int idx)
        : AutomatableParameter (getParamId (p, idx), getParamName (p, idx), p, {0, 1}),
        plugin (p), index (idx)
    {
        valueToStringFunction = [] (float v)         { return juce::String (v); };
        stringToValueFunction = [] (juce::String v)  { return v.getFloatValue(); };

        defaultValue = plugin.impl->getParameter (idx);

        setParameter (defaultValue, juce::sendNotification);

        autodetectRange();
    }

    ~AirWindowsAutomatableParameter()
    {
        notifyListenersOfDeletion();
    }

    void resetToDefault()
    {
        setParameter (defaultValue, juce::sendNotificationSync);
    }

    void autodetectRange()
    {
        float current = plugin.impl->getParameter (index);

        plugin.impl->setParameter (index, 0.0f);
        float v1 = getCurrentValueAsString().retainCharacters ("1234567890-.").getFloatValue();
        plugin.impl->setParameter (index, 1.0f);
        float v2 = getCurrentValueAsString().retainCharacters ("1234567890-.").getFloatValue();

        if (v2 > v1 && (v1 != 0.0f || v2 != 1.0f))
            setConversionRange ({v1, v2});

        plugin.impl->setParameter (index, current);
    }

    void refresh()
    {
        currentValue = currentParameterValue = plugin.impl->getParameter (index);
        curveHasChanged();
        listeners.call (&Listener::currentValueChanged, *this, currentValue);
    }

    void setConversionRange (juce::NormalisableRange<float> range)
    {
        conversionRange = range;

        stringToValueFunction = [this] (juce::String v) { return conversionRange.convertTo0to1 (v.getFloatValue()); };
    }

    void parameterChanged (float newValue, bool byAutomation) override
    {
        if (plugin.impl->getParameter (index) != newValue)
        {
            if (! byAutomation)
                markAsChanged();

            plugin.impl->setParameter (index, newValue);
        }
    }

    juce::String getCurrentValueAsString() override
    {
        char paramText[kVstMaxParamStrLen];
        plugin.impl->getParameterDisplay (index, paramText);
        auto t1 = juce::String (paramText);

        char labelText[kVstMaxParamStrLen];
        plugin.impl->getParameterLabel (index, labelText);
        auto t2 = juce::String (labelText);

        if (t2.isNotEmpty())
            return t1 + " " + t2;
        return t1;
    }

    void markAsChanged() const noexcept
    {
        getEdit().pluginChanged (plugin);
    }

    static juce::String getParamId (AirWindowsPlugin& p, int idx)
    {
        return getParamName (p, idx).toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz");
    }

    static juce::String getParamName (AirWindowsPlugin& p, int idx)
    {
        char paramName[kVstMaxParamStrLen];
        p.impl->getParameterName (idx, paramName);
        return juce::String (paramName);
    }

    AirWindowsPlugin& plugin;
    int index = 0;
    float defaultValue = 0.0f;
    juce::NormalisableRange<float> conversionRange;
};

//==============================================================================
AirWindowsPlugin::AirWindowsPlugin (PluginCreationInfo info, std::unique_ptr<AirWindowsBase> base)
    : Plugin (info), callback (*this), impl (std::move (base))
{
    auto um = getUndoManager();

    for (int i = 0; i < impl->getNumParameters(); i++)
    {
        if (AirWindowsAutomatableParameter::getParamName (*this, i) == "Dry/Wet")
            continue;

        auto param = new AirWindowsAutomatableParameter (*this, i);

        addAutomatableParameter (param);
        parameters.add (param);
    }

    restorePluginStateFromValueTree (state);

    addAutomatableParameter (dryGain = new PluginWetDryAutomatableParam ("dry level", TRANS("Dry Level"), *this));
    addAutomatableParameter (wetGain = new PluginWetDryAutomatableParam ("wet level", TRANS("Wet Level"), *this));

    dryValue.referTo (state, IDs::dry, um);
    wetValue.referTo (state, IDs::wet, um, 1.0f);

    dryGain->attachToCurrentValue (dryValue);
    wetGain->attachToCurrentValue (wetValue);
}

AirWindowsPlugin::~AirWindowsPlugin()
{
    dryGain->detachFromCurrentValue();
    wetGain->detachFromCurrentValue();
}

void AirWindowsPlugin::setConversionRange (int paramIdx, juce::NormalisableRange<float> range)
{
    if (auto p = dynamic_cast<AirWindowsAutomatableParameter*> (parameters[paramIdx].get()))
        p->setConversionRange (range);
    else
        jassertfalse;
}

void AirWindowsPlugin::resetToDefault()
{
    for (auto p : parameters)
        if (auto awp = dynamic_cast<AirWindowsAutomatableParameter*> (p))
            awp->resetToDefault();

    dryGain->setParameter (0.0f, juce::sendNotificationSync);
    wetGain->setParameter (1.0f, juce::sendNotificationSync);
}

int AirWindowsPlugin::getNumOutputChannelsGivenInputs (int)
{
    return impl->getNumOutputs();
}

void AirWindowsPlugin::initialise (const PluginInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
}

void AirWindowsPlugin::deinitialise()
{
}

void AirWindowsPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    // We need to lock the processing while a preset is being loaded or the parameters
    // will overwrite the plugin state before we can update the parameters from the
    // plugin state
    juce::ScopedLock sl (lock);

    if (fc.destBuffer == nullptr)
        return;

    SCOPED_REALTIME_CHECK

    for (auto p : parameters)
        if (auto awp = dynamic_cast<AirWindowsAutomatableParameter*> (p))
            impl->setParameter (awp->index, awp->getCurrentValue());

    juce::AudioBuffer<float> asb (fc.destBuffer->getArrayOfWritePointers(), fc.destBuffer->getNumChannels(),
                                  fc.bufferStartSample, fc.bufferNumSamples);

    auto dry = dryGain->getCurrentValue();
    auto wet = wetGain->getCurrentValue();

    if (dry <= 0.00004f)
    {
        processBlock (asb);
        zeroDenormalisedValuesIfNeeded (asb);

        if (wet < 0.999f)
            asb.applyGain (0, fc.bufferNumSamples, wet);
    }
    else
    {
        auto numChans = asb.getNumChannels();
        AudioScratchBuffer dryAudio (numChans, fc.bufferNumSamples);

        for (int i = 0; i < numChans; ++i)
            dryAudio.buffer.copyFrom (i, 0, asb, i, 0, fc.bufferNumSamples);

        processBlock (asb);
        zeroDenormalisedValuesIfNeeded (asb);

        if (wet < 0.999f)
            asb.applyGain (0, fc.bufferNumSamples, wet);

        for (int i = 0; i < numChans; ++i)
            asb.addFrom (i, 0, dryAudio.buffer.getReadPointer (i), fc.bufferNumSamples, dry);
    }
}

void AirWindowsPlugin::processBlock (juce::AudioBuffer<float>& buffer)
{
    auto numChans    = buffer.getNumChannels();
    auto samps       = buffer.getNumSamples();
    auto pluginChans = std::max (impl->getNumOutputs(), impl->getNumInputs());

    if (pluginChans > numChans)
    {
        AudioScratchBuffer input (pluginChans, samps);
        AudioScratchBuffer output (pluginChans, samps);

        input.buffer.clear();
        output.buffer.clear();

        input.buffer.copyFrom (0, 0, buffer, 0, 0, samps);

        impl->processReplacing ((float**)input.buffer.getArrayOfWritePointers(),
                                (float**)output.buffer.getArrayOfWritePointers(),
                                samps);

        buffer.copyFrom (0, 0, output.buffer, 0, 0, samps);
    }
    else
    {
        AudioScratchBuffer output (numChans, samps);
        output.buffer.clear();

        impl->processReplacing ((float**)buffer.getArrayOfWritePointers(),
                                (float**)output.buffer.getArrayOfWritePointers(),
                                samps);

        for (int i = 0; i < numChans; ++i)
            buffer.copyFrom (i, 0, output.buffer, i, 0, samps);
    }
}

void AirWindowsPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    juce::ScopedLock sl (lock);

    if (v.hasProperty (IDs::state))
    {
        juce::MemoryBlock data;

        {
            juce::MemoryOutputStream os (data, false);
            juce::Base64::convertFromBase64 (os, v[IDs::state].toString());
        }

        impl->setChunk (data.getData(), int (data.getSize()), false);
    }

    juce::CachedValue<float>* cvsFloat[]  = { &wetValue, &dryValue, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);

    for (auto p : parameters)
        if (auto awp = dynamic_cast<AirWindowsAutomatableParameter*> (p))
            awp->refresh();
}

void AirWindowsPlugin::flushPluginStateToValueTree()
{
    auto* um = getUndoManager();

    Plugin::flushPluginStateToValueTree();

    void* data = nullptr;
    int size = impl->getChunk (&data, false);

    if (data != nullptr)
    {
        state.setProperty (IDs::state, juce::Base64::toBase64 (data, size_t (size)), um);

        free (data);
    }
    else
    {
        state.removeProperty (IDs::state, um);
    }
}

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
