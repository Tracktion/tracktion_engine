/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
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
        valueToStringFunction = [] (float v)  { return String (v); };
        stringToValueFunction = [] (String v) { return v.getFloatValue(); };

        defaultValue = plugin.impl->getParameter (idx);

        setParameter (defaultValue, sendNotification);

        autodetectRange();
    }

    ~AirWindowsAutomatableParameter()
    {
        notifyListenersOfDeletion();
    }

    void resetToDefault()
    {
        setParameter (defaultValue, sendNotificationSync);
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

        stringToValueFunction = [this] (String v) { return conversionRange.convertTo0to1 (v.getFloatValue()); };
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

    String getCurrentValueAsString() override
    {
        char paramText[kVstMaxParamStrLen];
        plugin.impl->getParameterDisplay (index, paramText);
        auto t1 = String (paramText);

        char labelText[kVstMaxParamStrLen];
        plugin.impl->getParameterLabel (index, labelText);
        auto t2 = String (labelText);

        if (t2.isNotEmpty())
            return t1 + " " + t2;
        return t1;
    }

    void markAsChanged() const noexcept
    {
        getEdit().pluginChanged (plugin);
    }

    static String getParamId (AirWindowsPlugin& p, int idx)
    {
        return getParamName (p, idx).toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz");
    }

    static String getParamName (AirWindowsPlugin& p, int idx)
    {
        char paramName[kVstMaxParamStrLen];
        p.impl->getParameterName (idx, paramName);
        return String (paramName);
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

    dryGain->setParameter (0.0f, sendNotificationSync);
    wetGain->setParameter (1.0f, sendNotificationSync);
}

int AirWindowsPlugin::getNumOutputChannelsGivenInputs (int)
{
    return impl->getNumOutputs();
}

void AirWindowsPlugin::initialise (const PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
}

void AirWindowsPlugin::deinitialise()
{
}

void AirWindowsPlugin::applyToBuffer (const AudioRenderContext& fc)
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
    const int numChans    = buffer.getNumChannels();
    const int samps       = buffer.getNumSamples();
    const int pluginChans = jmax (impl->getNumOutputs(), impl->getNumInputs());

    if (pluginChans > numChans)
    {
        AudioScratchBuffer input (pluginChans, samps);
        AudioScratchBuffer output (pluginChans, samps);

        input.buffer.clear();
        output.buffer.clear();

        input.buffer.copyFrom (0, 0, buffer, 0, 0, samps);

        impl->processReplacing (input.buffer.getArrayOfWritePointers(),
                                output.buffer.getArrayOfWritePointers(),
                                samps);

        buffer.copyFrom (0, 0, output.buffer, 0, 0, samps);
    }
    else
    {
        AudioScratchBuffer output (numChans, samps);
        output.buffer.clear();

        impl->processReplacing (buffer.getArrayOfWritePointers(),
                                output.buffer.getArrayOfWritePointers(),
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
        MemoryBlock data;

        {
            MemoryOutputStream os (data, false);
            Base64::convertFromBase64 (os, v[IDs::state].toString());
        }

        impl->setChunk (data.getData(), int (data.getSize()), false);
    }

    CachedValue<float>* cvsFloat[]  = { &wetValue, &dryValue, nullptr };
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
        state.setProperty (IDs::state, Base64::toBase64 (data, size_t (size)), um);

        free (data);
    }
    else
    {
        state.removeProperty (IDs::state, um);
    }
}

