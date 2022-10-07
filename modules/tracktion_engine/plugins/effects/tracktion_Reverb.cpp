/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

static constexpr float scalewet = 3;
static constexpr float scaledry = 2;
static constexpr float freezemode = 0.5f;

//==============================================================================
ReverbPlugin::ReverbPlugin (PluginCreationInfo info) : Plugin (info)
{
    roomSizeParam = addParam ("room size", TRANS("Room Size"), { 0.0f, 1.0f },
                              [] (float value)           { return juce::String (1 + (int) (10.0f * value)); },
                              [] (const juce::String& s) { return s.getFloatValue(); });

    dampParam     = addParam ("damping", TRANS("Damping"), { 0.0f, 1.0f },
                              [] (float value)           { return juce::String ((int) (100.0f * value)) + "%"; },
                              [] (const juce::String& s) { return s.getFloatValue(); });

    wetParam      = addParam ("wet level", TRANS("Wet Level"), { 0.0f, 1.0f },
                              [] (float value)           { return gainToDbString (scalewet * value); },
                              [] (const juce::String& s) { return s.getFloatValue(); });

    dryParam      = addParam ("dry level", TRANS("Dry Level"), { 0.0f, 1.0f },
                              [] (float value)           { return gainToDbString (scaledry * value); },
                              [] (const juce::String& s) { return s.getFloatValue(); });

    widthParam    = addParam ("width", TRANS("Width"), { 0.0f, 1.0f },
                              [] (float value)           { return juce::String ((int) (100.0f * value)) + "%"; },
                              [] (const juce::String& s) { return s.getFloatValue(); });

    modeParam     = addParam ("mode", TRANS("Freeze"), { 0.0f, 1.0f },
                              [] (float value)           { return value >= freezemode ? TRANS("On") : TRANS("Off"); },
                              [] (const juce::String& s) { return s.getFloatValue(); });

    auto um = getUndoManager();

    roomSizeValue.referTo (state, IDs::roomSize, um, 0.3f);
    dampValue.referTo (state, IDs::damp, um, 0.5f);
    wetValue.referTo (state, IDs::wet, um, 1.0f / scalewet);
    dryValue.referTo (state, IDs::dry, um, 0.5f);
    widthValue.referTo (state, IDs::width, um, 1.0f);
    modeValue.referTo (state, IDs::mode, um);

    roomSizeParam->attachToCurrentValue (roomSizeValue);
    dampParam->attachToCurrentValue (dampValue);
    wetParam->attachToCurrentValue (wetValue);
    dryParam->attachToCurrentValue (dryValue);
    widthParam->attachToCurrentValue (widthValue);
    modeParam->attachToCurrentValue (modeValue);
}

ReverbPlugin::~ReverbPlugin()
{
    notifyListenersOfDeletion();

    roomSizeParam->detachFromCurrentValue();
    dampParam->detachFromCurrentValue();
    wetParam->detachFromCurrentValue();
    dryParam->detachFromCurrentValue();
    widthParam->detachFromCurrentValue();
    modeParam->detachFromCurrentValue();
}

const char* ReverbPlugin::xmlTypeName = "reverb";

void ReverbPlugin::initialise (const PluginInitialisationInfo& info)
{
    outputSilent = true;
    reverb.setSampleRate (info.sampleRate);
}

void ReverbPlugin::deinitialise()
{
}

void ReverbPlugin::reset()
{
    reverb.reset();
}

static bool isNotSilent (float v) noexcept
{
    const float zeroThresh = 1.0f / 8000.0f;
    return v < -zeroThresh || v > zeroThresh;
}

static bool isSilent (const float* data, int num) noexcept
{
    if (isNotSilent (data[0]) || isNotSilent (data[num / 2]))
        return false;

    auto range = juce::FloatVectorOperations::findMinAndMax (data, num);

    return ! (isNotSilent (range.getStart())
               || isNotSilent (range.getEnd()));
}

void ReverbPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (fc.destBuffer != nullptr)
    {
        SCOPED_REALTIME_CHECK

        juce::Reverb::Parameters params;
        params.roomSize   = roomSizeParam->getCurrentValue();
        params.damping    = dampParam->getCurrentValue();
        params.wetLevel   = wetParam->getCurrentValue();
        params.dryLevel   = dryParam->getCurrentValue();
        params.width      = widthParam->getCurrentValue();
        params.freezeMode = modeParam->getCurrentValue();

        if (memcmp (&params, &reverb.getParameters(), sizeof (params)) != 0)
            reverb.setParameters (params);

        const int num = fc.bufferNumSamples;
        float* const left = fc.destBuffer->getWritePointer (0, fc.bufferStartSample);

        if (fc.destBuffer->getNumChannels() >= 2)
        {
            clearChannels (*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

            float* const right = fc.destBuffer->getWritePointer (1, fc.bufferStartSample);

            if (outputSilent && isSilent (left, num) && isSilent (right, num))
                return;

            reverb.processStereo (left, right, num);

            outputSilent = isSilent (left, num) && isSilent (right, num);
        }
        else
        {
            if (outputSilent && isSilent (left, num))
                return;

            reverb.processMono (left, num);

            outputSilent = isSilent (left, num);
        }

        zeroDenormalisedValuesIfNeeded (*fc.destBuffer);
    }
}

void ReverbPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    juce::CachedValue<float>* cvsFloat[]  = { &roomSizeValue, &dampValue, &wetValue, &dryValue, &widthValue, &modeValue, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

void ReverbPlugin::setRoomSize (float value)    { roomSizeParam->setParameter (juce::jlimit (0.0f, 1.0f, value), juce::sendNotification); }
float ReverbPlugin::getRoomSize()               { return roomSizeParam->getCurrentValue(); }

void ReverbPlugin::setDamp (float value)        { dampParam->setParameter (juce::jlimit (0.0f, 1.0f, value), juce::sendNotification); }
float ReverbPlugin::getDamp()                   { return dampParam->getCurrentValue(); }

void ReverbPlugin::setWet (float value)         { wetParam->setParameter (juce::jlimit (0.0f, 1.0f, value), juce::sendNotification); }
float ReverbPlugin::getWet()                    { return wetParam->getCurrentValue(); }

void ReverbPlugin::setDry (float value)         { dryParam->setParameter (juce::jlimit (0.0f, 1.0f, value), juce::sendNotification); }
float ReverbPlugin::getDry()                    { return dryParam->getCurrentValue(); }

void ReverbPlugin::setWidth (float value)       { widthParam->setParameter (juce::jlimit (0.0f, 1.0f, value), juce::sendNotification); }
float ReverbPlugin::getWidth()                  { return widthParam->getCurrentValue(); }

void ReverbPlugin::setMode (float value)        { modeParam->setParameter (juce::jlimit (0.0f, 1.0f, value), juce::sendNotification); }
float ReverbPlugin::getMode()                   { return modeParam->getCurrentValue(); }

}} // namespace tracktion { inline namespace engine
