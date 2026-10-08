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
// Randomised clip launcher tests. Each seed builds an Edit of MIDI clips in
// slots and a random schedule of launches, stops and waits, plays it through
// an EnginePlayer and checks the MIDI each track's plugins get:
//  - invariants: a note is never struck while it's already on, and nothing is
//    left on at the end
//  - a model: the note-ons and note-offs the LaunchHandle calls made should
//    give, worked out from the clips' notes (see getExpectedEvents)
//
// Each schedule is also played with graph rebuilds between its steps and with
// an arrangement loop, neither of which should change what launched clips play.
//
// Set TE_CLIP_LAUNCHER_FUZZ_SEEDS to run more seeds, TE_CLIP_LAUNCHER_FUZZ_SEED
// to run one, and TE_CLIP_LAUNCHER_FUZZ_DUMP to print the events.
//==============================================================================
namespace clip_launcher_fuzz
{
    using namespace clip_launcher_test_utilities;

    enum class StepType { wait, launchScene, launchClip, stopTrack, stopAll, rebuild };

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
        std::vector<NoteSpec> notes;
    };

    struct Scenario
    {
        int numTracks = 3, numScenes = 4;
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
    };

    struct Result
    {
        std::vector<Events> events;     // [track]
        std::vector<Command> commands;
        double endTime = 0.0;
    };

    //==============================================================================
    inline juce::String describe (const Step& s)
    {
        auto q = juce::String (getLaunchQTypeChoices()[(int) s.quantisation]);

        switch (s.type)
        {
            case StepType::wait:        return "wait " + juce::String (s.seconds, 3) + "s";
            case StepType::launchScene: return "launch scene " + juce::String (s.scene) + " (" + q + ")";
            case StepType::launchClip:  return "launch clip t" + juce::String (s.track) + " s" + juce::String (s.scene) + " (" + q + ")";
            case StepType::stopTrack:   return "stop track " + juce::String (s.track) + " (" + q + ")";
            case StepType::stopAll:     return "stop all";
            case StepType::rebuild:     return "rebuild graph";
        }

        return {};
    }

    inline juce::String describe (const Scenario& sc)
    {
        juce::String s;

        for (int t = 0; t < sc.numTracks; ++t)
            for (int sl = 0; sl < sc.numScenes; ++sl)
                if (auto& c = sc.clips[(size_t) t][(size_t) sl])
                    s << "clip t" << t << " s" << sl << ": " << c->lengthBeats << " beats"
                      << (c->looping ? " looping" : " one-shot") << ", " << (int) c->notes.size() << " notes\n";

        double time = 0.5;

        for (auto& step : sc.steps)
        {
            s << juce::String (time, 4) << "s: " << describe (step) << "\n";

            if (step.type == StepType::wait)
                time += step.seconds;
        }

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

                static constexpr double lengths[] = { 2.0, 4.0, 5.0, 8.0, 16.0 };
                ClipSpec c;
                c.lengthBeats = lengths[r.nextInt (5)];
                c.looping = r.nextInt (4) != 0;

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
            else if (x < 52)    s = { StepType::launchScene, 0, r.nextInt (sc.numScenes), randomQ(), 0.0 };
            else if (x < 69)    s = { StepType::launchClip, r.nextInt (sc.numTracks), r.nextInt (sc.numScenes), randomQ(), 0.0 };
            else if (x < 76)    s = { StepType::stopTrack, r.nextInt (sc.numTracks), 0, randomQ(), 0.0 };
            else if (x < 80)    s = { StepType::stopAll, 0, 0, LaunchQType::none, 0.0 };
            else                s = { StepType::rebuild, 0, 0, LaunchQType::none, 0.0 };

            sc.steps.push_back (s);
        }

        return sc;
    }

    //==============================================================================
    inline Result run (Engine& engine, const Scenario& sc, Variant variant)
    {
        test_utilities::EnginePlayer player (engine, getPlayerParams());

        auto edit = test_utilities::createTestEdit (engine, sc.numTracks, Edit::EditRole::forEditing);
        edit->getSceneList().ensureNumberOfScenes (sc.numScenes);
        auto tracks = getAudioTracks (*edit);
        std::vector<MidiProbePlugin*> probes;
        std::vector<std::vector<std::shared_ptr<LaunchHandle>>> handles;    // [track][scene], null for an empty slot

        for (int t = 0; t < sc.numTracks; ++t)
        {
            auto track = tracks[t];
            track->getClipSlotList().ensureNumberOfSlots (sc.numScenes);
            probes.push_back (&addMidiProbePlugin (*track));
            auto& trackHandles = handles.emplace_back();

            for (int scene = 0; scene < sc.numScenes; ++scene)
            {
                auto& spec = sc.clips[(size_t) t][(size_t) scene];

                if (! spec)
                {
                    trackHandles.push_back (nullptr);
                    continue;
                }

                auto clip = insertMidiClipIntoSlot (*track->getClipSlotList().getClipSlots()[scene],
                                                    BeatDuration::fromBeats (spec->lengthBeats));

                if (! spec->looping)
                    clip->disableLooping();

                for (auto& n : spec->notes)
                    clip->getSequence().addNote (n.number, BeatPosition::fromBeats (n.start), BeatDuration::fromBeats (n.length),
                                                 100, 0, nullptr);

                trackHandles.push_back (clip->getLaunchHandle());
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
        int64_t numSamplesProcessed = 0;

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

        // Calls play or stop on a handle, logging it for the model
        auto call = [&] (int t, int scene, bool play, std::optional<MonotonicBeat> pos)
        {
            auto& h = handles[(size_t) t][(size_t) scene];

            if (! h)
                return;

            std::optional<double> position;

            if (pos)
                if (auto syncPoint = transport.getCurrentPlaybackContext()->getSyncPoint())
                    position = now() + (pos->v - syncPoint->monotonicBeat.v).inBeats();

            result.commands.push_back ({ now(), t, scene, play, position });

            if (play)
                h->play (pos);
            else
                h->stop (pos);
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
                    if (handles[(size_t) step.track][(size_t) step.scene])
                        launchOnTrack (step.track, step.scene, getPosition (step.quantisation));

                    break;

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
                        transport.ensureContextAllocated (true);

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

        for (auto probe : probes)
            result.events.push_back (probe->events);

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

    inline juce::String describe (const Events& events)
    {
        juce::String s;

        for (auto& e : events)
            s << "  " << (e.isNoteOn ? "on  " : "off ") << e.noteNumber << " at " << juce::String (e.outputTime, 5) << "s\n";

        return s;
    }

    //==============================================================================
    /** A model of what launched clips should play, from the LaunchHandle calls a run
        made. It follows the LaunchHandle rules: one queued play or stop per handle
        (a later call replaces it), a stop when stopped cancels a queued play, a
        one-shot stops at its end unless a play is queued, and launched clips run on
        the monotonic clock so the arrangement loop and graph rebuilds don't matter.
    */
    inline std::vector<Events> getExpectedEvents (const Scenario& sc, const std::vector<Command>& commands, double endTime)
    {
        struct Handle
        {
            const ClipSpec* spec = nullptr;
            bool playing = false, endHandled = false;
            double start = 0.0;
            struct Queued { bool play; double time; };
            std::optional<Queued> queued;
            std::vector<std::pair<double, double>> intervals;

            void startPlaying (double t)
            {
                stopPlaying (t);
                playing = true;
                endHandled = false;
                start = t;
            }

            void stopPlaying (double t)
            {
                if (playing && t > start)
                    intervals.emplace_back (start, t);

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
                            if (const auto end = h.start + h.spec->lengthBeats; end < nextTime)
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
                    next->startPlaying (queued.time);
                else
                    next->stopPlaying (queued.time);
            }
        };

        for (auto& c : commands)
        {
            advanceTo (c.time);
            auto& h = handles[(size_t) c.track][(size_t) c.scene];

            if (h.spec == nullptr)
                continue;

            const auto time = c.position ? std::max (*c.position, c.time) : c.time;

            if (c.play)
            {
                h.queued = Handle::Queued { true, time };
            }
            else if (! h.playing)
            {
                if (h.queued && h.queued->play)
                    h.queued.reset();
            }
            else
            {
                h.queued = Handle::Queued { false, time };
            }
        }

        advanceTo (endTime);

        std::vector<Events> expected ((size_t) sc.numTracks);

        for (size_t t = 0; t < handles.size(); ++t)
        {
            for (auto& h : handles[t])
            {
                if (h.spec == nullptr)
                    continue;

                h.stopPlaying (endTime);
                const auto length = h.spec->lengthBeats;

                for (auto [start, end] : h.intervals)
                {
                    for (int loop = 0; start + loop * length < end; ++loop)
                    {
                        if (loop > 0 && ! h.spec->looping)
                            break;

                        const auto loopStart = start + loop * length;

                        for (auto& n : h.spec->notes)
                        {
                            const auto on = loopStart + n.start;
                            const auto off = std::min ({ on + n.length, loopStart + length, end });

                            if (on >= end || off - on < 1.0e-6)
                                continue;

                            expected[t].push_back ({ true, n.number, on });
                            expected[t].push_back ({ false, n.number, off });
                        }
                    }
                }
            }
        }

        return expected;
    }

    /** Compares a run's events with the model's, note by note. */
    inline std::vector<juce::String> compareWithModel (const Scenario& sc, const Result& result, double toleranceSeconds)
    {
        const auto expected = getExpectedEvents (sc, result.commands, result.endTime);
        std::vector<juce::String> differences;

        if (juce::SystemStats::getEnvironmentVariable ("TE_CLIP_LAUNCHER_FUZZ_DUMP", {}).isNotEmpty())
            for (size_t t = 0; t < expected.size(); ++t)
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

    inline void dump (const juce::String& name, const Result& result)
    {
        if (juce::SystemStats::getEnvironmentVariable ("TE_CLIP_LAUNCHER_FUZZ_DUMP", {}).isEmpty())
            return;

        for (size_t t = 0; t < result.events.size(); ++t)
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

    // Restruck notes are sent 0.1ms late (see MidiNodeHelpers::createMessagesForTime)
    constexpr double modelTolerance = 0.00015;

    inline void checkVariant (Variant variant, const char* name)
    {
        auto& engine = *Engine::getEngines()[0];

        for (auto seed : getSeeds())
        {
            const auto sc = createScenario (seed);
            const auto result = run (engine, sc, variant);
            dump (name, result);
            report (seed, sc, juce::String (name) + ", note balance", checkNoteBalance (result));
            report (seed, sc, juce::String (name) + ", model", compareWithModel (sc, result, modelTolerance));
        }
    }

    TEST_CASE ("Clip launcher fuzz: launched clips play what the model expects (MIDI)")
    {
        checkVariant ({ .includeRebuilds = false, .arrangementLoop = false }, "plain");
    }

    TEST_CASE ("Clip launcher fuzz: graph rebuilds don't change what launched clips play (MIDI)")
    {
        checkVariant ({ .includeRebuilds = true, .arrangementLoop = false }, "rebuilds");
    }

    TEST_CASE ("Clip launcher fuzz: the arrangement loop doesn't change what launched clips play (MIDI)")
    {
        checkVariant ({ .includeRebuilds = false, .arrangementLoop = true }, "arrangement loop");
    }

    TEST_CASE ("Clip launcher fuzz: graph rebuilds with the arrangement loop (MIDI)")
    {
        checkVariant ({ .includeRebuilds = true, .arrangementLoop = true }, "rebuilds and arrangement loop");
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLIP_LAUNCHER
