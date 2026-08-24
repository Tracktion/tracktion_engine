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

    inline HostedAudioDeviceInterface::Parameters getPlayerParams()
    {
        return { .sampleRate = sampleRate, .blockSize = blockSize,
                 .inputChannels = 0, .outputChannels = 1,
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

    inline float getRMSLevel (const choc::buffer::ChannelArrayBuffer<float>& output, TimeRange range)
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
            const double s = output.getSample (0, (choc::buffer::FrameCount) i);
            sum += s * s;
        }

        return (float) std::sqrt (sum / (double) numSamples);
    }

    //==============================================================================
    /** Creates a mono wav file where the first half is a sine at freq1 and the
        second half a sine at freq2, so tests can detect which part of the source
        is being played.
    */
    inline std::unique_ptr<juce::TemporaryFile> createTwoToneFile (double durationOfEachToneSeconds,
                                                                   float freq1, float freq2)
    {
        const auto numFramesPerTone = (choc::buffer::FrameCount) (sampleRate * durationOfEachToneSeconds);
        auto buffer = choc::buffer::createChannelArrayBuffer (1, (int) (numFramesPerTone * 2),
                                                              [=] (auto, auto frame)
                                                              {
                                                                  const auto freq = frame < numFramesPerTone ? freq1 : freq2;
                                                                  const auto localFrame = frame < numFramesPerTone ? frame : frame - numFramesPerTone;
                                                                  return (float) std::sin (juce::MathConstants<double>::twoPi * freq * localFrame / sampleRate);
                                                              });

        return graph::test_utilities::writeToTemporaryFile<juce::WavAudioFormat> (buffer.getView(), sampleRate, 0);
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
        std::unique_ptr<Edit> edit;
        std::vector<std::unique_ptr<juce::TemporaryFile>> files;
        std::vector<AudioFile> audioFilesToMap;
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
                auto file = graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, 4.0, 1, (float) audioFrequencies[scene][t]);
                auto slot = tracks[t]->getClipSlotList().getClipSlots()[scene];
                auto clip = insertAudioClipIntoSlot (*slot, file->getFile());

                sceneFrequencies.push_back (audioFrequencies[scene][t]);
                sceneHandles.push_back (clip->getLaunchHandle());
                ctx.audioFilesToMap.emplace_back (engine, file->getFile());
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

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, 8.0, 1, 220.0f);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        test_utilities::waitForFileToBeMapped (AudioFile (engine, sinFile->getFile()));

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

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto twoToneFile = createTwoToneFile (4.0, 220.0f, 330.0f);
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        test_utilities::waitForFileToBeMapped (AudioFile (engine, twoToneFile->getFile()));

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

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, 8.0, 1, 220.0f);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        test_utilities::waitForFileToBeMapped (AudioFile (engine, sinFile->getFile()));

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

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto twoToneFile = createTwoToneFile (4.0, 220.0f, 330.0f);

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
        test_utilities::waitForFileToBeMapped (AudioFile (engine, twoToneFile->getFile()));

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

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto twoToneFile = createTwoToneFile (4.0, 220.0f, 330.0f);

        // Looping over the whole file (the slot clip default), starting 4s in
        // (the 330Hz section)
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile(), TimeDuration::fromSeconds (4.0));
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        test_utilities::waitForFileToBeMapped (AudioFile (engine, twoToneFile->getFile()));

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

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, 2.0, 1, 220.0f);
        auto clip = insertAudioClipIntoSlot (*slot, sinFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        test_utilities::waitForFileToBeMapped (AudioFile (engine, sinFile->getFile()));

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

        auto [edit, track, slot] = createEditWithClipSlot (engine);
        auto twoToneFile = createTwoToneFile (4.0, 220.0f, 330.0f);
        auto clip = insertAudioClipIntoSlot (*slot, twoToneFile->getFile());
        auto launchHandle = clip->getLaunchHandle();
        REQUIRE (launchHandle);

        launchHandle->play ({});
        edit->getTransport().play (false);
        test_utilities::waitForFileToBeMapped (AudioFile (engine, twoToneFile->getFile()));

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

    TEST_CASE ("Clip launcher: switching between arranger and launcher (audio)")
    {
        auto& engine = *Engine::getEngines()[0];
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto [edit, track, slot] = createEditWithClipSlot (engine);

        auto arrangerFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, 12.0, 1, 220.0f);
        insertWaveClip (*track, {}, arrangerFile->getFile(), { tr (0.0, 12.0) }, DeleteExistingClips::no)
            ->setUsesProxy (false);

        auto slotFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (sampleRate, 8.0, 1, 330.0f);
        auto slotClip = insertAudioClipIntoSlot (*slot, slotFile->getFile());
        auto launchHandle = slotClip->getLaunchHandle();
        REQUIRE (launchHandle);

        edit->getTransport().play (false);
        test_utilities::waitForFileToBeMapped (AudioFile (engine, arrangerFile->getFile()));
        test_utilities::waitForFileToBeMapped (AudioFile (engine, slotFile->getFile()));

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

        for (auto& af : ctx.audioFilesToMap)
            test_utilities::waitForFileToBeMapped (af);

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

        for (auto& af : ctx.audioFilesToMap)
            test_utilities::waitForFileToBeMapped (af);

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

        for (auto& af : ctx.audioFilesToMap)
            test_utilities::waitForFileToBeMapped (af);

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
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIP_LAUNCHER
