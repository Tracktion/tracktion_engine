/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


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
                 float pan,
                 bool openEnded_)
       : note (midiNote),
         offset (-sampleDelayFromBufferStart),
         audioData (data),
         openEnded (openEnded_)
    {
        resampler[0].reset();
        resampler[1].reset();

        const float volumeSliderPos = decibelsToVolumeFaderPosition (gainDb - (20.0f * (1.0f - velocity)));
        getGainsFromVolumeFaderPositionAndPan (volumeSliderPos, pan, getDefaultPanLaw(), gains[0], gains[1]);

        const double hz = MidiMessage::getMidiNoteInHertz (midiNote);
        playbackRatio = hz / MidiMessage::getMidiNoteInHertz (keyNote);
        playbackRatio *= file.getSampleRate() / sampleRate;
        samplesLeftToPlay = playbackRatio > 0 ? (1 + (int) (lengthInSamples / playbackRatio)) : 0;
    }

    void addNextBlock (juce::AudioBuffer<float>& outBuffer, int startSamp, int numSamples)
    {
        jassert (! isFinished);

        if (offset < 0)
        {
            const int num = jmin (-offset, numSamples);
            startSamp += num;
            numSamples -= num;
            offset += num;
        }

        int numSamps = jmin (numSamples, samplesLeftToPlay);

        if (numSamps > 0)
        {
            int numUsed = 0;

            for (int i = jmin (2, outBuffer.getNumChannels()); --i >= 0;)
            {
                numUsed = resampler[i]
                            .processAdding (playbackRatio,
                                            audioData.getReadPointer (jmin (i, audioData.getNumChannels() - 1), offset),
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
                endFade = jmax (0.0f, startFade - numSamps * 0.01f);
            }

            const int numSampsNeeded = 2 + roundToInt ((numSamps + 2) * playbackRatio);
            AudioScratchBuffer scratch (audioData.getNumChannels(), numSampsNeeded + 8);

            if (offset + numSampsNeeded < audioData.getNumSamples())
            {
                for (int i = scratch.buffer.getNumChannels(); --i >= 0;)
                    scratch.buffer.copyFrom (i, 0, audioData, i, offset, numSampsNeeded);
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

            for (int i = jmin (2, outBuffer.getNumChannels()); --i >= 0;)
                numUsed = resampler[i].processAdding (playbackRatio,
                                                      scratch.buffer.getReadPointer (jmin (i, scratch.buffer.getNumChannels() - 1)),
                                                      outBuffer.getWritePointer (i, startSamp),
                                                      numSamps, gains[i]);

            offset += numUsed;

            if (startFade <= 0.0f)
                isFinished = true;
        }
    }

    LagrangeInterpolator resampler[2];
    int note;
    int offset, samplesLeftToPlay = 0;
    float gains[2];
    double playbackRatio = 1.0;
    const juce::AudioBuffer<float>& audioData;
    float lastVals[4] = { 0, 0, 0, 0 };
    float startFade = 1.0f;
    bool openEnded, isFinished = false;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SampledNote)
};

//==============================================================================
SamplerPlugin::SamplerPlugin (PluginCreationInfo info)  : Plugin (info)
{
    triggerAsyncUpdate();
}

SamplerPlugin::~SamplerPlugin()
{
    notifyListenersOfDeletion();
}

const char* SamplerPlugin::xmlTypeName = "sampler";

void SamplerPlugin::valueTreeChanged()
{
    triggerAsyncUpdate();
    Plugin::valueTreeChanged();
}

void SamplerPlugin::handleAsyncUpdate()
{
    OwnedArray<SamplerSound> newSounds;

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

            s->keyNote      = jlimit (0, 127, static_cast<int> (v[IDs::keyNote]));
            s->minNote      = jlimit (0, 127, static_cast<int> (v[IDs::minNote]));
            s->maxNote      = jlimit (0, 127, static_cast<int> (v[IDs::maxNote]));
            s->pan          = jlimit (-1.0f, 1.0f, static_cast<float> (v[IDs::pan]));
            s->openEnded    = v[IDs::openEnded];

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
        const ScopedLock sl (lock);
        allNotesOff();
        soundList.swapWith (newSounds);

        sourceMediaChanged();
    }

    newSounds.clear();
    changed();
}

void SamplerPlugin::initialise (const PlaybackInitialisationInfo&)
{
    const ScopedLock sl (lock);
    allNotesOff();
}

