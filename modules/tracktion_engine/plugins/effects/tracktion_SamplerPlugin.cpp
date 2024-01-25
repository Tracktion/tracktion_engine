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

// this must be high enough for low freq sounds not to click
static constexpr int minimumSamplesToPlayWhenStopping = 8;
static constexpr int maximumSimultaneousNotes = 32;


struct SamplerPlugin::SampledNote   : public ReferenceCountedObject
{
public:
    SampledNote (int midiNote, int keyNote,
                 float velocity,
                 const AudioFile& file,
                 double sampleRate,
                 int sampleDelayFromBufferStart,
                 const juce::AudioBuffer<float>& data,
                 int lengthInSamples,
                 float gainDb,
                 float pan
                 // BEAT CONNECT MODIFICATION START
                 , bool openEnded_,
                 const FilterType filterType_ = FilterType::noFilter, 
                 const double filterFrequency_ = 0, 
                 const double filterGain_ = 0,
                 // =8> make sure all these defaults are accurate later =8>
                 const float chorusDepth_ = 0,
                 const float chorusMix_ = 0,
                 const bool chorusOn_ = false, // The chorus' bypass state
                 const float chorusSpeed_ = 0,
                 const float chrousWidth_ = 0,
                 const float delay_ = 0, // The delay's length
                 const float delayCrossfeed_ = 0,
                 const float delayFeedback_ = 0,
                 const float delayMix_ = 0,
                 const bool delayOn_ = 0, // The delay's bypass state
                 const float distortion_ = 0, // The distortion's intensity
                 const bool distortionOn_ = false,
                 const float reverbDamping_ = 0,
                 const float reverbMix_ = 0,
                 const bool reverbOn_ = false, // Reverb's bypass state
                 const float reverbSize_ = 0,
                 const float reverbWidth_ = 0
                 // =8> make sure all these defaults are accurate later =8>
                 // BEAT CONNECT MODIFICATION END
                 )
       : note (midiNote),
         offset (-sampleDelayFromBufferStart),
         audioData (data),
         openEnded (openEnded_)
         // BEAT CONNECT MODIFICATION START
         , filterType (filterType_),
         chorusDepth(chorusDepth_),
         chorusMix(chorusMix_),
         chorusOn(chorusOn_),
         chorusSpeed(chorusSpeed_),
         chorusWidth(chrousWidth_),
         delay(delay_),
         delayCrossfeed(delayCrossfeed_),
         delayFeedback(delayFeedback_),
         delayMix(delayMix_),
         delayOn(delayOn_),
         distortion(distortion_),
         distortionOn(distortionOn_),
         reverbDamping(reverbDamping_),
         reverbMix(reverbMix_),
         reverbOn(reverbOn_),
         reverbSize(reverbSize_),
         reverbWidth(reverbWidth_)
         // BEAT CONNECT MODIFICATION END
    {
        resampler[0].reset();
        resampler[1].reset();

        const float volumeSliderPos = decibelsToVolumeFaderPosition (gainDb - (20.0f * (1.0f - velocity)));
        getGainsFromVolumeFaderPositionAndPan (volumeSliderPos, pan, getDefaultPanLaw(), gains[0], gains[1]);

        const double hz = juce::MidiMessage::getMidiNoteInHertz (midiNote);
        playbackRatio = hz / juce::MidiMessage::getMidiNoteInHertz (keyNote);
        playbackRatio *= file.getSampleRate() / sampleRate;
        samplesLeftToPlay = playbackRatio > 0 ? (1 + (int) (lengthInSamples / playbackRatio)) : 0;
        setCoefficients(filterType_, filterFrequency_, filterGain_);
    }

