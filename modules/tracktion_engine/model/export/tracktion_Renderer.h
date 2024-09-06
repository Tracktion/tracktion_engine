/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline graph
{
    class Node;
    class PlayHead;
    class PlayHeadState;
}

namespace tracktion::inline engine
{

class NodeRenderContext;
struct ProcessState;

//==============================================================================
/**
    Lowest level render operation.
    This can be used when you want complete control over a render operation.
*/
class Renderer
{
public:
    //==============================================================================
    /**
        Holds all the properties of a single render operation.
    */
    struct Parameters
    {
        /// Use one of the constructors that takes an Engine or Edit
        Parameters() = delete;

        /// Constructs a Parameters object for an Engine
        Parameters (Engine& e) : engine (&e) {}

        /// Constructs a Parameters object for an Engine
        Parameters (Edit& ed) : engine (&ed.engine), edit (&ed) {}

        Parameters (const Parameters&) = default;               ///< Copy constructor
        Parameters (Parameters&&) = default;                    ///< Move constructor
        Parameters& operator= (const Parameters&) = default;    ///< Copy assignment operator
        Parameters& operator= (Parameters&&) = default;         ///< Copy assignment operator

        Engine* engine = nullptr;                               ///< The Engine to use
        Edit* edit = nullptr;                                   ///< The Edit to render, must not be nullptr by the time it is rendered
        juce::BigInteger tracksToDo;                            ///< An bitset of tracks to render, if this is empty, all tracks will be rendered. @see toBitSet/toTrackArray
        juce::Array<Clip*> allowedClips;                        ///< An array of clips to render, if this is empty, all clips will be included

        juce::File destFile;                                    ///< The destination file to write to, must be writable
        juce::AudioFormat* audioFormat = nullptr;               ///< The AudioFormat to use, can be nullptr if createMidiFile is true

        int bitDepth = 16;                                      ///< If this is an audio render, the bit depth to use
        int blockSizeForAudio = 512;                            ///< The block size to use
        double sampleRateForAudio = 44100.0;                    ///< The sample rate to use

        TimeRange time;                                         ///< The time range to render
        TimeDuration endAllowance;                              /**< An optional tail time for notes to end, delays/reverbs to decay etc.
                                                                     If the audio level drops to silence in this period, the render will be stopped.
                                                                     Mostly useful for freeze files and track renders, not usually in exporting. */

        bool createMidiFile = false;                            ///< If true, a MIDI file will be created
        bool trimSilenceAtEnds = false;                         /**< If true, the render won't start until a non-silent level has been reached
                                                                     and will end once it becomes silent again. */

        bool shouldNormalise = false;                           ///< If true, the resulting audio will be normalised by peak level
        bool shouldNormaliseByRMS = false;                      ///< If true, the resulting audio will be normalised by RMS level
        float normaliseToLevelDb = 0;                           ///< The level to normalise to
        bool canRenderInMono = true;                            ///< If false, the result audio will be forced to stereo
        bool mustRenderInMono = false;                          ///< If true, the resulting audio will be forced to mono
        bool usePlugins = true;                                 ///< If false, clip/tracks plugins will be ommited from the render
        bool useMasterPlugins = false;                          ///< If true, master plugins will be included
        bool realTimeRender = false;                            ///< If true, there will be a pause between each rendered block to simulate real-time
        bool ditheringEnabled = false;                          ///< If true, low-level noise will be added to the output for non-float formats
        bool checkNodesForAudio = true;                         ///< If true, attempting to render an Edit that doesn't produce audio will fail

        int quality = 0;                                        ///< For audio formats that support it, the desired quality index @see juce::AudioFormat::createWriterFor
        juce::StringPairArray metadata;                         ///< A map of meta data to add to the file

        /// @internal
        bool separateTracks = false;
        /// @internal
        ProjectItem::Category category = ProjectItem::Category::none;
        /// @internal
        float resultMagnitude = 0;
        /// @internal
        float resultRMS = 0;
        /// @internal
        float resultAudioDuration = 0;
    };

    //==============================================================================
    /** Task that actually performs the render operation in blocks.
        You should continually call the runJob method until it returns jobHasFinished.
    */
    class RenderTask    : public ThreadPoolJobWithProgress
    {
    public:
        /// Constructs a RenderTask for a set of parameters
        RenderTask (const juce::String& taskDescription,
                    const Renderer::Parameters&,
                    std::atomic<float>* progressToUpdate,
                    juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver*);

        /// Constructs a RenderTask for a set of parameters and existing graph
        RenderTask (const juce::String& taskDescription,
                    const Renderer::Parameters&,
                    std::unique_ptr<tracktion::graph::Node>,
                    std::unique_ptr<tracktion::graph::PlayHead>,
                    std::unique_ptr<tracktion::graph::PlayHeadState>,
                    std::unique_ptr<ProcessState>,
                    std::atomic<float>* progressToUpdate,
                    juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver*);

        /// Destructor
        ~RenderTask() override;

        /// Call until this returns jobHasFinished to perform the render
        JobStatus runJob() override;

        /// Returns the current progress of the render
        float getCurrentTaskProgress() override   { return progress; }

        /// The Parameters being used
        Renderer::Parameters params;

        /// An error message if the render failed to start
        juce::String errorMessage;

        //==============================================================================
        /** @internal */
        static void flushAllPlugins (const Plugin::Array&, double sampleRate, int samplesPerBlock);
        /** @internal */
        static void setAllPluginsRealtime (const Plugin::Array&, bool realtime);
        /** @internal */
        static bool addMidiMetaDataAndWriteToFile (juce::File, juce::MidiMessageSequence, const TempoSequence&);
        /** @internal */
        bool performNormalisingAndTrimming (const Renderer::Parameters& target,
                                            const Renderer::Parameters& intermediate);