//==============================================================================
const char* AirWindowsAcceleration::xmlTypeName = "airwindows_acceleration";
const char* AirWindowsADClip7::xmlTypeName = "airwindows_adclip7";
const char* AirWindowsADT::xmlTypeName = "airwindows_adt";
const char* AirWindowsAtmosphereBuss::xmlTypeName = "airwindows_atmosphere_buss";
const char* AirWindowsAtmosphereChannel::xmlTypeName = "airwindows_atmosphere";
const char* AirWindowsAura::xmlTypeName = "airwindows_aura";
const char* AirWindowsBassKit::xmlTypeName = "airwindows_basskit";
const char* AirWindowsBitGlitter::xmlTypeName = "airwindows_bitglitter";
const char* AirWindowsButterComp::xmlTypeName = "airwindows_buttercomp";
const char* AirWindowsButterComp2::xmlTypeName = "airwindows_buttercomp2";
const char* AirWindowsChannel4::xmlTypeName = "airwindows_channel4";
const char* AirWindowsChannel5::xmlTypeName = "airwindows_channel5";
const char* AirWindowsChorusEnsemble::xmlTypeName = "airwindows_chorusensemble";
const char* AirWindowsCrunchyGrooveWear::xmlTypeName = "airwindows_crunchygroovewear";
const char* AirWindowsDeEss::xmlTypeName = "airwindows_deess";
const char* AirWindowsDensity::xmlTypeName = "airwindows_density";
const char* AirWindowsDeRez::xmlTypeName = "airwindows_derez";
const char* AirWindowsDesk::xmlTypeName = "airwindows_desk";
const char* AirWindowsDesk4::xmlTypeName = "airwindows_desk4";
const char* AirWindowsDistance2::xmlTypeName = "airwindows_distance2";
const char* AirWindowsDrive::xmlTypeName = "airwindows_drive";
const char* AirWindowsDrumSlam::xmlTypeName = "airwindows_drumslam";
const char* AirWindowsDubSub::xmlTypeName = "airwindows_dubsub";
const char* AirWindowsEdIsDim::xmlTypeName = "airwindows_edisdim";
const char* AirWindowsElectroHat::xmlTypeName = "airwindows_electrohat";
const char* AirWindowsEnergy::xmlTypeName = "airwindows_energy";
const char* AirWindowsEnsemble::xmlTypeName = "airwindows_ensemble";
const char* AirWindowsFathomFive::xmlTypeName = "airwindows_fathomfive";
const char* AirWindowsFloor::xmlTypeName = "airwindows_floor";
const char* AirWindowsFromTape::xmlTypeName = "airwindows_fromtape";
const char* AirWindowsGatelope::xmlTypeName = "airwindows_gatelope";
const char* AirWindowsGolem::xmlTypeName = "airwindows_golem";
const char* AirWindowsGrooveWear::xmlTypeName = "airwindows_groovewear";
const char* AirWindowsGuitarConditioner::xmlTypeName = "airwindows_guitarconditioner";
const char* AirWindowsHardVacuum::xmlTypeName = "airwindows_hardvacuum";
const char* AirWindowsHombre::xmlTypeName = "airwindows_hombre";
const char* AirWindowsMelt::xmlTypeName = "airwindows_melt";
const char* AirWindowsMidSide::xmlTypeName = "airwindows_midside";
const char* AirWindowsNC17::xmlTypeName = "airwindows_nc17";
const char* AirWindowsNoise::xmlTypeName = "airwindows_noise";
const char* AirWindowsNonlinearSpace::xmlTypeName = "airwindows_nonlinearspace";
const char* AirWindowsPoint::xmlTypeName = "airwindows_point";
const char* AirWindowsPop::xmlTypeName = "airwindows_pop";
const char* AirWindowsPressure4::xmlTypeName = "airwindows_pressure4";
const char* AirWindowsPurestDrive::xmlTypeName = "airwindows_purestdrive";
const char* AirWindowsPurestWarm::xmlTypeName = "airwindows_purestwarm";
const char* AirWindowsRighteous4::xmlTypeName = "airwindows_righteous4";
const char* AirWindowsSingleEndedTriode::xmlTypeName = "airwindows_singleendedtriode";
const char* AirWindowsSlewOnly::xmlTypeName = "airwindows_slewonly";
const char* AirWindowsSpiral2::xmlTypeName = "airwindows_spiral2";
const char* AirWindowsStarChild::xmlTypeName = "airwindows_starchild";
const char* AirWindowsStereoFX::xmlTypeName = "airwindows_stereofx";
const char* AirWindowsSubsOnly::xmlTypeName = "airwindows_subsonly";
const char* AirWindowsSurge::xmlTypeName = "airwindows_surge";
const char* AirWindowsSwell::xmlTypeName = "airwindows_swell";
const char* AirWindowsTapeDust::xmlTypeName = "airwindows_tapedust";
const char* AirWindowsThunder::xmlTypeName = "airwindows_thunder";
const char* AirWindowsToTape5::xmlTypeName = "airwindows_totape5";
const char* AirWindowsToVinyl4::xmlTypeName = "airwindows_tovinyl4";
const char* AirWindowsTubeDesk::xmlTypeName = "airwindows_tubedesk";
const char* AirWindowsUnbox::xmlTypeName = "airwindows_unbox";
const char* AirWindowsVariMu::xmlTypeName = "airwindows_varimu";
const char* AirWindowsVoiceOfTheStarship::xmlTypeName = "airwindows_voiceofthestarship";
const char* AirWindowsWider::xmlTypeName = "airwindows_wider";

