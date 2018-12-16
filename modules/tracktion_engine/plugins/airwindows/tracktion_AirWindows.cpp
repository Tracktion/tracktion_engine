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
        
        setParameter (plugin.impl->getParameter (idx), sendNotification);
    }
    
    ~AirWindowsAutomatableParameter()
    {
        notifyListenersOfDeletion();
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
        return String (paramText).substring (0, 4);
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
};
    
//==============================================================================
AirWindowsPlugin::AirWindowsPlugin (PluginCreationInfo info, std::unique_ptr<AirWindowsBase> base)
    : Plugin (info), callback (*this), impl (std::move (base))
{
    auto um = getUndoManager();
    
    restorePluginStateFromValueTree (state);
    
    for (int i = 0; i < impl->getNumParameters(); i++)
    {
        if (AirWindowsAutomatableParameter::getParamName (*this, i) == "Dry/Wet")
            continue;
        
        auto param = new AirWindowsAutomatableParameter (*this, i);
        
        addAutomatableParameter (param);
        parameters.add (param);
    }
    
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
    const int numChans = buffer.getNumChannels();
    const int samps    = buffer.getNumSamples();
    
    AudioScratchBuffer output (numChans, samps);
    output.buffer.clear();
    
    impl->processReplacing (buffer.getArrayOfWritePointers(),
                            output.buffer.getArrayOfWritePointers(),
                            samps);
    
    for (int i = 0; i < numChans; ++i)
        buffer.copyFrom (i, 0, output.buffer, i, 0, samps);
}
    
void AirWindowsPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
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
const char* AirWindowsDeEss::xmlTypeName = "airwindows_deess";
const char* AirWindowsDrive::xmlTypeName = "airwindows_drive";
const char* AirWindowsHardVacuum::xmlTypeName = "airwindows_hardvacuum";
const char* AirWindowsNonlinearSpace::xmlTypeName = "airwindows_nonlinearspace";
const char* AirWindowsPurestDrive::xmlTypeName = "airwindows_purestdrive";
const char* AirWindowsTubeDesk::xmlTypeName = "airwindows_tubedesk";

AirWindowsDeEss::AirWindowsDeEss (PluginCreationInfo info) : AirWindowsPlugin (info, std::make_unique<airwindows::deess::DeEss> (&callback)) {}
AirWindowsDrive::AirWindowsDrive (PluginCreationInfo info) : AirWindowsPlugin (info, std::make_unique<airwindows::drive::Drive> (&callback)) {}
AirWindowsHardVacuum::AirWindowsHardVacuum (PluginCreationInfo info) : AirWindowsPlugin (info, std::make_unique<airwindows::hardvacuum::HardVacuum> (&callback)) {}
AirWindowsNonlinearSpace::AirWindowsNonlinearSpace (PluginCreationInfo info) : AirWindowsPlugin (info, std::make_unique<airwindows::nonlinearspace::NonlinearSpace> (&callback)) {}
AirWindowsPurestDrive::AirWindowsPurestDrive (PluginCreationInfo info) : AirWindowsPlugin (info, std::make_unique<airwindows::purestdrive::PurestDrive> (&callback)) {}
AirWindowsTubeDesk::AirWindowsTubeDesk (PluginCreationInfo info) : AirWindowsPlugin (info, std::make_unique<airwindows::tubedesk::TubeDesk> (&callback)) {}

}
