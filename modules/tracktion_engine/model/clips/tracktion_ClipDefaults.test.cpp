/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIP_DEFAULTS

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include "../../../tracktion_graph/tracktion_graph/tracktion_TestUtilities.h"

namespace tracktion::inline engine
{

//==============================================================================
/** Tests for the defaults applied to clips.

    These pin down which defaults are applied when a clip is created, and which
    of a clip's settings survive being moved or copied.
*/
namespace clip_defaults_tests
{
    struct TestContext
    {
        TestContext (Engine& e, int numTracks, int numSlots)
            : edit (test_utilities::createTestEdit (e, numTracks))
        {
            for (auto t : getAudioTracks (*edit))
                t->getClipSlotList().ensureNumberOfSlots (numSlots);

            edit->getSceneList().ensureNumberOfScenes (numSlots);
        }

        AudioTrack& getTrack (int index)
        {
            auto t = getAudioTracks (*edit)[index];
            assert (t != nullptr);
            return *t;
        }

        ClipSlot& getSlot (int trackIndex, int slotIndex)
        {
            auto s = getTrack (trackIndex).getClipSlotList().getClipSlots()[slotIndex];
            assert (s != nullptr);
            return *s;
        }

        std::unique_ptr<Edit> edit;
    };

    /** Copies a clip in to another owner, the way a drag-copy or duplicate does. */
    inline Clip* insertCopyOfClip (Clip& source, ClipOwner& destination)
    {
        return insertClipCopy (destination, ClipCopy::fromClip (source).withNewItemID (source.edit));
    }

    inline WaveAudioClip::Ptr insertWaveClipInto (ClipOwner& owner, const juce::File& f)
    {
        return insertWaveClip (owner, f.getFileNameWithoutExtension(), f,
                               {{ 0_tp, TimeDuration::fromSeconds (4.0) }},
                               DeleteExistingClips::no);
    }

    /** Turns a launcher clip in to one with non-default settings: no looping and
        no tempo remapping, which is what the user does in
        Tracktion/waveform_beta#1032.
    */
    inline void clearLoopAndTempoSettings (AudioClipBase& c)
    {
        c.disableLooping();
        c.setAutoTempo (false);
        c.setSyncType (Clip::syncAbsolute);
    }

    /** Counts EngineBehaviour callbacks so tests can assert how often the app is
        told about a clip.
    */
    struct CountingEngineBehaviour : public EngineBehaviour
    {
        bool autoInitialiseDeviceManager() override     { return false; }

        void newClipCreated (Clip& c, bool fromRecording) override
        {
            ++numClipsCreated;
            lastWasFromRecording = fromRecording;
            juce::ignoreUnused (c);
        }

        int numClipsCreated = 0;
        bool lastWasFromRecording = false;
    };
}

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("ClipDefaults: new clips on a track")
    {
        using namespace clip_defaults_tests;

        auto& engine = *Engine::getEngines()[0];
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0, 1, 220.0f);

        TestContext c (engine, 1, 1);

        auto wave = insertWaveClipInto (c.getTrack (0), sinFile->getFile());
        REQUIRE (wave != nullptr);

