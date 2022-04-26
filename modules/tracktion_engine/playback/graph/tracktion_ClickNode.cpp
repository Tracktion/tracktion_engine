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

namespace
{
    juce::AudioBuffer<float> loadWavDataIntoMemory (const void* data, size_t size, double targetSampleRate)
    {
        auto in = new juce::MemoryInputStream (data, size, false);

        juce::WavAudioFormat wavFormat;
        std::unique_ptr<juce::AudioFormatReader> r (wavFormat.createReaderFor (in, true));

        if (r == nullptr)
            return {};

        auto ratio = r->sampleRate / targetSampleRate;
        auto targetLength = (int) (r->lengthInSamples / ratio);

        juce::AudioBuffer<float> buf ((int) r->numChannels, targetLength);

        {
            juce::AudioFormatReaderSource readerSource (r.get(), false);

            juce::ResamplingAudioSource resamplerSource (&readerSource, false, (int) r->numChannels);
            resamplerSource.setResamplingRatio (ratio);
            resamplerSource.prepareToPlay (targetLength, targetSampleRate);

            juce::AudioSourceChannelInfo info;
            info.buffer = &buf;
            info.startSample = 0;
            info.numSamples = targetLength;
            resamplerSource.getNextAudioBlock (info);
        }

        return buf;
    }

    juce::AudioBuffer<float> loadWavDataIntoMemory (const juce::File& file, double targetSampleRate)
    {
        juce::MemoryBlock mb;
        file.loadFileAsData (mb);

        return loadWavDataIntoMemory (mb.getData(), mb.getSize(), targetSampleRate);
    }
}


//==============================================================================
//==============================================================================
ClickGenerator::ClickGenerator (Edit& e, bool isMidi, TimePosition endTime)
    : edit (e), midi (isMidi)
{
    endTime = juce::jmin (endTime, TimePosition::fromSeconds (juce::jmax (edit.getLength().inSeconds() * 2, 60.0 * 60.0)));

    auto pos = createPosition (edit.tempoSequence);
    pos.set (TimePosition::fromSeconds (1.0e-10));
    pos.addBars (-8);

    while (pos.getTime() < endTime)
    {
        auto barsBeats = pos.getBarsBeats();

        if (barsBeats.getWholeBeats() == 0)
            loudBeats.setBit (beatTimes.size());

        beatTimes.add (pos.getTime());
        pos.add (1_bd);
    }

    beatTimes.add (TimePosition::fromSeconds (1000000.0));
}