void SamplerPlugin::deinitialise()
{
    allNotesOff();
}

//==============================================================================
void SamplerPlugin::playNotes (const BigInteger& keysDown)
{
    const ScopedLock sl (lock);

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
                                                           ss->openEnded));
                    }
                }
            }
        }

        highlightedNotes = keysDown;
    }
}

void SamplerPlugin::allNotesOff()
{
    const ScopedLock sl (lock);
    playingNotes.clear();
    highlightedNotes.clear();
}

void SamplerPlugin::applyToBuffer (const AudioRenderContext& fc)
{
    if (fc.destBuffer != nullptr)
    {
        SCOPED_REALTIME_CHECK

        fc.setMaxNumChannels (2);

        const ScopedLock sl (lock);

        if (fc.bufferForMidiMessages != nullptr)
        {
            if (fc.bufferForMidiMessages->isAllNotesOff)
            {
                playingNotes.clear();
                highlightedNotes.clear();
            }
            else
            {
                for (auto& m : *fc.bufferForMidiMessages)
                {
                    if (m.isNoteOn())
                    {
                        const int note = m.getNoteNumber();
                        const int noteTimeSample = roundToInt (m.getTimeStamp() * sampleRate);

                        for (auto playingNote : playingNotes)
                        {
                            if (playingNote->note == note && ! playingNote->openEnded)
                            {
                                playingNote->samplesLeftToPlay = jmin (playingNote->samplesLeftToPlay,
                                                                       jmax (minimumSamplesToPlayWhenStopping, noteTimeSample));
                                highlightedNotes.clearBit (note);
                            }
                        }

                        for (auto ss : soundList)
                        {
                            if (ss->minNote <= note
                                && ss->maxNote >= note
                                && ss->audioData.getNumSamples() > 0
                                && playingNotes.size() < maximumSimultaneousNotes)
                            {
                                highlightedNotes.setBit (note);

                                playingNotes.add (new SampledNote (note,
                                                                   ss->keyNote,
                                                                   m.getVelocity() / 127.0f,
                                                                   ss->audioFile,
                                                                   sampleRate,
                                                                   noteTimeSample,
                                                                   ss->audioData,
                                                                   ss->fileLengthSamples,
                                                                   ss->gainDb,
                                                                   ss->pan,
                                                                   ss->openEnded));
                            }
                        }
                    }
                    else if (m.isNoteOff())
                    {
                        const int note = m.getNoteNumber();
                        const int noteTimeSample = roundToInt (m.getTimeStamp() * sampleRate);

                        for (auto playingNote : playingNotes)
                        {
                            if (playingNote->note == note && ! playingNote->openEnded)
                            {
                                playingNote->samplesLeftToPlay = jmin (playingNote->samplesLeftToPlay,
                                                                       jmax (minimumSamplesToPlayWhenStopping, noteTimeSample));

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
        }

        for (int i = playingNotes.size(); --i >= 0;)
        {
            auto sn = playingNotes.getUnchecked(i);

            sn->addNextBlock (*fc.destBuffer, fc.bufferStartSample, fc.bufferNumSamples);

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

String SamplerPlugin::getSoundName (int index) const
{
    return getSound (index)[IDs::name];
}

void SamplerPlugin::setSoundName (int index, const String& n)
{
    getSound (index).setProperty (IDs::name, n, getUndoManager());
}

bool SamplerPlugin::hasNameForMidiNoteNumber (int note, int, String& noteName)
{
    String s;

    {
        const ScopedLock sl (lock);

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
    const ScopedLock sl (lock);

    if (auto s = soundList[index])
        return s->audioFile;

    return {};
}

juce::String SamplerPlugin::getSoundMedia (int index) const
{
    const ScopedLock sl (lock);

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

double SamplerPlugin::getSoundLength (int index) const
{
    const double l = getSound (index)[IDs::length];

    if (l == 0.0)
    {
        const ScopedLock sl (lock);

        if (auto s = soundList[index])
            return s->length;
    }

    return l;
}

String SamplerPlugin::addSound (const juce::String& source, const String& name, double startTime, double length, float gainDb)
{
    const int maxNumSamples = 64;

    if (getNumSounds() >= maxNumSamples)
        return TRANS("Can't load any more samples");

    ValueTree v (IDs::SOUND);
    v.setProperty (IDs::source, source, nullptr);
    v.setProperty (IDs::name, name, nullptr);
    v.setProperty (IDs::startTime, startTime, nullptr);
    v.setProperty (IDs::length, length, nullptr);
    v.setProperty (IDs::keyNote, 72, nullptr);
    v.setProperty (IDs::minNote, 72 - 24, nullptr);
    v.setProperty (IDs::maxNote, 72 + 24, nullptr);
    v.setProperty (IDs::gainDb, gainDb, nullptr);
    v.setProperty (IDs::pan, (double) 0, nullptr);

    state.addChild (v, -1, getUndoManager());
    return {};
}

void SamplerPlugin::removeSound (int index)
{
    state.removeChild (index, getUndoManager());

    const ScopedLock sl (lock);
    playingNotes.clear();
    highlightedNotes.clear();
}

void SamplerPlugin::setSoundParams (int index, int keyNote, int minNote, int maxNote)
{
    auto um = getUndoManager();

    auto v = getSound (index);
    v.setProperty (IDs::keyNote, jlimit (0, 127, keyNote), um);
    v.setProperty (IDs::minNote, jlimit (0, 127, jmin (minNote, maxNote)), um);
    v.setProperty (IDs::maxNote, jlimit (0, 127, jmax (minNote, maxNote)), um);
}

void SamplerPlugin::setSoundGains (int index, float gainDb, float pan)
{
    auto um = getUndoManager();

    auto v = getSound (index);
    v.setProperty (IDs::gainDb, jlimit (-48.0f, 48.0f, gainDb), um);
    v.setProperty (IDs::pan, jlimit (-1.0f, 1.0f, pan), um);
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

void SamplerPlugin::setSoundMedia (int index, const String& source)
{
    auto v = getSound (index);
    v.setProperty (IDs::source, source, getUndoManager());
    triggerAsyncUpdate();
}

ValueTree SamplerPlugin::getSound (int soundIndex) const
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
        v.setProperty (IDs::source, newID, um);
        v.setProperty (IDs::startTime, static_cast<double> (v[IDs::startTime]) - newStartTime, um);
    }
    else
    {
        jassertfalse;
    }
}

void SamplerPlugin::sourceMediaChanged()
{
    const ScopedLock sl (lock);

    for (auto s : soundList)
        s->refreshFile();
}

void SamplerPlugin::restorePluginStateFromValueTree (const ValueTree& v)
{
    copyValueTree (state, v, getUndoManager());
}

//==============================================================================
SamplerPlugin::SamplerSound::SamplerSound (SamplerPlugin& sf,
                                           const String& source_,
                                           const String& name_,
                                           const double startTime_,
                                           const double length_,
                                           const float gainDb_)
    : owner (sf),
      source (source_),
      name (name_),
      gainDb (jlimit (-48.0f, 48.0f, gainDb_)),
      startTime (startTime_),
      length (length_),
      audioFile (SourceFileReference::findFileFromString (owner.edit, source))
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
        audioFile = AudioFile (SourceFileReference::findFileFromString (owner.edit, source));

       #if JUCE_DEBUG
        if (! audioFile.isValid() && ProjectItemID (source).isValid())
            DBG ("Failed to find media: " << source);
       #endif
    }

    if (audioFile.isValid())
    {
        const double minLength = 32.0 / audioFile.getSampleRate();

        startTime = jlimit (0.0, audioFile.getLength() - minLength, startTime_);

        if (length_ > 0)
            length = jlimit (minLength, audioFile.getLength() - startTime, length_);
        else
            length = audioFile.getLength();

        fileStartSample = roundToInt (startTime * audioFile.getSampleRate());
        fileLengthSamples = roundToInt (length * audioFile.getSampleRate());

        if (auto reader = owner.engine.getAudioFileManager().cache.createReader (audioFile))
        {
            audioData.setSize (audioFile.getNumChannels(), fileLengthSamples + 32);
            audioData.clear();

            auto audioDataChannelSet = AudioChannelSet::canonicalChannelSet (audioFile.getNumChannels());
            auto channelsToUse = AudioChannelSet::stereo();

            int total = fileLengthSamples;
            int offset = 0;

            while (total > 0)
            {
                const int numThisTime = jmin (8192, total);
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
        audioFile = AudioFile();
    }
}

void SamplerPlugin::SamplerSound::refreshFile()
{
    audioFile = {};
    setExcerpt (startTime, length);
}
