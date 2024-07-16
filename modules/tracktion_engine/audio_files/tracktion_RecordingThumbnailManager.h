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

/** Holds a pool of Thumbnails that are populated whilst recording.
    These can then be used to draw visual waveforms during the recoridng process.
*/
class RecordingThumbnailManager
{
public:
    /**
        A thumbnail represeting a recording file.
        Get one of these for a file with the getThumbnailFor function.
    */
    struct Thumbnail  : public juce::ReferenceCountedObject
    {
        using Ptr = juce::ReferenceCountedObjectPtr<Thumbnail>;

        Engine& engine;             /**< The Engine instance this belongs to. */
        const std::unique_ptr<juce::AudioThumbnailBase> thumb;  /**< The thumbnail. */
        juce::File file;            /**< The file this thumbnail represents. */
        const HashCode hash;        /**< A hash uniquely identifying this thumbnail. */
        TimePosition punchInTime;   /**< The time the start of this thumbnail represents. */

        /** Destructor. */
        ~Thumbnail()
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            engine.getRecordingThumbnailManager().thumbs.removeAllInstancesOf (this);
        }

        /** Destructor */
        void reset (int numChannels, double sampleRate)
        {
            thumb->reset (numChannels, sampleRate, 0);
            nextSampleNum = 0;
        }

        /** Adss a block of recorded data to the thumbnail.
            This is called by the engine so shouldn't need to be called manually.
        */
        void addBlock (const juce::AudioBuffer<float>& incoming, int startOffsetInBuffer, int numSamples)
        {
            thumb->addBlock (nextSampleNum, incoming, startOffsetInBuffer, numSamples);
            nextSampleNum += numSamples;
        }

    private:
        friend class RecordingThumbnailManager;
        std::atomic<int64_t> nextSampleNum { 0 };

        Thumbnail (Engine& e, const juce::File& f)
            : engine (e),
              thumb (engine.getUIBehaviour().createAudioThumbnail (1024, engine.getAudioFileFormatManager().readFormatManager,
                                                                   engine.getAudioFileManager().getAudioThumbnailCache())),
              file (f), hash (f.hashCode64())
        {
            TRACKTION_ASSERT_MESSAGE_THREAD
            engine.getRecordingThumbnailManager().thumbs.addIfNotAlreadyThere (this);
        }

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Thumbnail)
    };

    //==============================================================================
    /** Returns the Thumbnail for a given audio file.
        The engine will update this so you should only use it for drawing.
    */
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
