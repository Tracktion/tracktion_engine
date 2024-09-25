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
ClickGenerator::ClickGenerator (Edit& e, bool isMidi)
    : edit (e), midi (isMidi)
{
    assert (edit.getTransport().getCurrentPlaybackContext());
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

    tempoPosition.set (startTime);
}

namespace
{
    struct BeatInfo
    {
        TimePosition time;
        bool isFirstBeatOfBar = false;
    };

    inline BeatInfo getBeatInfo (const tempo::Sequence& sequence, tempo::Sequence::Position& tempoPosition)
    {
        const auto beats = tempoPosition.getBeats().inBeats();
        int beat = static_cast<int> (std::floor (beats));
        const auto beatTime = sequence.toTime (BeatPosition::fromBeats (beat));
        const bool isFirstBeatOfBar = tempoPosition.getBarsBeats().getWholeBeats() == 0;

        return { beatTime, isFirstBeatOfBar };
    }
}

void ClickGenerator::processBlock (choc::buffer::ChannelArrayView<float>* destBuffer,
                                   MidiMessageArray* bufferForMidiMessages, TimeRange editTime)
{
    // Use the end time here to avoid the end of blocks playing the start of a beat
    if (isMutedAtTime (editTime.getEnd()))
        return;

    const bool emphasis = edit.clickTrackEmphasiseBars;

    tempoPosition.set (editTime.getStart());
    auto beatInfo = getBeatInfo (sequence, tempoPosition);

    if (midi)
    {
        if (bufferForMidiMessages == nullptr)
            return;

        auto gain = edit.getClickTrackVolume();
        auto t = beatInfo.time;

        while (t < editTime.getEnd())
        {
            auto note = (emphasis && beatInfo.isFirstBeatOfBar) ? bigClickMidiNote
                                                                : littleClickMidiNote;

            if (t >= editTime.getStart())
                bufferForMidiMessages->addMidiMessage (juce::MidiMessage::noteOn (10, note, gain),
                                                       (t - editTime.getStart()).inSeconds(),
                                                       {});

            tempoPosition.add (1_bd);
            beatInfo = getBeatInfo (sequence, tempoPosition);
            t = beatInfo.time;
        }
    }
    else
    {
        if (destBuffer == nullptr)
            return;

        if (isPlaying())
        {
            auto num = std::min (samplesRemaining(), int (destBuffer->getNumFrames()));
            auto dstView = destBuffer->getFrameRange ({ 0, (choc::buffer::FrameCount) num });

            render (dstView);
        }

        auto t = beatInfo.time;

        while (t < editTime.getEnd())
        {
            auto b = (emphasis && beatInfo.isFirstBeatOfBar) ? &bigClick : &littleClick;

            if (b->getNumSamples() > 0 && t >= editTime.getStart() && ! isMutedAtTime (t))
            {
                const auto clickStartTime = t - editTime.getStart();
                const auto clickStartOffset = static_cast<int> (toSamples (clickStartTime, sampleRate));

                trigger (b);
                isMutedAtTime (t);

                auto num = std::min (samplesRemaining(), int (destBuffer->getNumFrames()) - int (clickStartOffset));
                auto dstView = destBuffer->getFrameRange ({ choc::buffer::FrameCount (clickStartOffset), choc::buffer::FrameCount (clickStartOffset + num) });

                render (dstView);
            }

            tempoPosition.add (1_bd);
            beatInfo = getBeatInfo (sequence, tempoPosition);
            t = beatInfo.time;
        }
    }
}

bool ClickGenerator::isMutedAtTime (TimePosition time) const
{
    const bool clickEnabled = edit.clickTrackEnabled.get();

    auto range = edit.getClickTrackRange();
    if (! range.isEmpty() && time < range.getStart())
        return true;

    if (clickEnabled && edit.clickTrackRecordingOnly)
        return ! (edit.getTransport().isRecording() || context.getNumActivelyRecordingDevices() >= 1);

    if (! clickEnabled)
        return ! range.contains (time);

    return ! clickEnabled;
}

bool ClickGenerator::isPlaying()
{
    return currentSample != nullptr && samplePos >= 0;
}

void ClickGenerator::trigger (juce::AudioBuffer<float>* s)
{
    currentSample = s;
    samplePos = 0;
}

int ClickGenerator::samplesRemaining()
{
    return currentSample->getNumSamples() - samplePos;
}

void ClickGenerator::render (choc::buffer::ChannelArrayView<float>& view)
{
    auto todo = view.getNumFrames();
    auto gain = edit.getClickTrackVolume();

    copyRemappingChannels (view, toBufferView (*currentSample).getFrameRange ({ choc::buffer::FrameCount (samplePos), choc::buffer::FrameCount (samplePos) + todo }));
    applyGain (view, gain);

    samplePos += static_cast<int> (todo);
    if (samplePos >= currentSample->getNumSamples())
        reset();
}

void ClickGenerator::reset()
{
    currentSample = nullptr;
    samplePos = -1;
}

//==============================================================================
//==============================================================================
ClickNode::ClickNode (Edit& e, int numAudioChannels, bool isMidi, tracktion::graph::PlayHead& playHeadToUse)
    : edit (e), playHead (playHeadToUse),
      clickGenerator (edit, isMidi),
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