AirWindowsAcceleration::Type AirWindowsAcceleration::pluginType = AirWindowsPlugin::filter;
AirWindowsADClip7::Type AirWindowsADClip7::pluginType = AirWindowsPlugin::dynamics;
AirWindowsADT::Type AirWindowsADT::pluginType = AirWindowsPlugin::delay;
AirWindowsAtmosphereBuss::Type AirWindowsAtmosphereBuss::pluginType = AirWindowsPlugin::emulation;
AirWindowsAtmosphereChannel::Type AirWindowsAtmosphereChannel::pluginType = AirWindowsPlugin::emulation;
AirWindowsAura::Type AirWindowsAura::pluginType = AirWindowsPlugin::eq;
AirWindowsBassKit::Type AirWindowsBassKit::pluginType = AirWindowsPlugin::utility;
AirWindowsBitGlitter::Type AirWindowsBitGlitter::pluginType = AirWindowsPlugin::distortion;
AirWindowsButterComp::Type AirWindowsButterComp::pluginType = AirWindowsPlugin::dynamics;
AirWindowsButterComp2::Type AirWindowsButterComp2::pluginType = AirWindowsPlugin::dynamics;
AirWindowsChannel4::Type AirWindowsChannel4::pluginType = AirWindowsPlugin::emulation;
AirWindowsChannel5::Type AirWindowsChannel5::pluginType = AirWindowsPlugin::emulation;
AirWindowsChorusEnsemble::Type AirWindowsChorusEnsemble::pluginType = AirWindowsPlugin::modulation;
AirWindowsCrunchyGrooveWear::Type AirWindowsCrunchyGrooveWear::pluginType = AirWindowsPlugin::emulation;
AirWindowsDeEss::Type AirWindowsDeEss::pluginType = AirWindowsPlugin::utility;
AirWindowsDensity::Type AirWindowsDensity::pluginType = AirWindowsPlugin::distortion;
AirWindowsDeRez::Type AirWindowsDeRez::pluginType = AirWindowsPlugin::emulation;
AirWindowsDesk::Type AirWindowsDesk::pluginType = AirWindowsPlugin::emulation;
AirWindowsDesk4::Type AirWindowsDesk4::pluginType = AirWindowsPlugin::emulation;
AirWindowsDistance2::Type AirWindowsDistance2::pluginType = AirWindowsPlugin::imaging;
AirWindowsDrive::Type AirWindowsDrive::pluginType = AirWindowsPlugin::distortion;
AirWindowsDrumSlam::Type AirWindowsDrumSlam::pluginType = AirWindowsPlugin::emulation;
AirWindowsDubSub::Type AirWindowsDubSub::pluginType = AirWindowsPlugin::filter;
AirWindowsEdIsDim::Type AirWindowsEdIsDim::pluginType = AirWindowsPlugin::utility;
AirWindowsElectroHat::Type AirWindowsElectroHat::pluginType = AirWindowsPlugin::utility;
AirWindowsEnergy::Type AirWindowsEnergy::pluginType = AirWindowsPlugin::filter;
AirWindowsEnsemble::Type AirWindowsEnsemble::pluginType = AirWindowsPlugin::modulation;
AirWindowsFathomFive::Type AirWindowsFathomFive::pluginType = AirWindowsPlugin::eq;
AirWindowsFloor::Type AirWindowsFloor::pluginType = AirWindowsPlugin::utility;
AirWindowsFromTape::Type AirWindowsFromTape::pluginType = AirWindowsPlugin::emulation;
AirWindowsGatelope::Type AirWindowsGatelope::pluginType = AirWindowsPlugin::dynamics;
AirWindowsGolem::Type AirWindowsGolem::pluginType = AirWindowsPlugin::imaging;
AirWindowsGrooveWear::Type AirWindowsGrooveWear::pluginType = AirWindowsPlugin::emulation;
AirWindowsGuitarConditioner::Type AirWindowsGuitarConditioner::pluginType = AirWindowsPlugin::distortion;
AirWindowsHardVacuum::Type AirWindowsHardVacuum::pluginType = AirWindowsPlugin::emulation;
AirWindowsHombre::Type AirWindowsHombre::pluginType = AirWindowsPlugin::delay;
AirWindowsMelt::Type AirWindowsMelt::pluginType = AirWindowsPlugin::modulation;
AirWindowsMidSide::Type AirWindowsMidSide::pluginType = AirWindowsPlugin::utility;
AirWindowsNC17::Type AirWindowsNC17::pluginType = AirWindowsPlugin::distortion;
AirWindowsNoise::Type AirWindowsNoise::pluginType = AirWindowsPlugin::utility;
AirWindowsNonlinearSpace::Type AirWindowsNonlinearSpace::pluginType = AirWindowsPlugin::reverb;
AirWindowsPoint::Type AirWindowsPoint::pluginType = AirWindowsPlugin::dynamics;
AirWindowsPop::Type AirWindowsPop::pluginType = AirWindowsPlugin::dynamics;
AirWindowsPressure4::Type AirWindowsPressure4::pluginType = AirWindowsPlugin::dynamics;
AirWindowsPurestDrive::Type AirWindowsPurestDrive::pluginType = AirWindowsPlugin::distortion;
AirWindowsPurestWarm::Type AirWindowsPurestWarm::pluginType = AirWindowsPlugin::filter;
AirWindowsRighteous4::Type AirWindowsRighteous4::pluginType = AirWindowsPlugin::dynamics;
AirWindowsSingleEndedTriode::Type AirWindowsSingleEndedTriode::pluginType = AirWindowsPlugin::emulation;
AirWindowsSlewOnly::Type AirWindowsSlewOnly::pluginType = AirWindowsPlugin::utility;
AirWindowsSpiral2::Type AirWindowsSpiral2::pluginType = AirWindowsPlugin::distortion;
AirWindowsStarChild::Type AirWindowsStarChild::pluginType = AirWindowsPlugin::filter;
AirWindowsStereoFX::Type AirWindowsStereoFX::pluginType = AirWindowsPlugin::imaging;
AirWindowsSubsOnly::Type AirWindowsSubsOnly::pluginType = AirWindowsPlugin::utility;
AirWindowsSurge::Type AirWindowsSurge::pluginType = AirWindowsPlugin::dynamics;
AirWindowsSwell::Type AirWindowsSwell::pluginType = AirWindowsPlugin::dynamics;
AirWindowsTapeDust::Type AirWindowsTapeDust::pluginType = AirWindowsPlugin::distortion;
AirWindowsThunder::Type AirWindowsThunder::pluginType = AirWindowsPlugin::dynamics;
AirWindowsToTape5::Type AirWindowsToTape5::pluginType = AirWindowsPlugin::emulation;
AirWindowsToVinyl4::Type AirWindowsToVinyl4::pluginType = AirWindowsPlugin::emulation;
AirWindowsTubeDesk::Type AirWindowsTubeDesk::pluginType = AirWindowsPlugin::emulation;
AirWindowsUnbox::Type AirWindowsUnbox::pluginType = AirWindowsPlugin::distortion;
AirWindowsVariMu::Type AirWindowsVariMu::pluginType = AirWindowsPlugin::dynamics;
AirWindowsVoiceOfTheStarship::Type AirWindowsVoiceOfTheStarship::pluginType = AirWindowsPlugin::utility;
AirWindowsWider::Type AirWindowsWider::pluginType = AirWindowsPlugin::imaging;

