/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


ClickNode::ClickNode (bool m, Edit& ed, double endTime)
   : edit (ed), midi (m)
{
    endTime = jmin (endTime, jmax (ed.getLength() * 2, 60.0 * 60.0));

    TempoSequencePosition pos (ed.tempoSequence);
    pos.setTime (1.0e-10);
    pos.addBars (-8);

    while (pos.getTime() < endTime)
    {
        auto barsBeats = pos.getBarsBeatsTime();

        if (roundToInt (barsBeats.beats) == 0)
            loudBeats.setBit (beatTimes.size());

        beatTimes.add (pos.getTime());
        pos.addBeats (1.0);
    }

    beatTimes.add (1000000.0);
}

ClickNode::~ClickNode()
{
    releaseAudioNodeResources();
}

void ClickNode::getAudioNodeProperties (AudioNodeProperties& info)
{
    info.hasAudio = ! midi;
    info.hasMidi = midi;
    info.numberOfChannels = 1;
}

void ClickNode::visitNodes (const VisitorFn& v)
{
    v (*this);
}

bool ClickNode::purgeSubNodes (bool keepAudio, bool keepMidi)
{
    return (keepMidi && midi) || (keepAudio && ! midi);
}

juce::AudioBuffer<float> loadWavDataIntoMemory (const void* data, size_t size, double targetSampleRate)
{
   auto in = new MemoryInputStream (data, size, false);

    WavAudioFormat wavFormat;
    ScopedPointer<AudioFormatReader> r (wavFormat.createReaderFor (in, true));

    if (r == nullptr)
        return {};

    auto ratio = r->sampleRate / targetSampleRate;
    auto targetLength = (int) (r->lengthInSamples / ratio);

    juce::AudioBuffer<float> buf ((int) r->numChannels, targetLength);

    {
        AudioFormatReaderSource readerSource (r, false);

        ResamplingAudioSource resamplerSource (&readerSource, false, (int) r->numChannels);
        resamplerSource.setResamplingRatio (ratio);
        resamplerSource.prepareToPlay (targetLength, targetSampleRate);

        AudioSourceChannelInfo info;
        info.buffer = &buf;
        info.startSample = 0;
        info.numSamples = targetLength;
        resamplerSource.getNextAudioBlock (info);
    }

    return buf;
}

juce::AudioBuffer<float> loadWavDataIntoMemory (const File& file, double targetSampleRate)
{
    MemoryBlock mb;
    file.loadFileAsData (mb);

    return loadWavDataIntoMemory (mb.getData(), mb.getSize(), targetSampleRate);
}

void ClickNode::prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info)
{
    CRASH_TRACER

    if (sampleRate != info.sampleRate)
    {
        sampleRate = info.sampleRate;
        bigClick = {};
        littleClick = {};
    }

    if (midi)
    {
        bigClickMidiNote    = getMidiClickNote (edit.engine, true);
        littleClickMidiNote = getMidiClickNote (edit.engine, false);
    }
    else
    {
        if (bigClick.getNumSamples() == 0)
        {
            File file (getClickWaveFile (edit.engine, true));

            if (file.existsAsFile())
                bigClick = loadWavDataIntoMemory (file, sampleRate);

            if (bigClick.getNumSamples() == 0)
                bigClick = loadWavDataIntoMemory (TracktionBinaryData::bigclick_wav, TracktionBinaryData::bigclick_wavSize, sampleRate);
        }

        if (littleClick.getNumSamples() == 0)
        {
            File file (getClickWaveFile (edit.engine, false));

            if (file.existsAsFile())
                littleClick = loadWavDataIntoMemory (file, sampleRate);

            if (littleClick.getNumSamples() == 0)
                littleClick = loadWavDataIntoMemory (TracktionBinaryData::littleclick_wav, TracktionBinaryData::littleclick_wavSize, sampleRate);
        }
    }

    for (currentBeat = 0; currentBeat < beatTimes.size(); ++currentBeat)
        if (beatTimes[currentBeat] > info.startTime)
            break;
}

