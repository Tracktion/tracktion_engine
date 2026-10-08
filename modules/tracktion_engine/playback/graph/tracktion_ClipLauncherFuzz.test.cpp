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
// Randomised clip launcher tests. Each seed builds an Edit of audio and MIDI
// clips in slots and a random schedule of launches, stops and waits, plays it
// through an EnginePlayer and checks what each track's plugins get against a
// model of what the LaunchHandle calls made should play (see getPlayIntervals):
//  - MIDI tracks: every note-on and note-off, worked out from the clips' notes,
//    and that a note is never struck while it's already on or left on
//  - audio tracks: every frame. An audio clip is one cycle of a sine and cosine,
//    so its phase says where in the clip it is, at a level saying which clip it
//    is, so each frame shows which clip is playing from where
//
// Each schedule is also played with graph rebuilds between its steps and with
// an arrangement loop, neither of which should change what launched clips play.
//
// Set TE_CLIP_LAUNCHER_FUZZ_SEEDS to run more seeds, TE_CLIP_LAUNCHER_FUZZ_SEED
// to run one, and TE_CLIP_LAUNCHER_FUZZ_DUMP to print the MIDI events.
//==============================================================================
namespace clip_launcher_fuzz
{
    using namespace clip_launcher_test_utilities;

    //==============================================================================
    /** A test-only plugin that records the first two channels of audio a track
        sends it, so each track's launched clips can be checked on their own.
    */
    class AudioProbePlugin  : public Plugin
    {
    public:
        AudioProbePlugin (PluginCreationInfo info)  : Plugin (info) {}
        ~AudioProbePlugin() override                            { notifyListenersOfDeletion(); }

        static const char* getPluginName()                      { return "Audio Probe"; }
        static constexpr const char* xmlTypeName = "audioProbe";

        juce::String getName() const override                   { return getPluginName(); }
        juce::String getPluginType() override                   { return xmlTypeName; }
        juce::String getSelectableDescription() override        { return getName(); }
        BusLayout getBusses() const override                    { return BusLayout::singleStereoInOut(); }

        void initialise (const PluginInitialisationInfo&) override {}
        void deinitialise() override {}
        void restorePluginStateFromValueTree (const juce::ValueTree&) override {}

        void applyToBuffer (const PluginRenderContext& fc) override
        {
            for (int chan = 0; chan < 2; ++chan)
                for (int i = 0; i < fc.bufferNumSamples; ++i)
                    samples[(size_t) chan].push_back (fc.destBuffer != nullptr && chan < fc.destBuffer->getNumChannels()
                                                        ? fc.destBuffer->getSample (chan, fc.bufferStartSample + i) : 0.0f);
        }

