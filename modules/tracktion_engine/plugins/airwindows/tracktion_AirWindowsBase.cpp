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

juce::String AirWindowsPlugin::getSelectableDescription()
{
    return getName() + " " + TRANS("Plugin");
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

    copyPropertiesToCachedValues (v, wetValue, dryValue);

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


} }
