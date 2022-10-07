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

class RecordingThumbnailManager
{
public:
    struct Thumbnail  : public juce::ReferenceCountedObject
    {
        using Ptr = juce::ReferenceCountedObjectPtr<Thumbnail>;

        Engine& engine;
        TracktionThumbnail thumb;
        juce::File file;
        const HashCode hash;
        TimePosition punchInTime;

        ~Thumbnail()
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            engine.getRecordingThumbnailManager().thumbs.removeAllInstancesOf (this);
        }

        void reset (int numChannels, double sampleRate)
        {
            thumb.reset (numChannels, sampleRate);
            nextSampleNum = 0;
        }

        void addBlock (const juce::AudioBuffer<float>& incoming, int startOffsetInBuffer, int numSamples)
        {
            thumb.addBlock (nextSampleNum, incoming, startOffsetInBuffer, numSamples);
            nextSampleNum += numSamples;
        }

    private:
        friend class RecordingThumbnailManager;
        std::atomic<int64_t> nextSampleNum { 0 };

        Thumbnail (Engine& e, const juce::File& f)
            : engine (e),
              thumb (1024, engine.getAudioFileFormatManager().readFormatManager,
                     engine.getAudioFileManager().getAudioThumbnailCache()),
                file (f), hash (f.hashCode64())
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            engine.getRecordingThumbnailManager().thumbs.addIfNotAlreadyThere (this);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Thumbnail)
    };

    //==============================================================================
    Thumbnail::Ptr getThumbnailFor (const juce::File& f)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD

        for (auto* t : thumbs)
            if (t->file == f)
                return *t;

        return *new Thumbnail (engine, f);
    }

private:
    Engine& engine;
    juce::Array<Thumbnail*> thumbs;

    friend class Engine;
    RecordingThumbnailManager (Engine& e) : engine (e) {}

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecordingThumbnailManager)
};

}} // namespace tracktion { inline namespace engine
