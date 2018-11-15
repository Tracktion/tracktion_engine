/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct VolAutomatableParameter : public AutomatableParameter
{
    VolAutomatableParameter (const String& xmlTag, const String& name,
                             Plugin& owner, Range<float> valueRange)
        : AutomatableParameter (xmlTag, name, owner, valueRange)
    {
    }

    ~VolAutomatableParameter()
    {
        notifyListenersOfDeletion();
    }

    String valueToString (float value) override
    {
        return Decibels::toString (volumeFaderPositionToDB (value) + 0.001);
    }

    float stringToValue (const String& str) override
    {
        return decibelsToVolumeFaderPosition (dbStringToDb (str));
    }
};

//==============================================================================
struct PanAutomatableParameter : public AutomatableParameter
{
    PanAutomatableParameter (const String& xmlTag, const String& name, Plugin& owner, Range<float> valueRange)
        : AutomatableParameter (xmlTag, name, owner, valueRange)
    {
    }

    ~PanAutomatableParameter()
    {
        notifyListenersOfDeletion();
    }

    String valueToString (float value) override
    {
        return getPanString (value);
    }

    float stringToValue (const String& str) override
    {
        const float v = str.retainCharacters ("012345679.-").getFloatValue();
        return str.contains (TRANS("Left")) ? -v : v;
    }
};

//==============================================================================
struct MasterVolParameter  : public VolAutomatableParameter
{
    MasterVolParameter (const String& xmlTag, const String& name, Plugin& owner, Range<float> valueRange)
        : VolAutomatableParameter (xmlTag, name, owner, valueRange)
    {
    }

    ~MasterVolParameter()
    {
        notifyListenersOfDeletion();
    }

    String getPluginAndParamName() const override        { return getParameterName(); }
    String getFullName() const override                  { return getParameterName(); }
};

//==============================================================================
struct MasterPanParameter  : public PanAutomatableParameter
{
    MasterPanParameter (const String& xmlTag, const String& name, Plugin& owner, Range<float> valueRange)
        : PanAutomatableParameter (xmlTag, name, owner, valueRange)
    {
    }

    ~MasterPanParameter()
    {
        notifyListenersOfDeletion();
    }

    String getPluginAndParamName() const override        { return getParameterName(); }
    String getFullName() const override                  { return getParameterName(); }
};

//==============================================================================
VolumeAndPanPlugin::VolumeAndPanPlugin (PluginCreationInfo info, bool isMasterVolumeNode) : Plugin (info)
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

ValueTree VolumeAndPanPlugin::create()
{
    ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, xmlTypeName, nullptr);
    return v;
}

const char* VolumeAndPanPlugin::xmlTypeName = "volume";

//==============================================================================
void VolumeAndPanPlugin::initialise (const PlaybackInitialisationInfo&)
{
    refreshVCATrack();
    getGainsFromVolumeFaderPositionAndPan (getSliderPos(), getPan(), getPanLaw(), lastGainL, lastGainR);
}

void VolumeAndPanPlugin::initialiseWithoutStopping (const PlaybackInitialisationInfo&)
{
    refreshVCATrack();
}

void VolumeAndPanPlugin::deinitialise()
{
    vcaTrack = nullptr;
}

//==============================================================================
static float getParentVcaDb (Track& track, double time)
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

void VolumeAndPanPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (isEnabled())
    {
        SCOPED_REALTIME_CHECK

        if (fc.destBuffer != nullptr)
        {
            const int numChansIn = fc.destBuffer->getNumChannels();

            // If we have more than a stereo set of channel, ignore the pan
            if (numChansIn > 2)
            {
                // If the number of channels is greater than two, just apply volume
                const float vcaPosDelta = vcaTrack != nullptr
                                            ? decibelsToVolumeFaderPosition (getParentVcaDb (*vcaTrack, fc.getEditTime().editRange1.getStart()))
                                            - decibelsToVolumeFaderPosition (0.0f)
                                            : 0.0f;
                const float gain = volumeFaderPositionToGain (getSliderPos() + vcaPosDelta) * (polarity ? -1 : 1);

                for (int i = 0; i < numChansIn; ++i)
                    fc.destBuffer->applyGainRamp (i, fc.bufferStartSample, fc.bufferNumSamples, lastGainL, gain);

                lastGainL = gain;
            }
            else
            {
                const float vcaPosDelta = vcaTrack != nullptr
                                        ? decibelsToVolumeFaderPosition (getParentVcaDb (*vcaTrack, fc.getEditTime().editRange1.getStart()))
                                            - decibelsToVolumeFaderPosition (0.0f)
                                        : 0.0f;

                float lgain, rgain;
                getGainsFromVolumeFaderPositionAndPan (getSliderPos() + vcaPosDelta, getPan(), getPanLaw(), lgain, rgain);
                lgain *= (polarity ? -1 : 1);
                rgain *= (polarity ? -1 : 1);

                fc.destBuffer->applyGainRamp (0, fc.bufferStartSample, fc.bufferNumSamples, lastGainL, lgain);

                if (numChansIn > 1)
                    fc.destBuffer->applyGainRamp (1, fc.bufferStartSample, fc.bufferNumSamples, lastGainR, rgain);

                lastGainL = lgain;
                lastGainR = rgain;
            }
        }

        if (applyToMidi && fc.bufferForMidiMessages != nullptr)
            fc.bufferForMidiMessages->multiplyVelocities (volumeFaderPositionToGain (getSliderPos()));
    }
}

void VolumeAndPanPlugin::refreshVCATrack()
{
    vcaTrack = ignoreVca ? nullptr : dynamic_cast<AudioTrack*> (getOwnerTrack());
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
    volParam->setParameter (jlimit (0.0f, 1.0f, newV), sendNotification);
}

void VolumeAndPanPlugin::setPan (float p)
{
    if (p >= -0.005f && p <= 0.005f)
        p = 0.0f;

    panParam->setParameter (jlimit (-1.0f, 1.0f, p), sendNotification);
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

void VolumeAndPanPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    CachedValue<float>* cvsFloat[]  = { &volume, &pan, nullptr };
    CachedValue<int>* cvsInt[]      = { &panLaw, nullptr };
    CachedValue<bool>* cvsBool[]    = { &applyToMidi, &ignoreVca, &polarity, nullptr };

    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
    copyPropertiesToNullTerminatedCachedValues (v, cvsInt);
    copyPropertiesToNullTerminatedCachedValues (v, cvsBool);
}