    void addNextBlock(juce::AudioBuffer<float>& outBuffer, int startSamp, int numSamples, EffectsModule& effectsModule_in)
    {
        jassert (! isFinished);

        if (offset < 0)
        {
            const int num = std::min (-offset, numSamples);
            startSamp += num;
            numSamples -= num;
            offset += num;
        }

        auto numSamps = std::min (numSamples, samplesLeftToPlay);

        if (numSamps > 0)
        {       
            AudioScratchBuffer scratch(audioData.getNumChannels(), numSamps);
            for (int i = scratch.buffer.getNumChannels(); --i >= 0;)
                scratch.buffer.copyFrom(i, 0, audioData, i, offset, numSamps);

            // BEATCONNECT MODIFICATION START
            effectsModule_in.applyEffects(scratch.buffer);
            // BEATCONNECT MODIFICATION END

            if (filterType != FilterType::noFilter)
            {
                iirFilterR.processSamples(scratch.buffer.getWritePointer(0), scratch.buffer.getNumSamples());
                iirFilterL.processSamples(scratch.buffer.getWritePointer(1), scratch.buffer.getNumSamples());
            }

            int numUsed = 0;

            for (int i = std::min (2, outBuffer.getNumChannels()); --i >= 0;)
            {
                numUsed = resampler[i]
                            .processAdding (playbackRatio,
                                            scratch.buffer.getReadPointer (std::min (i, scratch.buffer.getNumChannels() - 1), 0),
                                            outBuffer.getWritePointer (i, startSamp),
                                            numSamps,
                                            gains[i]);
            }

            offset += numUsed;
            samplesLeftToPlay -= numSamps;

            jassert (offset <= audioData.getNumSamples());
        }

        if (numSamples > numSamps && startFade > 0.0f)
        {
            startSamp += numSamps;
            numSamps = numSamples - numSamps;
            float endFade;

            if (numSamps > 100)
            {
                endFade = 0.0f;
                numSamps = 100;
            }
            else
            {
                endFade = std::max (0.0f, startFade - numSamps * 0.01f);
            }

            const int numSampsNeeded = 2 + juce::roundToInt ((numSamps + 2) * playbackRatio);
            AudioScratchBuffer scratch (audioData.getNumChannels(), numSampsNeeded + 8);

            if (offset + numSampsNeeded < audioData.getNumSamples())
            {
                for (int i = scratch.buffer.getNumChannels(); --i >= 0;)
                    scratch.buffer.copyFrom (i, 0, audioData, i, offset, numSampsNeeded);
                if (filterType != FilterType::noFilter)
                {
                    iirFilterR.processSamples(scratch.buffer.getWritePointer(0), scratch.buffer.getNumSamples());
                    iirFilterL.processSamples(scratch.buffer.getWritePointer(1), scratch.buffer.getNumSamples());
                }
            }
            else
            {
                scratch.buffer.clear();
            }

            if (numSampsNeeded > 2)
                AudioFadeCurve::applyCrossfadeSection (scratch.buffer, 0, numSampsNeeded - 2,
                                                       AudioFadeCurve::linear, startFade, endFade);

            startFade = endFade;

            int numUsed = 0;

            for (int i = std::min (2, outBuffer.getNumChannels()); --i >= 0;)
                numUsed = resampler[i].processAdding (playbackRatio,
                                                      scratch.buffer.getReadPointer (std::min (i, scratch.buffer.getNumChannels() - 1)),
                                                      outBuffer.getWritePointer (i, startSamp),
                                                      numSamps, gains[i]);

            offset += numUsed;

            if (startFade <= 0.0f)
                isFinished = true;
        }
    }

    // BEAT CONNECT MODIFICATION START
    void setCoefficients(const FilterType filter, const double frequency, const double gainFactor = 0)
    {
        auto& engine = *tracktion::engine::Engine::getEngines()[0];
        const double engineSampleRate = engine.getDeviceManager().getSampleRate();
        switch (filter) {
            case FilterType::noFilter: {
                return;
            }
            case FilterType::lowpass: {
                coefs = juce::IIRCoefficients::makeLowPass(engineSampleRate, frequency, iirFilterQuotient);
                break;
            }
            case FilterType::highpass: {
                coefs = juce::IIRCoefficients::makeHighPass(engineSampleRate, frequency, iirFilterQuotient);
                break;
            }
            case FilterType::bandpass: {
                coefs = juce::IIRCoefficients::makeBandPass(engineSampleRate, frequency, iirFilterQuotient);
                break;
            }
            case FilterType::notch: {
                coefs = juce::IIRCoefficients::makeNotchFilter(engineSampleRate, frequency, iirFilterQuotient);
                break;
            }
            case FilterType::allpass: {
                coefs = juce::IIRCoefficients::makeAllPass(engineSampleRate, frequency, iirFilterQuotient);
                break;
            }
            case FilterType::lowshelf: {
                coefs = juce::IIRCoefficients::makeLowShelf(engineSampleRate, frequency, iirFilterQuotient, (float)gainFactor);
                break;
            }
            case FilterType::highshelf: {
                coefs = juce::IIRCoefficients::makeHighShelf(engineSampleRate, frequency, iirFilterQuotient, (float)gainFactor);
                break;
            }
            case FilterType::peak: {
                coefs = juce::IIRCoefficients::makePeakFilter(engineSampleRate, frequency, iirFilterQuotient, (float)gainFactor);
                break;
            }
        }
        iirFilterR.setCoefficients(coefs);
        iirFilterL.setCoefficients(coefs);
        iirFilterR.reset();
        iirFilterL.reset();
        return;
    }
    // BEAT CONNECT MODIFICATION END

    juce::LagrangeInterpolator resampler[2];
    int note;
    int offset, samplesLeftToPlay = 0;
    float gains[2];
    double playbackRatio = 1.0;
    const juce::AudioBuffer<float>& audioData;
    float lastVals[4] = { 0, 0, 0, 0 };
    float startFade = 1.0f;
    bool openEnded, isFinished = false;
    // BEATCONNECT MODIFICATION START
    FilterType filterType;
    juce::IIRFilter iirFilterR;
    juce::IIRFilter iirFilterL;
    juce::IIRCoefficients coefs;
    double iirFilterQuotient = 0.710624337f;
    float chorusDepth; // =8> make sure all these defaults are accurate later =8>
    float chorusMix;
    bool chorusOn; // The chorus' bypass state
    float chorusSpeed;
    float chorusWidth;
    float delayCrossfeed;
    float delayFeedback;
    float delay; // The delay's length
    float delayMix;
    bool delayOn; // The delay's bypass state
    float distortion;// The distortion's intensity
    bool distortionOn;
    float reverbDamping;
    float reverbMix;
    bool reverbOn; // Reverb's bypass state
    float reverbSize;
    float reverbWidth;
    // BEATCONNECT MODIFICATION END

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampledNote)
};