        // A track clip is left alone: no looping, no forced tempo remapping and
        // the position it was given.
        CHECK (! wave->isLooping());
        CHECK (! wave->getAutoTempo());
        CHECK (wave->canUseProxy());
        CHECK (wave->getPosition().getStart() == 0_tp);
    }

    TEST_CASE ("ClipDefaults: new clips in a clip slot")
    {
        using namespace clip_defaults_tests;

        auto& engine = *Engine::getEngines()[0];
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0, 1, 220.0f);

        TestContext c (engine, 1, 2);

        SUBCASE ("Audio")
        {
            auto wave = insertWaveClipInto (c.getSlot (0, 0), sinFile->getFile());
            REQUIRE (wave != nullptr);

            // Launcher clips are made loopable and beat-based, and start at 0.
            CHECK (wave->isLooping());
            CHECK (wave->getAutoTempo());
            CHECK (! wave->canUseProxy());
            CHECK (wave->getPosition().getStart() == 0_tp);
        }

        SUBCASE ("MIDI")
        {
            auto midi = insertMIDIClip (c.getSlot (0, 1), { 0_tp, TimePosition::fromSeconds (2.0) });
            REQUIRE (midi != nullptr);

            CHECK (midi->isLooping());
            CHECK (! midi->canUseProxy());
            CHECK (midi->getPosition().getStart() == 0_tp);
        }
    }

    TEST_CASE ("ClipDefaults: moving a clip between tracks keeps its settings")
    {
        using namespace clip_defaults_tests;

        auto& engine = *Engine::getEngines()[0];
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0, 1, 220.0f);

        TestContext c (engine, 2, 1);

        auto wave = insertWaveClipInto (c.getTrack (0), sinFile->getFile());
        REQUIRE (wave != nullptr);
        wave->setAutoTempo (true);
        wave->setUsesProxy (false);

        CHECK (wave->moveTo (c.getTrack (1)));

        CHECK (wave->getAutoTempo());
        CHECK (! wave->canUseProxy());
        CHECK (wave->getTrack() == &c.getTrack (1));
    }

    TEST_CASE ("ClipDefaults: copying a launcher clip to another slot keeps its settings")
    {
        using namespace clip_defaults_tests;

        // Tracktion/waveform_beta#1032: a copy of a launcher clip whose looping
        // and tempo remapping had been turned off used to get them back, because
        // the slot branch of insertClipWithState applied the launcher defaults to
        // every insert.
        auto& engine = *Engine::getEngines()[0];
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0, 1, 220.0f);

        TestContext c (engine, 1, 2);

        auto source = insertWaveClipInto (c.getSlot (0, 0), sinFile->getFile());
        REQUIRE (source != nullptr);
        clearLoopAndTempoSettings (*source);
        REQUIRE (! source->isLooping());
        REQUIRE (! source->getAutoTempo());

        auto copy = dynamic_cast<AudioClipBase*> (insertCopyOfClip (*source, c.getSlot (0, 1)));
        REQUIRE (copy != nullptr);

        CHECK (! copy->isLooping());
        CHECK (! copy->getAutoTempo());
        CHECK (copy->getSyncType() == Clip::syncAbsolute);
    }

    TEST_CASE ("ClipDefaults: a copy keeps its own colour but is given one if it has none")
    {
        using namespace clip_defaults_tests;

        auto& engine = *Engine::getEngines()[0];
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0, 1, 220.0f);

        TestContext c (engine, 2, 1);

        auto source = insertWaveClipInto (c.getTrack (0), sinFile->getFile());
        REQUIRE (source != nullptr);

        SUBCASE ("An explicit colour survives")
        {
            source->setColour (juce::Colours::hotpink);

            auto copy = insertCopyOfClip (*source, c.getTrack (1));
            REQUIRE (copy != nullptr);
            CHECK (copy->getColour() == juce::Colours::hotpink);
        }

        SUBCASE ("A clip with no colour property gets the track's hue")
        {
            // i.e. a clip from an Edit saved before clips had a colour
            auto stateWithNoColour = source->state.createCopy();
            stateWithNoColour.removeProperty (IDs::colour, nullptr);
            EditItemID::remapIDs (stateWithNoColour, nullptr, *c.edit);

            auto copy = insertClipCopy (c.getTrack (1), ClipCopy::fromClipboardState (stateWithNoColour, false));
            REQUIRE (copy != nullptr);
            CHECK (copy->getColour() != copy->getDefaultColour());
        }
    }

    TEST_CASE ("ClipDefaults: a one-shot isn't made to loop in the launcher")
    {
        using namespace clip_defaults_tests;

        auto& engine = *Engine::getEngines()[0];
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0, 1, 220.0f);

        TestContext c (engine, 1, 1);

        auto wave = insertWaveClipInto (c.getTrack (0), sinFile->getFile());
        REQUIRE (wave != nullptr);

        // oneShot comes from the file's metadata, so set it on the LoopInfo state
        auto loopInfo = wave->getLoopInfo();
        loopInfo.state.setProperty (IDs::oneShot, true, nullptr);
        wave->setLoopInfo (loopInfo);
        REQUIRE (wave->getLoopInfo().isOneShot());

        // Copying it in to a slot prepares it for the launcher, but a single hit
        // shouldn't be looped
        auto copy = dynamic_cast<AudioClipBase*> (insertCopyOfClip (*wave, c.getSlot (0, 0)));
        REQUIRE (copy != nullptr);

        CHECK (! copy->isLooping());
        CHECK (copy->getAutoTempo());
        CHECK (copy->getPosition().getStart() == 0_tp);
    }

    TEST_CASE ("ClipDefaults: pasting a launcher clip keeps its settings")
    {
        using namespace clip_defaults_tests;

        // The Cmd+D / paste route. Clipboard::Clips records slotOffset for a clip
        // copied from a slot, so the paste knows the source was in the launcher.
        auto& engine = *Engine::getEngines()[0];
        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0, 1, 220.0f);

        TestContext c (engine, 1, 2);

        auto source = insertWaveClipInto (c.getSlot (0, 0), sinFile->getFile());
        REQUIRE (source != nullptr);
        clearLoopAndTempoSettings (*source);

        Clipboard::Clips content;
        content.addSelectedClips ({ source.get() }, Edit::getMaximumEditTimeRange(),
                                  Clipboard::Clips::AutomationLocked::no);
        REQUIRE (content.clips.size() == 1);
        CHECK (content.clips[0].slotOffset.has_value());

        EditInsertPoint insertPoint (*c.edit);
        Clipboard::ContentType::EditPastingOptions opts (*c.edit, insertPoint);
        opts.silent = true;
        opts.startTrack = &c.getTrack (0);
        opts.targetClipOwnerID = c.getSlot (0, 1).getClipOwnerID();

        CHECK (content.pasteIntoEdit (opts));

        auto pasted = dynamic_cast<AudioClipBase*> (c.getSlot (0, 1).getClip());
        REQUIRE (pasted != nullptr);

        CHECK (! pasted->isLooping());
        CHECK (! pasted->getAutoTempo());
    }

    TEST_CASE ("ClipDefaults: the app is told about clips once per creation")
    {
        using namespace clip_defaults_tests;

        // newClipCreated is called from the creation route only, so moves and
        // copies aren't reported to the app as new clips.
        // NB: this temporarily becomes Engine::instance; safe here because the
        // engine TestRunner never calls Engine::getInstance() and other tests use
        // getEngines().
        auto behaviour = std::make_unique<CountingEngineBehaviour>();
        auto behaviourPtr = behaviour.get();

        Engine engine { juce::String ("tracktion_clip_defaults_test"), nullptr, std::move (behaviour) };

        auto sinFile = graph::test_utilities::getSinFile<juce::WavAudioFormat> (44100.0, 5.0, 1, 220.0f);

        TestContext c (engine, 2, 1);

        auto wave = insertWaveClipInto (c.getTrack (0), sinFile->getFile());
        REQUIRE (wave != nullptr);
        CHECK_EQ (behaviourPtr->numClipsCreated, 1);

        // Moving a clip isn't creating one.
        behaviourPtr->numClipsCreated = 0;
        CHECK (wave->moveTo (c.getTrack (1)));
        CHECK_EQ (behaviourPtr->numClipsCreated, 0);
    }
}

} // namespace tracktion::inline engine

#endif // TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIP_DEFAULTS