    private:
        //==============================================================================
        std::unique_ptr<tracktion::graph::Node> graphNode;
        std::unique_ptr<tracktion::graph::PlayHead> playHead;
        std::unique_ptr<tracktion::graph::PlayHeadState> playHeadState;
        std::unique_ptr<ProcessState> processState;
        std::unique_ptr<NodeRenderContext> nodeRenderContext;

        std::atomic<float> progressInternal { 0.0f };
        std::atomic<float>& progress;
        juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver* sourceToUpdate = nullptr;

        //==============================================================================
        bool renderAudio (Renderer::Parameters&);
        bool renderMidi (Renderer::Parameters&);

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RenderTask)
    };

    //==============================================================================
    /** Cheks a file for write access etc. and presents pop-up options to the user
        if problems occur.
    */
    static bool checkTargetFile (Engine&, const juce::File&);

    /** Deinitialises all the plugins for the Edit. */
    static void turnOffAllPlugins (Edit&);

    //==============================================================================
    /// Blocking render operations
    /// @see EditRenderer below for async render operations
    //==============================================================================
    /** Renders an Edit to a file and creates a new ProjectItem for it. */
    static ProjectItem::Ptr renderToProjectItem (const juce::String& taskDescription,
                                                 const Parameters&,
                                                 ProjectItem::Category);

    /** Renders an Edit to a file given by the Parameters. */
    static juce::File renderToFile (const juce::String& taskDescription, const Parameters&);

    /** Renders an Edit to a file within the given time range and the track indicies described by the BigInteger. */
    static bool renderToFile (const juce::String& taskDescription,
                              const juce::File& outputFile,
                              Edit& edit,
                              TimeRange range,
                              const juce::BigInteger& tracksToDo,
                              bool usePlugins = true,
                              bool useACID = true,
                              juce::Array<Clip*> clips = {},
                              bool useThread = true);

    /** Renders an entire Edit to a file. */
    static bool renderToFile (Edit&, const juce::File&, bool useThread = true);

    //==============================================================================
    /** @see measureStatistics() */
    struct Statistics
    {
        float peak = 0;
        float average = 0;
        float audioDuration = 0;
    };

    /** Renders a section of an edit to measure various details about its audio content */
    static Statistics measureStatistics (const juce::String& taskDescription,
                                         Edit& edit, TimeRange range,
                                         const juce::BigInteger& tracksToDo,
                                         int blockSizeForAudio, double sampleRateForAudio = 44100.0);

    //==============================================================================
    /// @internal
    struct RenderResult
    {
        RenderResult() : result (juce::Result::fail ({})) {}

        RenderResult (const juce::Result& r, ProjectItem::Ptr mo)  : result (r)
        {
            if (mo != nullptr)
                items.add (mo);
        }

        RenderResult (const RenderResult& other)
            : result (other.result), items (other.items) {}

        RenderResult& operator= (const RenderResult& other)
        {
            result = other.result;
            items = other.items;
            return *this;
        }

        juce::Result result;
        juce::ReferenceCountedArray<ProjectItem> items;
    };

    //==============================================================================
    /// Temporarily disables clip slots. Useful for rendering/freezing tracks
    struct ScopedClipSlotDisabler
    {
        ScopedClipSlotDisabler (Edit& e, Track::Array& ta)
            : edit (e), tracks (ta)
        {
            for (auto t : tracks)
            {
                if (auto at = dynamic_cast<AudioTrack*> (t))
                {
                    playSlotClips.push_back (at->playSlotClips.get());
                    at->playSlotClips = false;
                }
            }
        }

        ~ScopedClipSlotDisabler()
        {
            for (size_t i = 0; auto t : tracks)
                if (auto at = dynamic_cast<AudioTrack*> (t))
                    at->playSlotClips = playSlotClips[i++];
        }

        Edit& edit;
        Track::Array tracks;
        std::vector<bool> playSlotClips;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedClipSlotDisabler)
    };
};


//==============================================================================
//==============================================================================
/// Creates ACID metadata info from the Edit
juce::StringPairArray createAcidInfo (Edit&, TimeRange);

//==============================================================================
//==============================================================================
/**
    Some useful utility functions for asyncronously rendering Edits on background
    threads. Just call one of the render functions with the appropriate arguments.
*/
class EditRenderer
{
public:
    //==============================================================================
    /**
        A handle to a rendering Edit.
        This internally runs a thread that renders the Edit.
    */
    class Handle
    {
    public:
        /// Destructor. Deleting the handle cancels the render.
        ~Handle();

        /** Call to cancel the render. */
        void cancel();

        /** Returns the current progress of the render. */
        float getProgress() const;

    private:
        friend EditRenderer;

        std::thread renderThread;
        std::atomic<float> progress { 0.0f };
        std::atomic<bool> hasBeenCancelled { false };
        std::shared_ptr<juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver> thumbnailToUpdate;

        Handle() = default;
    };

    //==============================================================================
    /** Loads an Edit asyncronously on a background thread.
        This returns a Handle with a LoadContext which you can use to cancel the
        operation or poll to get progress.
    */
    static std::shared_ptr<Handle> render (Renderer::Parameters,
                                           std::function<void (tl::expected<juce::File, std::string>)> finishedCallback,
                                           std::shared_ptr<juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver> thumbnailToUpdate = {});
};


//==============================================================================
//==============================================================================
/** @internal */
namespace render_utils
{
/** @internal */
std::unique_ptr<Renderer::RenderTask> createRenderTask (Renderer::Parameters, juce::String desc,
                                                        std::atomic<float>* progressToUpdate,
                                                        juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver* thumbnail);
}

} // namespace tracktion::inline engine