//==============================================================================
SamplerPlugin::SamplerPlugin (PluginCreationInfo info)  : Plugin (info)
{
    triggerAsyncUpdate(); // =8> Is the placement of this line important?
    // BEATCONNECT MODIFICATION START
    // =8> Get the default values from FourOsc
    /*chorusMixValue.referTo(state, IDs::chorusMix, nullptr, 0.0f);
    delayMixValue.referTo(state, IDs::delayMix, nullptr, 0.0f);
    distortionMixValue.referTo(state, IDs::distortionMix, nullptr, 0.0f);
    reverbMixValue.referTo(state, IDs::reverbMix, nullptr, 0.0f);

    chorusMix = addParam("chorusMix", TRANS("Chorus Mix"), {0.0f, 0.0f, 0.0f, 0.0f});
    delayMix = addParam("delayMix", TRANS("Delay Mix"), {0.0f, 0.0f, 0.0f, 0.0f});
    distortionMix = addParam("distortionMix", TRANS("Distortion Mix"), {0.0f, 0.0f, 0.0f, 0.0f});
    reverbMix = addParam("reverbMix", TRANS("Reverb Mix"), {0.0f, 0.0f, 0.0f, 0.0f});

    chorusMix->attachToCurrentValue(chorusMixValue);
    delayMix->attachToCurrentValue(delayMixValue);
    distortionMix->attachToCurrentValue(distortionMixValue);
    reverbMix->attachToCurrentValue(reverbMixValue);*/
    // BEATCONNECT MODIFICATION END
}

SamplerPlugin::~SamplerPlugin()
{
    notifyListenersOfDeletion();
}

const char* SamplerPlugin::xmlTypeName = "sampler";
// BEATCONNECT MODIFICATION START
const char* SamplerPlugin::uniqueId = "2dfa0d4a-fa7f-4785-b7c2-35bffc65ecd0";
// BEATCONNECT MODIFICATION END

void SamplerPlugin::valueTreeChanged()
{
    triggerAsyncUpdate();
    Plugin::valueTreeChanged();
}

void SamplerPlugin::handleAsyncUpdate()
{
    juce::OwnedArray<SamplerSound> newSounds;

    auto numSounds = state.getNumChildren();

    for (int i = 0; i < numSounds; ++i)
    {
        auto v = getSound (i);

        if (v.hasType (IDs::SOUND))
        {
            auto s = new SamplerSound (*this,
                                       v[IDs::source].toString(),
                                       v[IDs::name],
                                       v[IDs::startTime],
                                       v[IDs::length],
                                       v[IDs::gainDb]);

            s->keyNote      = juce::jlimit (0, 127, static_cast<int> (v[IDs::keyNote]));
            s->minNote      = juce::jlimit (0, 127, static_cast<int> (v[IDs::minNote]));
            s->maxNote      = juce::jlimit (0, 127, static_cast<int> (v[IDs::maxNote]));
            s->pan          = juce::jlimit (-1.0f, 1.0f, static_cast<float> (v[IDs::pan]));
            s->openEnded    = v[IDs::openEnded];

            // BEATCONNECT MODIFICATION START
            s->filterType = (FilterType)(int)v[IDs::filterType];
            
            if (s->filterType != FilterType::noFilter)
                s->filterFrequency = v[IDs::filterFreq];

            if (s->filterType == FilterType::lowshelf ||
                s->filterType == FilterType::highshelf ||
                s->filterType == FilterType::peak)
                s->filterGain = v[IDs::filterGain];
            // BEATCONNECT MODIFICATION END

            newSounds.add (s);
        }
    }

    for (auto newSound : newSounds)
    {
        for (auto s : soundList)
        {
            if (s->source == newSound->source
                && s->startTime == newSound->startTime
                && s->length == newSound->length)
            {
                newSound->audioFile = s->audioFile;
                newSound->fileStartSample = s->fileStartSample;
                newSound->fileLengthSamples = s->fileLengthSamples;
                newSound->audioData = s->audioData;
            }
        }
    }

    {
        const juce::ScopedLock sl (lock);
        allNotesOff();
        soundList.swapWith (newSounds);

        sourceMediaChanged();
    }

    newSounds.clear();
    changed();
}

void SamplerPlugin::initialise (const PluginInitialisationInfo&)
{
    const juce::ScopedLock sl (lock);
    allNotesOff();
}

void SamplerPlugin::deinitialise()
{
    allNotesOff();
}

