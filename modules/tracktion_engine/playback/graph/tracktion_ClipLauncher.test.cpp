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

    //==============================================================================
    // Launched clips across transport jumps (waveform_beta#1286, #1287). A looping
    // launched clip keeps its phase against the bar grid: its source position is
    // (edit beat - launch beat) mod loop length, whichever way the playhead moves.
    // Clips here are two-tone 4s loops (220Hz then 330Hz) launched at 1s, so the
    // 330Hz half plays from edit beats 3-5, 7-9... and 220Hz from 1-3, 5-7...
    //==============================================================================
    TEST_CASE ("Clip launcher: a backward jump keeps a looping launched clip in phase (audio)")
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
        process (player, 8_td); // to 9s

        // Back to the start while playing. Edit beats 0-1 are 330Hz (as -1 mod 4
        // is 3), then 220Hz from 1. Today the clip is silent until edit beat 1
        // and then restarts from its start, which happens to look the same, so
        // check the 330Hz part
        edit->getTransport().setPosition (0s);
        process (player, 3_td); // edit 0 to 3, output 9 to 12

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (9.1, 9.9), 330.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (10.1, 11.9), 220.0), 0.5f);
        CHECK (launchHandle->getPlayingStatus() == LaunchHandle::PlayState::playing);
    }

    TEST_CASE ("Clip launcher: stopping, relocating and playing keeps a looping launched clip playing (audio)")
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
        process (player, 4_td); // to 5s

        // Stop, go back to the start, play again: as Home then Play
        transport.stop (false, false);
        process (player, 1_td); // output 5 to 6
        transport.setPosition (0s);
        transport.play (false);
        process (player, 3_td); // edit 0 to 3, output 6 to 9

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (6.1, 6.9), 330.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (7.1, 8.9), 220.0), 0.5f);
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
        process (player, 2_td); // to 3s

        // Forward to edit beat 10, which is 1 beat into the 220Hz half
        edit->getTransport().setPosition (10s);
        process (player, 2_td); // edit 10 to 12, output 3 to 5

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (3.1, 3.9), 220.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (4.1, 4.9), 330.0), 0.5f);

        // The played range ends where the edit is, so a playhead drawn from it
        // shows what's heard: 3 beats into the loop
        auto playedRange = launchHandle->getPlayedRange();
        REQUIRE (playedRange);
        CHECK (playedRange->getEnd().inBeats() == doctest::Approx (12.0).epsilon (0.001));
        CHECK (std::fmod (playedRange->getLength().inBeats(), 4.0) == doctest::Approx (3.0).epsilon (0.001));
    }

    TEST_CASE ("Clip launcher: a transport jump stops a launched one-shot clip (audio)")
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

        edit->getTransport().setPosition (0s);
        process (player, 2_td); // output 3 to 5

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (1.1, 2.9), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (3.1, 4.9)), 0.005f);
        CHECK (launchHandle->getPlayingStatus() == LaunchHandle::PlayState::stopped);
    }

    TEST_CASE ("Clip launcher: an arrangement loop wrapping keeps a looping launched clip in phase (audio)")
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

        // After the wrap at output 6s: edit 0-1 is 330Hz, 1-3 is 220Hz
        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (6.1, 6.9), 330.0), 0.5f);
        CHECK_GT (getToneMagnitude (output, tr (7.1, 8.9), 220.0), 0.5f);
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

        // Forward to edit beat 9, two whole loops after the launch: the curve
        // follows the clip's loop, so it's back at the start of it. It used to
        // stop applying, as the playhead had moved out of the played range
        edit->getTransport().setPosition (9s);
        process (player, 2_td); // edit 9 to 11, output 3 to 5

        const auto output = player.getOutput();
        CHECK_GT (getToneMagnitude (output, tr (1.2, 1.8), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (2.2, 2.9)), 0.005f);
        CHECK_GT (getToneMagnitude (output, tr (3.2, 3.8), 220.0), 0.5f);
        CHECK_LT (getRMSLevel (output, tr (4.2, 4.9)), 0.005f);
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
