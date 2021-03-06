/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

namespace
{
    juce::AudioBuffer<float> loadWavDataIntoMemory (const void* data, size_t size, double targetSampleRate)
    {
        auto in = new MemoryInputStream (data, size, false);

        WavAudioFormat wavFormat;
        std::unique_ptr<AudioFormatReader> r (wavFormat.createReaderFor (in, true));

        if (r == nullptr)
            return {};

        auto ratio = r->sampleRate / targetSampleRate;
        auto targetLength = (int) (r->lengthInSamples / ratio);

        juce::AudioBuffer<float> buf ((int) r->numChannels, targetLength);

        {
            AudioFormatReaderSource readerSource (r.get(), false);

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
}

//==============================================================================
ClickGenerator::ClickGenerator (Edit& e, bool isMidi, double endTime)
    : edit (e), midi (isMidi)
{
    endTime = jmin (endTime, jmax (edit.getLength() * 2, 60.0 * 60.0));

    TempoSequencePosition pos (edit.tempoSequence);
    pos.setTime (1.0e-10);
    pos.addBars (-8);

    while (pos.getTime() < endTime)
    {
        auto barsBeats = pos.getBarsBeatsTime();

        if (barsBeats.getWholeBeats() == 0)
            loudBeats.setBit (beatTimes.size());

        beatTimes.add (pos.getTime());
        pos.addBeats (1.0);
    }

    beatTimes.add (1000000.0);
}

void ClickGenerator::prepareToPlay (double newSampleRate, double startTime)
{
    if (sampleRate != newSampleRate)
    {
        sampleRate = newSampleRate;
        bigClick = {};
        littleClick = {};
    }

    if (midi)
    {
        bigClickMidiNote    = ClickAudioNode::getMidiClickNote (edit.engine, true);
        littleClickMidiNote = ClickAudioNode::getMidiClickNote (edit.engine, false);
    }
    else
    {
        if (bigClick.getNumSamples() == 0)
        {
            File file (ClickAudioNode::getClickWaveFile (edit.engine, true));

            if (file.existsAsFile())
                bigClick = loadWavDataIntoMemory (file, sampleRate);

            if (bigClick.getNumSamples() == 0)
                bigClick = loadWavDataIntoMemory (TracktionBinaryData::bigclick_wav, TracktionBinaryData::bigclick_wavSize, sampleRate);
        }

        if (littleClick.getNumSamples() == 0)
        {
            File file (ClickAudioNode::getClickWaveFile (edit.engine, false));

            if (file.existsAsFile())
                littleClick = loadWavDataIntoMemory (file, sampleRate);

            if (littleClick.getNumSamples() == 0)
                littleClick = loadWavDataIntoMemory (TracktionBinaryData::littleclick_wav, TracktionBinaryData::littleclick_wavSize, sampleRate);
        }
    }

    for (currentBeat = 0; currentBeat < beatTimes.size(); ++currentBeat)
        if (beatTimes[currentBeat] > startTime)
            break;
}

void ClickGenerator::processBlock (choc::buffer::ChannelArrayView<float>* destBuffer,
                                   MidiMessageArray* bufferForMidiMessages, EditTimeRange editTime)
{
    if (isMutedAtTime (editTime.getStart()))
        return;

    auto gain = edit.getClickTrackVolume();
    const bool emphasis = edit.clickTrackEmphasiseBars;

    while (editTime.getStart() > beatTimes[currentBeat])
        ++currentBeat;

    while (currentBeat > 1 && editTime.getStart() < beatTimes[currentBeat - 1])
        --currentBeat;

    if (midi && bufferForMidiMessages != nullptr)
    {
        double t = beatTimes.getUnchecked (currentBeat);

        while (t < editTime.getEnd())
        {
            auto note = (emphasis && loudBeats[currentBeat]) ? bigClickMidiNote
                                                             : littleClickMidiNote;

            if (t >= editTime.getStart())
                bufferForMidiMessages->addMidiMessage (MidiMessage::noteOn (10, note, gain),
                                                       t - editTime.getStart(),
                                                       MidiMessageArray::notMPE);

            t = beatTimes.getUnchecked (++currentBeat);
        }
    }
    else if (! midi && destBuffer != nullptr)
    {
        --currentBeat;
        double t = beatTimes[currentBeat];

        while (t < editTime.getEnd())
        {
            auto& b = (emphasis && loudBeats[currentBeat]) ? bigClick : littleClick;

            if (b.getNumSamples() > 0)
            {
                auto clickStartOffset = roundToInt ((t - editTime.getStart()) * sampleRate);

                auto dstStart = (choc::buffer::FrameCount) std::max (0, clickStartOffset);
                auto srcStart = (choc::buffer::FrameCount) std::max (0, -clickStartOffset);
                auto num = std::min (b.getNumSamples() - (int) srcStart,
                                     (int) destBuffer->getNumFrames() - (int) dstStart);

                if (num > 0)
                {
                    auto dstView = destBuffer->getFrameRange ({ dstStart, dstStart + (choc::buffer::FrameCount) num });
                    copyRemappingChannels (dstView, toBufferView (b).getFrameRange ({ srcStart, srcStart + (choc::buffer::FrameCount) num }));
                    applyGain (dstView, gain);
                }
            }

            t = beatTimes.getUnchecked (++currentBeat);
        }
    }
}

bool ClickGenerator::isMutedAtTime (double time) const
{
    const bool clickEnabled = edit.clickTrackEnabled.get();

    if (clickEnabled && edit.clickTrackRecordingOnly)
        return ! edit.getTransport().isRecording();

    if (! clickEnabled)
        return ! edit.getClickTrackRange().contains (time);

    return ! clickEnabled;
}

//==============================================================================
ClickAudioNode::ClickAudioNode (bool m, Edit& ed, double endTime)
   : edit (ed), midi (m)
{
    endTime = jmin (endTime, jmax (ed.getLength() * 2, 60.0 * 60.0));

    TempoSequencePosition pos (ed.tempoSequence);
    pos.setTime (1.0e-10);
    pos.addBars (-8);

    while (pos.getTime() < endTime)
    {
        auto barsBeats = pos.getBarsBeatsTime();

        if (barsBeats.getWholeBeats() == 0)
            loudBeats.setBit (beatTimes.size());

        beatTimes.add (pos.getTime());
        pos.addBeats (1.0);
    }

    beatTimes.add (1000000.0);
}

ClickAudioNode::~ClickAudioNode()
{
    releaseAudioNodeResources();
}

void ClickAudioNode::getAudioNodeProperties (AudioNodeProperties& info)
{
    info.hasAudio = ! midi;
    info.hasMidi = midi;
    info.numberOfChannels = 1;
}

void ClickAudioNode::visitNodes (const VisitorFn& v)
{
    v (*this);
}

bool ClickAudioNode::purgeSubNodes (bool keepAudio, bool keepMidi)
{
    return (keepMidi && midi) || (keepAudio && ! midi);
}

void ClickAudioNode::prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info)
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

bool ClickAudioNode::isReadyToRender()
{
    return true;
}

void ClickAudioNode::releaseAudioNodeResources()
{
    bigClick = {};
    littleClick = {};
}

void ClickAudioNode::renderOver (const AudioRenderContext& rc)
{
    callRenderAdding (rc);
}

void ClickAudioNode::renderAdding (const AudioRenderContext& rc)
{
    if (rc.playhead.isUserDragging() || ! rc.playhead.isPlaying())
        return;

    invokeSplitRender (rc, *this);
}

void ClickAudioNode::renderSection (const AudioRenderContext& rc, EditTimeRange editTime)
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
int ClickAudioNode::getMidiClickNote (Engine& e, bool big)
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

juce::String ClickAudioNode::getClickWaveFile (Engine& e, bool big)
{
    return e.getPropertyStorage().getProperty (big ? SettingID::clickTrackSampleBig
                                                   : SettingID::clickTrackSampleSmall);
}

void ClickAudioNode::setMidiClickNote (Engine& e, bool big, int noteNum)
{
    auto& storage = e.getPropertyStorage();

    if (big)
        storage.setProperty (SettingID::clickTrackMidiNoteBig, String (noteNum));
    else
        storage.setProperty (SettingID::clickTrackMidiNoteLittle, String (noteNum));

    TransportControl::restartAllTransports (e, false);
}

void ClickAudioNode::setClickWaveFile (Engine& e, bool big, const String& filename)
{
    auto& storage = e.getPropertyStorage();

    if (big)
        storage.setProperty (SettingID::clickTrackSampleBig, filename);
    else
        storage.setProperty (SettingID::clickTrackSampleSmall, filename);

    TransportControl::restartAllTransports (e, false);
}

}