AirWindowsAcceleration::AirWindowsAcceleration (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::acceleration::Acceleration> (&callback)) {}

AirWindowsADClip7::AirWindowsADClip7 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::adclip7::ADClip7> (&callback)) {}

AirWindowsADT::AirWindowsADT (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::adt::ADT> (&callback)) {}

AirWindowsAtmosphereBuss::AirWindowsAtmosphereBuss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::atmospherebuss::AtmosphereBuss> (&callback)) {}

AirWindowsAtmosphereChannel::AirWindowsAtmosphereChannel (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::atmospherechannel::AtmosphereChannel> (&callback)) {}

AirWindowsAura::AirWindowsAura (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::aura::Aura> (&callback)) {}

AirWindowsBassKit::AirWindowsBassKit (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::basskit::BassKit> (&callback)) {}

AirWindowsBitGlitter::AirWindowsBitGlitter (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::bitglitter::BitGlitter> (&callback)) {}

AirWindowsButterComp::AirWindowsButterComp (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::buttercomp::ButterComp> (&callback)) {}

AirWindowsButterComp2::AirWindowsButterComp2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::buttercomp2::ButterComp2> (&callback)) {}

AirWindowsChannel4::AirWindowsChannel4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::channel4::Channel4> (&callback)) {}

AirWindowsChannel5::AirWindowsChannel5 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::channel5::Channel5> (&callback)) {}

