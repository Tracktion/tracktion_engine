/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS

#include "../../../3rd_party/doctest/tracktion_doctest.hpp"
#include "../../../modules/tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"
#include "../../utilities/tracktion_TestUtilities.h"

namespace tracktion::inline engine
{

#if ENGINE_UNIT_TESTS_RENDERING
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
            /// Destructor
            ~Handle();

            void cancel();
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
            operation or poll to get progress/status messages.
        */
        static std::shared_ptr<Handle> render (Renderer::Parameters,
                                               std::function<void (tl::expected<juce::File, std::string>)> finishedCallback,
                                               std::shared_ptr<juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver> thumbnailToUpdate = {});
    };

    EditRenderer::Handle::~Handle()
    {
        cancel();
        renderThread.join();
    }

    void EditRenderer::Handle::cancel()
    {
        hasBeenCancelled = true;
    }

    float EditRenderer::Handle::getProgress() const
    {
        return progress;
    }

    auto EditRenderer::render (Renderer::Parameters r,
                               std::function<void (tl::expected<juce::File, std::string>)> finishedCallback,
                               std::shared_ptr<juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver> thumbnailToUpdate) -> std::shared_ptr<Handle>
    {
        assert (finishedCallback && "You must supply a finished callback");

        std::shared_ptr<Handle> renderHandle (new Handle());
        renderHandle->thumbnailToUpdate = std::move (thumbnailToUpdate);

        auto destFile = r.destFile;
        auto renderTask = render_utils::createRenderTask (std::move (r), {},
                                                          &renderHandle->progress,
                                                          renderHandle->thumbnailToUpdate.get());

        renderHandle->renderThread = std::thread ([destFile,
                                                  &hasBeenCancelledFlag = renderHandle->hasBeenCancelled,
                                                  finishedCallback = std::move (finishedCallback),
                                                  renderTask = std::move (renderTask)]
        {
            for (;;)
            {
                if (hasBeenCancelledFlag)
                    return finishedCallback (tl::unexpected (NEEDS_TRANS("Cancelled")));

                if (renderTask->runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
                    continue;

                // Finished
                if (auto err = renderTask->errorMessage; err.isNotEmpty())
                    return finishedCallback (tl::unexpected (err.toStdString()));

                return finishedCallback (destFile);
            }
        });

        return renderHandle;
    }

TEST_SUITE("tracktion_engine")
{
    TEST_CASE ("Renderer single audio track")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);

        auto fileLength = 5_td;
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, fileLength.inSeconds());

        auto track = getAudioTracks (*edit)[0];
        insertWaveClip (*track, {}, sinFile->getFile(), { .time = { 0_tp, fileLength } },
                        DeleteExistingClips::no);

        juce::TemporaryFile destFile (".wav");
        Renderer::Parameters params (*edit);
        params.destFile = destFile.getFile();
        params.time = params.time.withLength (fileLength);
        params.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
        std::atomic<bool> callbackFinished { false };

        auto thumbnail = std::make_shared<juce::AudioThumbnail> (256,
                                                                 engine.getAudioFileFormatManager().readFormatManager,
                                                                 engine.getAudioFileManager().getAudioThumbnailCache());

        auto handle = EditRenderer::render (std::move (params),
                                            [&callbackFinished, f = destFile.getFile()] (auto res)
                                            {
                                                CHECK (res);
                                                CHECK (*res == f);
                                                callbackFinished = true;
                                            },
                                            thumbnail);

        test_utilities::runDispatchLoopUntilTrue (callbackFinished);

        CHECK (callbackFinished);
        CHECK_EQ (handle->getProgress(), 1.0f);
        auto buffer = test_utilities::loadFileInToBuffer (engine, destFile.getFile());
        CHECK_EQ (buffer->getNumSamples(), toSamples (fileLength, 44100.0));

        // N.B. The samples/length on the thumbnail are quatised to the low-res rate so aren't accurate
        CHECK (thumbnail->isFullyLoaded());
        CHECK (thumbnail->getNumSamplesFinished() >= toSamples (fileLength, 44100.0));
        CHECK (thumbnail->getTotalLength() >= fileLength.inSeconds());
    }
}