//==============================================================================
void SamplerPlugin::playNotes (const juce::BigInteger& keysDown)
{
    const juce::ScopedLock sl (lock);

    if (highlightedNotes != keysDown)
    {
        for (int i = playingNotes.size(); --i >= 0;)
            if ((! keysDown [playingNotes.getUnchecked(i)->note])
                 && highlightedNotes [playingNotes.getUnchecked(i)->note]
                 && ! playingNotes.getUnchecked(i)->openEnded)
                playingNotes.getUnchecked(i)->samplesLeftToPlay = minimumSamplesToPlayWhenStopping;

        for (int note = 128; --note >= 0;)
        {
            if (keysDown [note] && ! highlightedNotes [note])
            {
                for (auto ss : soundList)
                {
                    if (ss->minNote <= note
                         && ss->maxNote >= note
                         && ss->audioData.getNumSamples() > 0
                         && (! ss->audioFile.isNull())
                         && playingNotes.size() < maximumSimultaneousNotes)
                    {
                        playingNotes.add (new SampledNote (note,
                                                           ss->keyNote,
                                                           0.75f,
                                                           ss->audioFile,
                                                           sampleRate,
                                                           0,
                                                           ss->audioData,
                                                           ss->fileLengthSamples,
                                                           ss->gainDb,
                                                           ss->pan,
                                                           ss->openEnded
                                                           // BEATCONNECT MODIFICATION START
                                                           , ss->filterType,
                                                           ss->filterFrequency,
                                                           ss->filterGain
                                                           // BEATCONNECT MODIFICATION END
                                                           ));
                    }
                }
            }
        }

        highlightedNotes = keysDown;
    }
}

void SamplerPlugin::allNotesOff()
{
    const juce::ScopedLock sl (lock);
    playingNotes.clear();
    highlightedNotes.clear();
}

void SamplerPlugin::applyToBuffer (const PluginRenderContext& fc)
{
    if (fc.destBuffer != nullptr)
    {
        SCOPED_REALTIME_CHECK

        const juce::ScopedLock sl (lock);

        clearChannels (*fc.destBuffer, 2, -1, fc.bufferStartSample, fc.bufferNumSamples);

        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff)
            {
                playingNotes.clear();
                highlightedNotes.clear();
            }

            for (auto& m : *fc.bufferForMidiMessages)
            {
                if (m.isNoteOn())
                {
                    const int note = m.getNoteNumber();
                    const int noteTimeSample = juce::roundToInt (m.getTimeStamp() * sampleRate);

                    for (auto playingNote : playingNotes)
                    {
                        if (playingNote->note == note && ! playingNote->openEnded)
                        {
                            playingNote->samplesLeftToPlay = std::min (playingNote->samplesLeftToPlay,
                                                                       std::max (minimumSamplesToPlayWhenStopping,
                                                                                 noteTimeSample));
                            highlightedNotes.clearBit (note);
                        }
                    }

                    for (auto ss : soundList)
                    {
                        int adjustedMidiNote = note;
                        if (ss->minNote <= note
                            && ss->maxNote >= note
                            && ss->audioData.getNumSamples() > 0
                            && playingNotes.size() < maximumSimultaneousNotes)
                        {
                        // BEATCONNECT MODIFICATION START
                        if (fc.bufferForMidiMessages->size() > 1 && &m != fc.bufferForMidiMessages->begin() && (&m - 1)->isPitchWheel()) {
                            const double pitchWheelPosition = (&m - 1)->getPitchWheelValue();
                            
                            // Given the variables keyNote, pitchWheelPosition, and pitchWheelSemitoneRange,
                            // and given system constants, solve for the new note values
                            double noteHz = bc::MidiNote::getNoteFrequency(ss->keyNote);
                            double newNoteHz = bc::MidiNote::getFrequencyByPitchWheel(pitchWheelPosition, DrumMachinePlugin::pitchWheelSemitoneRange, noteHz);
                            adjustedMidiNote = bc::MidiNote::getMidiNote(newNoteHz);
                        }
                        // BEATCONNECT MODIFICATION END

                        highlightedNotes.setBit(note);
                        playingNotes.add(new SampledNote
                            // BEATCONNECT MODIFICATION START
                            (adjustedMidiNote,
                                // BEATCONNECT MODIFICATION END
                                ss->keyNote,
                                m.getVelocity() / 127.0f,
                                ss->audioFile,
                                sampleRate,
                                noteTimeSample,
                                ss->audioData,
                                ss->fileLengthSamples,
                                ss->gainDb,
                                ss->pan,
                                ss->openEnded
                                // BEATCONNECT MODIFICATION START
                                , ss->filterType,
                                ss->filterFrequency,
                                ss->filterGain
                                // BEATCONNECT MODIFICATION END
                            ));
                        }
                    }
                }
                else if (m.isNoteOff())
                {
                    const int note = m.getNoteNumber();
                    const int noteTimeSample = juce::roundToInt (m.getTimeStamp() * sampleRate);

                    for (auto playingNote : playingNotes)
                    {
                        if (playingNote->note == note && ! playingNote->openEnded)
                        {
                            playingNote->samplesLeftToPlay = std::min (playingNote->samplesLeftToPlay,
                                                                       std::max (minimumSamplesToPlayWhenStopping,
                                                                                 noteTimeSample));

                            highlightedNotes.clearBit (note);
                        }
                    }
                }
                else if (m.isAllNotesOff() || m.isAllSoundOff())
                {
                    playingNotes.clear();
                    highlightedNotes.clear();
                }
            }
        }

        for (int i = playingNotes.size(); --i >= 0;)
        {
            auto sn = playingNotes.getUnchecked (i);

            // BEATCONNECT MODIFICATIONS START
            auto effectsModulesIterator = effectsModules.find(sn->note);
            EffectsModule& effectsMod();

            jassert(effectsModulesIterator != effectsModules.end());
            if (effectsModulesIterator == effectsModules.end())
            {
                // =8> fill in a proper failure case.
                // Explode in flames
            }
                
             
            // BEATCONNECT MODIFICATIONS END

            sn->addNextBlock (*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples
            // BEATCONNECT MODIFICATIONS START
                             ,
                             effectsModulesIterator->second
            // BEATCONNECT MODIFICATIONS END
            );

            if (sn->isFinished)
                playingNotes.remove (i);
        }
    }
}