AirWindowsChorusEnsemble::AirWindowsChorusEnsemble (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::chorusensemble::ChorusEnsemble> (&callback)) {}

AirWindowsCrunchyGrooveWear::AirWindowsCrunchyGrooveWear (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::crunchygroovewear::CrunchyGrooveWear> (&callback)) {}

AirWindowsDeEss::AirWindowsDeEss (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::deess::DeEss> (&callback)) {}

AirWindowsDensity::AirWindowsDensity (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::density::Density> (&callback)) {}

AirWindowsDeRez::AirWindowsDeRez (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::derez::DeRez> (&callback)) {}

AirWindowsDesk::AirWindowsDesk (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::desk::Desk> (&callback)) {}

AirWindowsDesk4::AirWindowsDesk4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::desk4::Desk4> (&callback)) {}

AirWindowsDistance2::AirWindowsDistance2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::distance2::Distance2> (&callback)) {}

AirWindowsDrive::AirWindowsDrive (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::drive::Drive> (&callback)) {}

AirWindowsDrumSlam::AirWindowsDrumSlam (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::drumslam::DrumSlam> (&callback)) {}

AirWindowsDubSub::AirWindowsDubSub (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::dubsub::DubSub> (&callback)) {}

AirWindowsEdIsDim::AirWindowsEdIsDim (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::edisdim::EdIsDim> (&callback)) {}

AirWindowsElectroHat::AirWindowsElectroHat (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::electrohat::ElectroHat> (&callback)) {}

AirWindowsEnergy::AirWindowsEnergy (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::energy::Energy> (&callback)) {}

AirWindowsEnsemble::AirWindowsEnsemble (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::ensemble::Ensemble> (&callback)) {}

AirWindowsFathomFive::AirWindowsFathomFive (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::fathomfive::FathomFive> (&callback)) {}

AirWindowsFloor::AirWindowsFloor (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::awfloor::Floor> (&callback)) {}

AirWindowsFromTape::AirWindowsFromTape (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::fromtape::FromTape> (&callback)) {}

