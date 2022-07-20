/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace graph
{
    class Node;
    class PlayHead;
    class PlayHeadState;
}}

namespace tracktion { inline namespace engine
{

class NodeRenderContext;
struct ProcessState;

//==============================================================================
/**
*/
class Renderer
{
public:
    struct Parameters
    {
        Parameters() = delete;
        Parameters (Engine& e) : engine (&e) {}
        Parameters (Edit& ed) : engine (&ed.engine), edit (&ed) {}

        Parameters (const Parameters&) = default;
        Parameters (Parameters&&) = default;
        Parameters& operator= (const Parameters&) = default;
        Parameters& operator= (Parameters&&) = default;

        Engine* engine = nullptr;
        Edit* edit = nullptr;
        juce::BigInteger tracksToDo;
        juce::Array<Clip*> allowedClips;

        juce::File destFile;
        juce::AudioFormat* audioFormat = nullptr;

        int bitDepth = 16;
        int blockSizeForAudio = 44100;
        double sampleRateForAudio = 44100.0;

        TimeRange time;
        TimeDuration endAllowance;

        bool createMidiFile = false;
        bool trimSilenceAtEnds = false;

        bool shouldNormalise = false;
        bool shouldNormaliseByRMS = false;
        float normaliseToLevelDb = 0;
        bool canRenderInMono = true;
        bool mustRenderInMono = false;
        bool usePlugins = true;
        bool useMasterPlugins = false;
        bool realTimeRender = false;
        bool ditheringEnabled = false;
        bool separateTracks = false;
        bool addAntiDenormalisationNoise = false;

        int quality = 0;
        juce::StringPairArray metadata;
        ProjectItem::Category category = ProjectItem::Category::none;

        float resultMagnitude = 0;
        float resultRMS = 0;
        float resultAudioDuration = 0;
    };

    //==============================================================================
    /** Task that actually performs the render operation in blocks.
        You should continually call the runJob method until it returns jobHasFinished.
    */
    class RenderTask    : public ThreadPoolJobWithProgress
    {
    public:
        RenderTask (const juce::String& taskDescription,
                    const Renderer::Parameters&,
                    std::atomic<float>* progressToUpdate,
                    juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver*);
        
        RenderTask (const juce::String& taskDescription,
                    const Renderer::Parameters&,
                    std::unique_ptr<tracktion::graph::Node>,
                    std::unique_ptr<tracktion::graph::PlayHead>,
                    std::unique_ptr<tracktion::graph::PlayHeadState>,
                    std::unique_ptr<ProcessState>,
                    std::atomic<float>* progressToUpdate,
                    juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver*);

        ~RenderTask() override;

        JobStatus runJob() override;
        float getCurrentTaskProgress() override   { return progress; }

        Renderer::Parameters params;
        juce::String errorMessage;

        //==============================================================================
        static void flushAllPlugins (const Plugin::Array&, double sampleRate, int samplesPerBlock);
        static void setAllPluginsRealtime (const Plugin::Array&, bool realtime);
        static bool addMidiMetaDataAndWriteToFile (juce::File, juce::MidiMessageSequence, const TempoSequence&);
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
    /** Renders an Edit to a file and creates a new ProjectItem for it. */
    static ProjectItem::Ptr renderToProjectItem (const juce::String& taskDescription, const Parameters& params);

    /** Renders an Edit to a file given by the Parameters. */
    static juce::File renderToFile (const juce::String& taskDescription, const Parameters& params);

    /** Renders an Edit to a file within the given time range and the track indicies described by the BigInteger. */
    static bool renderToFile (const juce::String& taskDescription,
                              const juce::File& outputFile,
                              Edit& edit,
                              TimeRange range,
                              const juce::BigInteger& tracksToDo,
                              bool usePlugins = true,
                              juce::Array<Clip*> clips = {},
                              bool useThread = true);

    //==============================================================================
    /** @see measureStatistics()
    */
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
};

}} // namespace tracktion { inline namespace engine