//==============================================================================
int SamplerPlugin::getNumSounds() const
{
    return std::accumulate (state.begin(), state.end(), 0,
                            [] (int total, auto v) { return total + (v.hasType (IDs::SOUND) ? 1 : 0); });
}

juce::String SamplerPlugin::getSoundName (int index) const
{
    return getSound (index)[IDs::name];
}

void SamplerPlugin::setSoundName (int index, const juce::String& n)
{
    getSound (index).setProperty (IDs::name, n, getUndoManager());
}

bool SamplerPlugin::hasNameForMidiNoteNumber (int note, int, juce::String& noteName)
{
    juce::String s;

    {
        const juce::ScopedLock sl (lock);

        for (auto ss : soundList)
        {
            if (ss->minNote <= note && ss->maxNote >= note)
            {
                if (s.isNotEmpty())
                    s << " + " << ss->name;
                else
                    s = ss->name;
            }
        }
    }

    noteName = s;
    return true;
}

AudioFile SamplerPlugin::getSoundFile (int index) const
{
    const juce::ScopedLock sl (lock);

    if (auto s = soundList[index])
        return s->audioFile;

    return AudioFile (edit.engine);
}

juce::String SamplerPlugin::getSoundMedia (int index) const
{
    const juce::ScopedLock sl (lock);

    if (auto s = soundList[index])
        return s->source;

    return {};
}

int SamplerPlugin::getKeyNote (int index) const             { return getSound (index)[IDs::keyNote]; }
int SamplerPlugin::getMinKey (int index) const              { return getSound (index)[IDs::minNote]; }
int SamplerPlugin::getMaxKey (int index) const              { return getSound (index)[IDs::maxNote]; }
float SamplerPlugin::getSoundGainDb (int index) const       { return getSound (index)[IDs::gainDb]; }
float SamplerPlugin::getSoundPan (int index) const          { return getSound (index)[IDs::pan]; }
double SamplerPlugin::getSoundStartTime (int index) const   { return getSound (index)[IDs::startTime]; }
bool SamplerPlugin::isSoundOpenEnded (int index) const      { return getSound (index)[IDs::openEnded]; }
// BEATCONNECT MODIFICATION START
FilterType SamplerPlugin::getFilterType(const int index) const
{
    return (FilterType)(int)getSound (index)[IDs::filterType];
}
double SamplerPlugin::getFilterFrequency(const int index) const { return getSound(index)[IDs::filterFreq]; }
double SamplerPlugin::getFilterGain(const int index) const      { return getSound(index)[IDs::filterGain]; }
// BEATCONNECT MODIFICATION END

double SamplerPlugin::getSoundLength (int index) const
{
    const double l = getSound (index)[IDs::length];

    if (l == 0.0)
    {
        const juce::ScopedLock sl (lock);

        if (auto s = soundList[index])
            return s->length;
    }

    return l;
}