AirWindowsGatelope::AirWindowsGatelope (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::gatelope::Gatelope> (&callback)) {}

AirWindowsGolem::AirWindowsGolem (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::golem::Golem> (&callback)) {}

AirWindowsGrooveWear::AirWindowsGrooveWear (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::groovewear::GrooveWear> (&callback)) {}

AirWindowsGuitarConditioner::AirWindowsGuitarConditioner (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::guitarconditioner::GuitarConditioner> (&callback)) {}

AirWindowsHardVacuum::AirWindowsHardVacuum (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::hardvacuum::HardVacuum> (&callback)) {}

AirWindowsHombre::AirWindowsHombre (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::hombre::Hombre> (&callback)) {}

AirWindowsMelt::AirWindowsMelt (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::melt::Melt> (&callback)) {}

AirWindowsMidSide::AirWindowsMidSide (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::midside::MidSide> (&callback)) {}

AirWindowsNC17::AirWindowsNC17 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::nc17::NCSeventeen> (&callback)) {}

AirWindowsNoise::AirWindowsNoise (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::noise::Noise> (&callback)) {}

AirWindowsNonlinearSpace::AirWindowsNonlinearSpace (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::nonlinearspace::NonlinearSpace> (&callback)) {}

AirWindowsPoint::AirWindowsPoint (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::point::Point> (&callback)) {}

AirWindowsPop::AirWindowsPop (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::pop::Pop> (&callback)) {}

AirWindowsPressure4::AirWindowsPressure4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::pressure4::Pressure4> (&callback)) {}

AirWindowsPurestDrive::AirWindowsPurestDrive (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::purestdrive::PurestDrive> (&callback)) {}

AirWindowsPurestWarm::AirWindowsPurestWarm (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::purestwarm::PurestWarm> (&callback)) {}

AirWindowsRighteous4::AirWindowsRighteous4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::righteous4::Righteous4> (&callback)) {}

AirWindowsSingleEndedTriode::AirWindowsSingleEndedTriode (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::singleendedtriode::SingleEndedTriode> (&callback)) {}

AirWindowsSlewOnly::AirWindowsSlewOnly (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::slewonly::SlewOnly> (&callback)) {}

AirWindowsSpiral2::AirWindowsSpiral2 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::spiral2::Spiral2> (&callback)) {}

AirWindowsStarChild::AirWindowsStarChild (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::starchild::StarChild> (&callback)) {}

AirWindowsStereoFX::AirWindowsStereoFX (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::stereofx::StereoFX> (&callback)) {}

AirWindowsSubsOnly::AirWindowsSubsOnly (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::subsonly::SubsOnly> (&callback)) {}

AirWindowsSurge::AirWindowsSurge (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::surge::Surge> (&callback)) {}

AirWindowsSwell::AirWindowsSwell (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::swell::Swell> (&callback)) {}

AirWindowsTapeDust::AirWindowsTapeDust (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::tapedust::TapeDust> (&callback)) {}

AirWindowsThunder::AirWindowsThunder (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::thunder::Thunder> (&callback)) {}

AirWindowsToTape5::AirWindowsToTape5 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::totape5::ToTape5> (&callback)) {}

AirWindowsToVinyl4::AirWindowsToVinyl4 (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::tovinyl4::ToVinyl4> (&callback)) {}

AirWindowsTubeDesk::AirWindowsTubeDesk (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::tubedesk::TubeDesk> (&callback)) {}

AirWindowsUnbox::AirWindowsUnbox (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::unbox::UnBox> (&callback)) {}

AirWindowsVariMu::AirWindowsVariMu (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::varimu::VariMu> (&callback)) {}

AirWindowsVoiceOfTheStarship::AirWindowsVoiceOfTheStarship (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::voiceofthestarship::VoiceOfTheStarship> (&callback)) {}

AirWindowsWider::AirWindowsWider (PluginCreationInfo info)
    : AirWindowsPlugin (info, std::make_unique<airwindows::wider::Wider> (&callback)) {}

}
