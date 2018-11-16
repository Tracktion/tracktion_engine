/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct PitchShiftPlugin::Pimpl
{
    static constexpr int samplesPerBlock = 512;

    Pimpl (PitchShiftPlugin& p) : owner (p)
    {
    }

    void initialise (double sr, float semitonesUp, TimeStretcher::Mode newMode, TimeStretcher::ElastiqueProOptions newOptions)
    {
        if (timestretcher == nullptr || mode != newMode || elastiqueOptions != newOptions)
        {
            mode = newMode;
            elastiqueOptions = newOptions;
            timestretcher = new TimeStretcher();
        }

        if (! timestretcher->isInitialised())
        {
            timestretcher->initialise (sr, samplesPerBlock, 2, mode, elastiqueOptions, true);
            jassert (timestretcher->isInitialised());
        }

        if (timestretcher->isInitialised())
        {
            timestretcher->reset();
            timestretcher->setSpeedAndPitch (1.0f, semitonesUp);

            latencySamples = timestretcher->getMaxFramesNeeded();

            inputFifo.setSize (2, latencySamples + 2000);
            outputFifo.setSize (2, latencySamples + 2000);
        }

        inputFifo.reset();
        outputFifo.reset();

        outputFifo.writeSilence (latencySamples);

        latencySeconds = latencySamples / sr;
    }

    void applyToBuffer (const AudioRenderContext& fc, float semis)
    {
        SCOPED_REALTIME_CHECK

        if (timestretcher->isInitialised())
        {
            auto newMode = (TimeStretcher::Mode) owner.mode.get();
            auto newOptions = owner.elastiqueOptions.get();

            if (newMode != mode || newOptions != elastiqueOptions)
                initialise (owner.sampleRate, semis, newMode, newOptions);

            inputFifo.write (*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples);

            int needed = timestretcher->getFramesNeeded();

            if (! timestretcher->setSpeedAndPitch (1.0f, semis))
                jassertfalse;

            while (inputFifo.getNumReady() >= needed && outputFifo.getFreeSpace() >= samplesPerBlock)
            {
                timestretcher->processData (inputFifo, needed, outputFifo);
                needed = timestretcher->getFramesNeeded();
            }

            if (outputFifo.getNumReady() < fc.bufferNumSamples)
            {
                jassertfalse;
                fc.destBuffer->clear (fc.bufferStartSample, fc.bufferNumSamples);
            }
            else
            {
                outputFifo.read (*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples);
            }

            if (fc.bufferForMidiMessages != nullptr)
                fc.bufferForMidiMessages->addToNoteNumbers (roundToInt (semis));
        }
    }

    PitchShiftPlugin& owner;

    ScopedPointer<TimeStretcher> timestretcher;
    TimeStretcher::Mode mode;
    TimeStretcher::ElastiqueProOptions elastiqueOptions;

    AudioFifo inputFifo { 2, 2000 }, outputFifo { 2, 2000 };

    double latencySeconds = 0.0;
    int latencySamples = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Pimpl)
};

//==============================================================================
PitchShiftPlugin::PitchShiftPlugin (PluginCreationInfo info) : Plugin (info)
{
    pimpl.reset (new Pimpl (*this));

    auto um = getUndoManager();

    semitones = addParam ("semitones up", TRANS("Semitones"),
                          { -PitchShiftPlugin::getMaximumSemitones(), PitchShiftPlugin::getMaximumSemitones() },
                          [] (float value)      { return std::abs (value) < 0.01f ? "(" + TRANS("Original pitch") + ")"
                                                                                  : getSemitonesAsString (value); },
                          [] (const String& s)  { return jlimit (-PitchShiftPlugin::getMaximumSemitones(),
                                                                 PitchShiftPlugin::getMaximumSemitones(),
                                                                 s.getFloatValue()); });

    semitonesValue.referTo (state, IDs::semitonesUp, um);
    mode.referTo (state, IDs::mode, um, (int) TimeStretcher::defaultMode);
    elastiqueOptions.referTo (state, IDs::elastiqueOptions, um);

    semitones->attachToCurrentValue (semitonesValue);
}

PitchShiftPlugin::PitchShiftPlugin (Edit& ed, const juce::ValueTree& v)
    : PitchShiftPlugin (PluginCreationInfo (ed, v, false))
{
}

PitchShiftPlugin::~PitchShiftPlugin()
{
    notifyListenersOfDeletion();

    semitones->detachFromCurrentValue();
}

ValueTree PitchShiftPlugin::create()
{
    ValueTree v (IDs::PLUGIN);
    v.setProperty (IDs::type, xmlTypeName, nullptr);
    return v;
}

const char* PitchShiftPlugin::xmlTypeName = "pitchShifter";

//==============================================================================
void PitchShiftPlugin::initialise (const PlaybackInitialisationInfo& info)
{
    pimpl->initialise (info.sampleRate, semitones->getCurrentValue(),
                       (TimeStretcher::Mode) mode.get(), elastiqueOptions.get());
}

void PitchShiftPlugin::deinitialise()
{
}

void PitchShiftPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    fc.setMaxNumChannels (2);
    pimpl->applyToBuffer (fc, semitones->getCurrentValue());
}

double PitchShiftPlugin::getLatencySeconds()
{
    return pimpl->latencySeconds;
}

String PitchShiftPlugin::getSelectableDescription()
{
    return TRANS("Pitch Shifter Plugin");
}

void PitchShiftPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    CachedValue<float>* cvsFloat[]  = { &semitonesValue, nullptr };
    CachedValue<int>* cvsInt[]      = { &mode, nullptr };
    copyPropertiesToNullTerminatedCachedValues (v, cvsFloat);
    copyPropertiesToNullTerminatedCachedValues (v, cvsInt);
}