#endif


#if ENGINE_UNIT_TESTS_FREEZE

//==============================================================================
//==============================================================================
class TrackFreezeTests  : public juce::UnitTest
{
public:
    TrackFreezeTests()
        : juce::UnitTest ("Track Freeze", "tracktion_engine")
    {
    }

    void runTest() override
    {
        auto& engine = *Engine::getEngines()[0];

        beginTest ("End allowance");
        {
            auto edit = test_utilities::createTestEdit (engine);
            auto track = getAudioTracks (*edit)[0];

            // Create a track with a plugin with end allowance
            auto synth = dynamic_cast<FourOscPlugin*> (edit->getPluginCache().createNewPlugin (FourOscPlugin::xmlTypeName, {}).get());
            static auto organPatch = "<PLUGIN type=\"4osc\" id=\"1069\" enabled=\"1\" filterType=\"1\" presetName=\"4OSC: Organ\" filterFreq=\"127.0\" ampAttack=\"0.60000002384185791016\" ampDecay=\"10.0\" ampSustain=\"100.0\" ampRelease=\"0.40000000596046447754\" waveShape1=\"4\" tune2=\"-24.0\" waveShape2=\"4\"> <MODMATRIX/> </PLUGIN>";

            if (auto e = juce::parseXML (organPatch))
                if (auto v = juce::ValueTree::fromXml (*e); v.isValid())
                    synth->restorePluginStateFromValueTree (v);

            const auto tailLength = synth->getTailLength();
            expectGreaterThan (tailLength, 0.0);

            track->pluginList.insertPlugin (*synth, 0, nullptr);

            // Insert a MIDI clip with a 1-beat note
            auto midiClip = track->insertMIDIClip ({ 0.0s, TimePosition (1.0s) }, nullptr);
            midiClip->getSequence().addNote (69, BeatPosition::fromBeats (0.0), BeatDuration::fromBeats (1.0), 127, 0, nullptr);
            const auto trackLength = track->getLengthIncludingInputTracks();
            expectWithinAbsoluteError (trackLength.inSeconds(), 1.0, 0.001);

            // Check expected end allowance
            juce::Array<EditItemID> trackIDs { track->itemID };
            juce::Array<Clip*> clips { midiClip.get() };
            const auto endAllowance = RenderOptions::findEndAllowance (*edit, &trackIDs, &clips);
            expectWithinAbsoluteError (endAllowance.inSeconds(), tailLength, 0.001);

            // Ensure freezing that track has the track length plus the end allowance
            const auto expectedTotalLength = trackLength + TimeDuration::fromSeconds (tailLength);

            {
                track->setFrozen (true, AudioTrack::individualFreeze);
                const auto freezeFile = AudioFile (engine, TemporaryFileManager::getFreezeFileForTrack (*track));
                expectWithinAbsoluteError (freezeFile.getLength(), expectedTotalLength.inSeconds(), 0.001);

                engine.getAudioFileManager().releaseAllFiles();
                edit->getTempDirectory (false).deleteRecursively();
                expect (! freezeFile.getFile().exists());
            }

            {
                track->setFrozen (false, AudioTrack::individualFreeze);

                // Create a new track and set the destination of the first one to this
                auto track2 = edit->insertNewAudioTrack ({ {},{} }, nullptr);
                track->getOutput().setOutputToTrack (track2.get());
                trackIDs = juce::Array<EditItemID> { track2->itemID };
                expectEquals (RenderOptions::findEndAllowance (*edit, &trackIDs, nullptr), TimeDuration(),
                              "End allowance of new empty track is not 0");

                // Now freeze this track and it should contain track's end allowance
                track2->setFrozen (true, AudioTrack::individualFreeze);
                const auto freezeFile = AudioFile (engine, TemporaryFileManager::getFreezeFileForTrack (*track2));
                expectWithinAbsoluteError (freezeFile.getLength(), expectedTotalLength.inSeconds(), 0.001);

                engine.getAudioFileManager().releaseAllFiles();
                edit->getTempDirectory (false).deleteRecursively();
                expect (! freezeFile.getFile().exists());
            }
        }
    }
};

static TrackFreezeTests trackFreezeTests;

#endif

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS