/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIP_LAUNCHER

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <tracktion_engine/utilities/tracktion_TestUtilities.h>

namespace tracktion::inline engine
{

//==============================================================================
// These tests play an Edit live through an EnginePlayer and drive clip
// LaunchHandles the way the clip launcher UI does, then verify the audible
// output. Clips contain test tones at known frequencies so the presence or
// absence of each clip can be checked independently, even in a mixed output.
//
// All tests use the standard 60bpm test Edit so 1 beat == 1 second and
// (in 4/4) 1 bar == 4 seconds.
//==============================================================================
namespace clip_launcher_test_utilities
{
    constexpr double sampleRate = 44100.0;
    constexpr int blockSize = 512;

    inline HostedAudioDeviceInterface::Parameters getPlayerParams (int numOutputChannels = 1)
    {
        return { .sampleRate = sampleRate, .blockSize = blockSize,
                 .inputChannels = 0, .outputChannels = numOutputChannels,
                 .inputNames = {}, .outputNames = {} };
    }

    inline TimeRange tr (double startSeconds, double endSeconds)
    {
        return { TimePosition::fromSeconds (startSeconds), TimePosition::fromSeconds (endSeconds) };
    }

    //==============================================================================
    /** Returns the amplitude of the given frequency over a range of the output
        using the Goertzel algorithm. A full-scale sine at that frequency returns ~1.
    */
    inline float getToneMagnitude (const choc::buffer::ChannelArrayBuffer<float>& output,
                                   TimeRange range, double frequency)
    {
        const auto startSample = toSamples (range.getStart(), sampleRate);
        const auto endSample = std::min (toSamples (range.getEnd(), sampleRate),
                                         (int64_t) output.getNumFrames());
        const auto numSamples = endSample - startSample;

        if (numSamples <= 0)
            return 0.0f;

        const double coeff = 2.0 * std::cos (juce::MathConstants<double>::twoPi * frequency / sampleRate);
        double s1 = 0.0, s2 = 0.0;

        for (auto i = startSample; i < endSample; ++i)
        {
            const double s0 = output.getSample (0, (choc::buffer::FrameCount) i) + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        const auto power = s1 * s1 + s2 * s2 - coeff * s1 * s2;
        return (float) (2.0 * std::sqrt (std::max (0.0, power)) / (double) numSamples);
    }

    inline float getRMSLevel (const choc::buffer::ChannelArrayBuffer<float>& output, TimeRange range,
                              int channel = 0)
    {
        const auto startSample = toSamples (range.getStart(), sampleRate);
        const auto endSample = std::min (toSamples (range.getEnd(), sampleRate),
                                         (int64_t) output.getNumFrames());
        const auto numSamples = endSample - startSample;

        if (numSamples <= 0)
            return 0.0f;

        double sum = 0.0;

        for (auto i = startSample; i < endSample; ++i)
        {
            const double s = output.getSample ((choc::buffer::ChannelCount) channel, (choc::buffer::FrameCount) i);
            sum += s * s;
        }

        return (float) std::sqrt (sum / (double) numSamples);
    }

    //==============================================================================
    /** A mono audio file held in memory and registered with the AudioFileManager.
        Clips using it read it synchronously, so they're audible from their first
        block. Files on disk are read through the AudioFileCache, whose reads time
        out while EnginePlayer runs faster than real time, which made the first
        launch of a clip ~150ms late.

        The clip readers point at this buffer, so create it before the Edit that
        uses it: it must outlive the Edit.
    */
    struct MemoryAudioFile
    {
        MemoryAudioFile (Engine& e, const choc::buffer::ChannelArrayBuffer<float>& source)
            : engine (e),
              buffer (1, source.getNumFrames()),
              file ("/memory/clip-launcher-test-" + juce::Uuid().toString() + ".wav")
        {
            copy (buffer, source);
            engine.getAudioFileManager().registerMemoryBuffer (file.getFullPathName().toStdString(),
                                                               buffer.getView(), sampleRate);
        }

        ~MemoryAudioFile()
        {
            engine.getAudioFileManager().unregisterMemoryBuffer (file.getFullPathName().toStdString());
        }

        juce::File getFile() const      { return file; }

        Engine& engine;
        choc::buffer::InterleavedBuffer<float> buffer;
        const juce::File file;

        JUCE_DECLARE_NON_COPYABLE (MemoryAudioFile)
    };

    inline std::unique_ptr<MemoryAudioFile> createMemoryFile (Engine& engine, double durationSeconds,
                                                              std::function<float (choc::buffer::FrameCount)> getSample)
    {
        auto buffer = choc::buffer::createChannelArrayBuffer (1, (int) (sampleRate * durationSeconds),
                                                              [&] (auto, auto frame) { return getSample (frame); });
        return std::make_unique<MemoryAudioFile> (engine, buffer);
    }

    inline float getSineSample (double frequency, choc::buffer::FrameCount frame)
    {
        return (float) std::sin (juce::MathConstants<double>::twoPi * frequency * frame / sampleRate);
    }

    /** Creates a mono in-memory sine file. */
    inline std::unique_ptr<MemoryAudioFile> createSineFile (Engine& engine, double durationSeconds, float frequency)
    {
        return createMemoryFile (engine, durationSeconds,
                                 [=] (auto frame) { return getSineSample (frequency, frame); });
    }

    /** Creates a mono in-memory file where the first half is a sine at freq1 and
        the second half a sine at freq2, so tests can detect which part of the
        source is being played.
    */
    inline std::unique_ptr<MemoryAudioFile> createTwoToneFile (Engine& engine, double durationOfEachToneSeconds,
                                                               float freq1, float freq2)
    {
        const auto numFramesPerTone = (choc::buffer::FrameCount) (sampleRate * durationOfEachToneSeconds);

        return createMemoryFile (engine, durationOfEachToneSeconds * 2.0,
                                 [=] (auto frame)
                                 {
                                     return frame < numFramesPerTone ? getSineSample (freq1, frame)
                                                                     : getSineSample (freq2, frame - numFramesPerTone);
                                 });
    }

    /** Finds when the output changes from fromFreq to toFreq, e.g. the midpoint
        of a createTwoToneFile source, to within a few milliseconds.
        Returns nullopt if fromFreq isn't followed by toFreq within the range.
    */
    inline std::optional<TimePosition> findToneChange (const choc::buffer::ChannelArrayBuffer<float>& output,
                                                       TimeRange searchRange, double fromFreq, double toFreq)
    {
        // A window half-way across the change has equal magnitudes of both
        // tones, so the first window dominated by toFreq is centred on it
        constexpr double windowSeconds = 0.05, hopSeconds = 0.002, minMagnitude = 0.1;
        bool seenFromFreq = false;

        for (auto t = searchRange.getStart().inSeconds(); t + windowSeconds <= searchRange.getEnd().inSeconds(); t += hopSeconds)
        {
            const auto window = tr (t, t + windowSeconds);
            const auto fromMagnitude = getToneMagnitude (output, window, fromFreq);
            const auto toMagnitude = getToneMagnitude (output, window, toFreq);

            if (fromMagnitude > toMagnitude && fromMagnitude > minMagnitude)
                seenFromFreq = true;
            else if (seenFromFreq && toMagnitude > fromMagnitude && toMagnitude > minMagnitude)
                return TimePosition::fromSeconds (t + windowSeconds / 2.0);
        }

        return {};
    }

    /** Returns the position within a createTwoToneFile source that is audible
        at outputTime, measured from the tone change found in searchRange
        (which is source position durationOfEachToneSeconds). Compare it with
        LaunchHandle::getPlayedRange() to check the UI and audio agree.
        Assumes the clip didn't loop or jump between the change and outputTime.
    */
    inline std::optional<TimeDuration> getAudibleContentPosition (const choc::buffer::ChannelArrayBuffer<float>& output,
                                                                  TimeRange searchRange, double freq1, double freq2,
                                                                  double durationOfEachToneSeconds, TimePosition outputTime)
    {
        if (auto change = findToneChange (output, searchRange, freq1, freq2))
            return TimeDuration::fromSeconds (durationOfEachToneSeconds) + (outputTime - *change);

        return {};
    }

    //==============================================================================
    struct TestEditWithSlot
    {
        std::unique_ptr<Edit> edit;
        AudioTrack* track = nullptr;
        ClipSlot* slot = nullptr;
    };

    inline TestEditWithSlot createEditWithClipSlot (Engine& engine)
    {
        auto edit = test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
        auto track = getAudioTracks (*edit)[0];
        track->getClipSlotList().ensureNumberOfSlots (1);
        edit->getSceneList().ensureNumberOfScenes (1);
        auto slot = track->getClipSlotList().getClipSlots()[0];

        return { std::move (edit), track, slot };
    }

    /** Inserts an audio file in to a slot. N.B. insertNewClip gives clips added
        to a ClipSlot launcher defaults: no proxy, auto-tempo and looping over
        their full length.
    */
    inline WaveAudioClip::Ptr insertAudioClipIntoSlot (ClipSlot& slot, const juce::File& file,
                                                       TimeDuration offset = {})
    {
        AudioFile af (slot.edit.engine, file);
        return insertWaveClip (slot, file.getFileName(), file,
                               { tr (0.0, af.getLength()), offset },
                               DeleteExistingClips::yes);
    }

    /** Inserts an empty MIDI clip in to a slot; notes are added by the caller.
        As above, the clip defaults to looping over its full length.
    */
    inline MidiClip::Ptr insertMidiClipIntoSlot (ClipSlot& slot, BeatDuration length)
    {
        // 60bpm test Edit: 1 beat == 1 second
        return insertMIDIClip (slot, tr (0.0, length.inBeats()));
    }

    /** Adds a 4OSC sine patch with a fast envelope so note starts/stops are
        detectable with tight timing windows.
    */
    inline void addSineSynthPlugin (AudioTrack& track)
    {
        auto synth = dynamic_cast<FourOscPlugin*> (track.edit.getPluginCache().createNewPlugin (FourOscPlugin::xmlTypeName, {}).get());
        static auto sinePatch = "<PLUGIN type=\"4osc\" enabled=\"1\" presetName=\"4OSC: Sine\" ampAttack=\"0.001\" ampDecay=\"10.0\" ampSustain=\"100.0\" ampRelease=\"0.01\" waveShape1=\"1\"> <MODMATRIX/> </PLUGIN>";

        if (auto e = juce::parseXML (sinePatch))
            if (auto v = juce::ValueTree::fromXml (*e); v.isValid())
                synth->restorePluginStateFromValueTree (v);

        track.pluginList.insertPlugin (*synth, 0, nullptr);
    }

    //==============================================================================
    /** A test-only plugin that records the note-ons and note-offs a track sends it,
        timed by the output samples it has processed, so tests can check a note is
        struck and released exactly once (which a tone measurement can't).
    */
    class MidiProbePlugin  : public Plugin
    {
    public:
        MidiProbePlugin (PluginCreationInfo info)  : Plugin (info) {}
        ~MidiProbePlugin() override                             { notifyListenersOfDeletion(); }

        static const char* getPluginName()                      { return "MIDI Probe"; }
        static constexpr const char* xmlTypeName = "midiProbe";

        juce::String getName() const override                   { return getPluginName(); }
        juce::String getPluginType() override                   { return xmlTypeName; }
        juce::String getSelectableDescription() override        { return getName(); }
        BusLayout getBusses() const override                    { return BusLayout::singleStereoInOut(); }
        bool takesMidiInput() override                          { return true; }

        void initialise (const PluginInitialisationInfo&) override {}
        void deinitialise() override {}
        void restorePluginStateFromValueTree (const juce::ValueTree&) override {}

        void applyToBuffer (const PluginRenderContext& fc) override
        {
            if (fc.bufferForMidiMessages != nullptr)
                for (auto& m : *fc.bufferForMidiMessages)
                    if (m.isNoteOnOrOff())
                        events.push_back ({ m.isNoteOn(), m.getNoteNumber(),
                                            (double) numSamplesProcessed / sampleRate + m.getTimeStamp() });

            numSamplesProcessed += fc.bufferNumSamples;
        }

        struct Event
        {
            bool isNoteOn = false;
            int noteNumber = 0;
            double outputTime = 0.0;
        };

        /** Returns the note-ons or note-offs of a note sent within an output range. */
        int countEvents (bool noteOns, int noteNumber, TimeRange range) const
        {
            return (int) std::count_if (events.begin(), events.end(),
                                        [&] (const Event& e)
                                        {
                                            return e.isNoteOn == noteOns && e.noteNumber == noteNumber
                                                    && range.contains (TimePosition::fromSeconds (e.outputTime));
                                        });
        }

        std::vector<Event> events;
        int64_t numSamplesProcessed = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiProbePlugin)
    };

    /** Adds a MidiProbePlugin to the start of a track's plugins. */
    inline MidiProbePlugin& addMidiProbePlugin (AudioTrack& track)
    {
        track.edit.engine.getPluginManager().createBuiltInType<MidiProbePlugin>();
        auto probe = dynamic_cast<MidiProbePlugin*> (track.edit.getPluginCache().createNewPlugin (MidiProbePlugin::xmlTypeName, {}).get());
        assert (probe != nullptr);
        track.pluginList.insertPlugin (*probe, 0, nullptr);
        return *probe;
    }

    //==============================================================================
    /** A multi-track, multi-scene Edit where every slot contains a test tone at
        a unique frequency, so each clip's presence in the mixed output can be
        verified independently. The first two tracks are audio, the last two are
        MIDI driven by a sine synth.
    */
    struct SceneTestContext
    {
        std::vector<std::unique_ptr<MemoryAudioFile>> files;    // N.B. declared before the Edit to outlive it
        std::unique_ptr<Edit> edit;
        std::vector<std::vector<double>> frequencies;                       // [scene][track]
        std::vector<std::vector<std::shared_ptr<LaunchHandle>>> handles;    // [scene][track]

        static constexpr int numAudioTracks = 2, numMidiTracks = 2;
    };

    inline SceneTestContext createSceneTestEdit (Engine& engine, int numScenes)
    {
        // Integer audio frequencies give a whole number of cycles over the 4s
        // files so the default full-length loop is seamless. No frequency,
        // audio or MIDI, collides with a harmonic of a concurrent one
        static constexpr int audioFrequencies[2][SceneTestContext::numAudioTracks] = { { 220, 330 }, { 262, 494 } };
        static constexpr int midiNotes[2][SceneTestContext::numMidiTracks] = { { 69, 73 }, { 62, 79 } };
        assert (numScenes <= 2);

        SceneTestContext ctx;
        ctx.edit = test_utilities::createTestEdit (engine, SceneTestContext::numAudioTracks + SceneTestContext::numMidiTracks,
                                                   Edit::EditRole::forEditing);
        auto tracks = getAudioTracks (*ctx.edit);

        for (auto t : tracks)
            t->getClipSlotList().ensureNumberOfSlots (numScenes);

        ctx.edit->getSceneList().ensureNumberOfScenes (numScenes);

        for (int i = 0; i < SceneTestContext::numMidiTracks; ++i)
            addSineSynthPlugin (*tracks[SceneTestContext::numAudioTracks + i]);

        for (int scene = 0; scene < numScenes; ++scene)
        {
            std::vector<double> sceneFrequencies;
            std::vector<std::shared_ptr<LaunchHandle>> sceneHandles;

            for (int t = 0; t < SceneTestContext::numAudioTracks; ++t)
            {
                auto file = createSineFile (engine, 4.0, (float) audioFrequencies[scene][t]);
                auto slot = tracks[t]->getClipSlotList().getClipSlots()[scene];
                auto clip = insertAudioClipIntoSlot (*slot, file->getFile());

                sceneFrequencies.push_back (audioFrequencies[scene][t]);
                sceneHandles.push_back (clip->getLaunchHandle());
                ctx.files.push_back (std::move (file));
            }

            for (int t = 0; t < SceneTestContext::numMidiTracks; ++t)
            {
                auto slot = tracks[SceneTestContext::numAudioTracks + t]->getClipSlotList().getClipSlots()[scene];
                auto clip = insertMidiClipIntoSlot (*slot, 4_bd);
                clip->getSequence().addNote (midiNotes[scene][t], 0_bp, 4_bd, 127, 0, nullptr);

                sceneFrequencies.push_back (juce::MidiMessage::getMidiNoteInHertz (midiNotes[scene][t]));
                sceneHandles.push_back (clip->getLaunchHandle());
            }

            ctx.frequencies.push_back (std::move (sceneFrequencies));
            ctx.handles.push_back (std::move (sceneHandles));
        }

        return ctx;
    }

    /** Checks each clip in a scene is audible (or not) in a range of the output. */
    inline void checkSceneAudible (const choc::buffer::ChannelArrayBuffer<float>& output,
                                   const SceneTestContext& ctx, int scene,
                                   TimeRange range, bool shouldBeAudible)
    {
        for (size_t i = 0; i < ctx.frequencies[(size_t) scene].size(); ++i)
        {
            const auto frequency = ctx.frequencies[(size_t) scene][i];
            const bool isAudioTrack = i < SceneTestContext::numAudioTracks;
            CAPTURE (scene);
            CAPTURE (frequency);

            if (shouldBeAudible)
                CHECK_GT (getToneMagnitude (output, range, frequency), isAudioTrack ? 0.5f : 0.05f);
            else
                CHECK_LT (getToneMagnitude (output, range, frequency), isAudioTrack ? 0.05f : 0.01f);
        }
    }

    //==============================================================================
    struct LaunchPosition
    {
        MonotonicBeat monotonicBeat;
        TimePosition editTime;
    };

    /** Returns the next quantised launch position based on the current playback
        sync point, the same way the clip launcher UI does.
    */
    inline std::optional<LaunchPosition> getNextQuantisedLaunchPosition (Edit& edit, LaunchQType q)
    {
        if (auto epc = edit.getTransport().getCurrentPlaybackContext())
        {
            if (auto syncPoint = epc->getSyncPoint())
            {
                const auto quantisedBeat = getNext (q, edit.tempoSequence, syncPoint->beat);
                return LaunchPosition { MonotonicBeat { syncPoint->monotonicBeat.v + (quantisedBeat - syncPoint->beat) },
                                        edit.tempoSequence.toTime (quantisedBeat) };
            }
        }

        return {};
    }
}

//==============================================================================
//==============================================================================
TEST_SUITE ("tracktion_engine")
{
    using namespace clip_launcher_test_utilities;

    TEST_CASE ("Clip launcher: launch and stop during playback (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);

        process (player, 1_td);
        launchHandle->play ({});
        process (player, 3_td);
        launchHandle->stop ({});
        process (player, 2_td);

        const auto output = player.getOutput();
        CHECK_LT (getRMSLevel (output, tr (0.0, 0.9)), 0.005f);
        CHECK_GT (getToneMagnitude (output, tr (1.1, 3.9), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (4.1, 6.0)), 0.005f);
    }

    TEST_CASE ("Clip launcher: launching mid-timeline plays clip from its start (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto twoToneFile = createTwoToneFile (engine, 4.0, 220.0f, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);

        process (player, 5_td);
        launchHandle->play ({});
        process (player, 3_td);

        const auto output = player.getOutput();
        CHECK_LT (getRMSLevel (output, tr (0.0, 4.9)), 0.005f);

        // The clip must play from its own start (the 220Hz section), not from
        // the 5s timeline position (which would be in the 330Hz section)
        CHECK_GT (getToneMagnitude (output, tr (5.1, 7.9), 220.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (5.1, 7.9), 330.0), 0.05f);
    }

    TEST_CASE ("Clip launcher: quantised launch and stop (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);

        process (player, 1.5_td);

        // Next bar boundary is at 4s (60bpm, 4/4)
        auto launchPos = getNextQuantisedLaunchPosition (*edit, LaunchQType::bar);
        REQUIRE (launchPos);
        CHECK (launchPos->editTime == TimePosition::fromSeconds (4.0));
        launchHandle->play (launchPos->monotonicBeat);

        process (player, 4.5_td); // to 6s

        // Next bar boundary is at 8s
        auto stopPos = getNextQuantisedLaunchPosition (*edit, LaunchQType::bar);
        REQUIRE (stopPos);
        CHECK (stopPos->editTime == TimePosition::fromSeconds (8.0));
        launchHandle->stop (stopPos->monotonicBeat);

        process (player, 4_td); // to 10s

        const auto output = player.getOutput();
        CHECK_LT (getRMSLevel (output, tr (0.0, 3.9)), 0.005f);
        CHECK_GT (getToneMagnitude (output, tr (4.1, 7.9), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (8.1, 10.0)), 0.005f);
    }

    TEST_CASE ("Clip launcher: launch with clip source offset (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto twoToneFile = createTwoToneFile (engine, 4.0, 220.0f, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);

        // Offset of 4s skips the whole 220Hz section. Looping is disabled so
        // this is a one-shot clip that should auto-stop when it finishes.
        // N.B. disableLooping() folds the loop start back in to the offset so
        // the offset has to be set afterwards
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        clip->disableLooping();
        clip->setOffset (TimeDuration::fromSeconds (4.0));
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);

        process (player, 1_td);
        launchHandle->play ({});
        process (player, 10_td);

        const auto output = player.getOutput();

        // Only the 330Hz section should be heard, for the 4s left after the offset
        CHECK_GT (getToneMagnitude (output, tr (1.1, 4.9), 330.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (0.0, 11.0), 220.0), 0.05f);
        CHECK_LT (getRMSLevel (output, tr (5.1, 11.0)), 0.005f);

        // The one-shot clip should have stopped itself after its 8 beat duration
        CHECK (launchHandle->getPlayingStatus() == LaunchHandle::PlayState::stopped);
    }

    TEST_CASE ("Clip launcher: looping clip with loop-start offset (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto twoToneFile = createTwoToneFile (engine, 4.0, 220.0f, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);

        // Looping over the whole file (the slot clip default), starting 4s in
        // (the 330Hz section)
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile(), TimeDuration::fromSeconds (4.0));
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);

        process (player, 1_td);
        launchHandle->play ({});
        process (player, 11_td);

        const auto output = player.getOutput();

        // Starts at the offset (330Hz), wraps to the loop start (220Hz), then back to 330Hz
        CHECK_GT (getToneMagnitude (output, tr (1.1, 4.9), 330.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (1.1, 4.9), 220.0), 0.05f);

        CHECK_GT (getToneMagnitude (output, tr (5.1, 8.9), 220.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (5.1, 8.9), 330.0), 0.05f);

        CHECK_GT (getToneMagnitude (output, tr (9.1, 11.9), 330.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (9.1, 11.9), 220.0), 0.05f);
    }

    TEST_CASE ("Clip launcher: looping clip continues across loop boundary (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 2.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);

        process (player, 1_td);
        launchHandle->play ({});
        process (player, 6_td);

        const auto output = player.getOutput();

        // The loop wraps at 3s and 5s; the tone should be present throughout
        for (double windowStart = 1.1; windowStart < 6.0; windowStart += 0.5)
            CHECK_GT (getToneMagnitude (output, tr (windowStart, windowStart + 0.5), 220.0), 0.5f);
    }

    TEST_CASE ("Clip launcher: retrigger restarts clip from its start (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto twoToneFile = createTwoToneFile (engine, 4.0, 220.0f, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        launchHandle->play ({});
        edit->getTransport().play (false);

        process (player, 5_td);

        // 220Hz for the first 4s, then in to the 330Hz section
        const auto firstPart = player.getOutput();
        CHECK_GT (getToneMagnitude (firstPart, tr (0.1, 3.9), 220.0), 0.5f);
        CHECK_GT (getToneMagnitude (firstPart, tr (4.1, 4.9), 330.0), 0.5f);

        // Retriggering must restart from the 220Hz section
        launchHandle->play ({});
        process (player, 3_td);

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (5.1, 7.9), 220.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (5.1, 7.9), 330.0), 0.05f);
    }

    TEST_CASE ("Clip launcher: retriggering a playing clip doesn't click (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        // A low frequency sine changes by less than 0.01 per sample, and it
        // starts at zero so a restart on one of its peaks is a full-scale step
        constexpr double frequency = 55.0;
        auto sinFile = createSineFile (engine, 8.0, (float) frequency);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());

        // The default time-stretch mode depends on which stretchers are built so pin one that
        // every test build enables
        clip->setTimeStretchMode (TimeStretcher::soundtouchBetter);
        REQUIRE (clip->getTimeStretchMode() == TimeStretcher::soundtouchBetter);

        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        launchHandle->play ({});
        edit->getTransport().play (false);

        process (player, 1.5_td);

        // Retrigger mid-block, on a peak of the playing sine
        const auto retriggerTime = TimePosition::fromSeconds (2.0 + 0.25 / frequency);
        const auto retriggerBeat = edit->tempoSequence.toBeats (retriggerTime);

        auto epc = edit->getTransport().getCurrentPlaybackContext();
        REQUIRE (epc);
        auto syncPoint = epc->getSyncPoint();
        REQUIRE (syncPoint);

        launchHandle->play (MonotonicBeat { syncPoint->monotonicBeat.v + (retriggerBeat - syncPoint->beat) });
        process (player, 1_td);

        auto playedRange = launchHandle->getPlayedRange();
        REQUIRE (playedRange);
        CHECK (playedRange->getStart().inBeats() == doctest::Approx (retriggerBeat.inBeats()).epsilon (0.001));

        const auto output = player.getOutput();
        const auto startSample = toSamples (TimePosition::fromSeconds (1.9), sampleRate);
        const auto endSample = toSamples (TimePosition::fromSeconds (2.4), sampleRate);
        float maxStep = 0.0f;

        for (auto i = startSample + 1; i < endSample; ++i)
            maxStep = std::max (maxStep, std::abs (output.getSample (0, (choc::buffer::FrameCount) i)
                                                   - output.getSample (0, (choc::buffer::FrameCount) (i - 1))));

        CHECK_LT (maxStep, 0.1f);
    }

    TEST_CASE ("Clip launcher: retriggering a playing clip keeps its start transient (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        // A 1kHz sine for the first second, starting at zero and peaking after
        // 11 samples like a drum hit, then silence for the rest of the file
        constexpr double frequency = 1000.0;
        const auto numToneFrames = (choc::buffer::FrameCount) sampleRate;
        auto file = createMemoryFile (engine, 8.0,
                                      [=] (auto frame) { return frame < numToneFrames ? getSineSample (frequency, frame) : 0.0f; });
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, file->getFile());

        // The default time-stretch mode depends on which stretchers are built so pin one. The
        // sample-alignment check below needs a stretcher whose output is sample-accurate at a 1:1
        // ratio, which Signalsmith is but RubberBand and SoundTouch aren't, so it's only made when
        // Signalsmith is built. Otherwise SoundTouch, which every test build enables, is used
       #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
        constexpr auto stretchMode = TimeStretcher::signalsmithDefault;
       #else
        constexpr auto stretchMode = TimeStretcher::soundtouchBetter;
       #endif

        clip->setTimeStretchMode (stretchMode);
        REQUIRE (clip->getTimeStretchMode() == stretchMode);

        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        launchHandle->play ({});
        edit->getTransport().play (false);

        process (player, 1.5_td);

        // Retrigger mid-block, whilst the clip is silent
        const auto retriggerTime = TimePosition::fromSeconds (2.25);
        const auto retriggerBeat = edit->tempoSequence.toBeats (retriggerTime);

        auto epc = edit->getTransport().getCurrentPlaybackContext();
        REQUIRE (epc);
        auto syncPoint = epc->getSyncPoint();
        REQUIRE (syncPoint);

        launchHandle->play (MonotonicBeat { syncPoint->monotonicBeat.v + (retriggerBeat - syncPoint->beat) });
        process (player, 1_td);

        // There's no jump to smooth, so the first half-cycle of the restarted tone should come
        // through at full level and sample-aligned with the retrigger point (i.e. with no stale
        // resampler history or uncompensated latency)
        const auto output = player.getOutput();
        const auto retriggerSample = toSamples (retriggerTime, sampleRate);
        float firstPeak = 0.0f, maxError = 0.0f;

        for (int i = 0; i < 22; ++i)
        {
            const auto sample = output.getSample (0, (choc::buffer::FrameCount) (retriggerSample + i));
            const auto expected = (float) std::sin (juce::MathConstants<double>::twoPi * frequency * i / sampleRate);
            firstPeak = std::max (firstPeak, std::abs (sample));
            maxError = std::max (maxError, std::abs (sample - expected));
        }

        CHECK_GT (firstPeak, 0.9f);

       #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
        CHECK_LT (maxError, 0.05f);
       #else
        juce::ignoreUnused (maxError);
       #endif
    }

    TEST_CASE ("Clip launcher: switching between arranger and launcher (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto arrangerFile = createSineFile (engine, 12.0, 220.0f);
        auto slotFile = createSineFile (engine, 8.0, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);

        insertWaveClip (*track, {}, arrangerFile->getFile(), { tr (0.0, 12.0) }, DeleteExistingClips::no)
            ->setUsesProxy (false);

        auto slotClip = insertAudioClipIntoSlot (*slot, slotFile->getFile());
        auto launchHandle = slotClip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);

        process (player, 2_td);

        // In the app, ArrangerLauncherSwitchingNode's shared timer latches
        // playSlotClips once a slot starts playing. The message loop isn't
        // running here so set it at the launch point ourselves
        launchHandle->play ({});
        track->playSlotClips = true;
        process (player, 4_td);

        // Stopping the clip leaves the track in launcher mode, so it goes silent
        launchHandle->stop ({});
        process (player, 2_td);

        // The user's "play arranger" action brings back the arranger clips
        track->playSlotClips = false;
        process (player, 2_td);

        const auto output = player.getOutput();

        CHECK_GT (getToneMagnitude (output, tr (0.5, 1.9), 220.0), 0.5f);

        CHECK_GT (getToneMagnitude (output, tr (2.1, 5.9), 330.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (2.1, 5.9), 220.0), 0.05f);

        CHECK_LT (getRMSLevel (output, tr (6.1, 7.9)), 0.005f);

        CHECK_GT (getToneMagnitude (output, tr (8.2, 9.9), 220.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (8.2, 9.9), 330.0), 0.05f);
    }

    //==============================================================================
    TEST_CASE ("Clip launcher: launch and stop during playback (MIDI)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        addSineSynthPlugin (*track);

        auto clip = insertMidiClipIntoSlot (*slot, 8_bd);
        clip->getSequence().addNote (69, 0_bp, 8_bd, 127, 0, nullptr);
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        const auto noteFreq = juce::MidiMessage::getMidiNoteInHertz (69);
        edit->getTransport().play (false);

        process (player, 1_td);
        launchHandle->play ({});
        process (player, 3_td);
        launchHandle->stop ({});
        process (player, 2_td);

        const auto output = player.getOutput();
        CHECK_LT (getRMSLevel (output, tr (0.0, 0.9)), 0.005f);
        CHECK_GT (getToneMagnitude (output, tr (1.2, 3.9), noteFreq), 0.05f);

        // The note-off must have been sent: no hanging note after the stop
        CHECK_LT (getRMSLevel (output, tr (4.5, 6.0)), 0.005f);
    }

    TEST_CASE ("Clip launcher: quantised launch (MIDI)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        addSineSynthPlugin (*track);

        auto clip = insertMidiClipIntoSlot (*slot, 8_bd);
        clip->getSequence().addNote (69, 0_bp, 8_bd, 127, 0, nullptr);
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        const auto noteFreq = juce::MidiMessage::getMidiNoteInHertz (69);
        edit->getTransport().play (false);

        process (player, 1.5_td);

        auto launchPos = getNextQuantisedLaunchPosition (*edit, LaunchQType::bar);
        REQUIRE (launchPos);
        CHECK (launchPos->editTime == TimePosition::fromSeconds (4.0));
        launchHandle->play (launchPos->monotonicBeat);

        process (player, 4.5_td); // to 6s

        const auto output = player.getOutput();
        CHECK_LT (getRMSLevel (output, tr (0.0, 3.9)), 0.005f);
        CHECK_GT (getToneMagnitude (output, tr (4.2, 6.0), noteFreq), 0.05f);
    }

    TEST_CASE ("Clip launcher: launching mid-timeline plays clip from its start (MIDI)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        addSineSynthPlugin (*track);

        // First half of the clip is note 69, second half note 76
        auto clip = insertMidiClipIntoSlot (*slot, 8_bd);
        clip->getSequence().addNote (69, 0_bp, 4_bd, 127, 0, nullptr);
        clip->getSequence().addNote (76, 4_bp, 4_bd, 127, 0, nullptr);
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        const auto firstNoteFreq = juce::MidiMessage::getMidiNoteInHertz (69);
        const auto secondNoteFreq = juce::MidiMessage::getMidiNoteInHertz (76);
        edit->getTransport().play (false);

        process (player, 5_td);
        launchHandle->play ({});
        process (player, 5_td);

        const auto output = player.getOutput();
        CHECK_LT (getRMSLevel (output, tr (0.0, 4.9)), 0.005f);

        // The clip must play from its own start (note 69), not the 5s timeline
        // position (which would be in the note 76 section)
        CHECK_GT (getToneMagnitude (output, tr (5.2, 8.8), firstNoteFreq), 0.05f);
        CHECK_LT (getToneMagnitude (output, tr (5.2, 8.8), secondNoteFreq), 0.01f);
        CHECK_GT (getToneMagnitude (output, tr (9.2, 9.9), secondNoteFreq), 0.05f);
    }

    TEST_CASE ("Clip launcher: looping MIDI clip continues across loop boundary (MIDI)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        addSineSynthPlugin (*track);

        auto clip = insertMidiClipIntoSlot (*slot, 2_bd);
        clip->getSequence().addNote (69, 0_bp, 2_bd, 127, 0, nullptr);
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        const auto noteFreq = juce::MidiMessage::getMidiNoteInHertz (69);
        edit->getTransport().play (false);

        process (player, 1_td);
        launchHandle->play ({});
        process (player, 6_td);

        const auto output = player.getOutput();

        // The loop wraps at 3s and 5s; the note should re-trigger each time
        CHECK_GT (getToneMagnitude (output, tr (1.2, 2.8), noteFreq), 0.05f);
        CHECK_GT (getToneMagnitude (output, tr (3.2, 4.8), noteFreq), 0.05f);
        CHECK_GT (getToneMagnitude (output, tr (5.2, 6.8), noteFreq), 0.05f);
    }

    //==============================================================================
    TEST_CASE ("Clip launcher: launching a scene starts all clips (mixed audio and MIDI)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto ctx = createSceneTestEdit (engine, 1);

        // Queue the whole scene then start the transport, as launching a scene
        // from stopped does in the app
        for (auto& handle : ctx.handles[0])
            handle->play ({});

        ctx.edit->getTransport().play (false);

        process (player, 4_td);

        // Every clip in the scene must be heard
        checkSceneAudible (player.getOutput(), ctx, 0, tr (0.3, 3.9), true);
    }

    TEST_CASE ("Clip launcher: quantised scene launch starts all clips together")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto ctx = createSceneTestEdit (engine, 1);
        ctx.edit->getTransport().play (false);

        process (player, 1.5_td);

        // Launch the whole scene at the next bar boundary (4s)
        auto launchPos = getNextQuantisedLaunchPosition (*ctx.edit, LaunchQType::bar);
        REQUIRE (launchPos);
        CHECK (launchPos->editTime == TimePosition::fromSeconds (4.0));

        for (auto& handle : ctx.handles[0])
            handle->play (launchPos->monotonicBeat);

        process (player, 5.5_td); // to 7s

        const auto output = player.getOutput();
        CHECK_LT (getRMSLevel (output, tr (0.0, 3.9)), 0.005f);
        checkSceneAudible (output, ctx, 0, tr (4.3, 6.9), true);
    }

    TEST_CASE ("Clip launcher: switching scenes moves all tracks to the new scene")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto ctx = createSceneTestEdit (engine, 2);
        ctx.edit->getTransport().play (false);

        process (player, 1_td);

        for (auto& handle : ctx.handles[0])
            handle->play ({});

        process (player, 5_td); // to 6s

        // Switch to the second scene at the next bar boundary (8s): launch its
        // clips and stop the first scene's, as launching a scene row does
        auto switchPos = getNextQuantisedLaunchPosition (*ctx.edit, LaunchQType::bar);
        REQUIRE (switchPos);
        CHECK (switchPos->editTime == TimePosition::fromSeconds (8.0));

        for (auto& handle : ctx.handles[1])
            handle->play (switchPos->monotonicBeat);

        for (auto& handle : ctx.handles[0])
            handle->stop (switchPos->monotonicBeat);

        process (player, 5_td); // to 11s

        const auto output = player.getOutput();

        // First scene only before the switch, second scene only after it
        checkSceneAudible (output, ctx, 0, tr (1.3, 7.9), true);
        checkSceneAudible (output, ctx, 1, tr (1.3, 7.9), false);

        checkSceneAudible (output, ctx, 1, tr (8.3, 10.9), true);
        checkSceneAudible (output, ctx, 0, tr (8.3, 10.9), false);
    }

    //==============================================================================
    TEST_CASE ("Clip launcher: launcher clips can't be split or merged")
    {
        auto& engine = *Engine::getEngines()[0];
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        track->getClipSlotList().ensureNumberOfSlots (2);
        edit->getSceneList().ensureNumberOfScenes (2);
        auto otherSlot = track->getClipSlotList().getClipSlots()[1];

        auto clip = insertMidiClipIntoSlot (*slot, 4_bd);
        auto otherClip = insertMidiClipIntoSlot (*otherSlot, 4_bd);
        REQUIRE (clip);
        REQUIRE (otherClip);

        // The merged clip would go on the arranger and the launcher clips be removed
        auto arrangerClip = insertMIDIClip (*track, tr (4.0, 8.0));
        CHECK (mergeMidiClips ({ clip.get(), otherClip.get() }).failed());
        CHECK (mergeMidiClips ({ clip.get(), arrangerClip.get() }).failed());
        CHECK (slot->getClip() == clip.get());
        CHECK (otherSlot->getClip() == otherClip.get());
        CHECK (track->getClips().size() == 1);

        // Inserting the second half in to the slot would evict the original
        CHECK (split (*clip, TimePosition::fromSeconds (2.0)) == nullptr);
        CHECK (slot->getClip() == clip.get());
        CHECK (clip->getPosition().getLength() == TimeDuration::fromSeconds (4.0));

        // Arranger clips still split
        CHECK (split (*arrangerClip, TimePosition::fromSeconds (6.0)) != nullptr);
        CHECK (track->getClips().size() == 2);
    }

    //==============================================================================
    // Clip automation played live (waveform_beta#1283). The existing curve tests
    // render offline; these play the Edit through an EnginePlayer as the app does.
    // Each curve is a step: full level up to the step beat, then the step value
    //==============================================================================
    namespace clip_automation_test_utilities
    {
        inline AutomationCurveModifier::Ptr addStepCurve (Clip& clip, AutomatableParameter& param,
                                                          BeatPosition stepBeat, float before, float after)
        {
            auto curveMod = clip.getAutomationCurveList (true)->addCurve (param);
            auto& curve = curveMod->getCurve (CurveModifierType::absolute).curve;
            curve.addPoint (stepBeat, before, 0.0, nullptr);
            curve.addPoint (stepBeat, after, 0.0, nullptr);
            return curveMod;
        }

        inline VolumeAndPanPlugin& getVolumePluginWithoutSmoothing (AudioTrack& track)
        {
            auto volumePlugin = track.getVolumePlugin();
            REQUIRE (volumePlugin);
            volumePlugin->smoothingRampTimeSeconds = 0.0;
            return *volumePlugin;
        }
    }

    TEST_CASE ("Clip automation: volume curve on an arranger clip (live)")
    {
        using namespace clip_automation_test_utilities;
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { tr (0.0, 8.0) }, DeleteExistingClips::no);
        clip->setUsesProxy (false);

        auto& volumePlugin = getVolumePluginWithoutSmoothing (*track);
        addStepCurve (*clip, *volumePlugin.volParam, 2_bp, 1.0f, 0.0f);

        edit->getTransport().play (false);
        process (player, 4_td);

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (0.2, 1.8), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (2.2, 3.9)), 0.005f);
    }

    TEST_CASE ("Clip automation: volume curve added while the arranger clip plays (live)")
    {
        using namespace clip_automation_test_utilities;
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { tr (0.0, 8.0) }, DeleteExistingClips::no);
        clip->setUsesProxy (false);

        auto& volumePlugin = getVolumePluginWithoutSmoothing (*track);

        // As a user draws it: the graph is already playing
        edit->getTransport().play (false);
        process (player, 1_td);
        addStepCurve (*clip, *volumePlugin.volParam, 3_bp, 1.0f, 0.0f);

        // A new curve is picked up by a 10ms timer, which needs the message
        // loop, so run its update here
        volumePlugin.volParam->updateStream();
        process (player, 4_td); // to 5s

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (1.5, 2.8), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (3.5, 4.9)), 0.005f);
    }

    TEST_CASE ("Clip automation: pan curve on an arranger clip (live)")
    {
        using namespace clip_automation_test_utilities;
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams (2));

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { tr (0.0, 8.0) }, DeleteExistingClips::no);
        clip->setUsesProxy (false);

        // Centre, then hard right from 2s
        auto& volumePlugin = getVolumePluginWithoutSmoothing (*track);
        addStepCurve (*clip, *volumePlugin.panParam, 2_bp, 0.0f, 1.0f);

        edit->getTransport().play (false);
        process (player, 4_td);

        const auto output = player.getOutput();
        CHECK_GT (getRMSLevel (output, tr (0.2, 1.8), 0), 0.2f);
        CHECK_GT (getRMSLevel (output, tr (0.2, 1.8), 1), 0.2f);

        CHECK_LT (getRMSLevel (output, tr (2.2, 3.9), 0), 0.005f);
        CHECK_GT (getRMSLevel (output, tr (2.2, 3.9), 1), 0.4f);
    }

    TEST_CASE ("Clip automation: volume curve on a launcher clip (live)")
    {
        using namespace clip_automation_test_utilities;
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        // The curve is in the clip's own beats, so it steps 2 beats after the launch
        auto& volumePlugin = getVolumePluginWithoutSmoothing (*track);
        addStepCurve (*clip, *volumePlugin.volParam, 2_bp, 1.0f, 0.0f);

        edit->getTransport().play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 4_td); // to 5s

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (1.2, 2.8), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (3.2, 4.9)), 0.005f);
    }

    TEST_CASE ("Clip automation: a launcher clip's curve doesn't apply to other launched clips on its track (live)")
    {
        // waveform_beta#1298
        using namespace clip_automation_test_utilities;
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        track->getClipSlotList().ensureNumberOfSlots (2);
        edit->getSceneList().ensureNumberOfScenes (2);
        auto otherSlot = track->getClipSlotList().getClipSlots()[1];

        // A silent curve on a clip that's never launched
        auto automatedClip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto& volumePlugin = getVolumePluginWithoutSmoothing (*track);
        addStepCurve (*automatedClip, *volumePlugin.volParam, 0_bp, 0.0f, 0.0f);

        auto clip = insertAudioClipIntoSlot (*otherSlot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 4_td); // to 5s

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (1.2, 4.9), 220.0), 0.5f);
    }

    TEST_CASE ("Clip automation: a single-point volume curve holds its value (live)")
    {
        using namespace clip_automation_test_utilities;
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { tr (0.0, 8.0) }, DeleteExistingClips::no);
        clip->setUsesProxy (false);

        // The editor draws a single point as a flat line at its value
        auto& volumePlugin = getVolumePluginWithoutSmoothing (*track);
        auto curveMod = clip->getAutomationCurveList (true)->addCurve (*volumePlugin.volParam);
        curveMod->getCurve (CurveModifierType::absolute).curve.addPoint (1_bp, 0.0f, 0.0, nullptr);

        edit->getTransport().play (false);
        process (player, 3_td);

        const auto output = player.getOutput();
        CHECK_LT (getRMSLevel (output, tr (0.2, 2.9)), 0.005f);
    }

    TEST_CASE ("Clip automation: a track moved in to a submix folder keeps its clip automation (live)")
    {
        using namespace clip_automation_test_utilities;
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { tr (0.0, 8.0) }, DeleteExistingClips::no);
        clip->setUsesProxy (false);

        auto& volumePlugin = getVolumePluginWithoutSmoothing (*track);
        addStepCurve (*clip, *volumePlugin.volParam, 2_bp, 1.0f, 0.0f);

        auto submix = edit->insertNewFolderTrack ({ nullptr, track }, nullptr, true);
        edit->moveTrack (track, { submix.get(), nullptr });
        REQUIRE (track->getParentFolderTrack() == submix.get());

        edit->getTransport().play (false);
        process (player, 4_td);

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (0.2, 1.8), 220.0), 0.4f);
        CHECK_LT (getRMSLevel (output, tr (2.2, 3.9)), 0.005f);
    }

    // A redo re-adds the parameter's curve assignment before the clip's curve, which
    // left the parameter not following it (waveform_beta#1283)
    TEST_CASE ("Clip automation: a pasted clip's automation plays after undo and redo (live)")
    {
        using namespace clip_automation_test_utilities;
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto edit = test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
        auto& um = edit->getUndoManager();
        auto sourceTrack = getAudioTracks (*edit)[0];
        auto destTrack = getAudioTracks (*edit)[1];

        auto clip = insertWaveClip (*sourceTrack, {}, sinFile->getFile(), { tr (0.0, 8.0) }, DeleteExistingClips::no);
        clip->setUsesProxy (false);
        addStepCurve (*clip, *sourceTrack->getVolumePlugin()->volParam, 2_bp, 1.0f, 0.0f);
        sourceTrack->setMute (true);
        um.beginNewTransaction();

        Clipboard::Clips content;
        content.addSelectedClips ({ clip.get() }, Edit::getMaximumEditTimeRange(),
                                  Clipboard::Clips::AutomationLocked::no);
        EditInsertPoint insertPoint (*edit);
        Clipboard::ContentType::EditPastingOptions opts (*edit, insertPoint);
        opts.silent = true;
        opts.startTrack = destTrack;
        REQUIRE (content.pasteIntoEdit (opts));
        um.beginNewTransaction();

        um.undo();
        um.redo();
        REQUIRE (destTrack->getClips().size() == 1);

        auto& volumePlugin = getVolumePluginWithoutSmoothing (*destTrack);
        CHECK (volumePlugin.volParam->isAutomationActive());

        edit->getTransport().play (false);
        process (player, 4_td);

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (0.2, 1.8), 220.0), 0.4f);
        CHECK_LT (getRMSLevel (output, tr (2.2, 3.9)), 0.005f);
    }

    TEST_CASE ("Clip automation: a clip and curve added in one step play after undo and redo (live)")
    {
        using namespace clip_automation_test_utilities;
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto& um = edit->getUndoManager();
        um.beginNewTransaction();

        auto clip = insertWaveClip (*track, {}, sinFile->getFile(), { tr (0.0, 8.0) }, DeleteExistingClips::no);
        clip->setUsesProxy (false);
        addStepCurve (*clip, *track->getVolumePlugin()->volParam, 2_bp, 1.0f, 0.0f);
        um.beginNewTransaction();

        um.undo();
        um.redo();
        REQUIRE (track->getClips().size() == 1);

        auto& volumePlugin = getVolumePluginWithoutSmoothing (*track);
        CHECK (volumePlugin.volParam->isAutomationActive());

        edit->getTransport().play (false);
        process (player, 4_td);

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (0.2, 1.8), 220.0), 0.4f);
        CHECK_LT (getRMSLevel (output, tr (2.2, 3.9)), 0.005f);
    }

    //==============================================================================
    // Launched clips across transport jumps (waveform_beta#1286, #1287). Launched
    // clips run on the monotonic clock, so a jump in the Edit position (a relocate,
    // stop-move-play or arrangement loop wrap) doesn't change what they play: they
    // carry on from where they were. Clips here are two-tone 4s loops (220Hz then
    // 330Hz) launched at 1s, so they play 220Hz for 2s then 330Hz for 2s, repeating.
    //==============================================================================
    TEST_CASE ("Clip launcher: a backward jump doesn't move a looping launched clip (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto twoToneFile = createTwoToneFile (engine, 2.0, 220.0f, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 8_td); // to 9s, two whole loops in

        // Back to the start while playing: it carries on from the start of its loop
        edit->getTransport().setPosition (0s);
        process (player, 3_td); // edit 0 to 3, output 9 to 12

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (9.1, 10.9), 220.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (11.1, 11.9), 330.0), 0.5f);
        CHECK (launchHandle->getPlayingStatus() == LaunchHandle::PlayState::playing);
    }

    TEST_CASE ("Clip launcher: stopping, relocating and playing carries a looping launched clip on (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto twoToneFile = createTwoToneFile (engine, 2.0, 220.0f, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        auto& transport = edit->getTransport();
        transport.play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 3_td); // to 4s, 3 beats in

        // Stop, go back to the start, play again: as Home then Play. It carries on
        // from 3 beats in, as the beat count doesn't move whilst stopped
        transport.stop (false, false);
        process (player, 1_td); // output 4 to 5
        transport.setPosition (0s);
        transport.play (false);
        process (player, 3_td); // edit 0 to 3, output 5 to 8

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (5.1, 5.9), 330.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (6.1, 7.9), 220.0), 0.5f);
    }

    TEST_CASE ("Clip launcher: a forward jump keeps the played range and the audio in step (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto twoToneFile = createTwoToneFile (engine, 2.0, 220.0f, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 2_td); // to 3s, 2 beats in

        // Forward to edit beat 10: it carries on with its 330Hz half
        edit->getTransport().setPosition (10s);
        process (player, 2_td); // edit 10 to 12, output 3 to 5

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (3.1, 4.9), 330.0), 0.5f);

        // The played range ends where the edit is and is as long as it's played
        // for, so a playhead drawn from it shows what's heard: a whole loop in
        auto playedRange = launchHandle->getPlayedRange();
        REQUIRE (playedRange);
        CHECK (playedRange->getEnd().inBeats() == doctest::Approx (12.0).epsilon (0.001));
        CHECK (playedRange->getLength().inBeats() == doctest::Approx (4.0).epsilon (0.001));
    }

    TEST_CASE ("Clip launcher: a transport jump doesn't stop a launched one-shot clip (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        clip->disableLooping();
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 2_td); // to 3s

        // It carries on to its end, 8s after launching at 1s
        edit->getTransport().setPosition (0s);
        process (player, 7_td); // output 3 to 10

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (1.1, 2.9), 220.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (3.1, 8.9), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (9.2, 9.9)), 0.005f);
        CHECK (launchHandle->getPlayingStatus() == LaunchHandle::PlayState::stopped);
    }

    TEST_CASE ("Clip launcher: an arrangement loop wrap doesn't move a looping launched clip (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto twoToneFile = createTwoToneFile (engine, 2.0, 220.0f, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        // A 6 beat arrangement loop, which isn't a whole number of clip loops
        auto& transport = edit->getTransport();
        transport.setLoopRange (tr (0.0, 6.0));
        transport.looping = true;
        transport.play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 8_td); // edit 1-6 then 0-3, output 1 to 9

        // At the wrap at output 6s it's 5 beats in, so 1 beat into its loop: 220Hz
        // for another beat, then 330Hz, carrying on regardless of the wrap
        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (6.1, 6.9), 220.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (7.1, 8.9), 330.0), 0.5f);
    }

    TEST_CASE ("Clip launcher: an arrangement loop wrap doesn't stop a launched one-shot clip (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 6.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        clip->disableLooping();
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        auto& transport = edit->getTransport();
        transport.setLoopRange (tr (0.0, 4.0));
        transport.looping = true;
        transport.play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 8_td); // wrapping at output 4s and 8s, to 9s

        // It plays its 6s from 1s to 7s, through the wrap at 4s
        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (1.1, 6.9), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (7.2, 8.9)), 0.005f);
    }

    TEST_CASE ("Clip launcher: a held note carries on through an arrangement loop wrap (MIDI)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        addSineSynthPlugin (*track);

        // One note held for the whole 8 beat clip, so it's only struck at the clip's start
        auto clip = insertMidiClipIntoSlot (*slot, 8_bd);
        clip->getSequence().addNote (69, 0_bp, 8_bd, 127, 0, nullptr);
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        auto& transport = edit->getTransport();
        transport.setLoopRange (tr (0.0, 4.0));
        transport.looping = true;
        transport.play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 7_td); // wrapping at output 4s, to 8s

        // The wrap mustn't silence the note until the clip loops back to its start at 9s
        const auto noteFreq = juce::MidiMessage::getMidiNoteInHertz (69);
        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (1.2, 3.9), noteFreq), 0.05f);
        CHECK_GT (getToneMagnitude (output, tr (4.05, 4.5), noteFreq), 0.05f);
        CHECK_GT (getToneMagnitude (output, tr (4.5, 7.9), noteFreq), 0.05f);
    }

    TEST_CASE ("Clip launcher: an arrangement loop wrap doesn't restrike a held note (MIDI)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto& probe = addMidiProbePlugin (*track);

        auto clip = insertMidiClipIntoSlot (*slot, 8_bd);
        clip->getSequence().addNote (69, 0_bp, 8_bd, 127, 0, nullptr);
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        auto& transport = edit->getTransport();
        transport.setLoopRange (tr (0.0, 4.0));
        transport.looping = true;
        transport.play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 7_td); // wrapping at output 4s, to 8s

        // Struck once at the launch and held through the wrap at 4s, not cut and struck again
        CHECK_EQ (probe.countEvents (true, 69, tr (0.0, 8.0)), 1);
        CHECK_EQ (probe.countEvents (false, 69, tr (0.0, 8.0)), 0);
    }

    TEST_CASE ("Clip launcher: an arrangement loop wrap in a later clip loop doesn't drop a held note (MIDI)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto& probe = addMidiProbePlugin (*track);

        auto clip = insertMidiClipIntoSlot (*slot, 4_bd);
        clip->getSequence().addNote (69, 0_bp, 4_bd, 127, 0, nullptr);
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        auto& transport = edit->getTransport();
        transport.setLoopRange (tr (0.0, 6.0));
        transport.looping = true;
        transport.play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 7_td); // clip loops at output 5s, wraps at 6s, to 8s

        // The wrap at 6s is in the clip's second loop: the note struck at 5s is held to 8s
        CHECK_EQ (probe.countEvents (true, 69, tr (4.9, 8.0)), 1);
        CHECK_EQ (probe.countEvents (false, 69, tr (5.1, 8.0)), 0);
    }

    TEST_CASE ("Clip launcher: stopping and playing restrikes a note held in a later clip loop (MIDI)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto& probe = addMidiProbePlugin (*track);

        auto clip = insertMidiClipIntoSlot (*slot, 4_bd);
        clip->getSequence().addNote (69, 0_bp, 4_bd, 127, 0, nullptr);
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        auto& transport = edit->getTransport();
        transport.play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 5_td); // clip loops at output 5s, to 6s

        // Stopping releases the note. Playing again carries on 1 beat in to the
        // clip's second loop, so the held note has to be struck again
        transport.stop (false, false);
        process (player, 1_td); // output 6 to 7
        transport.play (false);
        process (player, 1_td); // output 7 to 8

        CHECK_EQ (probe.countEvents (false, 69, tr (5.9, 7.0)), 1);
        CHECK_EQ (probe.countEvents (true, 69, tr (6.9, 7.1)), 1);
        CHECK_EQ (probe.countEvents (false, 69, tr (7.1, 8.0)), 0);
    }

    TEST_CASE ("Clip launcher: retriggering a clip mid-block keeps the note-offs before it (MIDI)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto& probe = addMidiProbePlugin (*track);

        // Note 60 ends just before the relaunch, in the same block (5.9985s to 6.0101s)
        auto clip = insertMidiClipIntoSlot (*slot, 8_bd);
        clip->getSequence().addNote (60, 0_bp, 1.999_bd, 127, 0, nullptr);
        clip->getSequence().addNote (69, 0_bp, 8_bd, 127, 0, nullptr);
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        auto& transport = edit->getTransport();
        transport.play (false);
        process (player, 1_td);

        auto launchPos = getNextQuantisedLaunchPosition (*edit, LaunchQType::bar);
        REQUIRE (launchPos);
        launchHandle->play (launchPos->monotonicBeat); // starts at 4s
        process (player, 4_td); // to 5s

        // Relaunch 2 beats in, mid-block, just after note 60 ends
        launchHandle->play (MonotonicBeat { launchPos->monotonicBeat.v + 2.0_bd });
        process (player, 2_td); // to 7s

        // Note 60: struck at 4s, released at ~6s, struck again at 6s
        CHECK_EQ (probe.countEvents (true, 60, tr (3.9, 4.1)), 1);
        CHECK_EQ (probe.countEvents (false, 60, tr (5.9, 6.1)), 1);
        CHECK_EQ (probe.countEvents (true, 60, tr (5.9, 6.1)), 1);

        // And the relaunch's note-ons are at 6s, not earlier in the block
        for (auto& e : probe.events)
            if (e.isNoteOn && e.outputTime > 5.0)
                CHECK_GE (e.outputTime, 5.999);
    }

    TEST_CASE ("Clip launcher: switching scenes with an arrangement loop doesn't leave notes stuck or struck twice (MIDI)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        // Like the session in Tracktion/waveform_beta#1286: identical 16 beat clips in
        // several scenes, a string holding one note for the whole clip and a piano
        // whose chords are held across bars, with a 4 bar arrangement loop
        constexpr int numScenes = 3;
        auto edit = test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
        edit->getSceneList().ensureNumberOfScenes (numScenes);
        std::vector<MidiProbePlugin*> probes;
        std::vector<std::vector<std::shared_ptr<LaunchHandle>>> handles; // [track][scene], null for an empty slot

        for (auto track : getAudioTracks (*edit))
        {
            track->getClipSlotList().ensureNumberOfSlots (numScenes);
            probes.push_back (&addMidiProbePlugin (*track));
            const bool isString = probes.size() == 1;
            auto& trackHandles = handles.emplace_back();

            for (int scene = 0; scene < numScenes; ++scene)
            {
                // The string has no clip in the last scene, so switching to it stops the string
                if (isString && scene == numScenes - 1)
                {
                    trackHandles.push_back (nullptr);
                    continue;
                }

                auto clip = insertMidiClipIntoSlot (*track->getClipSlotList().getClipSlots()[scene], 16_bd);
                auto& seq = clip->getSequence();

                if (isString)
                {
                    seq.addNote (72, 0_bp, 16_bd, 127, 0, nullptr);
                }
                else
                {
                    for (auto [start, length] : { std::pair (0.5, 2.25), std::pair (3.5, 2.9), std::pair (8.0, 3.5), std::pair (12.0, 4.0) })
                        for (int note : { 51, 58, 62 })
                            seq.addNote (note, BeatPosition::fromBeats (start), BeatDuration::fromBeats (length), 100, 0, nullptr);
                }

                trackHandles.push_back (clip->getLaunchHandle());
            }
        }

        auto& transport = edit->getTransport();
        transport.setLoopRange (tr (0.0, 16.0));
        transport.looping = true;
        transport.play (false);
        process (player, 1_td);

        // Launch a scene as the launcher does: play each track's clip in it and stop the others
        auto launchScene = [&] (int scene, LaunchQType q)
        {
            const auto pos = getNextQuantisedLaunchPosition (*edit, q);
            REQUIRE (pos);

            for (auto& trackHandles : handles)
            {
                for (int s = 0; s < numScenes; ++s)
                {
                    if (auto& h = trackHandles[(size_t) s]; h && s == scene)
                        h->play (pos->monotonicBeat);
                    else if (h)
                        h->stop (pos->monotonicBeat);
                }
            }
        };

        // Scene changes and relaunches, quantised to bars and unquantised, wrapping the arrangement loop often
        const std::pair<int, LaunchQType> launches[] = { { 0, LaunchQType::bar }, { 1, LaunchQType::bar }, { 0, LaunchQType::bar },
                                                         { 0, LaunchQType::bar }, { 2, LaunchQType::bar }, { 1, LaunchQType::none },
                                                         { 0, LaunchQType::bar }, { 1, LaunchQType::none }, { 2, LaunchQType::bar },
                                                         { 0, LaunchQType::bar }, { 0, LaunchQType::none }, { 1, LaunchQType::bar } };

        for (auto [scene, q] : launches)
        {
            launchScene (scene, q);
            process (player, 7.3_td);
        }

        // Stop everything, then the transport
        for (auto& trackHandles : handles)
            for (auto& h : trackHandles)
                if (h)
                    h->stop ({});

        process (player, 1_td);
        transport.stop (false, false);
        process (player, 1_td);

        // Every note-on is ended by a note-off before the note is struck again, and nothing is left on
        for (auto probe : probes)
        {
            auto events = probe->events;
            // Ordered as plugins get them, with note-offs before note-ons at the same time
            std::stable_sort (events.begin(), events.end(), [] (auto& a, auto& b)
                                                            {
                                                                if (a.outputTime == b.outputTime)
                                                                    return ! a.isNoteOn && b.isNoteOn;

                                                                return a.outputTime < b.outputTime;
                                                            });
            std::map<int, bool> noteIsOn;
            int numNoteOns = 0;

            for (auto& e : events)
            {
                CAPTURE (e.noteNumber);
                CAPTURE (e.outputTime);
                CHECK (noteIsOn[e.noteNumber] != e.isNoteOn);
                noteIsOn[e.noteNumber] = e.isNoteOn;
                numNoteOns += e.isNoteOn ? 1 : 0;
            }

            CHECK_GT (numNoteOns, 5);

            for (auto [note, isOn] : noteIsOn)
            {
                CAPTURE (note);
                CHECK (! isOn);
            }
        }
    }

    TEST_CASE ("Clip launcher: a timed stop is unaffected by a transport jump (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 4.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        // A Stop follow action after 6 beats of playing
        clip->followActionDurationType = Clip::FollowActionDurationType::beats;
        clip->followActionBeats = 6_bd;
        auto followActions = clip->getFollowActions();
        REQUIRE (followActions);
        REQUIRE (followActions->getActions().size() == 1);
        followActions->getActions()[0]->action = FollowAction::globalStop;

        // Rebuild the graph with it, as the Edit's change timer would
        edit->getTransport().ensureContextAllocated (true);

        edit->getTransport().play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 2_td); // to 3s

        // The jump doesn't change how long it's played for, so it still stops 6s after launching
        edit->getTransport().setPosition (0s);
        process (player, 6_td); // output 3 to 9

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (3.2, 6.8), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (7.2, 8.9)), 0.005f);
    }

    TEST_CASE ("Clip automation: a launcher clip's automation keeps applying after a transport jump (live)")
    {
        using namespace clip_automation_test_utilities;
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 4.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        // Full level for the first beat of playing, then silent
        auto& volumePlugin = getVolumePluginWithoutSmoothing (*track);
        addStepCurve (*clip, *volumePlugin.volParam, 1_bp, 1.0f, 0.0f);

        edit->getTransport().play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 2_td); // to 3s

        // Forward to edit beat 9: the clip carries on 2 beats in, so the curve is
        // silent until the clip's next loop starts, 4 beats after the launch. It
        // used to stop applying, as the playhead had moved out of the played range
        edit->getTransport().setPosition (9s);
        process (player, 4_td); // edit 9 to 13, output 3 to 7

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (1.2, 1.8), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (2.2, 4.9)), 0.005f);
        CHECK_GT (getToneMagnitude (output, tr (5.2, 5.8), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (6.2, 6.9)), 0.005f);
    }

    TEST_CASE ("Clip launcher: going back to the arrangement cancels a queued launch (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        process (player, 1_td);

        // Queued for the next bar (4s), then back to the arrangement before then
        auto launchPos = getNextQuantisedLaunchPosition (*edit, LaunchQType::bar);
        REQUIRE (launchPos);
        launchHandle->play (launchPos->monotonicBeat);
        process (player, 1_td); // to 2s

        track->playSlotClips = true;
        track->playSlotClips = false;
        CHECK (! launchHandle->getQueuedStatus());

        process (player, 4_td); // to 6s, past the launch point

        const auto output = player.getOutput();
        CHECK_LT (getRMSLevel (output, tr (0.0, 6.0)), 0.005f);
        CHECK (launchHandle->getPlayingStatus() == LaunchHandle::PlayState::stopped);
        CHECK (! track->playSlotClips.get());
    }

    TEST_CASE ("Clip launcher: a render cancels a queued launch on the rendered tracks (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        // Renders turn the tracks' slots off for their duration, which stops a
        // launched clip and so, for the same reason, cancels a queued one
        launchHandle->play (MonotonicBeat { 4_bp });
        REQUIRE (launchHandle->getQueuedStatus() == LaunchHandle::QueueState::playQueued);

        {
            Track::Array tracks;
            tracks.add (track);
            const Renderer::ScopedClipSlotDisabler disabler (*edit, tracks);
        }

        CHECK (! launchHandle->getQueuedStatus());
    }

    TEST_CASE ("Clip launcher: relaunching a one-shot before it ends isn't lost to its end (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        // A 2 beat one-shot, launched at 1 so it ends at 3
        auto sinFile = createSineFile (engine, 2.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        clip->disableLooping();
        edit->getTransport().ensureContextAllocated (true);
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        process (player, 1_td);
        launchHandle->play ({});
        process (player, 1.5_td); // to 2.5s

        // Relaunch at the next bar (4s), after it ends. Its end used to replace
        // the queued play with a stop
        auto launchPos = getNextQuantisedLaunchPosition (*edit, LaunchQType::bar);
        REQUIRE (launchPos);
        launchHandle->play (launchPos->monotonicBeat);
        process (player, 3.5_td); // to 6s

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (1.2, 2.8), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (3.2, 3.8)), 0.005f);
        CHECK_GT (getToneMagnitude (output, tr (4.2, 5.8), 220.0), 0.5f);
    }

    TEST_CASE ("Clip launcher: disabling a launched or queued clip stops it")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        launchHandle->play ({});
        process (player, 1_td);
        REQUIRE (launchHandle->getPlayingStatus() == LaunchHandle::PlayState::playing);

        // Disabled, it has no playback node, so it would otherwise stay
        // "playing" and carry on part way through when enabled again
        clip->disabled = true;
        CHECK (launchHandle->getQueuedStatus() == LaunchHandle::QueueState::stopQueued);

        clip->disabled = false;
        launchHandle->stop ({});
        process (player, 0.5_td);
        REQUIRE (launchHandle->getPlayingStatus() == LaunchHandle::PlayState::stopped);

        // A queued launch is cancelled
        launchHandle->play (MonotonicBeat { 100_bp });
        clip->disabled = true;
        CHECK (! launchHandle->getQueuedStatus());
    }

    TEST_CASE ("Clip launcher: follow actions don't launch disabled clips (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto fileA = createSineFile (engine, 8.0, 220.0f);
        auto fileB = createSineFile (engine, 8.0, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        track->getClipSlotList().ensureNumberOfSlots (2);
        edit->getSceneList().ensureNumberOfScenes (2);
        auto clipA = insertAudioClipIntoSlot (*slot, fileA->getFile());
        auto clipB = insertAudioClipIntoSlot (*track->getClipSlotList().getClipSlots()[1], fileB->getFile());

        // After 2 beats A plays the next clip, which is disabled
        clipA->followActionDurationType = Clip::FollowActionDurationType::beats;
        clipA->followActionBeats = 2_bd;
        auto followActions = clipA->getFollowActions();
        REQUIRE (followActions);
        REQUIRE (followActions->getActions().size() == 1);
        followActions->getActions()[0]->action = FollowAction::trackNext;
        clipB->disabled = true;
        edit->getTransport().ensureContextAllocated (true);

        edit->getTransport().play (false);
        clipA->getLaunchHandle()->play ({});
        process (player, 4_td);

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (0.2, 1.8), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (2.2, 3.9)), 0.005f);
        CHECK (clipB->getLaunchHandle()->getPlayingStatus() == LaunchHandle::PlayState::stopped);
        CHECK (! clipB->getLaunchHandle()->getQueuedStatus());
    }

    TEST_CASE ("Clip launcher: group follow actions skip disabled clips (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto fileA = createSineFile (engine, 8.0, 220.0f);
        auto fileB = createSineFile (engine, 8.0, 330.0f);
        auto fileC = createSineFile (engine, 8.0, 440.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        track->getClipSlotList().ensureNumberOfSlots (3);
        edit->getSceneList().ensureNumberOfScenes (3);
        auto slots = track->getClipSlotList().getClipSlots();
        auto clipA = insertAudioClipIntoSlot (*slots[0], fileA->getFile());
        auto clipB = insertAudioClipIntoSlot (*slots[1], fileB->getFile());
        auto clipC = insertAudioClipIntoSlot (*slots[2], fileC->getFile());

        // After 2 beats A plays the next clip in its group. B is disabled, so that's C
        clipA->followActionDurationType = Clip::FollowActionDurationType::beats;
        clipA->followActionBeats = 2_bd;
        auto followActions = clipA->getFollowActions();
        REQUIRE (followActions);
        REQUIRE (followActions->getActions().size() == 1);
        followActions->getActions()[0]->action = FollowAction::currentGroupNext;
        clipB->disabled = true;
        edit->getTransport().ensureContextAllocated (true);

        edit->getTransport().play (false);
        clipA->getLaunchHandle()->play ({});
        process (player, 4_td);

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (0.2, 1.8), 220.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (2.2, 3.9), 440.0), 0.5f);
        CHECK (clipB->getLaunchHandle()->getPlayingStatus() == LaunchHandle::PlayState::stopped);
        CHECK (clipC->getLaunchHandle()->getPlayingStatus() == LaunchHandle::PlayState::playing);
    }

    TEST_CASE ("Clip launcher: a Play Other follow action launches another clip when slots before it are empty (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto fileA = createSineFile (engine, 8.0, 220.0f);
        auto fileB = createSineFile (engine, 8.0, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        track->getClipSlotList().ensureNumberOfSlots (3);
        edit->getSceneList().ensureNumberOfScenes (3);

        // The first slot is empty, so A's slot index (1) is B's index among the clips
        auto slots = track->getClipSlotList().getClipSlots();
        auto clipA = insertAudioClipIntoSlot (*slots[1], fileA->getFile());
        auto clipB = insertAudioClipIntoSlot (*slots[2], fileB->getFile());

        // After 2 beats A plays another clip on the track, and the only other one is B
        clipA->followActionDurationType = Clip::FollowActionDurationType::beats;
        clipA->followActionBeats = 2_bd;
        auto followActions = clipA->getFollowActions();
        REQUIRE (followActions);
        REQUIRE (followActions->getActions().size() == 1);
        followActions->getActions()[0]->action = FollowAction::trackOther;
        edit->getTransport().ensureContextAllocated (true);

        edit->getTransport().play (false);
        clipA->getLaunchHandle()->play ({});
        process (player, 4_td);

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (0.2, 1.8), 220.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (2.2, 3.9), 330.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (2.2, 3.9), 220.0), 0.1f);
        CHECK (clipB->getLaunchHandle()->getPlayingStatus() == LaunchHandle::PlayState::playing);
    }

    TEST_CASE ("Clip launcher: playing and queued clips carry on across graph rebuilds (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto fileA = createSineFile (engine, 16.0, 220.0f);
        auto fileB = createSineFile (engine, 16.0, 330.0f);
        auto fileC = createSineFile (engine, 16.0, 440.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        track->getClipSlotList().ensureNumberOfSlots (3);
        edit->getSceneList().ensureNumberOfScenes (3);

        // The playing clip is in the last slot, so the launcher sorts it ahead of the others
        auto slots = track->getClipSlotList().getClipSlots();
        auto clipC = insertAudioClipIntoSlot (*slots[0], fileC->getFile());
        auto clipB = insertAudioClipIntoSlot (*slots[1], fileB->getFile());
        auto clipA = insertAudioClipIntoSlot (*slots[2], fileA->getFile());
        edit->getTransport().ensureContextAllocated (true);

        edit->getTransport().play (false);
        clipA->getLaunchHandle()->play ({});
        process (player, 1.5_td);

        // B takes over from A at the next bar (4s, 60bpm 4/4)
        auto switchPos = getNextQuantisedLaunchPosition (*edit, LaunchQType::bar);
        REQUIRE (switchPos);
        CHECK (switchPos->editTime == TimePosition::fromSeconds (4.0));
        clipB->getLaunchHandle()->play (switchPos->monotonicBeat);
        clipA->getLaunchHandle()->stop (switchPos->monotonicBeat);
        process (player, 1_td);     // to 2.5s

        // Rebuild with A playing and B queued, then again with B playing
        edit->getTransport().ensureContextAllocated (true);
        process (player, 4_td);     // to 6.5s
        edit->getTransport().ensureContextAllocated (true);
        process (player, 1.5_td);   // to 8s

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (0.2, 1.4), 220.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (2.6, 3.9), 220.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (0.2, 3.9), 330.0), 0.1f);

        CHECK_GT (getToneMagnitude (output, tr (4.1, 6.4), 330.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (6.6, 7.9), 330.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (4.1, 7.9), 220.0), 0.1f);

        CHECK_LT (getToneMagnitude (output, tr (0.2, 7.9), 440.0), 0.1f);

        CHECK (clipA->getLaunchHandle()->getPlayingStatus() == LaunchHandle::PlayState::stopped);
        CHECK (clipB->getLaunchHandle()->getPlayingStatus() == LaunchHandle::PlayState::playing);
        CHECK (clipC->getLaunchHandle()->getPlayingStatus() == LaunchHandle::PlayState::stopped);
    }

    TEST_CASE ("Clip launcher: building the playback graph doesn't change the Edit")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());
        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);

        // A launcher clip without follow actions, as loaded from an older Edit
        auto clipState = insertAudioClipIntoSlot (*slot, sinFile->getFile())->state.createCopy();
        clipState.removeChild (clipState.getChildWithName (IDs::FOLLOWACTIONS), nullptr);
        slot->state.removeChild (slot->state.getChildWithName (IDs::AUDIOCLIP), nullptr);
        slot->state.appendChild (clipState, nullptr);
        auto clip = slot->getClip();
        REQUIRE (clip);
        REQUIRE (! clip->state.getChildWithName (IDs::FOLLOWACTIONS).isValid());

        edit->getUndoManager().clearUndoHistory();
        edit->resetChangedStatus();

        edit->getTransport().ensureContextAllocated (true);
        REQUIRE (edit->getTransport().getCurrentPlaybackContext());

        CHECK (! edit->getUndoManager().canUndo());
        CHECK (! edit->hasChangedSinceSaved());
    }

    TEST_CASE ("Scenes: moving a scene moves every track's slot with it")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
        auto& sceneList = edit->getSceneList();
        sceneList.ensureNumberOfScenes (3);
        auto tracks = getAudioTracks (*edit);

        const auto initialScenes = sceneList.getScenes();

        for (int i = 0; i < initialScenes.size(); ++i)
            initialScenes[i]->name = juce::String ("Scene ") + juce::String (i);

        // One track is missing its last slot, e.g. from an older Edit
        auto& shortSlots = tracks[1]->getClipSlotList();
        shortSlots.deleteSlot (*shortSlots.getClipSlots()[2]);

        auto clipIn = [&] (AudioTrack& t, int slotIndex)
        {
            return insertMIDIClip (*t.getClipSlotList().getClipSlots()[slotIndex], "Clip", { 0_tp, 1_tp });
        };

        auto clip0 = clipIn (*tracks[0], 0);
        auto clip1 = clipIn (*tracks[1], 0);

        auto checkOrder = [&] (std::initializer_list<int> sceneNumbers, int clipSlotIndex)
        {
            auto scenes = sceneList.getScenes();
            REQUIRE (scenes.size() == (int) sceneNumbers.size());

            int i = 0;

            for (auto n : sceneNumbers)
                CHECK (scenes[i++]->name.get() == juce::String ("Scene ") + juce::String (n));

            for (auto t : tracks)
                CHECK (t->getClipSlotList().getClipSlots().size() == scenes.size());

            CHECK (clip0->getClipSlot()->getIndex() == clipSlotIndex);
            CHECK (clip1->getClipSlot()->getIndex() == clipSlotIndex);
        };

        edit->getUndoManager().beginNewTransaction();
        sceneList.moveScene (0, 2);
        checkOrder ({ 1, 2, 0 }, 2);

        edit->getUndoManager().beginNewTransaction();
        sceneList.moveScene (2, 1);
        checkOrder ({ 1, 0, 2 }, 1);

        // Out of range moves do nothing
        sceneList.moveScene (0, 3);
        sceneList.moveScene (-1, 0);
        checkOrder ({ 1, 0, 2 }, 1);

        edit->getUndoManager().undo();
        checkOrder ({ 1, 2, 0 }, 2);
    }

    TEST_CASE ("Scenes: deleting a scene on a track with fewer slots than scenes")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
        auto& sceneList = edit->getSceneList();
        sceneList.ensureNumberOfScenes (3);
        auto tracks = getAudioTracks (*edit);

        // e.g. an Edit saved with a slot missing on one track
        auto& slots = tracks[1]->getClipSlotList();
        slots.deleteSlot (*slots.getClipSlots()[2]);
        REQUIRE (slots.getClipSlots().size() == 2);

        sceneList.deleteScene (*sceneList.getScenes()[2]);
        CHECK (sceneList.getNumScenes() == 2);
        CHECK (tracks[0]->getClipSlotList().getClipSlots().size() == 2);
        CHECK (slots.getClipSlots().size() == 2);
    }

    TEST_CASE ("Clip launcher: a queued launch survives the playback context being recreated (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto sinFile = createSineFile (engine, 8.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        auto& transport = edit->getTransport();
        transport.play (false);
        process (player, 5_td);

        // Queued for the next bar (8s), then the context is recreated, as minimising
        // with "stop playback when minimised" or toggling low latency monitoring does
        auto launchPos = getNextQuantisedLaunchPosition (*edit, LaunchQType::bar);
        REQUIRE (launchPos);
        launchHandle->play (launchPos->monotonicBeat);

        transport.stop (false, false);
        transport.freePlaybackContext();
        transport.ensureContextAllocated();
        transport.setPosition (5s);
        transport.play (false);
        process (player, 5_td); // edit 5 to 10, output 5 to 10

        // It starts at the bar it was queued for, not when the restarted beat
        // count catches up with the old one
        const auto output = player.getOutput();
        CHECK_LT (getRMSLevel (output, tr (5.2, 7.8)), 0.005f);
        CHECK_GT (getToneMagnitude (output, tr (8.2, 9.8), 220.0), 0.5f);
    }

    TEST_CASE ("Clip launcher: recording into a clip slot puts the recording in the slot (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        auto params = getPlayerParams();
        params.inputChannels = 1;
        test_utilities::EnginePlayer player (engine, params);

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto& tc = edit->getTransport();
        tc.ensureContextAllocated();

        // 2s of 220Hz in to the input
        juce::AudioBuffer<float> input (1, static_cast<int> (2.0 * params.sampleRate));

        for (int i = 0; i < input.getNumSamples(); ++i)
            input.setSample (0, i, 0.5f * std::sin (juce::MathConstants<float>::twoPi * 220.0f * (float) i / (float) params.sampleRate));

        auto dest = edit->getCurrentPlaybackContext()->getAllInputs()[0]->setTarget (slot->itemID, false, nullptr);
        REQUIRE (dest);
        (*dest)->recordEnabled = true;
        edit->dispatchPendingUpdatesSynchronously();

        test_utilities::TempCurrentWorkingDirectory tempDir;
        tc.record (false);
        player.process (input);
        tc.stop (false, true);

        // The recording is in the slot, not on the arranger. N.B. Launching it isn't
        // tested here: a newly recorded file can't be mapped without a message loop
        CHECK (track->getClips().isEmpty());
        auto recordedClip = dynamic_cast<WaveAudioClip*> (slot->getClip());
        REQUIRE (recordedClip);

        auto recordedFile = recordedClip->getSourceFileReference().getFile();
        auto recorded = test_utilities::loadFileInToBuffer (engine, recordedFile);
        REQUIRE (recorded);
        CHECK (recorded->getNumSamples() == input.getNumSamples());
        CHECK_GT (recorded->getRMSLevel (0, 0, recorded->getNumSamples()), 0.3f);
    }

    TEST_CASE ("Clip launcher: a launched clip follows a tempo change in the tempo map (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        // 4 beats of 220Hz then 4 of 330Hz at the Edit's 60bpm
        auto twoToneFile = createTwoToneFile (engine, 4.0, 220.0f, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        REQUIRE (clip->getAutoTempo());

        // Twice as fast from beat 2
        edit->tempoSequence.insertTempo (2_bp, 120.0, 0.0f);
        const auto toneChangeTime = edit->tempoSequence.toTime (4_bp);
        REQUIRE (toneChangeTime < 3.6_tp);
        edit->getTransport().ensureContextAllocated (true);

        edit->getTransport().play (false);
        clip->getLaunchHandle()->play ({});
        process (player, 5_td);

        // The change between the tones is still on beat 4
        const auto output = player.getOutput();
        auto change = findToneChange (output, tr (0.5, 4.5), 220.0, 330.0);
        REQUIRE (change);
        CHECK (change->inSeconds() == doctest::Approx (toneChangeTime.inSeconds()).epsilon (0.02));
    }

    TEST_CASE ("Clip launcher: a launched clip keeps playing when the tempo is changed whilst playing (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto twoToneFile = createTwoToneFile (engine, 4.0, 220.0f, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        edit->getTransport().ensureContextAllocated (true);

        edit->getTransport().play (false);
        clip->getLaunchHandle()->play ({});
        process (player, 1_td);

        edit->tempoSequence.getTempo (0)->setBpm (120.0);
        edit->getTransport().ensureContextAllocated (true);
        process (player, 4_td);

        // It carries on from beat 1 at the new tempo, rather than jumping to where
        // 1s falls in the new tempo, so the tones change 3 beats (1.5s) later
        const auto output = player.getOutput();
        CHECK (clip->getLaunchHandle()->getPlayingStatus() == LaunchHandle::PlayState::playing);
        CHECK_GT (getRMSLevel (output, tr (1.1, 4.9)), 0.1f);

        auto change = findToneChange (output, tr (0.5, 4.5), 220.0, 330.0);
        REQUIRE (change);
        CHECK (change->inSeconds() == doctest::Approx (2.5).epsilon (0.04));
    }

    TEST_CASE ("Clip launcher: rendering an Edit leaves out a track left playing its launcher (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        auto arrangerFile = createSineFile (engine, 2.0, 220.0f);
        auto slotFile = createSineFile (engine, 2.0, 330.0f);
        auto otherFile = createSineFile (engine, 2.0, 440.0f);

        auto edit = test_utilities::createTestEdit (engine, 2, Edit::EditRole::forEditing);
        auto tracks = getAudioTracks (*edit);
        tracks[0]->getClipSlotList().ensureNumberOfSlots (1);
        edit->getSceneList().ensureNumberOfScenes (1);

        insertWaveClip (*tracks[0], {}, arrangerFile->getFile(), { tr (0.0, 2.0) }, DeleteExistingClips::no)
            ->setUsesProxy (false);
        insertAudioClipIntoSlot (*tracks[0]->getClipSlotList().getClipSlots()[0], slotFile->getFile());
        insertWaveClip (*tracks[1], {}, otherFile->getFile(), { tr (0.0, 2.0) }, DeleteExistingClips::no)
            ->setUsesProxy (false);

        // The first track is left playing its launcher, as after launching a clip, which is
        // saved with the Edit. With nothing launched it plays nothing, so it renders silent
        tracks[0]->playSlotClips = true;

        juce::TemporaryFile destFile (".wav");
        Renderer::Parameters params (*edit);
        params.destFile = destFile.getFile();
        params.audioFormat = engine.getAudioFileFormatManager().getWavFormat();
        params.time = { 0_tp, 2_tp };
        params.tracksToDo = toBitSet (getAllTracks (*edit));
        params.sampleRateForAudio = 44100.0;
        params.blockSizeForAudio = 512;

        const auto file = Renderer::renderToFile ("Render", params);
        REQUIRE (file.existsAsFile());

        auto buffer = test_utilities::loadFileInToBuffer (engine, file);
        REQUIRE (buffer);
        choc::buffer::ChannelArrayBuffer<float> output (choc::buffer::Size::create (buffer->getNumChannels(), buffer->getNumSamples()));
        choc::buffer::copy (output, toBufferView (*buffer));
        CHECK_GT (getToneMagnitude (output, tr (0.1, 1.9), 440.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (0.1, 1.9), 220.0), 0.05f);
        CHECK_LT (getToneMagnitude (output, tr (0.1, 1.9), 330.0), 0.05f);

        // The render doesn't switch the track back to its arrangement
        CHECK (tracks[0]->playSlotClips.get());

        // Rendering that track's arranger clip itself (a clip render) does play it
        auto arrangerClip = tracks[0]->getClips()[0];
        REQUIRE (arrangerClip);
        params.allowedClips = { arrangerClip };
        juce::TemporaryFile clipDestFile (".wav");
        params.destFile = clipDestFile.getFile();

        const auto clipFile = Renderer::renderToFile ("Render", params);
        REQUIRE (clipFile.existsAsFile());

        auto clipBuffer = test_utilities::loadFileInToBuffer (engine, clipFile);
        REQUIRE (clipBuffer);
        choc::buffer::ChannelArrayBuffer<float> clipOutput (choc::buffer::Size::create (clipBuffer->getNumChannels(), clipBuffer->getNumSamples()));
        choc::buffer::copy (clipOutput, toBufferView (*clipBuffer));
        CHECK_GT (getToneMagnitude (clipOutput, tr (0.1, 1.9), 220.0), 0.5f);
        CHECK_LT (getToneMagnitude (clipOutput, tr (0.1, 1.9), 330.0), 0.05f);
        CHECK_LT (getToneMagnitude (clipOutput, tr (0.1, 1.9), 440.0), 0.05f);
    }

    TEST_CASE ("Clip launcher: a Return to arrangement follow action returns the track to its arrangement (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto arrangerFile = createSineFile (engine, 12.0, 220.0f);
        auto slotFile = createSineFile (engine, 8.0, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);

        insertWaveClip (*track, {}, arrangerFile->getFile(), { tr (0.0, 12.0) }, DeleteExistingClips::no)
            ->setUsesProxy (false);

        // After 2 beats of playing, return to the arrangement
        auto slotClip = insertAudioClipIntoSlot (*slot, slotFile->getFile());
        slotClip->followActionDurationType = Clip::FollowActionDurationType::beats;
        slotClip->followActionBeats = 2_bd;
        auto followActions = slotClip->getFollowActions();
        REQUIRE (followActions);
        REQUIRE (followActions->getActions().size() == 1);
        followActions->getActions()[0]->action = FollowAction::globalReturnToArrangement;
        edit->getTransport().ensureContextAllocated (true);

        edit->getTransport().play (false);
        process (player, 1_td);

        // As the app's timer would when the slot starts playing
        slotClip->getLaunchHandle()->play ({});
        track->playSlotClips = true;
        process (player, 3_td); // to 4s: the follow action runs at 3s

        // The follow action runs on the audio thread, so it's applied by the
        // timer on the message thread. Run that here, as there's no message loop
        auto& timer = static_cast<juce::Timer&> (engine.getBackToArrangerUpdateTimer());
        timer.timerCallback();
        CHECK (! track->playSlotClips.get());
        process (player, 1_td); // to 5s
        timer.timerCallback();
        CHECK (! track->playSlotClips.get());
        process (player, 2_td); // to 7s

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (1.2, 2.8), 330.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (1.2, 2.8), 220.0), 0.05f);

        CHECK_GT (getToneMagnitude (output, tr (4.2, 6.8), 220.0), 0.5f);
        CHECK_LT (getToneMagnitude (output, tr (4.2, 6.8), 330.0), 0.05f);
    }

    TEST_CASE ("Clip launcher: audible content position matches the played range (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto twoToneFile = createTwoToneFile (engine, 4.0, 220.0f, 330.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);

        process (player, 5_td);
        launchHandle->play ({});
        process (player, 6_td); // to 11s

        const auto output = player.getOutput();
        const auto endTime = TimePosition::fromSeconds (output.getNumFrames() / sampleRate);

        // Launched at 5s, so the source's tone change is heard at 9s
        auto change = findToneChange (output, tr (5.1, 10.9), 220.0, 330.0);
        REQUIRE (change);
        CHECK_LT (std::abs (change->inSeconds() - 9.0), 0.02);

        auto contentPosition = getAudibleContentPosition (output, tr (5.1, 10.9), 220.0, 330.0, 4.0, endTime);
        REQUIRE (contentPosition);

        // 60bpm: 1 beat == 1 second
        auto playedRange = launchHandle->getPlayedRange();
        REQUIRE (playedRange);
        CHECK_LT (std::abs (contentPosition->inSeconds() - playedRange->getLength().inBeats()), 0.02);
    }

    TEST_CASE ("Clip launcher: per-channel level follows the track pan (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams (2));

        auto sinFile = createSineFile (engine, 4.0, 220.0f);
        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        auto volumePlugin = track->getVolumePlugin();
        REQUIRE (volumePlugin);
        volumePlugin->setPan (1.0f);

        edit->getTransport().play (false);

        launchHandle->play ({});
        process (player, 2_td);

        const auto output = player.getOutput();
        REQUIRE (output.getNumChannels() == 2);
        CHECK_LT (getRMSLevel (output, tr (0.5, 1.9), 0), 0.005f);
        CHECK_GT (getRMSLevel (output, tr (0.5, 1.9), 1), 0.4f);
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIP_LAUNCHER
