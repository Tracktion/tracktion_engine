/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

struct VolAutomatableParameter : public AutomatableParameter
{
    VolAutomatableParameter (const juce::String& xmlTag, const juce::String& name,
                             Plugin& owner, juce::Range<float> r)
        : AutomatableParameter (xmlTag, name, owner, r)
    {
    }

    ~VolAutomatableParameter() override
    {
        notifyListenersOfDeletion();
    }

    juce::String valueToString (float value) override
    {
        return juce::Decibels::toString (volumeFaderPositionToDB (value) + 0.001);
    }

    float stringToValue (const juce::String& str) override
    {
        return decibelsToVolumeFaderPosition (dbStringToDb (str));
    }
};

//==============================================================================
struct PanAutomatableParameter : public AutomatableParameter
{
    PanAutomatableParameter (const juce::String& xmlTag, const juce::String& name,
                             Plugin& owner, juce::Range<float> r)
        : AutomatableParameter (xmlTag, name, owner, r)
    {
    }

    ~PanAutomatableParameter() override
    {
        notifyListenersOfDeletion();
    }

    juce::String valueToString (float value) override
    {
        return getPanString (value);
    }

    float stringToValue (const juce::String& str) override
    {
        const float v = str.retainCharacters ("0123456789.-").getFloatValue();
        return str.contains (TRANS("Left")) ? -v : v;
    }
};

//==============================================================================
struct MasterVolParameter  : public VolAutomatableParameter
{
    MasterVolParameter (const juce::String& xmlTag, const juce::String& name,
                        Plugin& owner, juce::Range<float> r)
        : VolAutomatableParameter (xmlTag, name, owner, r)
    {
    }

    ~MasterVolParameter() override
    {
        notifyListenersOfDeletion();
    }

    juce::String getPluginAndParamName() const override        { return getParameterName(); }
    juce::String getFullName() const override                  { return getParameterName(); }
};

//==============================================================================
struct MasterPanParameter  : public PanAutomatableParameter
{
    MasterPanParameter (const juce::String& xmlTag, const juce::String& name,
                        Plugin& owner, juce::Range<float> r)
        : PanAutomatableParameter (xmlTag, name, owner, r)
    {
    }

    ~MasterPanParameter() override
    {
        notifyListenersOfDeletion();
    }

    juce::String getPluginAndParamName() const override        { return getParameterName(); }
    juce::String getFullName() const override                  { return getParameterName(); }
};

//==============================================================================
VolumeAndPanPlugin::VolumeAndPanPlugin (PluginCreationInfo info, bool isMasterVolumeNode)
    : Plugin (info),
      isMasterVolume (isMasterVolumeNode)
{
    auto um = getUndoManager();

    volume.referTo (state, IDs::volume, um, decibelsToVolumeFaderPosition (0));
    pan.referTo (state, IDs::pan, um);
    applyToMidi.referTo (state, IDs::applyToMidi, um);
    ignoreVca.referTo (state, IDs::ignoreVca, um);
    polarity.referTo (state, IDs::polarity, um);
    panLaw.referTo (state, IDs::panLaw, um);

    if (isMasterVolumeNode)
    {
        addAutomatableParameter (volParam = new MasterVolParameter ("master volume", TRANS("Master volume"), *this, { 0.0f, 1.0f }));
        addAutomatableParameter (panParam = new MasterPanParameter ("master pan", TRANS("Master pan"), *this, { -1.0f, 1.0f }));
    }
    else
    {
        addAutomatableParameter (volParam = new VolAutomatableParameter ("volume", TRANS("Volume"), *this, { 0.0f, 1.0f }));
        addAutomatableParameter (panParam = new PanAutomatableParameter ("pan", TRANS("Pan"), *this, { -1.0f, 1.0f }));
    }

    volParam->attachToCurrentValue (volume);
    panParam->attachToCurrentValue (pan);
}

VolumeAndPanPlugin::VolumeAndPanPlugin (Edit& ed, const juce::ValueTree& v, bool isMaster)
    : VolumeAndPanPlugin (PluginCreationInfo (ed, v, false), isMaster)
{
}

VolumeAndPanPlugin::~VolumeAndPanPlugin()
{
    notifyListenersOfDeletion();

    volParam->detachFromCurrentValue();
    panParam->detachFromCurrentValue();
}

juce::ValueTree VolumeAndPanPlugin::create()
{
    juce::ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, xmlTypeName, nullptr);
    return v;
}

const char* VolumeAndPanPlugin::xmlTypeName = "volume";

//==============================================================================
void VolumeAndPanPlugin::initialise (const PluginInitialisationInfo& info)
{
    refreshVCATrack();

    setSmoothedValueTargets (info.startTime, true);

    smoothedGainL.reset (info.sampleRate, smoothingRampTimeSeconds);
    smoothedGainR.reset (info.sampleRate, smoothingRampTimeSeconds);
    smoothedGain.reset (info.sampleRate, smoothingRampTimeSeconds);
}

void VolumeAndPanPlugin::initialiseWithoutStopping (const PluginInitialisationInfo&)
{
    TRACKTION_ASSERT_MESSAGE_THREAD
    refreshVCATrack();
}

void VolumeAndPanPlugin::deinitialise()
{
    juce::ReferenceCountedObjectPtr<AudioTrack> deleter;

    {
        const std::scoped_lock sl (vcaTrackLock);
        std::swap (vcaTrack, deleter);
    }
}

//==============================================================================
static float getParentVcaDb (Track& track, TimePosition time)
{
    float posOffset = 0.0f;

    for (FolderTrack::Ptr p = track.getParentFolderTrack(); p != nullptr; p = p->getParentFolderTrack())
    {
        float db = p->getVcaDb (time);

        if (db < -96.0f)
            return -100.0f;

        posOffset += decibelsToVolumeFaderPosition (db)
                       - decibelsToVolumeFaderPosition (0.0f);
    }

    return volumeFaderPositionToDB (decibelsToVolumeFaderPosition (0) + posOffset);
}

float VolumeAndPanPlugin::getVCAPosDelta (TimePosition time)
{
    const std::scoped_lock sl (vcaTrackLock);

    if (vcaTrack != nullptr)
        return decibelsToVolumeFaderPosition (getParentVcaDb (*vcaTrack, time))
                 - decibelsToVolumeFaderPosition (0.0f);

    return {};
}

void VolumeAndPanPlugin::setSmoothedValueTargets (TimePosition time, bool updateGain)
{
    auto sliderPos = getSliderPos() + getVCAPosDelta (time);

    float gainL, gainR;
    getGainsFromVolumeFaderPositionAndPan (sliderPos, getPan(), getPanLaw(), gainL, gainR);

    if (polarity)
    {
        gainL = -gainL;
        gainR = -gainR;
    }

    smoothedGainL.setTargetValue (gainL);
    smoothedGainR.setTargetValue (gainR);

    if (updateGain)
    {
        auto gain = volumeFaderPositionToGain (sliderPos);

        if (polarity)
            gain = -gain;

        smoothedGain.setTargetValue (gain);
    }
}

void VolumeAndPanPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (isEnabled())
    {
        SCOPED_REALTIME_CHECK

        if (auto buffer = fc.destBuffer)
        {
            const auto numChansIn = buffer->getNumChannels();

            setSmoothedValueTargets (fc.editTime.getStart(), numChansIn > 2);

            smoothedGainL.applyGain (buffer->getWritePointer (0, fc.bufferStartSample), fc.bufferNumSamples);

            if (numChansIn > 1)
            {
                smoothedGainR.applyGain (buffer->getWritePointer (1, fc.bufferStartSample), fc.bufferNumSamples);

                // If the number of channels is greater than two, apply volume to the rest
                if (numChansIn > 2)
                {
                    auto originalGain = smoothedGain;

                    for (int i = 2; i < numChansIn; ++i)
                    {
                        smoothedGain = originalGain;
                        smoothedGain.applyGain (buffer->getWritePointer (i, fc.bufferStartSample), fc.bufferNumSamples);
                    }
                }
            }
        }

        if (applyToMidi && fc.bufferForMidiMessages != nullptr)
            fc.bufferForMidiMessages->multiplyVelocities (volumeFaderPositionToGain (getSliderPos()));
    }
}

void VolumeAndPanPlugin::refreshVCATrack()
{
    juce::ReferenceCountedObjectPtr<AudioTrack> newVcaTrack (ignoreVca ? nullptr : dynamic_cast<AudioTrack*> (getOwnerTrack()));

    {
        const std::scoped_lock sl (vcaTrackLock);
        std::swap (vcaTrack, newVcaTrack);
    }
}

float VolumeAndPanPlugin::getVolumeDb() const
{
    return volumeFaderPositionToDB (volParam->getCurrentValue());
}

void VolumeAndPanPlugin::setVolumeDb (float vol)
{
    setSliderPos (decibelsToVolumeFaderPosition (vol));
}

void VolumeAndPanPlugin::setSliderPos (float newV)
{
    volParam->setParameter (juce::jlimit (0.0f, 1.0f, newV), juce::sendNotification);
}

void VolumeAndPanPlugin::setPan (float p)
{
    if (p >= -0.005f && p <= 0.005f)
        p = 0.0f;

    panParam->setParameter (juce::jlimit (-1.0f, 1.0f, p), juce::sendNotification);
}

PanLaw VolumeAndPanPlugin::getPanLaw() const noexcept
{
    PanLaw p = (PanLaw) panLaw.get();
    return p == PanLawDefault ? getDefaultPanLaw() : p;
}

//==============================================================================
void VolumeAndPanPlugin::setAppliedToMidiVolumes (bool b)
{
    if (b != applyToMidi)
    {
        applyToMidi = b;
        changed();
    }
}

void VolumeAndPanPlugin::muteOrUnmute()
{
    if (getVolumeDb() > -90.0f)
    {
        lastVolumeBeforeMute = getVolumeDb();
        setVolumeDb (lastVolumeBeforeMute - 0.01f); // needed so that automation is recorded correctly
        setVolumeDb (-100.0f);
    }
    else
    {
        if (lastVolumeBeforeMute < -100.0f)
            lastVolumeBeforeMute = 0.0f;

        setVolumeDb (getVolumeDb() + 0.01f); // needed so that automation is recorded correctly
        setVolumeDb (lastVolumeBeforeMute);
    }
}

void VolumeAndPanPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    copyPropertiesToCachedValues (v, volume, pan, panLaw, applyToMidi, ignoreVca, polarity);

    for (auto p : getAutomatableParameters())
        p->updateFromAttachedValue();
}

}} // namespace tracktion { inline namespace engine