bool ClickNode::isReadyToRender()
{
    return true;
}

void ClickNode::releaseAudioNodeResources()
{
    bigClick = {};
    littleClick = {};
}

void ClickNode::renderOver (const AudioRenderContext& rc)
{
    callRenderAdding (rc);
}

void ClickNode::renderAdding (const AudioRenderContext& rc)
{
    if (rc.playhead.isUserDragging() || ! rc.playhead.isPlaying())
        return;

    invokeSplitRender (rc, *this);
}

void ClickNode::renderSection (const AudioRenderContext& rc, EditTimeRange editTime)
{
    auto gain = edit.getClickTrackVolume();
    const bool emphasis = edit.clickTrackEmphasiseBars;

    while (editTime.getStart() > beatTimes[currentBeat])
        ++currentBeat;

    while (currentBeat > 1 && editTime.getStart() < beatTimes[currentBeat - 1])
        --currentBeat;

    if (midi && rc.bufferForMidiMessages != nullptr)
    {
        double t = beatTimes.getUnchecked (currentBeat);

        while (t < editTime.getEnd())
        {
            auto note = (emphasis && loudBeats[currentBeat]) ? bigClickMidiNote
                                                             : littleClickMidiNote;

            if (t >= editTime.getStart())
                rc.bufferForMidiMessages->addMidiMessage (MidiMessage::noteOn (10, note, gain),
                                                          t - editTime.getStart(),
                                                          MidiMessageArray::notMPE);

            t = beatTimes.getUnchecked (++currentBeat);
        }
    }
    else if (! midi && rc.destBuffer != nullptr)
    {
        --currentBeat;
        double t = beatTimes[currentBeat];

        while (t < editTime.getEnd())
        {
            auto& b = (emphasis && loudBeats[currentBeat]) ? bigClick : littleClick;

            if (b.getNumSamples() > 0)
            {
                auto clickStartOffset = roundToInt ((t - editTime.getStart()) * sampleRate);

                const int dstStart = jmax (0, clickStartOffset);
                const int srcStart = jmax (0, -clickStartOffset);
                const int num = jmin (b.getNumSamples() - srcStart,
                                      rc.bufferNumSamples - dstStart);

                if (num > 0)
                    for (int i = rc.destBuffer->getNumChannels(); --i >= 0;)
                        rc.destBuffer->addFrom (i, rc.bufferStartSample + dstStart,
                                                b, jmin (i, b.getNumChannels() - 1), srcStart,
                                                num, gain);
            }

            t = beatTimes.getUnchecked (++currentBeat);
        }
    }
}

//==============================================================================
int ClickNode::getMidiClickNote (Engine& e, bool big)
{
    auto& storage = e.getPropertyStorage();
    int n;

    if (big)
    {
        n = storage.getProperty (SettingID::clickTrackMidiNoteBig, 37);

        if (n < 0 || n > 127)
            n = 37;
    }
    else
    {
        n = storage.getProperty (SettingID::clickTrackMidiNoteLittle, 76);

        if (n < 0 || n > 127)
            n = 76;
    }

    return n;
}

juce::String ClickNode::getClickWaveFile (Engine& e, bool big)
{
    return e.getPropertyStorage().getProperty (big ? SettingID::clickTrackSampleBig
                                                   : SettingID::clickTrackSampleSmall);
}

void ClickNode::setMidiClickNote (Engine& e, bool big, int noteNum)
{
    auto& storage = e.getPropertyStorage();

    if (big)
        storage.setProperty (SettingID::clickTrackMidiNoteBig, String (noteNum));
    else
        storage.setProperty (SettingID::clickTrackMidiNoteLittle, String (noteNum));

    TransportControl::restartAllTransports (e, false);
}

void ClickNode::setClickWaveFile (Engine& e, bool big, const String& filename)
{
    auto& storage = e.getPropertyStorage();

    if (big)
        storage.setProperty (SettingID::clickTrackSampleBig, filename);
    else
        storage.setProperty (SettingID::clickTrackSampleSmall, filename);

    TransportControl::restartAllTransports (e, false);
}