juce::String SamplerPlugin::addSound (const juce::String& source, const juce::String& name,
                                      double startTime, double length, float gainDb,
                                      int keyNote, int minNote, int maxNote
                                      // BEATCONNECT MODIFICATION START
                                      , bool openEnded, 
                                      int filterType, double filterFrequency, double filterGain,
                                      float chorusDepth, float chorusMix, bool chorusOn,
                                      float chorusSpeed, float chorusWidth,
                                      float delayCrossfed, float delayFeedback, float delay,
                                      float delayMix, bool delayOn,
                                      float distortion, bool distortionOn,
                                      float reverbDamping, float reverbMix, bool reverbOn,
                                      float reverbSize, float reverbWidth
                                      // BEATCONNECT MODIFICATION END
                                      )
{
    const int maxNumSamples = 64;

    if (getNumSounds() >= maxNumSamples)
        return TRANS("Can't load any more samples");

    auto v = createValueTree (IDs::SOUND,
                              IDs::source, source,
                              IDs::name, name,
                              IDs::startTime, startTime,
                              IDs::length, length,
                              IDs::keyNote, keyNote,
                              IDs::minNote, minNote,
                              IDs::maxNote, maxNote,
                              IDs::gainDb, gainDb,
                              IDs::pan, (double) 0
                              // BEATCONNECT MODIFICATION START
                              , IDs::openEnded, openEnded,
                              IDs::chorusDepth, chorusDepth,
                              IDs::chorusMix, chorusMix,
                              IDs::chorusOn, chorusOn,// The chorus' bypass state
                              IDs::chosusSpeed, chorusSpeed,
                              IDs::chrousWidth, chorusWidth,
                              IDs::delayCrossfeed, delayCrossfed,
                              IDs::delayFeedback, delayFeedback,
                              IDs::delay, delay, // The delay's length
                              IDs::delayMix, delayMix,
                              IDs::delayOn, delayOn, // The delay's bypass state
                              IDs::distortion, distortion, // The distortion's intensity
                              IDs::distortionOn, distortionOn,
                              IDs::reverbDamping, reverbDamping,
                              IDs::reverbMix, reverbMix,
                              IDs::reverbOn, reverbOn, // Reverb's bypass state
                              IDs::reverbSize, reverbSize,
                              IDs::reverbWidth, reverbWidth
                              // BEATCONNECT MODIFICATION END
                              );
    
    // BEATCONNECT MODIFICATION START
    // Add EffectsModule if the keyNote is unique
    // =8> this sampleRate might not be exactly correct, the sample rate of the audio file might need to be used!
    // EffectsModule effectsModule(sampleRate, keyNote);
    
    // auto effectsModule = effectsModules.insert((const int)keyNote, test);
    auto effectsModule = effectsModules.emplace(std::pair<const int, EffectsModule&>((const int)keyNote, EffectsModule(sampleRate, keyNote)));
    // =8> Add the AutomatableParameters here?

                juce::ValueTree soundNode = v;
                std::string soundNodeDebug = soundNode.toXmlString().toStdString();
                int breakpoint = 8888;

                chorusMixValue.referTo(soundNode, IDs::chorusMix, nullptr, 0.0f); 
                delayMixValue.referTo(soundNode, IDs::delayMix, nullptr, 0.0f); 
                distortionMixValue.referTo(soundNode, IDs::distortionMix, nullptr, 0.0f);
                reverbMixValue.referTo(soundNode, IDs::reverbMix, nullptr, 0.0f);

                auto tests = effectsModule.first->second.chorusMix;
                int breaks = 8888;

                effectsModule.first->second.chorusMix = addParam("chorusMix", TRANS("Chorus Mix"), {0.0f, 1.0f}); 
                effectsModule.first->second.delayMix = addParam(
                    "delayMix",
                TRANS("Delay Mix"), {0.0f, 1.0f}); 
                effectsModule.first->second.distortionMix =
                addParam("distortionMix", TRANS("Distortion Mix"), {0.0f,
                1.0f}); 
                effectsModule.first->second.reverbMix = addParam(
                    "reverbMix",
                TRANS("Reverb Mix"), {0.0f, 1.0f});

                effectsModule.first->second.chorusMix->attachToCurrentValue(
                    chorusMixValue);
                effectsModule.first->second.delayMix->attachToCurrentValue(
                    delayMixValue);
                effectsModule.first->second.distortionMix->attachToCurrentValue(
                    distortionMixValue);
                effectsModule.first->second.reverbMix->attachToCurrentValue(
                    reverbMixValue);

                // What is the state of the EffectsModule added to the map?
                breakpoint = 8888;
    // BEATCONNECT MODIFICATION END

    if (filterType != (int)FilterType::noFilter) 
    {
        v.setProperty(IDs::filterType, filterType, nullptr);
        v.setProperty(IDs::filterFreq, filterFrequency, nullptr);

        if (filterType == (int)FilterType::lowshelf ||
            filterType == (int)FilterType::highshelf ||
            filterType == (int)FilterType::peak)
                v.setProperty(IDs::filterGain, filterGain, nullptr);
    }

    auto getRandomColour = []() -> juce::String
    {
        // This list comes from Figma. Format is RGB
        juce::StringArray availableColours;
        availableColours.add("#9C4302");    // Brown
        availableColours.add("#FF7D34");    // Brownish Orange
        availableColours.add("#FFC839");    // Mustard
        availableColours.add("#FBFF35");     // Dim Yellow

        availableColours.add("#C2FF41");     // Yellow
        availableColours.add("#37FF4B");     // Green
        availableColours.add("#33FFDA");     // Cyan
        availableColours.add("#00B1FD");     // Light Blue

        availableColours.add("#1C69FF");     // Steel Blue
        availableColours.add("#4131FF");     // Deep Blue
        availableColours.add("#7432FF");     // Purple
        availableColours.add("#AC42FF");     // Magenta

        availableColours.add("#FF65E6");     // Pink
        availableColours.add("#FF12AE");     // Fuscia
        availableColours.add("#FF2372");     // Reddish
        availableColours.add("#FF0606");     // Red

        auto randomInt = juce::Random::getSystemRandom().nextInt(availableColours.size());

        return availableColours[randomInt] + "FF";
    };

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        juce::ValueTree child = state.getChild(i);

        if (child.hasType("SamplerDrumPad") && 
            (int)child.getProperty("note") == keyNote)
        {
            child.setProperty(IDs::name, name, nullptr);
            child.setProperty(IDs::colour, getRandomColour(), nullptr);
            break;
        }
    }
    // BEATCONNECT MODIFICATION END

    state.addChild (v, -1, getUndoManager());
    return {};
}

void SamplerPlugin::removeSound (int index)
{
    // BEATCONNECT MODIFICATION START
    // If the keyNote is no longer in use, remove the corresponding EffectsModule
    const int removeSoundsKeyNote = state.getChild(index).getProperty(IDs::keyNote);
    bool keyNotePresisting = false;
    int keyNoteAccumulator = 0;

    for (auto element : state) 
        if ((const int)element.getProperty(IDs::keyNote) == removeSoundsKeyNote)
        {
            keyNoteAccumulator++;
            if (keyNoteAccumulator > 1)
            {
                keyNotePresisting = true;
                break;
            }
        }

    if (!keyNotePresisting)
        effectsModules.erase(removeSoundsKeyNote);
    // BEATCONNECT MODIFICATION END

    state.removeChild (index, getUndoManager());

    const juce::ScopedLock sl (lock);
    playingNotes.clear();
    highlightedNotes.clear();
}

void SamplerPlugin::setSoundParams (int index, int keyNote, int minNote, int maxNote)
{
    auto um = getUndoManager();

    auto v = getSound (index);
    v.setProperty (IDs::keyNote, juce::jlimit (0, 127, keyNote), um);
    v.setProperty (IDs::minNote, juce::jlimit (0, 127, std::min (minNote, maxNote)), um);
    v.setProperty (IDs::maxNote, juce::jlimit (0, 127, std::max (minNote, maxNote)), um);
}
// BEATCONNECT MODIFICATION START
void SamplerPlugin::setFilterType(const int index, FilterType filterType) {
    auto um = getUndoManager();

    auto v = getSound(index);
    v.setProperty(IDs::filterType, juce::jlimit((int)FilterType::noFilter, (int)FilterType::peak, 
        (int)filterType), um);
    
    return;
}

void SamplerPlugin::setFilterFrequency(const int index, const double filterFrequency) {
    jassert(filterFrequency > 0.0);

    if (filterFrequency <= 0.0)
        return;

    auto um = getUndoManager();

    auto v = getSound(index);
    v.setProperty(IDs::filterFreq, filterFrequency, um);
    return;
}

void SamplerPlugin::setFilterGain(const int index, const double filterGain) {
    auto um = getUndoManager();

    FilterType filterType = getFilterType(index);
    if (filterType != FilterType::lowshelf ||
        filterType != FilterType::highshelf ||
        filterType != FilterType::peak)
        return;

    auto v = getSound(index);
    v.setProperty(IDs::filterGain, filterGain, um);
    return;
}

void SamplerPlugin::setSoundFilter(const int index, const FilterType filterType,
    const double filterFrequency, const double filterGain) {

    auto v = getSound(index);

    if (getFilterType(index) != filterType)
        setFilterType(index, filterType);
    
    if (getFilterFrequency(index) != filterFrequency)
        setFilterFrequency(index, filterFrequency);
    
    if (getFilterGain(index) != filterGain)
        setFilterFrequency(index, filterFrequency);

    setFilterType(index, filterType);
    return;
}
// BEATCONNECT MODIFICATION END

void SamplerPlugin::setSoundGains (int index, float gainDb, float pan)
{
    auto um = getUndoManager();

    auto v = getSound (index);
    v.setProperty (IDs::gainDb, juce::jlimit (-48.0f, 48.0f, gainDb), um);
    v.setProperty (IDs::pan,    juce::jlimit (-1.0f,  1.0f,  pan), um);
}

void SamplerPlugin::setSoundExcerpt (int index, double start, double length)
{
    auto um = getUndoManager();

    auto v = getSound (index);
    v.setProperty (IDs::startTime, start, um);
    v.setProperty (IDs::length, length, um);
}

void SamplerPlugin::setSoundOpenEnded (int index, bool b)
{
    auto um = getUndoManager();

    auto v = getSound (index);
    v.setProperty (IDs::openEnded, b, um);
}

void SamplerPlugin::setSoundMedia (int index, const juce::String& source)
{
    auto v = getSound (index);
    v.setProperty (IDs::source, source, getUndoManager());
    triggerAsyncUpdate();
}