        std::array<std::vector<float>, 2> samples;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioProbePlugin)
    };

    /** Adds an AudioProbePlugin to the start of a track's plugins. */
    inline AudioProbePlugin& addAudioProbePlugin (AudioTrack& track)
    {
        track.edit.engine.getPluginManager().createBuiltInType<AudioProbePlugin>();
        auto probe = dynamic_cast<AudioProbePlugin*> (track.edit.getPluginCache().createNewPlugin (AudioProbePlugin::xmlTypeName, {}).get());
        assert (probe != nullptr);
        track.pluginList.insertPlugin (*probe, 0, nullptr);
        return *probe;
    }

    /** The level of an audio clip, which says which scene it's in. */
    inline float getAudioClipLevel (int scene)
    {
        return 0.2f + 0.15f * (float) scene;
    }

    //==============================================================================
    enum class StepType { wait, launchScene, launchClip, launchLegato, stopTrack, stopAll, rebuild, changeClip };

    struct Step
    {
        StepType type = StepType::wait;
        int track = 0, scene = 0;
        LaunchQType quantisation = LaunchQType::none;
        double seconds = 0.0;
    };

    struct NoteSpec
    {
        int number = 60;
        double start = 0.0, length = 1.0;
    };

    struct ClipSpec
    {
        double lengthBeats = 4.0;
        bool looping = true;
        std::vector<NoteSpec> notes;    // MIDI clips only
    };

    struct Scenario
    {
        static constexpr int numAudioTracks = 2, numMidiTracks = 3;
        static constexpr int numTracks = numAudioTracks + numMidiTracks, numScenes = 4;

        static bool isAudioTrack (int t)    { return t < numAudioTracks; }

        std::vector<std::vector<std::optional<ClipSpec>>> clips;    // [track][scene]
        std::vector<Step> steps;
    };

    struct Variant
    {
        bool includeRebuilds = true;
        bool arrangementLoop = false;
    };

    using Events = std::vector<MidiProbePlugin::Event>;

    /** A LaunchHandle call made by a run, in output seconds. At the test Edit's
        60bpm a beat is a second, so launch positions are too.
    */
    struct Command
    {
        double time = 0.0;
        int track = 0, scene = 0;
        bool play = false;
        std::optional<double> position;
        std::optional<int> syncedFromScene; // A legato launch's, the clip it takes over the phase of
    };

    struct Result
    {
        std::vector<Events> events;                             // [track], empty for audio tracks
        std::vector<std::array<std::vector<float>, 2>> audio;   // [track], empty for MIDI tracks
        std::vector<Command> commands;
        std::vector<double> rebuildsOfChangedClips;             // A changed clip's wave node fades from the old one's
        double arrangementLoopLength = 0.0;                     // Starting at 0, 0 for no loop
        double endTime = 0.0;
    };

    //==============================================================================
    inline juce::String describe (const Step& s)
    {
        auto q = juce::String (getLaunchQTypeChoices()[(int) s.quantisation]);
        auto clip = "t" + juce::String (s.track) + " s" + juce::String (s.scene);

        switch (s.type)
        {
            case StepType::wait:        return "wait " + juce::String (s.seconds, 3) + "s";
            case StepType::launchScene: return "launch scene " + juce::String (s.scene) + " (" + q + ")";
            case StepType::launchClip:  return "launch clip " + clip + " (" + q + ")";
            case StepType::launchLegato: return "launch a clip legato from t" + juce::String (s.track) + " s" + juce::String (s.scene) + " (" + q + ")";
            case StepType::stopTrack:   return "stop track " + juce::String (s.track) + " (" + q + ")";
            case StepType::stopAll:     return "stop all";
            case StepType::rebuild:     return "rebuild graph";
            case StepType::changeClip:  return "change clip " + clip + " resampling quality";
        }

        return {};
    }

    inline juce::String describe (const Scenario& sc)
    {
        juce::String s;

        for (int t = 0; t < sc.numTracks; ++t)
        {
            for (int sl = 0; sl < sc.numScenes; ++sl)
            {
                if (auto& c = sc.clips[(size_t) t][(size_t) sl])
                {
                    s << "clip t" << t << " s" << sl << ": " << (sc.isAudioTrack (t) ? "audio, " : "MIDI, ")
                      << c->lengthBeats << " beats" << (c->looping ? " looping" : " one-shot");

                    if (! sc.isAudioTrack (t))
                        s << ", " << (int) c->notes.size() << " notes";

                    s << "\n";
                }
            }
        }

        double time = 0.5;

        for (auto& step : sc.steps)
        {
            s << juce::String (time, 4) << "s: " << describe (step) << "\n";

            if (step.type == StepType::wait)
                time += step.seconds;
        }

        return s;
    }

    inline juce::String describe (const Events& events)
    {
        juce::String s;

        for (auto& e : events)
            s << "  " << (e.isNoteOn ? "on  " : "off ") << e.noteNumber << " at " << juce::String (e.outputTime, 5) << "s\n";

        return s;
    }

    //==============================================================================
    inline Scenario createScenario (int seed)
    {
        juce::Random r (seed);
        Scenario sc;
        sc.clips.resize ((size_t) sc.numTracks);

        for (int t = 0; t < sc.numTracks; ++t)
        {
            for (int scene = 0; scene < sc.numScenes; ++scene)
            {
                if (r.nextInt (5) == 0)
                {
                    sc.clips[(size_t) t].push_back (std::nullopt);
                    continue;
                }

                ClipSpec c;
                c.looping = r.nextInt (4) != 0;

                if (sc.isAudioTrack (t))
                {
                    static constexpr double lengths[] = { 2.0, 4.0, 5.0, 8.0 };
                    c.lengthBeats = lengths[r.nextInt (4)];
                    sc.clips[(size_t) t].push_back (std::move (c));
                    continue;
                }

                static constexpr double lengths[] = { 2.0, 4.0, 5.0, 8.0, 16.0 };
                c.lengthBeats = lengths[r.nextInt (5)];

                // Each scene uses its own notes, so a track's events can be told apart by
                // clip. A note number is used once per clip, so the source never overlaps itself
                std::vector<int> noteNumbers;

                for (int n = 0; n < 12; ++n)
                    noteNumbers.insert (noteNumbers.begin() + r.nextInt (n + 1), 36 + scene * 12 + n);

                auto nextNote = [&] { auto n = noteNumbers.back(); noteNumbers.pop_back(); return n; };

                switch (r.nextInt (3))
                {
                    case 0:     // A pad, one note for the whole clip
                        c.notes.push_back ({ nextNote(), 0.0, c.lengthBeats });
                        break;

                    case 1:     // Chords, some held across bar lines and past the clip end
                        for (int i = 0; i < 1 + r.nextInt (4); ++i)
                        {
                            const auto start = r.nextInt ((int) (c.lengthBeats * 4)) / 4.0;
                            const auto length = 0.25 + r.nextInt ((int) (c.lengthBeats * 4)) / 4.0;

                            for (int n = 0; n < 3; ++n)
                                c.notes.push_back ({ nextNote(), start, length });
                        }
                        break;

                    default:    // Short notes
                        for (int i = 0; i < 1 + r.nextInt (8); ++i)
                            c.notes.push_back ({ nextNote(), r.nextInt ((int) (c.lengthBeats * 4)) / 4.0, 0.25 });
                        break;
                }

                sc.clips[(size_t) t].push_back (std::move (c));
            }
        }

        static constexpr LaunchQType quantisations[] = { LaunchQType::bar, LaunchQType::bar, LaunchQType::quarter,
                                                         LaunchQType::none, LaunchQType::half };
        auto randomQ = [&] { return quantisations[r.nextInt (5)]; };

        for (int i = 0; i < 40; ++i)
        {
            Step s;
            const auto x = r.nextInt (100);

            if (x < 35)         s = { StepType::wait, 0, 0, LaunchQType::none, 0.05 + r.nextDouble() * 6.0 };
            else if (x < 51)    s = { StepType::launchScene, 0, r.nextInt (sc.numScenes), randomQ(), 0.0 };
            else if (x < 56)    s = { StepType::launchClip, r.nextInt (sc.numTracks), r.nextInt (sc.numScenes), randomQ(), 0.0 };
            else if (x < 67)    s = { StepType::launchLegato, r.nextInt (sc.numTracks), r.nextInt (sc.numScenes), randomQ(), 0.0 };
            else if (x < 74)    s = { StepType::stopTrack, r.nextInt (sc.numTracks), 0, randomQ(), 0.0 };
            else if (x < 78)    s = { StepType::stopAll, 0, 0, LaunchQType::none, 0.0 };
            else if (x < 82)    s = { StepType::changeClip, r.nextInt (sc.numAudioTracks), r.nextInt (sc.numScenes), LaunchQType::none, 0.0 };
            else                s = { StepType::rebuild, 0, 0, LaunchQType::none, 0.0 };

            sc.steps.push_back (s);
        }

        return sc;
    }

    //==============================================================================
    inline Result run (Engine& engine, const Scenario& sc, Variant variant)
    {
        test_utilities::EnginePlayer player (engine, getPlayerParams());
        std::vector<std::unique_ptr<MemoryAudioFile>> files;    // N.B. declared before the Edit to outlive it

        auto edit = test_utilities::createTestEdit (engine, sc.numTracks, Edit::EditRole::forEditing);
        edit->getSceneList().ensureNumberOfScenes (sc.numScenes);
        auto tracks = getAudioTracks (*edit);
        std::vector<MidiProbePlugin*> midiProbes;
        std::vector<AudioProbePlugin*> audioProbes;
        std::vector<std::vector<Clip::Ptr>> clips;      // [track][scene], null for an empty slot

        for (int t = 0; t < sc.numTracks; ++t)
        {
            auto track = tracks[t];
            track->getClipSlotList().ensureNumberOfSlots (sc.numScenes);
            midiProbes.push_back (sc.isAudioTrack (t) ? nullptr : &addMidiProbePlugin (*track));
            audioProbes.push_back (sc.isAudioTrack (t) ? &addAudioProbePlugin (*track) : nullptr);
            auto& trackClips = clips.emplace_back();

            for (int scene = 0; scene < sc.numScenes; ++scene)
            {
                auto& spec = sc.clips[(size_t) t][(size_t) scene];
                auto slot = track->getClipSlotList().getClipSlots()[scene];

                if (! spec)
                {
                    trackClips.push_back (nullptr);
                    continue;
                }

                if (sc.isAudioTrack (t))
                {
                    const auto numFrames = (choc::buffer::FrameCount) (spec->lengthBeats * sampleRate);
                    const auto level = getAudioClipLevel (scene);
                    auto source = choc::buffer::createChannelArrayBuffer (2, numFrames, [&] (auto chan, auto frame)
                                                                          {
                                                                              const auto phase = juce::MathConstants<double>::twoPi * frame / numFrames;
                                                                              return level * (float) (chan == 0 ? std::sin (phase) : std::cos (phase));
                                                                          });
                    files.push_back (std::make_unique<MemoryAudioFile> (engine, source));
                    auto clip = insertAudioClipIntoSlot (*slot, files.back()->getFile());

                    // The default time-stretcher depends on the build (see compareAudioWithModel)
                   #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
                    clip->setTimeStretchMode (TimeStretcher::signalsmithDefault);
                   #endif

                    if (! spec->looping)
                        clip->disableLooping();

                    trackClips.push_back (clip);
                    continue;
                }

                auto clip = insertMidiClipIntoSlot (*slot, BeatDuration::fromBeats (spec->lengthBeats));

                if (! spec->looping)
                    clip->disableLooping();

                for (auto& n : spec->notes)
                    clip->getSequence().addNote (n.number, BeatPosition::fromBeats (n.start), BeatDuration::fromBeats (n.length),
                                                 100, 0, nullptr);

                trackClips.push_back (clip);
            }
        }

        auto& transport = edit->getTransport();

        if (variant.arrangementLoop)
        {
            // 5 bars, like the session in Tracktion/waveform_beta#1286. Bar aligned, so
            // quantised launch positions are the same with and without it
            transport.setLoopRange (tr (0.0, 20.0));
            transport.looping = true;
        }

        Result result;
        result.arrangementLoopLength = variant.arrangementLoop ? 20.0 : 0.0;
        int64_t numSamplesProcessed = 0;
        bool clipChangedSinceRebuild = false;

        auto processFor = [&] (TimeDuration d)
        {
            process (player, d);
            numSamplesProcessed += toSamples (d, sampleRate);
        };

        auto now = [&] { return (double) numSamplesProcessed / sampleRate; };

        transport.play (false);
        processFor (0.5_td);

        auto getPosition = [&] (LaunchQType q) -> std::optional<MonotonicBeat>
        {
            if (q == LaunchQType::none)
                return std::nullopt;

            if (auto pos = getNextQuantisedLaunchPosition (*edit, q))
                return pos->monotonicBeat;

            return std::nullopt;
        };

        auto getSyncPoint = [&] { return *transport.getCurrentPlaybackContext()->getSyncPoint(); };

        auto toOutputTime = [&] (MonotonicBeat b)
        {
            return now() + (b.v - getSyncPoint().monotonicBeat.v).inBeats();
        };

        // Calls play or stop on a clip's handle, logging it for the model
        auto call = [&] (int t, int scene, bool play, std::optional<MonotonicBeat> pos)
        {
            auto& clip = clips[(size_t) t][(size_t) scene];

            if (! clip)
                return;

            result.commands.push_back ({ now(), t, scene, play, pos ? std::optional (toOutputTime (*pos)) : std::nullopt, std::nullopt });

            if (play)
                clip->getLaunchHandle()->play (pos);
            else
                clip->getLaunchHandle()->stop (pos);
        };

        // Launches a track's clip and stops its others, as the launcher does
        auto launchOnTrack = [&] (int t, int scene, std::optional<MonotonicBeat> pos)
        {
            for (int s = 0; s < sc.numScenes; ++s)
                call (t, s, s == scene, pos);
        };

        for (auto& step : sc.steps)
        {
            switch (step.type)
            {
                case StepType::wait:
                    processFor (TimeDuration::fromSeconds (step.seconds));
                    break;

                case StepType::launchScene:
                {
                    const auto pos = getPosition (step.quantisation);

                    for (int t = 0; t < sc.numTracks; ++t)
                        launchOnTrack (t, step.scene, pos);

                    break;
                }

                case StepType::launchClip:
                    if (clips[(size_t) step.track][(size_t) step.scene])
                        launchOnTrack (step.track, step.scene, getPosition (step.quantisation));

                    break;

                case StepType::launchLegato:
                {
                    // Legato only matters with another clip playing on the track, so this uses the
                    // first track from the step's that has one, and a clip on it that isn't playing
                    auto isPlaying = [&] (int t, int s)
                    {
                        auto& c = clips[(size_t) t][(size_t) s];
                        return c && c->getLaunchHandle()->getPlayingStatus() == LaunchHandle::PlayState::playing;
                    };

                    std::optional<std::pair<int, int>> target;

                    for (int i = 0; i < sc.numTracks && ! target; ++i)
                    {
                        const auto t = (step.track + i) % sc.numTracks;
                        bool anyPlaying = false;

                        for (int s = 0; s < sc.numScenes; ++s)
                            anyPlaying = anyPlaying || isPlaying (t, s);

                        for (int j = 0; j < sc.numScenes && anyPlaying && ! target; ++j)
                            if (const auto s = (step.scene + j) % sc.numScenes; clips[(size_t) t][(size_t) s] && ! isPlaying (t, s))
                                target = std::pair (t, s);
                    }

                    if (! target)
                        break;

                    // As ClipLauncherBehaviour's launchClip does for a clip in legato mode: it takes
                    // over the phase of the track's playing clip from its launch position
                    const auto [track, scene] = *target;
                    const auto pos = getPosition (step.quantisation);
                    std::shared_ptr<LaunchHandle> handleToSyncFrom;
                    int sceneToSyncFrom = 0;

                    for (int s = 0; s < sc.numScenes; ++s)
                    {
                        if (s == scene || ! clips[(size_t) track][(size_t) s])
                            continue;

                        if (auto other = clips[(size_t) track][(size_t) s]->getLaunchHandle(); other->getPlayedMonotonicRange())
                        {
                            handleToSyncFrom = std::make_shared<LaunchHandle> (*other);
                            sceneToSyncFrom = s;
                        }

                        call (track, s, false, pos);
                    }

                    const auto launchPos = pos.value_or (getSyncPoint().monotonicBeat);
                    result.commands.push_back ({ now(), track, scene, true, toOutputTime (launchPos), sceneToSyncFrom });
                    clips[(size_t) track][(size_t) scene]->getLaunchHandle()->playSynced (*handleToSyncFrom, launchPos);
                    break;
                }

                case StepType::stopTrack:
                {
                    const auto pos = getPosition (step.quantisation);

                    for (int s = 0; s < sc.numScenes; ++s)
                        call (step.track, s, false, pos);

                    break;
                }

                case StepType::stopAll:
                    for (int t = 0; t < sc.numTracks; ++t)
                        for (int s = 0; s < sc.numScenes; ++s)
                            call (t, s, false, {});

                    break;

                case StepType::rebuild:
                    if (variant.includeRebuilds)
                    {
                        transport.ensureContextAllocated (true);

                        if (std::exchange (clipChangedSinceRebuild, false))
                            result.rebuildsOfChangedClips.push_back (now());
                    }

                    break;

                case StepType::changeClip:
                    // Changes the clip's wave node, but not what it plays
                    if (auto clip = dynamic_cast<AudioClipBase*> (clips[(size_t) step.track][(size_t) step.scene].get()))
                    {
                        clip->setResamplingQuality (clip->getResamplingQuality() == ResamplingQuality::lagrange
                                                        ? ResamplingQuality::sincBest : ResamplingQuality::lagrange);
                        clipChangedSinceRebuild = true;
                    }

                    break;
            }
        }

        // Stop everything, then the transport
        for (int t = 0; t < sc.numTracks; ++t)
            for (int s = 0; s < sc.numScenes; ++s)
                call (t, s, false, {});

        processFor (1_td);
        result.endTime = now();
        transport.stop (false, false);
        processFor (1_td);

        for (int t = 0; t < sc.numTracks; ++t)
        {
            result.events.push_back (midiProbes[(size_t) t] != nullptr ? midiProbes[(size_t) t]->events : Events());
            result.audio.push_back (audioProbes[(size_t) t] != nullptr ? audioProbes[(size_t) t]->samples
                                                                       : std::array<std::vector<float>, 2>());
        }

        return result;
    }

    //==============================================================================
    /** Checks no note is struck whilst it's already on and nothing is left on.
        Events are checked in the order the plugin got them.
    */
    inline std::vector<juce::String> checkNoteBalance (const Result& result)
    {
        std::vector<juce::String> failures;

        for (size_t t = 0; t < result.events.size(); ++t)
        {
            std::map<int, bool> noteIsOn;

            for (auto& e : result.events[t])
            {
                if (e.isNoteOn && noteIsOn[e.noteNumber])
                    failures.push_back ("track " + juce::String ((int) t) + ": note " + juce::String (e.noteNumber)
                                        + " struck while on at " + juce::String (e.outputTime, 4) + "s");

                noteIsOn[e.noteNumber] = e.isNoteOn;
            }

            for (auto [note, isOn] : noteIsOn)
                if (isOn)
                    failures.push_back ("track " + juce::String ((int) t) + ": note " + juce::String (note) + " left on");
        }

        return failures;
    }

    //==============================================================================
    /** When a clip plays, and where its content starts, which is earlier than when it
        plays if it's taken over the phase of another clip with a legato launch.
    */
    struct Interval
    {
        double start = 0.0, end = 0.0, contentStart = 0.0;
    };

    using Intervals = std::vector<Interval>;

    /** A model of when each clip should play, from the LaunchHandle calls a run made,
        in output times. It follows the LaunchHandle rules: one queued play or stop
        per handle (a later call replaces it), a stop when stopped cancels a queued
        play, a one-shot stops at its end unless a play is queued, a synced play
        carries on from the other clip's start, and launched clips run on the
        monotonic clock so the arrangement loop and graph rebuilds don't matter.
        A one-shot's content ends at its length.
    */
    inline std::vector<std::vector<Intervals>> getPlayIntervals (const Scenario& sc, const Result& result)
    {
        struct Handle
        {
            const ClipSpec* spec = nullptr;
            bool playing = false, endHandled = false;
            double start = 0.0, contentStart = 0.0;
            struct Queued { bool play; double time; std::optional<double> syncedStart; };
            std::optional<Queued> queued;
            Intervals intervals;

            void startPlaying (double t, double contentStartToUse)
            {
                stopPlaying (t);
                playing = true;
                endHandled = false;
                start = t;
                contentStart = contentStartToUse;
            }

            void stopPlaying (double t)
            {
                if (playing && t > start)
                    intervals.push_back ({ start, t, contentStart });

                playing = false;
            }
        };

        std::vector<std::vector<Handle>> handles ((size_t) sc.numTracks, std::vector<Handle> ((size_t) sc.numScenes));

        for (size_t t = 0; t < handles.size(); ++t)
            for (size_t s = 0; s < handles[t].size(); ++s)
                if (auto& spec = sc.clips[t][s])
                    handles[t][s].spec = &*spec;

        // Applies everything due before t, earliest first
        auto advanceTo = [&] (double t)
        {
            for (;;)
            {
                Handle* next = nullptr;
                double nextTime = t;
                bool isEndOfPlay = false;

                for (auto& trackHandles : handles)
                {
                    for (auto& h : trackHandles)
                    {
                        if (h.queued && h.queued->time < nextTime)
                        {
                            next = &h;
                            nextTime = h.queued->time;
                            isEndOfPlay = false;
                        }

                        if (h.playing && ! h.spec->looping && ! h.endHandled)
                        {
                            if (const auto end = h.contentStart + h.spec->lengthBeats; end < nextTime)
                            {
                                next = &h;
                                nextTime = end;
                                isEndOfPlay = true;
                            }
                        }
                    }
                }

                if (next == nullptr)
                    return;

                if (isEndOfPlay)
                {
                    next->endHandled = true;

                    // A queued play carries on to it, otherwise the one-shot stops here
                    if (next->queued && next->queued->play)
                        continue;

                    next->queued.reset();
                    next->stopPlaying (nextTime);
                    continue;
                }

                const auto queued = *next->queued;
                next->queued.reset();

                if (queued.play)
                    next->startPlaying (queued.time, queued.syncedStart.value_or (queued.time));
                else
                    next->stopPlaying (queued.time);
            }
        };

        for (auto& c : result.commands)
        {
            advanceTo (c.time);
            auto& h = handles[(size_t) c.track][(size_t) c.scene];

            if (h.spec == nullptr)
                continue;

            const auto time = c.position ? std::max (*c.position, c.time) : c.time;

            if (c.play)
            {
                std::optional<double> syncedStart;

                if (c.syncedFromScene)
                    if (auto& other = handles[(size_t) c.track][(size_t) *c.syncedFromScene]; other.playing)
                        syncedStart = other.contentStart;

                h.queued = Handle::Queued { true, time, syncedStart };
            }
            else if (! h.playing)
            {
                if (h.queued && h.queued->play)
                    h.queued.reset();
            }
            else
            {
                h.queued = Handle::Queued { false, time, std::nullopt };
            }
        }

        advanceTo (result.endTime);

        std::vector<std::vector<Intervals>> intervals ((size_t) sc.numTracks, std::vector<Intervals> ((size_t) sc.numScenes));

        for (size_t t = 0; t < handles.size(); ++t)
        {
            for (size_t s = 0; s < handles[t].size(); ++s)
            {
                auto& h = handles[t][s];

                if (h.spec == nullptr)
                    continue;

                h.stopPlaying (result.endTime);

                for (auto i : h.intervals)
                    intervals[t][s].push_back ({ i.start, h.spec->looping ? i.end : std::min (i.end, i.contentStart + h.spec->lengthBeats),
                                                 i.contentStart });
            }
        }

        return intervals;
    }

    /** Returns the note-ons and note-offs the model says each MIDI track should get. */
    inline std::vector<Events> getExpectedEvents (const Scenario& sc, const Result& result)
    {
        const auto intervals = getPlayIntervals (sc, result);
        std::vector<Events> expected ((size_t) sc.numTracks);

        for (int t = 0; t < sc.numTracks; ++t)
        {
            if (sc.isAudioTrack (t))
                continue;

            for (int s = 0; s < sc.numScenes; ++s)
            {
                auto& spec = sc.clips[(size_t) t][(size_t) s];

                if (! spec)
                    continue;

                const auto length = spec->lengthBeats;

                for (auto i : intervals[(size_t) t][(size_t) s])
                {
                    for (auto loopStart = i.contentStart; loopStart < i.end; loopStart += length)
                    {
                        for (auto& n : spec->notes)
                        {
                            // A note already on where a legato launch starts is struck just after it
                            // (see MidiNodeHelpers::createMessagesForTime)
                            const auto on = loopStart + n.start < i.start ? i.start + 0.0001 : loopStart + n.start;
                            const auto off = std::min ({ loopStart + n.start + n.length, loopStart + length, i.end });

                            if (on >= i.end || off - on < 1.0e-4)
                                continue;

                            expected[(size_t) t].push_back ({ true, n.number, on });
                            expected[(size_t) t].push_back ({ false, n.number, off });
                        }
                    }
                }
            }
        }

        return expected;
    }

    /** Compares the MIDI tracks' events with the model's, note by note. */
    inline std::vector<juce::String> compareMidiWithModel (const Scenario& sc, const Result& result, double toleranceSeconds)
    {
        const auto expected = getExpectedEvents (sc, result);
        std::vector<juce::String> differences;

        if (juce::SystemStats::getEnvironmentVariable ("TE_CLIP_LAUNCHER_FUZZ_DUMP", {}).isNotEmpty())
            for (size_t t = 0; t < expected.size(); ++t)
                if (! sc.isAudioTrack ((int) t))
                    std::cout << "track " << t << ": " << expected[t].size() << " expected, " << result.events[t].size() << " played\n"
                              << "expected:\n" << describe (expected[t]);

        auto byNote = [] (const Events& events)
        {
            std::map<int, Events> notes;

            for (auto& e : events)
                notes[e.noteNumber].push_back (e);

            // Note-offs before note-ons at the same time, as plugins get them
            for (auto& [n, list] : notes)
                std::stable_sort (list.begin(), list.end(), [] (auto& a, auto& b)
                                  {
                                      if (std::abs (a.outputTime - b.outputTime) < 1.0e-9)
                                          return ! a.isNoteOn && b.isNoteOn;

                                      return a.outputTime < b.outputTime;
                                  });

            return notes;
        };

        for (size_t t = 0; t < expected.size(); ++t)
        {
            auto expectedNotes = byNote (expected[t]);
            auto actualNotes = byNote (result.events[t]);
            std::set<int> noteNumbers;

            for (auto& [n, list] : expectedNotes)  noteNumbers.insert (n);
            for (auto& [n, list] : actualNotes)    noteNumbers.insert (n);

            for (auto n : noteNumbers)
            {
                auto& e = expectedNotes[n];
                auto& a = actualNotes[n];

                for (size_t i = 0; i < std::max (e.size(), a.size()); ++i)
                {
                    auto describeEvent = [] (const Events& events, size_t index) -> juce::String
                    {
                        if (index >= events.size())
                            return "nothing";

                        return juce::String (events[index].isNoteOn ? "on" : "off") + " at "
                                + juce::String (events[index].outputTime, 4) + "s";
                    };

                    if (i < e.size() && i < a.size()
                        && e[i].isNoteOn == a[i].isNoteOn
                        && std::abs (e[i].outputTime - a[i].outputTime) <= toleranceSeconds)
                        continue;

                    differences.push_back ("track " + juce::String ((int) t) + " note " + juce::String (n)
                                           + ": expected " + describeEvent (e, i) + ", got " + describeEvent (a, i));
                    break;
                }
            }
        }

        return differences;
    }

    /** Checks every frame of the audio tracks shows the clip the model says should be
        playing, from the position it should be at, or silence. Frames close to a
        start or stop are skipped, as they're faded, as are those close to a rebuild
        after a clip change, where the new wave node fades from the old one.
    */
    inline std::vector<juce::String> compareAudioWithModel (const Scenario& sc, const Result& result)
    {
        constexpr int64_t fadeFrames = 64;
        constexpr double silence = 0.001;

        // Sinc resampling dips the level by ~1.5% just before a loop point, as its window reads past it
        constexpr double levelTolerance = 0.02;
        constexpr double positionToleranceFrames = 2.0;

        const auto intervals = getPlayIntervals (sc, result);
        const auto endFrame = toSamples (TimePosition::fromSeconds (result.endTime), sampleRate);
        auto toFrame = [] (double seconds) { return (int64_t) std::llround (seconds * sampleRate); };
        std::vector<juce::String> differences;

        for (int t = 0; t < sc.numAudioTracks; ++t)
        {
            struct Section
            {
                int scene;
                int64_t start, end, contentStart, length;
            };

            std::vector<Section> sections;
            std::vector<std::pair<int64_t, int64_t>> skipped;   // [start, end) frame ranges

            auto skipAround = [&] (int64_t frame, int64_t numFrames) { skipped.emplace_back (frame - numFrames, frame + numFrames); };

            for (int s = 0; s < sc.numScenes; ++s)
            {
                if (auto& spec = sc.clips[(size_t) t][(size_t) s])
                {
                    for (auto i : intervals[(size_t) t][(size_t) s])
                    {
                        sections.push_back ({ s, toFrame (i.start), toFrame (i.end), toFrame (i.contentStart), toFrame (spec->lengthBeats) });
                        skipAround (sections.back().start, fadeFrames);
                        skipAround (sections.back().end, fadeFrames);
                    }
                }
            }

            for (auto rebuild : result.rebuildsOfChangedClips)
                skipAround (toFrame (rebuild), blockSize + fadeFrames);

            // N.B. A wave node reads the block after an arrangement loop wrap as a jump, so it
            // re-seeks and crossfades from its last sample, up to 3 frames out (see
            // WaveNodeRealTime::processSection). Launched clips shouldn't need to
            if (result.arrangementLoopLength > 0.0)
                for (auto wrap = result.arrangementLoopLength; wrap < result.endTime; wrap += result.arrangementLoopLength)
                    skipped.emplace_back (toFrame (wrap), toFrame (wrap) + fadeFrames);

            std::sort (skipped.begin(), skipped.end());

            auto& sine = result.audio[(size_t) t][0];
            auto& cosine = result.audio[(size_t) t][1];
            const auto numFrames = std::min ((int64_t) sine.size(), endFrame);
            size_t skippedIndex = 0;
            int64_t numBadFrames = 0;
            std::optional<juce::String> firstBadFrame;

            for (int64_t f = 0; f < numFrames; ++f)
            {
                while (skippedIndex < skipped.size() && skipped[skippedIndex].second <= f)
                    ++skippedIndex;

                if (skippedIndex < skipped.size() && skipped[skippedIndex].first <= f)
                    continue;

                const Section* expected = nullptr;

                for (auto& section : sections)
                    if (section.start <= f && f < section.end)
                        expected = &section;

                const auto level = std::hypot (sine[(size_t) f], cosine[(size_t) f]);
                const auto phase = std::atan2 ((double) sine[(size_t) f], (double) cosine[(size_t) f]) / juce::MathConstants<double>::twoPi;
                bool ok = false;
                juce::String expectedDescription, gotDescription = "level " + juce::String (level, 4);

                if (expected == nullptr)
                {
                    ok = level < silence;
                    expectedDescription = "silence";
                }
                else
                {
                    const auto expectedPosition = (double) ((f - expected->contentStart) % expected->length);
                    const auto length = (double) expected->length;
                    auto difference = phase * length - expectedPosition;
                    difference -= length * std::round (difference / length);

                    ok = std::abs (level - getAudioClipLevel (expected->scene)) < levelTolerance
                         && std::abs (difference) <= positionToleranceFrames;
                    expectedDescription = "scene " + juce::String (expected->scene) + " at " + juce::String ((int) expectedPosition);
                    gotDescription << " at " + juce::String (expectedPosition + difference, 1);
                }

                if (ok)
                    continue;

                if (! firstBadFrame)
                    firstBadFrame = "at " + juce::String ((double) f / sampleRate, 5) + "s expected " + expectedDescription
                                     + ", got " + gotDescription;

                ++numBadFrames;
            }

            if (firstBadFrame)
                differences.push_back ("audio track " + juce::String (t) + ": " + juce::String (numBadFrames) + " frames wrong, first "
                                       + *firstBadFrame);
        }

        return differences;
    }

    //==============================================================================
    inline void dump (const juce::String& name, const Result& result)
    {
        if (juce::SystemStats::getEnvironmentVariable ("TE_CLIP_LAUNCHER_FUZZ_DUMP", {}).isEmpty())
            return;

        for (size_t t = 0; t < result.events.size(); ++t)
            if (! Scenario::isAudioTrack ((int) t))
                std::cout << name << " track " << t << ":\n" << describe (result.events[t]);
    }

    inline std::vector<int> getSeeds()
    {
        if (auto seed = juce::SystemStats::getEnvironmentVariable ("TE_CLIP_LAUNCHER_FUZZ_SEED", {}); seed.isNotEmpty())
            return { seed.getIntValue() };

        const auto numSeeds = juce::SystemStats::getEnvironmentVariable ("TE_CLIP_LAUNCHER_FUZZ_SEEDS", "10").getIntValue();
        std::vector<int> seeds;

        for (int i = 1; i <= numSeeds; ++i)
            seeds.push_back (i);

        return seeds;
    }

    inline void report (int seed, const Scenario& sc, const juce::String& check, const std::vector<juce::String>& failures)
    {
        if (failures.empty())
            return;

        juce::String message;
        message << "seed " << seed << ", " << check << ":\n";

        for (size_t i = 0; i < std::min<size_t> (failures.size(), 8); ++i)
            message << "  " << failures[i] << "\n";

        if (failures.size() > 8)
            message << "  ... and " << (int) (failures.size() - 8) << " more\n";

        message << describe (sc);
        FAIL_CHECK (message.toStdString());
    }
}