void ClickGenerator::prepareToPlay (double newSampleRate, TimePosition startTime)
{
    if (sampleRate != newSampleRate)
    {
        sampleRate = newSampleRate;
        bigClick = {};
        littleClick = {};
    }

    if (midi)
    {
        bigClickMidiNote    = Click::getMidiClickNote (edit.engine, true);
        littleClickMidiNote = Click::getMidiClickNote (edit.engine, false);
    }
    else
    {
        if (bigClick.getNumSamples() == 0)
        {
            juce::File file (Click::getClickWaveFile (edit.engine, true));

            if (file.existsAsFile())
                bigClick = loadWavDataIntoMemory (file, sampleRate);

            if (bigClick.getNumSamples() == 0)
                bigClick = loadWavDataIntoMemory (TracktionBinaryData::bigclick_wav, TracktionBinaryData::bigclick_wavSize, sampleRate);
        }

        if (littleClick.getNumSamples() == 0)
        {
            juce::File file (Click::getClickWaveFile (edit.engine, false));

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
                                   MidiMessageArray* bufferForMidiMessages, TimeRange editTime)
{
    // Use the end time here to avoid the end of blocks playing the start of a beat
    if (isMutedAtTime (editTime.getEnd()))
        return;

    auto gain = edit.getClickTrackVolume();
    const bool emphasis = edit.clickTrackEmphasiseBars;

    while (editTime.getStart() > beatTimes[currentBeat])
        ++currentBeat;

    while (currentBeat > 1 && editTime.getStart() < beatTimes[currentBeat - 1])
        --currentBeat;

    if (midi && bufferForMidiMessages != nullptr)
    {
        auto t = beatTimes.getUnchecked (currentBeat);

        while (t < editTime.getEnd())
        {
            auto note = (emphasis && loudBeats[currentBeat]) ? bigClickMidiNote
                                                             : littleClickMidiNote;

            if (t >= editTime.getStart())
                bufferForMidiMessages->addMidiMessage (juce::MidiMessage::noteOn (10, note, gain),
                                                       (t - editTime.getStart()).inSeconds(),
                                                       MidiMessageArray::notMPE);

            t = beatTimes.getUnchecked (++currentBeat);
        }
    }
    else if (! midi && destBuffer != nullptr)
    {
        --currentBeat;
        auto t = beatTimes[currentBeat];

        while (t < editTime.getEnd())
        {
            auto& b = (emphasis && loudBeats[currentBeat]) ? bigClick : littleClick;

            if (b.getNumSamples() > 0)
            {
                auto clickStartOffset = juce::roundToInt (((t - editTime.getStart()).inSeconds()) * sampleRate);

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

bool ClickGenerator::isMutedAtTime (TimePosition time) const
{
    const bool clickEnabled = edit.clickTrackEnabled.get();

    if (clickEnabled && edit.clickTrackRecordingOnly)
        return ! edit.getTransport().isRecording();

    if (! clickEnabled)
        return ! edit.getClickTrackRange().contains (time);

    return ! clickEnabled;
}


//==============================================================================
//==============================================================================
ClickNode::ClickNode (Edit& e, int numAudioChannels, bool isMidi, tracktion::graph::PlayHead& playHeadToUse)
    : edit (e), playHead (playHeadToUse),
      clickGenerator (edit, isMidi, Edit::getMaximumEditEnd()),
      numChannels (numAudioChannels), generateMidi (isMidi)
{
}

std::vector<tracktion::graph::Node*> ClickNode::getDirectInputNodes()
{
    return {};
}

tracktion::graph::NodeProperties ClickNode::getNodeProperties()
{
    tracktion::graph::NodeProperties props;
    props.hasAudio = ! generateMidi;
    props.hasMidi = generateMidi;
    props.numberOfChannels = numChannels;
    props.nodeID = (size_t) edit.getProjectItemID().getRawID();

    return props;
}

void ClickNode::prepareToPlay (const tracktion::graph::PlaybackInitialisationInfo& info)
{
    sampleRate = info.sampleRate;
    clickGenerator.prepareToPlay (sampleRate,
                                  TimePosition::fromSamples (playHead.getPosition(), sampleRate));
}

bool ClickNode::isReadyToProcess()
{
    return true;
}

void ClickNode::process (ProcessContext& pc)
{
    SCOPED_REALTIME_CHECK

    if (playHead.isUserDragging() || ! playHead.isPlaying())
        return;

    const auto splitTimelinePosition = referenceSampleRangeToSplitTimelineRange (playHead, pc.referenceSampleRange);
    const auto editTime = tracktion::timeRangeFromSamples (splitTimelinePosition.timelineRange1, sampleRate);
    clickGenerator.processBlock (&pc.buffers.audio, &pc.buffers.midi, editTime);
}


//==============================================================================
namespace Click
{
    int getMidiClickNote (Engine& e, bool big)
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

    juce::String getClickWaveFile (Engine& e, bool big)
    {
        return e.getPropertyStorage().getProperty (big ? SettingID::clickTrackSampleBig
                                                       : SettingID::clickTrackSampleSmall);
    }

    void setMidiClickNote (Engine& e, bool big, int noteNum)
    {
        auto& storage = e.getPropertyStorage();

        if (big)
            storage.setProperty (SettingID::clickTrackMidiNoteBig, juce::String (noteNum));
        else
            storage.setProperty (SettingID::clickTrackMidiNoteLittle, juce::String (noteNum));

        TransportControl::restartAllTransports (e, false);
    }

    void setClickWaveFile (Engine& e, bool big, const juce::String& filename)
    {
        auto& storage = e.getPropertyStorage();

        if (big)
            storage.setProperty (SettingID::clickTrackSampleBig, filename);
        else
            storage.setProperty (SettingID::clickTrackSampleSmall, filename);

        TransportControl::restartAllTransports (e, false);
    }
}

}} // namespace tracktion { inline namespace engine