juce::ValueTree SamplerPlugin::getSound (int soundIndex) const
{
    int index = 0;

    for (auto v : state)
        if (v.hasType (IDs::SOUND))
            if (index++ == soundIndex)
                return v;

    return {};
}

//==============================================================================
juce::Array<Exportable::ReferencedItem> SamplerPlugin::getReferencedItems()
{
    juce::Array<ReferencedItem> results;

    // must be careful to generate this list in the right order..
    for (int i = 0; i < getNumSounds(); ++i)
    {
        auto v = getSound (i);

        Exportable::ReferencedItem ref;
        ref.itemID = ProjectItemID::fromProperty (v, IDs::source);
        ref.firstTimeUsed = v[IDs::startTime];
        ref.lengthUsed = v[IDs::length];
        results.add (ref);
    }

    return results;
}

void SamplerPlugin::reassignReferencedItem (const ReferencedItem& item, ProjectItemID newID, double newStartTime)
{
    auto index = getReferencedItems().indexOf (item);

    if (index >= 0)
    {
        auto um = getUndoManager();

        auto v = getSound (index);
        v.setProperty (IDs::source, newID.toString(), um);
        v.setProperty (IDs::startTime, static_cast<double> (v[IDs::startTime]) - newStartTime, um);
    }
    else
    {
        jassertfalse;
    }
}

void SamplerPlugin::sourceMediaChanged()
{
    const juce::ScopedLock sl (lock);

    for (auto s : soundList)
        s->refreshFile();
}

void SamplerPlugin::restorePluginStateFromValueTree (const juce::ValueTree& v)
{
    copyValueTree (state, v, getUndoManager());
}

//==============================================================================
SamplerPlugin::SamplerSound::SamplerSound (SamplerPlugin& sf,
                                           const juce::String& source_,
                                           const juce::String& name_,
                                           const double startTime_,
                                           const double length_,
                                           const float gainDb_)
    : owner (sf),
      source (source_),
      name (name_),
      gainDb (juce::jlimit (-48.0f, 48.0f, gainDb_)),
      startTime (startTime_),
      length (length_),
      audioFile (owner.edit.engine, SourceFileReference::findFileFromString (owner.edit, source))
{
    setExcerpt (startTime_, length_);

    keyNote = audioFile.getInfo().loopInfo.getRootNote();

    if (keyNote < 0)
        keyNote = 72;

    maxNote = keyNote + 24;
    minNote = keyNote - 24;
}

void SamplerPlugin::SamplerSound::setExcerpt (double startTime_, double length_)
{
    CRASH_TRACER

    if (! audioFile.isValid())
    {
        audioFile = AudioFile (owner.edit.engine, SourceFileReference::findFileFromString (owner.edit, source));

       #if JUCE_DEBUG
        if (! audioFile.isValid() && ProjectItemID (source).isValid())
            DBG ("Failed to find media: " << source);
       #endif
    }

    if (audioFile.isValid())
    {
        const double minLength = 32.0 / audioFile.getSampleRate();

        startTime = juce::jlimit (0.0, audioFile.getLength() - minLength, startTime_);

        if (length_ > 0)
            length = juce::jlimit (minLength, audioFile.getLength() - startTime, length_);
        else
            length = audioFile.getLength();

        fileStartSample   = juce::roundToInt (startTime * audioFile.getSampleRate());
        fileLengthSamples = juce::roundToInt (length * audioFile.getSampleRate());

        if (auto reader = owner.engine.getAudioFileManager().cache.createReader (audioFile))
        {
            audioData.setSize (audioFile.getNumChannels(), fileLengthSamples + 32);
            audioData.clear();

            auto audioDataChannelSet = juce::AudioChannelSet::canonicalChannelSet (audioFile.getNumChannels());
            auto channelsToUse = juce::AudioChannelSet::stereo();

            int total = fileLengthSamples;
            int offset = 0;

            while (total > 0)
            {
                const int numThisTime = std::min (8192, total);
                reader->setReadPosition (fileStartSample + offset);

                if (! reader->readSamples (numThisTime, audioData, audioDataChannelSet, offset, channelsToUse, 2000))
                {
                    jassertfalse;
                    break;
                }

                offset += numThisTime;
                total -= numThisTime;
            }
        }
        else
        {
            audioData.clear();
        }

        // add a quick fade-in if needed..
        int fadeLen = 0;
        for (int i = audioData.getNumChannels(); --i >= 0;)
        {
            const float* d = audioData.getReadPointer (i);

            if (std::abs (*d) > 0.01f)
                fadeLen = 30;
        }

        if (fadeLen > 0)
            AudioFadeCurve::applyCrossfadeSection (audioData, 0, fadeLen, AudioFadeCurve::concave, 0.0f, 1.0f);
    }
    else
    {
        audioFile = AudioFile (owner.edit.engine);
    }
}

void SamplerPlugin::SamplerSound::refreshFile()
{
    audioFile = AudioFile (owner.edit.engine);
    setExcerpt (startTime, length);
}

}} // namespace tracktion { inline namespace engine