//==============================================================================
//==============================================================================
TEST_SUITE ("tracktion_engine")
{
    using namespace clip_launcher_fuzz;

    // An event less than half a sample before the end of a block is played at the start of the
    // next, and a note-off on a clip's end is nudged back a sample
    constexpr double midiTolerance = 1.5 / sampleRate;

    inline void checkVariant (Variant variant, const char* name)
    {
        auto& engine = *Engine::getEngines()[0];

        for (auto seed : getSeeds())
        {
            const auto sc = createScenario (seed);
            const auto result = run (engine, sc, variant);
            dump (name, result);
            report (seed, sc, juce::String (name) + ", note balance", checkNoteBalance (result));
            report (seed, sc, juce::String (name) + ", MIDI model", compareMidiWithModel (sc, result, midiTolerance));
            // Signalsmith plays a clip at its own tempo unchanged, but other time-stretchers change
            // its start, e.g. RubberBand's output is ~10% too loud for over 1000 frames and
            // SoundTouch's is late by its latency, so audio is only checked with Signalsmith
           #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
            report (seed, sc, juce::String (name) + ", audio model", compareAudioWithModel (sc, result));
           #endif
        }
    }

    TEST_CASE ("Clip launcher fuzz: launched clips play what the model expects")
    {
        checkVariant ({ .includeRebuilds = false, .arrangementLoop = false }, "plain");
    }

    TEST_CASE ("Clip launcher fuzz: graph rebuilds don't change what launched clips play")
    {
        checkVariant ({ .includeRebuilds = true, .arrangementLoop = false }, "rebuilds");
    }

    TEST_CASE ("Clip launcher fuzz: the arrangement loop doesn't change what launched clips play")
    {
        checkVariant ({ .includeRebuilds = false, .arrangementLoop = true }, "arrangement loop");
    }

    TEST_CASE ("Clip launcher fuzz: graph rebuilds with the arrangement loop")
    {
        checkVariant ({ .includeRebuilds = true, .arrangementLoop = true }, "rebuilds and arrangement loop");
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIP_LAUNCHER
