/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_PLAYBACK && ENGINE_UNIT_TESTS_CLIP_LAUNCHER

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>
#include <tracktion_engine/testing/tracktion_EnginePlayer.h>
#include <tracktion_engine/utilities/tracktion_TestUtilities.h>

namespace tracktion::inline engine
{

//==============================================================================
// Changing the tempo whilst playing, e.g. dragging a tempo curve point up and down,
// should keep the playhead on the same beat, so MIDI and beat-based audio carry on
// seamlessly, looped or not, in the arrangement and the launcher.
//
// Each seed builds an Edit with a random tempo curve, arrangement MIDI clips (played
// both in the default seconds-based way and the beat-based way), beat-based audio
// clips and launched MIDI and audio clips, then plays it while a random schedule
// changes, drags and moves tempo points. A model of where the playhead should be, on
// the same beat after every change and looping at the same beats, is checked against
// what each track's plugins get:
//  - every section processed starts where the model says
//  - MIDI tracks get every note-on and note-off where the model puts them
//  - beat-based audio clips read their source from where the model puts it, and
//    their output, a steady tone, has no gaps, fades or jumps
//
// Clips that don't follow the beat (e.g. ones playing a pre-rendered proxy) can't
// follow a tempo change seamlessly, so they aren't tested here.
//
// Set TE_TEMPO_CHANGE_FUZZ_SEEDS to run more seeds and TE_TEMPO_CHANGE_FUZZ_SEED
// to run one. To look in to a failure, TE_TEMPO_CHANGE_FUZZ_DUMP prints the sections
// around the first playhead problem and each wrong note's events,
// TE_TEMPO_CHANGE_FUZZ_DUMP_AT=<output sample> prints the sections and MIDI events
// around a sample and TE_TEMPO_CHANGE_FUZZ_DUMP_AUDIO=<seconds> prints the level and
// phase of a failing audio clip's output from a time.
//==============================================================================
namespace tempo_change_tests
{
    using namespace clip_launcher_test_utilities;

    //==============================================================================
    /** A test-only plugin that records everything a track sends it: each section
        it's processed for, the note-ons and note-offs and the first two channels.
    */
    class TempoProbePlugin  : public Plugin
    {
    public:
        TempoProbePlugin (PluginCreationInfo info)  : Plugin (info) {}
        ~TempoProbePlugin() override                            { notifyListenersOfDeletion(); }

        static const char* getPluginName()                      { return "Tempo Probe"; }
        static constexpr const char* xmlTypeName = "tempoProbe";

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
            // The tempo map can only change between audio callbacks, so this is the one used for this section
            const auto& tempoMap = edit.tempoSequence.getInternalSequence();

            if (! tempoMaps.contains (tempoMap.hash()))
                tempoMaps.emplace (tempoMap.hash(), tempoMap);

            sections.push_back ({ numSamplesProcessed, fc.bufferNumSamples, fc.editTime, fc.isPlaying, tempoMap.hash() });

            if (fc.bufferForMidiMessages != nullptr)
                for (auto& m : *fc.bufferForMidiMessages)
                    if (m.isNoteOnOrOff())
                        events.push_back ({ m.isNoteOn(), m.getNoteNumber(),
                                            (double) numSamplesProcessed + m.getTimeStamp() * sampleRate });

            for (int chan = 0; chan < 2; ++chan)
                for (int i = 0; i < fc.bufferNumSamples; ++i)
                    audio[(size_t) chan].push_back (fc.destBuffer != nullptr && chan < fc.destBuffer->getNumChannels()
                                                      ? fc.destBuffer->getSample (chan, fc.bufferStartSample + i) : 0.0f);

            numSamplesProcessed += fc.bufferNumSamples;
        }

        struct Section
        {
            int64_t outputStart = 0;
            int numSamples = 0;
            TimeRange editTime;
            bool isPlaying = false;
            size_t tempoMapHash = 0;
        };

        struct Event
        {
            bool isNoteOn = false;
            int noteNumber = 0;
            double outputSample = 0.0;
            double tolerance = 0.0;     // How far off it can be, for expected events
        };

        std::vector<Section> sections;
        std::map<size_t, tempo::Sequence> tempoMaps;    // Each tempo map used, by hash
        std::vector<Event> events;
        std::array<std::vector<float>, 2> audio;
        int64_t numSamplesProcessed = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TempoProbePlugin)
    };

    /** Adds a TempoProbePlugin to the start of a track's plugins. */
    inline TempoProbePlugin& addTempoProbePlugin (AudioTrack& track)
    {
        track.edit.engine.getPluginManager().createBuiltInType<TempoProbePlugin>();
        auto probe = dynamic_cast<TempoProbePlugin*> (track.edit.getPluginCache().createNewPlugin (TempoProbePlugin::xmlTypeName, {}).get());
        assert (probe != nullptr);
        track.pluginList.insertPlugin (*probe, 0, nullptr);
        return *probe;
    }

    //==============================================================================
    /** Beat-based audio clips play a stereo file of a steady sine and cosine, so the
        level of each frame of the output is the tone's level unless something has
        faded or cut it, and the phase steps on steadily unless it has jumped.
        A time-stretcher keeps the tone's pitch whatever the tempo.
    */
    constexpr double fileBpm = 120.0;
    constexpr double toneFrequency = 440.0;     // A whole number of cycles a beat, so a clip looping on beats is seamless
    constexpr float toneLevel = 0.5f;

    inline std::unique_ptr<MemoryAudioFile> createToneFile (Engine& engine, double numBeats)
    {
        const auto numFrames = (choc::buffer::FrameCount) std::llround (numBeats * 60.0 / fileBpm * sampleRate);
        auto source = choc::buffer::createChannelArrayBuffer (2, numFrames, [] (auto chan, auto frame)
                                                              {
                                                                  const auto phase = juce::MathConstants<double>::twoPi * toneFrequency * frame / sampleRate;
                                                                  return toneLevel * (float) (chan == 0 ? std::sin (phase) : std::cos (phase));
                                                              });
        return std::make_unique<MemoryAudioFile> (engine, source);
    }

    /** Makes an audio clip play its file in beats at fileBpm, stretched in real time. */
    inline void makeBeatBased (AudioClipBase& clip, double numBeats)
    {
        clip.setUsesProxy (false);
        clip.setAutoTempo (true);
        clip.getLoopInfo().setNumBeats (numBeats);

        // The default time-stretcher depends on the build. Signalsmith passes a steady tone
        // through cleanly, so the output checks are tightest with it
       #if TRACKTION_ENABLE_TIMESTRETCH_SIGNALSMITH
        clip.setTimeStretchMode (TimeStretcher::signalsmithDefault);
       #endif
    }

    //==============================================================================
    struct TempoPoint
    {
        double beat = 0.0, bpm = 120.0;
        float curve = 1.0f;
    };

    struct NoteSpec
    {
        int number = 60;
        double start = 0.0, length = 1.0;
    };

    struct MidiClipSpec
    {
        double start = 0.0, length = 4.0;
        std::optional<double> loopLength;
        std::vector<NoteSpec> notes;
    };

    struct AudioClipSpec
    {
        double start = 0.0, length = 4.0;
        double fadeSeconds = 0.0;
    };

    enum class StepType { wait, setBpm, drag, moveTempo, rebuild, syncLoop };

    struct Step
    {
        StepType type = StepType::wait;
        int numBlocks = 1, tempo = 0;
        double value = 0.0;
    };

    struct Scenario
    {
        std::vector<TempoPoint> tempos;
        double loopStart = 0.0, loopEnd = 16.0, startBeat = 0.0;
        std::vector<MidiClipSpec> midiClips;
        std::vector<AudioClipSpec> audioClips;
        MidiClipSpec launcherMidi;
        double launcherAudioLength = 4.0;
        std::vector<Step> steps;
    };

    struct Variant
    {
        bool looping = false;
        bool rebuilds = false;
    };

    enum TrackIndex
    {
        midiTrack,              // Arrangement MIDI clips, played the default way (a sequence in seconds)
        beatMidiTrack,          // The same clips, played from beats (MidiClip::setUsesProxy (false))
        audioTrack,             // Beat-based arrangement audio clips
        launcherMidiTrack,      // A launched MIDI clip
        launcherAudioTrack,     // A launched beat-based audio clip
        numTracks
    };

    inline bool isMidiTrack (int t)     { return t == midiTrack || t == beatMidiTrack || t == launcherMidiTrack; }
    inline bool isLauncherTrack (int t) { return t == launcherMidiTrack || t == launcherAudioTrack; }

    //==============================================================================
    inline juce::String describe (const Step& s)
    {
        switch (s.type)
        {
            case StepType::wait:        return "wait " + juce::String (s.numBlocks) + " blocks";
            case StepType::setBpm:      return "set tempo " + juce::String (s.tempo) + " to " + juce::String (s.value, 2) + "bpm";
            case StepType::drag:        return "drag tempo " + juce::String (s.tempo) + " to " + juce::String (s.value, 2) + "bpm over "
                                                + juce::String (s.numBlocks) + " blocks";
            case StepType::moveTempo:   return "move tempo " + juce::String (s.tempo) + " to beat " + juce::String (s.value, 2);
            case StepType::rebuild:     return "pump messages (graph rebuild)";
            case StepType::syncLoop:    return "transport timer loop sync";
        }

        return {};
    }

    inline juce::String describe (const Scenario& sc, Variant v, const std::vector<double>& stepTimes)
    {
        juce::String s;
        s << (v.looping ? "looping beats " + juce::String (sc.loopStart) + "-" + juce::String (sc.loopEnd) : juce::String ("not looping"))
          << (v.rebuilds ? ", with rebuilds" : "") << ", start beat " << juce::String (sc.startBeat, 2) << "\n";

        for (auto& t : sc.tempos)
            s << "tempo at beat " << t.beat << ": " << juce::String (t.bpm, 2) << "bpm, curve " << t.curve << "\n";

        for (auto& c : sc.midiClips)
            s << "MIDI clip beats " << c.start << "-" << (c.start + c.length)
              << (c.loopLength ? ", looping every " + juce::String (*c.loopLength) : juce::String())
              << ", " << (int) c.notes.size() << " notes\n";

        for (auto& c : sc.audioClips)
            s << "audio clip beats " << c.start << "-" << (c.start + c.length) << ", fades " << c.fadeSeconds << "s\n";

        for (size_t i = 0; i < sc.steps.size(); ++i)
            s << "  " << (i < stepTimes.size() ? juce::String (stepTimes[i], 4) + "s: " : juce::String())
              << describe (sc.steps[i]) << "\n";

        return s;
    }

    //==============================================================================
    inline std::vector<NoteSpec> createNotes (juce::Random& r, double length, int firstNote)
    {
        std::vector<NoteSpec> notes;
        const auto numQuarters = (int) (length * 4.0);

        for (int i = 0; i < 1 + r.nextInt (6); ++i)
        {
            const auto start = r.nextInt (numQuarters) / 4.0;
            const auto maxLength = length - start;
            notes.push_back ({ firstNote + i, start, std::min (maxLength, 0.25 + r.nextInt (8) / 4.0) });
        }

        return notes;
    }

    inline Scenario createScenario (int seed, Variant variant)
    {
        juce::Random r (seed);
        Scenario sc;

        auto randomBpm = [&] { return 50.0 + r.nextDouble() * 150.0; };
        auto randomCurve = [&]
        {
            static constexpr float curves[] = { 1.0f, 1.0f, 1.0f, -1.0f, 0.0f, 0.3f };
            return curves[r.nextInt (6)];
        };

        sc.tempos.push_back ({ 0.0, randomBpm(), randomCurve() });

        for (double beat = 8.0 + 4.0 * r.nextInt (4); beat < 48.0 && sc.tempos.size() < 3; beat += 8.0 + 4.0 * r.nextInt (4))
            sc.tempos.push_back ({ beat, randomBpm(), randomCurve() });

        sc.loopStart = 4.0 * r.nextInt (4);
        sc.loopEnd = sc.loopStart + 4.0 * (2 + r.nextInt (4));
        sc.startBeat = variant.looping ? sc.loopStart + r.nextInt ((int) ((sc.loopEnd - sc.loopStart) * 4.0)) / 4.0
                                       : r.nextInt (32) / 4.0;

        // Clips one after another, sometimes with gaps between them, so they start and stop
        // whilst playing and the tempo changes in them
        for (double start = r.nextInt (4); start < 40.0;)
        {
            MidiClipSpec c;
            c.start = start;
            c.length = 4.0 + r.nextInt (13);

            if (r.nextBool())
                c.loopLength = (double) (2 + r.nextInt (3));

            c.notes = createNotes (r, c.loopLength.value_or (c.length), 36 + 12 * (int) sc.midiClips.size());
            sc.midiClips.push_back (c);
            start += c.length + r.nextInt (3);
        }

        for (double start = r.nextInt (4); start < 40.0;)
        {
            AudioClipSpec c;
            c.start = start;
            c.length = 4.0 + r.nextInt (13);
            c.fadeSeconds = r.nextBool() ? 0.0 : 0.005 + r.nextDouble() * 0.05;
            sc.audioClips.push_back (c);
            start += c.length + 1.0 + r.nextInt (3);
        }

        sc.launcherMidi.length = 2.0 + r.nextInt (5);
        sc.launcherMidi.notes = createNotes (r, sc.launcherMidi.length, 60);
        sc.launcherAudioLength = 2.0 + r.nextInt (5);

        for (int i = 0; i < 30; ++i)
        {
            Step s;
            const auto x = r.nextInt (100);
            s.tempo = r.nextInt ((int) sc.tempos.size());

            if (x < 30)         s = { StepType::wait, 1 + r.nextInt (60), 0, 0.0 };
            else if (x < 45)    s = { StepType::setBpm, 1, s.tempo, randomBpm() };
            else if (x < 70)    s = { StepType::drag, 4 + r.nextInt (40), s.tempo, randomBpm() };
            else if (x < 80)    s = { StepType::moveTempo, 1, s.tempo, 0.0 };
            else if (x < 92)    s = { StepType::rebuild, 1, 0, 0.0 };
            else                s = { StepType::syncLoop, 1, 0, 0.0 };

            if ((s.type == StepType::rebuild || s.type == StepType::syncLoop) && ! variant.rebuilds)
                s = { StepType::wait, 1 + r.nextInt (10), 0, 0.0 };

            if (s.type == StepType::moveTempo && s.tempo == 0)
                s.tempo = 1;

            if (s.type == StepType::moveTempo && s.tempo >= (int) sc.tempos.size())
                s = { StepType::wait, 1 + r.nextInt (10), 0, 0.0 };

            s.value = s.type == StepType::moveTempo ? r.nextDouble() : s.value;
            sc.steps.push_back (s);
        }

        return sc;
    }

    //==============================================================================
    struct MapChange
    {
        int64_t outputSample = 0;   // The first output sample it's used for
        tempo::Sequence map;
    };

    struct ClipPlayheadReading
    {
        int track = 0, clip = 0;
        int64_t outputSample = 0;   // Where the last section of the block started
        TimePosition sourcePosition;
    };

    struct Result
    {
        std::vector<juce::String> clipsOffTheirBeats;                   // Checked at the end of the run
        std::map<size_t, tempo::Sequence> probeTempoMaps;               // Each tempo map the Edit played with
        std::vector<double> stepTimes;                                  // The output time each step started
        std::vector<TempoProbePlugin::Section> sections;
        std::vector<std::vector<TempoProbePlugin::Event>> events;      // [track]
        std::vector<std::array<std::vector<float>, 2>> audio;          // [track]
        std::vector<MapChange> maps;
        std::vector<ClipPlayheadReading> clipPlayheads;
        std::map<int64_t, juce::Range<int64_t>> loopRanges;             // The playhead's loop range for each block, by its output start
        int64_t launchOutputSample = 0;     // Where the launched clips start
        int64_t endOutputSample = 0;        // Where the transport stops
        std::vector<int64_t> rebuildOutputSamples;
    };

    //==============================================================================
    inline Result run (Engine& engine, const Scenario& sc, Variant variant)
    {
        test_utilities::EnginePlayer player (engine, getPlayerParams());
        std::vector<std::unique_ptr<MemoryAudioFile>> files;    // N.B. declared before the Edit to outlive it

        auto edit = test_utilities::createTestEdit (engine, numTracks, Edit::EditRole::forEditing);
        edit->getSceneList().ensureNumberOfScenes (1);
        auto& ts = edit->tempoSequence;
        auto tracks = getAudioTracks (*edit);
        std::vector<TempoProbePlugin*> probes;

        for (auto t : tracks)
            probes.push_back (&addTempoProbePlugin (*t));

        ts.getTempo (0)->set (0_bp, sc.tempos[0].bpm, sc.tempos[0].curve, false);

        for (size_t i = 1; i < sc.tempos.size(); ++i)
            ts.insertTempo (BeatPosition::fromBeats (sc.tempos[i].beat), sc.tempos[i].bpm, sc.tempos[i].curve);

        ts.updateTempoData();

        auto toTime = [&] (double beat) { return ts.toTime (BeatPosition::fromBeats (beat)); };
        auto beatRange = [&] (double start, double length) { return TimeRange (toTime (start), toTime (start + length)); };

        auto addMidiClip = [&] (ClipOwner& owner, const MidiClipSpec& spec, bool beatBased)
        {
            auto clip = insertMIDIClip (owner, beatRange (spec.start, spec.length));

            if (beatBased)
                clip->setUsesProxy (false);

            if (spec.loopLength)
                clip->setLoopRangeBeats ({ 0_bp, BeatPosition::fromBeats (*spec.loopLength) });
            else
                clip->disableLooping();

            for (auto& n : spec.notes)
                clip->getSequence().addNote (n.number, BeatPosition::fromBeats (n.start), BeatDuration::fromBeats (n.length),
                                             100, 0, nullptr);

            return clip;
        };

        for (auto& spec : sc.midiClips)
        {
            addMidiClip (*tracks[midiTrack], spec, false);
            addMidiClip (*tracks[beatMidiTrack], spec, true);
        }

        std::vector<std::pair<int, AudioClipBase*>> audioClips;    // [track, clip]

        for (auto& spec : sc.audioClips)
        {
            files.push_back (createToneFile (engine, spec.length));
            auto clip = insertWaveClip (*tracks[audioTrack], {}, files.back()->getFile(),
                                        { beatRange (spec.start, spec.length) }, DeleteExistingClips::no);
            makeBeatBased (*clip, spec.length);
            clip->setPosition ({ beatRange (spec.start, spec.length) });

            if (spec.fadeSeconds > 0.0)
            {
                clip->setFadeIn (TimeDuration::fromSeconds (spec.fadeSeconds));
                clip->setFadeOut (TimeDuration::fromSeconds (spec.fadeSeconds));
            }

            audioClips.emplace_back (audioTrack, clip.get());
        }

        // Launched clips, which loop from where they're launched
        for (auto t : { launcherMidiTrack, launcherAudioTrack })
            tracks[t]->getClipSlotList().ensureNumberOfSlots (1);

        auto launchedMidi = addMidiClip (*tracks[launcherMidiTrack]->getClipSlotList().getClipSlots()[0],
                                         sc.launcherMidi, true);
        launchedMidi->setLoopRangeBeats ({ 0_bp, BeatPosition::fromBeats (sc.launcherMidi.length) });

        files.push_back (createToneFile (engine, sc.launcherAudioLength));
        auto launchedAudio = insertWaveClip (*tracks[launcherAudioTrack]->getClipSlotList().getClipSlots()[0], {},
                                             files.back()->getFile(), { beatRange (0.0, sc.launcherAudioLength) },
                                             DeleteExistingClips::no);
        makeBeatBased (*launchedAudio, sc.launcherAudioLength);
        launchedAudio->setPosition ({ beatRange (0.0, sc.launcherAudioLength) });
        launchedAudio->setLoopRangeBeats ({ 0_bp, BeatPosition::fromBeats (sc.launcherAudioLength) });
        audioClips.emplace_back (launcherAudioTrack, launchedAudio.get());

        auto& transport = edit->getTransport();
        transport.setLoopRange (beatRange (sc.loopStart, sc.loopEnd - sc.loopStart));
        transport.looping = variant.looping;
        transport.setPosition (toTime (sc.startBeat));

        Result result;
        result.maps.push_back ({ 0, ts.getInternalSequence() });

        auto processBlocks = [&] (int numBlocks)
        {
            for (int i = 0; i < numBlocks; ++i)
            {
                const auto blockStart = probes[0]->numSamplesProcessed;
                player.process (blockSize);

                if (auto epc = transport.getCurrentPlaybackContext(); epc != nullptr && variant.looping)
                    if (auto ph = epc->getNodePlayHead())
                        result.loopRanges[blockStart] = ph->getLoopRange();

                // The clip playheads say where each beat-based clip read its source from,
                // for the last section of the block
                for (size_t c = 0; c < audioClips.size(); ++c)
                {
                    auto [t, clip] = audioClips[c];
                    auto& sections = probes[(size_t) t]->sections;

                    if (auto pos = clip->getPlayhead()->getPosition (std::numeric_limits<uint32_t>::max()); pos && ! sections.empty())
                        result.clipPlayheads.push_back ({ t, (int) c, sections.back().outputStart, *pos });

                    clip->getPlayhead()->setPosition (std::nullopt);
                }
            }
        };

        auto logMapChange = [&]
        {
            result.maps.push_back ({ probes[0]->numSamplesProcessed, ts.getInternalSequence() });
        };

        // Sets a tempo the way TempoCurveEditor does, keeping the Edit's positions on their beats
        auto setTempo = [&] (int index, BeatPosition beat, double bpm)
        {
            auto tempo = ts.getTempo (index);
            tempo->set (beat, bpm, tempo->getCurve(), true);
            logMapChange();
        };

        transport.play (false);
        processBlocks (1);

        // Launch the launcher clips at the start of the next block
        result.launchOutputSample = probes[0]->numSamplesProcessed;
        launchedMidi->getLaunchHandle()->play ({});
        launchedAudio->getLaunchHandle()->play ({});
        processBlocks (8);

        for (auto& step : sc.steps)
        {
            result.stepTimes.push_back ((double) probes[0]->numSamplesProcessed / sampleRate);

            switch (step.type)
            {
                case StepType::wait:
                    processBlocks (step.numBlocks);
                    break;

                case StepType::setBpm:
                    setTempo (step.tempo, ts.getTempo (step.tempo)->getStartBeat(), step.value);
                    processBlocks (1);
                    break;

                case StepType::drag:
                {
                    const auto startBpm = ts.getTempo (step.tempo)->getBpm();

                    for (int i = 1; i <= step.numBlocks; ++i)
                    {
                        setTempo (step.tempo, ts.getTempo (step.tempo)->getStartBeat(),
                                  startBpm + (step.value - startBpm) * i / step.numBlocks);
                        processBlocks (1);
                    }

                    break;
                }

                case StepType::moveTempo:
                {
                    // Between its neighbours, on a whole beat, as TempoCurveEditor moves them
                    const auto prev = ts.getTempo (step.tempo - 1)->getStartBeat().inBeats() + 1.0;
                    const auto next = step.tempo + 1 < ts.getNumTempos() ? ts.getTempo (step.tempo + 1)->getStartBeat().inBeats() - 1.0
                                                                         : prev + 16.0;
                    if (next >= prev)
                        setTempo (step.tempo, BeatPosition::fromBeats (std::round (prev + (next - prev) * step.value)),
                                  ts.getTempo (step.tempo)->getBpm());

                    processBlocks (1);
                    break;
                }

                case StepType::rebuild:
                    // Lets the Edit's restart timer rebuild the graph, as it would after each change
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (5);
                    result.rebuildOutputSamples.push_back (probes[0]->numSamplesProcessed);
                    processBlocks (1);
                    break;

                case StepType::syncLoop:
                    // As TransportControl's timer periodically does
                    if (auto epc = transport.getCurrentPlaybackContext(); epc != nullptr && transport.looping)
                    {
                        if (auto ph = epc->getNodePlayHead())
                        {
                            epc->setExactLoopTimes (transport.getLoopRange());
                            ph->setLoopRange (true, toSamples (transport.getLoopRange(), sampleRate));
                        }
                    }

                    processBlocks (1);
                    break;
            }
        }

        // Every change keeps the Edit's clips on their beats
        auto checkClipBeats = [&] (Clip& clip, double start, double length, const juce::String& name)
        {
            const auto beats = ts.toBeats (clip.getEditTimeRange());

            if (std::abs (beats.getStart().inBeats() - start) > 1.0e-6 || std::abs (beats.getEnd().inBeats() - (start + length)) > 1.0e-6)
                result.clipsOffTheirBeats.push_back (name + " is at beats " + juce::String (beats.getStart().inBeats(), 6) + "-"
                                                     + juce::String (beats.getEnd().inBeats(), 6) + ", expected "
                                                     + juce::String (start) + "-" + juce::String (start + length));
        };

        for (int t : { midiTrack, beatMidiTrack })
            for (size_t i = 0; i < sc.midiClips.size(); ++i)
                checkClipBeats (*tracks[t]->getClips()[(int) i], sc.midiClips[i].start, sc.midiClips[i].length,
                                "track " + juce::String (t) + " MIDI clip " + juce::String ((int) i));

        for (size_t i = 0; i < sc.audioClips.size(); ++i)
            checkClipBeats (*tracks[audioTrack]->getClips()[(int) i], sc.audioClips[i].start, sc.audioClips[i].length,
                            "audio clip " + juce::String ((int) i));

        processBlocks (20);
        result.endOutputSample = probes[0]->numSamplesProcessed;
        transport.stop (false, false);
        processBlocks (10);

        result.sections = probes[0]->sections;
        result.probeTempoMaps = probes[0]->tempoMaps;

        for (auto p : probes)
        {
            result.events.push_back (p->events);
            result.audio.push_back (p->audio);
        }

        return result;
    }

    //==============================================================================
    /** A stretch of output where the playhead moves on steadily through the Edit. */
    struct Segment
    {
        int64_t outputStart = 0, numSamples = 0;
        int64_t editStart = 0;          // The Edit sample it starts at
        size_t map = 0;                 // Index in Result::maps
        double monotonicStart = 0.0;    // The monotonic beat it starts at
        bool startsAfterJump = false, endsAtJump = false;
        bool endsAtStop = false;        // Plugins clear their own notes when stopping, so no note-offs are expected
        double beatOffset = 0.0;        // What makes up the difference between its beat and the nearest sample's after a tempo change

        int64_t getOutputEnd() const    { return outputStart + numSamples; }
    };

    /** Where the playhead should be: on the same beat after a tempo change, looping
        at the same beats, as the Edit's loop range is moved with its beats.
    */
    inline std::vector<Segment> createModel (Variant variant, const Result& result)
    {
        std::vector<Segment> segments;

        // The playhead starts where it's put; that isn't what's being tested
        auto firstPlaying = std::find_if (result.sections.begin(), result.sections.end(),
                                          [] (auto& s) { return s.isPlaying && s.numSamples > 0; });

        if (firstPlaying == result.sections.end())
            return segments;

        auto mapIndexAt = [&] (int64_t outputSample)
        {
            size_t index = 0;

            for (size_t i = 0; i < result.maps.size(); ++i)
                if (result.maps[i].outputSample <= outputSample)
                    index = i;

            return index;
        };

        // The loop's end can be a sample either side of its beat, so it's checked separately
        // (see checkLoopRanges) and the playhead's followed here
        auto getLoop = [&] (int64_t blockStart)
        {
            if (auto r = result.loopRanges.find (blockStart); r != result.loopRanges.end())
                return r->second;

            return juce::Range<int64_t>();
        };

        auto toBeats = [&] (size_t mapIndex, int64_t editSample)
        {
            return result.maps[mapIndex].map.toBeats (TimePosition::fromSamples (editSample, sampleRate)).inBeats();
        };

        int64_t output = firstPlaying->outputStart;
        int64_t edit = toSamples (firstPlaying->editTime.getStart(), sampleRate);
        int64_t lastEditEnd = edit;
        size_t map = mapIndexAt (output);
        auto loop = getLoop (output);
        double monotonic = 0.0, beatOffset = 0.0;
        bool afterJump = true;

        while (output < result.endOutputSample)
        {
            loop = getLoop (output);

            if (const auto newMap = mapIndexAt (output); newMap != map)
            {
                // Back on the beat the last block ended on, as near as the samples allow
                const auto beat = toBeats (map, lastEditEnd) + beatOffset;
                map = newMap;
                edit = toSamples (result.maps[map].map.toTime (BeatPosition::fromBeats (beat)), sampleRate);

                if (variant.looping)
                {
                    edit = loop.clipValue (edit);

                    if (edit == loop.getEnd())
                        edit = loop.getStart();
                }

                beatOffset = beat - toBeats (map, edit);
            }

            const auto blockEnd = std::min (output + blockSize, result.endOutputSample);

            while (output < blockEnd)
            {
                auto numSamples = blockEnd - output;

                if (variant.looping)
                    numSamples = std::min (numSamples, loop.getEnd() - edit);

                const bool wraps = variant.looping && edit + numSamples == loop.getEnd();
                const bool stops = output + numSamples == result.endOutputSample;

                if (afterJump)
                    beatOffset = 0.0;

                segments.push_back ({ output, numSamples, edit, map, monotonic, afterJump, wraps || stops, stops, beatOffset });

                monotonic += toBeats (map, edit + numSamples) - toBeats (map, edit);
                lastEditEnd = edit + numSamples;
                edit += numSamples;
                output += numSamples;
                afterJump = wraps;

                if (wraps)
                    edit = loop.getStart();
            }
        }

        return segments;
    }

    //==============================================================================
    using Event = TempoProbePlugin::Event;
    using Events = std::vector<Event>;

    struct Note
    {
        int number = 0;
        double on = 0.0, off = 0.0;    // In beats
    };

    /** A MIDI clip's notes, in Edit beats from its start. */
    inline std::vector<Note> getNotes (const MidiClipSpec& spec)
    {
        std::vector<Note> notes;

        if (! spec.loopLength)
        {
            for (auto& n : spec.notes)
                notes.push_back ({ n.number, spec.start + n.start, spec.start + std::min (n.start + n.length, spec.length) });

            return notes;
        }

        for (double loopStart = 0.0; loopStart < spec.length; loopStart += *spec.loopLength)
        {
            for (auto& n : spec.notes)
            {
                const auto on = loopStart + n.start;
                const auto off = std::min ({ on + n.length, loopStart + *spec.loopLength, spec.length });

                if (on < spec.length && off - on > 1.0e-4)
                    notes.push_back ({ n.number, spec.start + on, spec.start + off });
            }
        }

        return notes;
    }

    /** Maps between a segment's output samples and beats in some timeline: the Edit's
        or, for a launched clip, the clip's content, which starts at contentOffset.
    */
    struct SegmentMapping
    {
        Segment segment;
        const tempo::Sequence* map = nullptr;
        double contentOffset = 0.0;   // The segment's start in the timeline, minus its start in Edit beats

        double getEditBeatAtStart() const
        {
            return map->toBeats (TimePosition::fromSamples (segment.editStart, sampleRate)).inBeats();
        }

        double getStart() const     { return getEditBeatAtStart() + contentOffset; }
        double getEnd() const
        {
            return map->toBeats (TimePosition::fromSamples (segment.editStart + segment.numSamples, sampleRate)).inBeats() + contentOffset;
        }

        double toOutputSample (double beat) const
        {
            const auto editTime = map->toTime (BeatPosition::fromBeats (beat - contentOffset)).inSeconds();
            return (double) segment.outputStart + (editTime * sampleRate - (double) segment.editStart);
        }

        double beatAt (int64_t outputSample) const
        {
            return map->toBeats (TimePosition::fromSamples (segment.editStart + (outputSample - segment.outputStart), sampleRate)).inBeats()
                    + contentOffset;
        }
    };

    /** A note already on where playback jumps to is struck just after it
        (see MidiNodeHelpers::createMessagesForTime), as might one on it.
    */
    constexpr double chasedNoteDelay = 0.0001;

    // An event less than half a sample before the end of a section is played at the start of
    // the next, a note-off on a clip's end is nudged back a sample and splitting a block at a
    // tempo change rounds to whole samples
    constexpr double midiToleranceSamples = 2.5;

    // Note-offs are nudged back a little so they're before a note-on at the same time
    // (see MidiNote::getPlaybackTime)
    constexpr double noteOffTolerance = midiToleranceSamples + 0.0001 * sampleRate;

    /** The note-ons and note-offs the model says a track gets. A note on where a segment
        starts after a jump is struck just after its start, and one on where a segment ends at a
        jump is stopped at its end. An event less than half a sample before the end of a
        segment is played in the next.
    */
    inline Events getExpectedEvents (const std::vector<Note>& notes, const std::vector<SegmentMapping>& mappings)
    {
        Events expected;

        for (auto& m : mappings)
        {
            const auto b0 = m.getStart(), b1 = m.getEnd();
            const auto halfSample = (b1 - b0) / (double) m.segment.numSamples / 2.0;
            const auto start = b0 - halfSample, end = b1 - halfSample;

            for (auto& n : notes)
            {
                if (n.off <= start || n.on >= end)
                    continue;

                // A note ending within half a sample of a jump isn't chased
                if (m.segment.startsAfterJump && n.on < start && n.off < b0 + halfSample)
                    continue;

                const auto chased = m.segment.startsAfterJump && m.toOutputSample (n.on) < (double) m.segment.outputStart + chasedNoteDelay * sampleRate;
                std::optional<Event> on, off;

                if (chased)
                    on = Event { true, n.number, (double) m.segment.outputStart + chasedNoteDelay * sampleRate / 2.0,
                                 midiToleranceSamples + chasedNoteDelay * sampleRate / 2.0 };
                else if (n.on >= start)
                    on = Event { true, n.number, m.toOutputSample (n.on), midiToleranceSamples };

                if (n.off < end)
                    off = Event { false, n.number, m.toOutputSample (n.off), noteOffTolerance };
                else if (m.segment.endsAtJump && ! m.segment.endsAtStop)
                    off = Event { false, n.number, (double) m.segment.getOutputEnd() - 1.0, noteOffTolerance };

                // A note less than a sample long may or may not be played (see compareEvents)
                if (on && off && off->outputSample - on->outputSample <= 1.5)
                    continue;

                if (on)     expected.push_back (*on);
                if (off)    expected.push_back (*off);
            }
        }

        return expected;
    }

    inline juce::String describe (const Event& e)
    {
        return juce::String (e.isNoteOn ? "on" : "off") + " at " + juce::String (e.outputSample / sampleRate, 5) + "s";
    }

    /** Compares a track's events with the model's, note by note. */
    inline std::vector<juce::String> compareEvents (const Events& expectedEvents, const Events& actualEvents,
                                                    int64_t endOutputSample)
    {
        auto byNote = [endOutputSample] (const Events& events)
        {
            std::map<int, Events> notes;

            // Up to the stop, which plugins clear up after themselves
            for (auto& e : events)
                if (e.outputSample < (double) endOutputSample - 3.0)
                    notes[e.noteNumber].push_back (e);

            for (auto& [n, list] : notes)
            {
                // Events at the same time stay in the order plugins got them (or the model made them),
                // so a note stopping and starting again isn't mistaken for one less than a sample long
                std::stable_sort (list.begin(), list.end(), [] (auto& a, auto& b) { return a.outputSample < b.outputSample - 0.01; });

                // A note less than a sample long, e.g. one on the last sample before a loop wraps,
                // may or may not be played, so they're left out, as are note-offs for notes that
                // aren't on, which plugins ignore
                for (size_t i = 0; i + 1 < list.size();)
                {
                    if (list[i].isNoteOn && ! list[i + 1].isNoteOn && list[i + 1].outputSample - list[i].outputSample <= 1.5)
                        list.erase (list.begin() + (long) i, list.begin() + (long) i + 2);
                    else
                        ++i;
                }

                bool isOn = false;

                for (size_t i = 0; i < list.size();)
                {
                    if (! list[i].isNoteOn && ! isOn)
                    {
                        list.erase (list.begin() + (long) i);
                        continue;
                    }

                    isOn = list[i++].isNoteOn;
                }
            }

            return notes;
        };

        auto expected = byNote (expectedEvents);
        auto actual = byNote (actualEvents);
        std::set<int> noteNumbers;

        for (auto& [n, list] : expected)  noteNumbers.insert (n);
        for (auto& [n, list] : actual)    noteNumbers.insert (n);

        std::vector<juce::String> differences;

        for (auto n : noteNumbers)
        {
            auto& e = expected[n];
            auto& a = actual[n];

            for (size_t i = 0; i < std::max (e.size(), a.size()); ++i)
            {
                if (i < e.size() && i < a.size()
                    && e[i].isNoteOn == a[i].isNoteOn
                    && std::abs (e[i].outputSample - a[i].outputSample) <= e[i].tolerance)
                    continue;

                differences.push_back ("note " + juce::String (n) + ": expected "
                                       + (i < e.size() ? describe (e[i]) : juce::String ("nothing")) + ", got "
                                       + (i < a.size() ? describe (a[i]) : juce::String ("nothing")));

                if (juce::SystemStats::getEnvironmentVariable ("TE_TEMPO_CHANGE_FUZZ_DUMP", {}).isNotEmpty())
                {
                    std::cout << "note " << n << " expected:";

                    for (auto& ev : e)
                        std::cout << " " << (ev.isNoteOn ? "on@" : "off@") << ev.outputSample;

                    std::cout << "\nnote " << n << " got:";

                    for (auto& ev : a)
                        std::cout << " " << (ev.isNoteOn ? "on@" : "off@") << ev.outputSample;

                    std::cout << "\n";
                }

                break;
            }
        }

        return differences;
    }

    /** Checks no note is struck whilst it's already on. */
    inline std::vector<juce::String> checkNoteBalance (const Events& events)
    {
        std::vector<juce::String> failures;
        std::map<int, bool> noteIsOn;

        for (auto& e : events)
        {
            if (e.isNoteOn && noteIsOn[e.noteNumber])
                failures.push_back ("note " + juce::String (e.noteNumber) + " struck while on " + describe (e));

            noteIsOn[e.noteNumber] = e.isNoteOn;
        }

        return failures;
    }

    //==============================================================================
    /** A beat-based clip, as the beats of a timeline (the Edit's, or a launched clip's
        content) where it plays its source from its start.
    */
    struct AudioRegion
    {
        double start = 0.0, end = 0.0;
        double fadeSeconds = 0.0;
        std::optional<double> loopLength;
    };

    /** Checks each block's clip playhead reading, which says where a beat-based clip
        read its source from, is where the model says.
    */
    inline std::vector<juce::String> checkClipPlayheads (const Result& result, int track, int clipIndex, const AudioRegion& region,
                                                         const std::vector<SegmentMapping>& mappings)
    {
        std::vector<juce::String> failures;
        int numWrong = 0;

        for (auto& reading : result.clipPlayheads)
        {
            if (reading.track != track || reading.clip != clipIndex || reading.outputSample >= result.endOutputSample)
                continue;

            auto m = std::find_if (mappings.begin(), mappings.end(),
                                   [&] (auto& sm) { return sm.segment.outputStart <= reading.outputSample
                                                            && reading.outputSample < sm.segment.getOutputEnd(); });

            if (m == mappings.end())
                continue;

            const auto beat = m->beatAt (reading.outputSample);
            const auto oneSample = (m->getEnd() - m->getStart()) / (double) m->segment.numSamples;

            if (beat < region.start + oneSample || beat >= region.end - oneSample)
                continue;

            auto sourceBeat = beat - region.start;

            if (region.loopLength)
            {
                // On a loop point it could be read from either end
                const auto loops = sourceBeat / *region.loopLength;

                if (std::abs (loops - std::round (loops)) * *region.loopLength < oneSample)
                    continue;

                sourceBeat = std::fmod (sourceBeat, *region.loopLength);
            }

            const auto expected = sourceBeat * 60.0 / fileBpm;
            const auto difference = (reading.sourcePosition.inSeconds() - expected) * sampleRate;

            // A launched clip's start can be a sample or two out (as the clip launcher fuzz allows)
            if (std::abs (difference) <= 2.5)
                continue;

            if (numWrong++ == 0)
                failures.push_back ("at " + juce::String ((double) reading.outputSample / sampleRate, 5) + "s read source from "
                                    + juce::String (reading.sourcePosition.inSeconds(), 5) + "s, expected "
                                    + juce::String (expected, 5) + "s (beat " + juce::String (beat, 4) + ")");
        }

        if (numWrong > 1)
            failures.push_back (juce::String (numWrong) + " readings wrong");

        return failures;
    }

    /** Checks the output of a beat-based clip, a steady tone, keeps its level and steps
        its phase on steadily: anything faded, cut or jumped shows in one or the other.
        Frames close to a clip's or segment's edges are skipped, as they're faded or the
        time-stretcher is starting.
    */
    inline std::vector<juce::String> checkToneContinuity (const std::array<std::vector<float>, 2>& audio, const AudioRegion& region,
                                                          const std::vector<SegmentMapping>& mappings, int64_t endOutputSample)
    {
        // A time-stretcher takes a while to start a clip, more so the more it's stretched, and its
        // level wavers a little as the speed changes. A gap, fade or jump shows in the phase though
        constexpr double marginSeconds = 0.15;
        constexpr double levelTolerance = 0.25, phaseTolerance = 0.4;
        const auto phaseStep = juce::MathConstants<double>::twoPi * toneFrequency / sampleRate;

        std::vector<juce::String> failures;
        int64_t numBadFrames = 0;
        std::optional<juce::String> firstBadFrame;
        const auto marginFrames = (int64_t) (marginSeconds * sampleRate);

        // Joins the segments into runs the clip plays through without a jump
        struct Run { int64_t start, end; };
        std::vector<Run> runs;

        for (auto& m : mappings)
        {
            const auto b0 = std::max (m.getStart(), region.start);
            const auto b1 = std::min (m.getEnd(), region.end);

            if (b1 <= b0)
                continue;

            const auto start = (int64_t) std::ceil (m.toOutputSample (b0));
            const auto end = std::min ((int64_t) std::floor (m.toOutputSample (b1)), endOutputSample);
            const bool continuesRun = ! runs.empty() && runs.back().end == start && ! m.segment.startsAfterJump;

            if (continuesRun)
                runs.back().end = end;
            else
                runs.push_back ({ start, end });
        }

        // N.B. A time-stretched clip's output isn't seamless where it loops, as the time-stretcher is
        // reset there, whatever the tempo. That's not to do with tempo changes so it's skipped here
        std::vector<int64_t> loopPoints;

        if (region.loopLength)
            for (auto& m : mappings)
                for (auto loopPoint = region.start + *region.loopLength * std::ceil ((m.getStart() - region.start) / *region.loopLength);
                     loopPoint < m.getEnd(); loopPoint += *region.loopLength)
                    loopPoints.push_back ((int64_t) m.toOutputSample (loopPoint));

        const auto loopPointMarginFrames = marginFrames;

        auto isNearLoopPoint = [&] (int64_t f)
        {
            auto next = std::lower_bound (loopPoints.begin(), loopPoints.end(), f - loopPointMarginFrames);
            return next != loopPoints.end() && *next <= f + loopPointMarginFrames;
        };

        const auto fadeFrames = (int64_t) (region.fadeSeconds * sampleRate);

        for (auto run : runs)
        {
            const auto start = run.start + marginFrames + fadeFrames;
            const auto end = run.end - marginFrames - fadeFrames;

            for (auto f = std::max (start, (int64_t) 1); f < end && f < (int64_t) audio[0].size(); ++f)
            {
                if (isNearLoopPoint (f))
                    continue;

                const auto l = (double) audio[0][(size_t) f], r = (double) audio[1][(size_t) f];
                const auto level = std::hypot (l, r);
                auto phaseError = std::atan2 (l, r) - std::atan2 ((double) audio[0][(size_t) f - 1], (double) audio[1][(size_t) f - 1]) - phaseStep;
                phaseError -= juce::MathConstants<double>::twoPi * std::round (phaseError / juce::MathConstants<double>::twoPi);

                if (std::abs (level - toneLevel) <= toneLevel * levelTolerance && std::abs (phaseError) <= phaseTolerance)
                    continue;

                if (! firstBadFrame)
                    firstBadFrame = "at " + juce::String ((double) f / sampleRate, 5) + "s level " + juce::String (level, 4)
                                     + ", phase step error " + juce::String (phaseError, 3);

                ++numBadFrames;
            }
        }

        if (firstBadFrame)
            failures.push_back (juce::String (numBadFrames) + " frames wrong, first " + *firstBadFrame);

        if (auto at = juce::SystemStats::getEnvironmentVariable ("TE_TEMPO_CHANGE_FUZZ_DUMP_AUDIO", {}); at.isNotEmpty() && firstBadFrame)
        {
            const auto from = (int64_t) (at.getDoubleValue() * sampleRate);

            for (auto w = from; w < from + (int64_t) (0.3 * sampleRate) && w + 256 < (int64_t) audio[0].size(); w += 256)
            {
                double minLevel = 10.0, maxLevel = 0.0, maxPhaseError = 0.0;

                for (auto f = w; f < w + 256; ++f)
                {
                    const auto l = (double) audio[0][(size_t) f], r = (double) audio[1][(size_t) f];
                    const auto level = std::hypot (l, r);
                    auto phaseError = std::atan2 (l, r) - std::atan2 ((double) audio[0][(size_t) f - 1], (double) audio[1][(size_t) f - 1]) - phaseStep;
                    phaseError -= juce::MathConstants<double>::twoPi * std::round (phaseError / juce::MathConstants<double>::twoPi);
                    minLevel = std::min (minLevel, level);
                    maxLevel = std::max (maxLevel, level);
                    maxPhaseError = std::max (maxPhaseError, std::abs (phaseError));
                }

                std::cout << "  " << juce::String ((double) w / sampleRate, 4) << "s level " << juce::String (minLevel, 3) << "-"
                          << juce::String (maxLevel, 3) << " phase error " << juce::String (maxPhaseError, 3) << "\n";
            }
        }

        return failures;
    }

    //==============================================================================
    inline std::vector<SegmentMapping> getEditMappings (const std::vector<Segment>& segments, const Result& result)
    {
        std::vector<SegmentMapping> mappings;

        for (auto& s : segments)
            mappings.push_back ({ s, &result.maps[s.map].map, s.beatOffset });

        return mappings;
    }

    /** Maps the segments from the launch to the launched clips' content beats, which
        carry on through the Edit's loop and tempo changes.
    */
    inline std::vector<SegmentMapping> getLaunchedMappings (const std::vector<Segment>& segments, const Result& result)
    {
        std::vector<SegmentMapping> mappings;
        std::optional<double> launchMonotonic;

        for (auto& s : segments)
        {
            if (s.outputStart < result.launchOutputSample)
                continue;

            if (! launchMonotonic)
                launchMonotonic = s.monotonicStart;

            SegmentMapping m { s, &result.maps[s.map].map, 0.0 };
            m.contentOffset = (s.monotonicStart - *launchMonotonic) - m.getEditBeatAtStart();

            // Launched clips don't jump at the Edit's loop
            m.segment.startsAfterJump = mappings.empty();
            m.segment.endsAtJump = false;
            mappings.push_back (m);
        }

        if (! mappings.empty())
            mappings.back().segment.endsAtJump = true;

        return mappings;
    }

    /** Checks every tempo map the Edit was played with is one the test made, i.e. the model
        knows about every change.
    */
    inline std::vector<juce::String> checkTempoMaps (const Result& result, const std::map<size_t, tempo::Sequence>& probeMaps)
    {
        std::vector<juce::String> failures;
        std::set<size_t> known;

        for (auto& m : result.maps)
            known.insert (m.map.hash());

        for (auto& section : result.sections)
        {
            if (section.outputStart >= result.endOutputSample || known.contains (section.tempoMapHash))
                continue;

            juce::String description;

            if (auto m = probeMaps.find (section.tempoMapHash); m != probeMaps.end())
                for (double beat = 0.0; beat <= 64.0; beat += 8.0)
                    description << " " << juce::String (m->second.toTime (BeatPosition::fromBeats (beat)).inSeconds(), 4);

            failures.push_back ("at " + juce::String ((double) section.outputStart / sampleRate, 5)
                                + "s an unknown tempo map was used, beats 0-64 by 8 at" + description);
            known.insert (section.tempoMapHash);
        }

        return failures;
    }

    /** Checks the playhead's loop is on the loop's beats, to the nearest sample or so. */
    inline std::vector<juce::String> checkLoopRanges (const Scenario& sc, const Result& result)
    {
        std::vector<juce::String> failures;
        int numWrong = 0;

        for (auto [blockStart, range] : result.loopRanges)
        {
            if (blockStart >= result.endOutputSample)
                continue;

            size_t mapIndex = 0;

            for (size_t i = 0; i < result.maps.size(); ++i)
                if (result.maps[i].outputSample <= blockStart)
                    mapIndex = i;

            auto& m = result.maps[mapIndex].map;
            const auto start = toSamples (m.toTime (BeatPosition::fromBeats (sc.loopStart)), sampleRate);
            const auto end = toSamples (m.toTime (BeatPosition::fromBeats (sc.loopEnd)), sampleRate);

            if (std::abs (range.getStart() - start) <= 1 && std::abs (range.getEnd() - end) <= 1)
                continue;

            if (numWrong++ == 0)
                failures.push_back ("at " + juce::String ((double) blockStart / sampleRate, 5) + "s the loop was "
                                    + juce::String (range.getStart()) + "-" + juce::String (range.getEnd()) + ", expected "
                                    + juce::String (start) + "-" + juce::String (end));
        }

        if (numWrong > 1)
            failures.push_back (juce::String (numWrong) + " blocks wrong");

        return failures;
    }

    /** Checks each section processed starts where the model says. */
    inline std::vector<juce::String> checkSections (const Result& result, const std::vector<Segment>& segments)
    {
        std::vector<juce::String> failures;
        int numWrong = 0;

        for (auto& section : result.sections)
        {
            if (! section.isPlaying || section.numSamples == 0 || section.outputStart >= result.endOutputSample)
                continue;

            auto s = std::find_if (segments.begin(), segments.end(),
                                   [&] (auto& seg) { return seg.outputStart <= section.outputStart && section.outputStart < seg.getOutputEnd(); });

            if (s == segments.end())
                continue;

            const auto expected = s->editStart + (section.outputStart - s->outputStart);
            const auto actual = toSamples (section.editTime.getStart(), sampleRate);

            if (std::abs (actual - expected) <= 1)
                continue;

            if (numWrong++ == 0)
            {
                failures.push_back ("at " + juce::String ((double) section.outputStart / sampleRate, 5) + "s the playhead was at "
                                    + juce::String (section.editTime.getStart().inSeconds(), 5) + "s, expected "
                                    + juce::String ((double) expected / sampleRate, 5) + "s");

                if (juce::SystemStats::getEnvironmentVariable ("TE_TEMPO_CHANGE_FUZZ_DUMP", {}).isNotEmpty())
                {
                    const auto from = section.outputStart - 6 * blockSize, to = section.outputStart + 3 * blockSize;
                    std::cout << "sections:\n";

                    for (auto& sec : result.sections)
                        if (sec.outputStart >= from && sec.outputStart <= to)
                            std::cout << "  out " << sec.outputStart << " n " << sec.numSamples << " edit " << toSamples (sec.editTime.getStart(), sampleRate)
                                      << " map " << sec.tempoMapHash << "\n";

                    std::cout << "model:\n";

                    for (auto& seg : segments)
                        if (seg.outputStart >= from && seg.outputStart <= to)
                            std::cout << "  out " << seg.outputStart << " n " << seg.numSamples << " edit " << seg.editStart
                                      << " map " << seg.map << " hash " << result.maps[seg.map].map.hash()
                                      << " beat " << result.maps[seg.map].map.toBeats (TimePosition::fromSamples (seg.editStart, sampleRate)).inBeats()
                                      << (seg.startsAfterJump ? " jumped-in" : "") << (seg.endsAtJump ? " jumps-out" : "") << "\n";

                    std::cout << "map changes:\n";

                    for (size_t i = 0; i < result.maps.size(); ++i)
                        if (result.maps[i].outputSample >= from && result.maps[i].outputSample <= to)
                            std::cout << "  " << i << " at out " << result.maps[i].outputSample << " hash " << result.maps[i].map.hash() << "\n";
                }
            }
        }

        if (numWrong > 1)
            failures.push_back (juce::String (numWrong) + " sections wrong");

        return failures;
    }

    //==============================================================================
    inline std::vector<int> getSeeds()
    {
        if (auto seed = juce::SystemStats::getEnvironmentVariable ("TE_TEMPO_CHANGE_FUZZ_SEED", {}); seed.isNotEmpty())
            return { seed.getIntValue() };

        const auto numSeeds = juce::SystemStats::getEnvironmentVariable ("TE_TEMPO_CHANGE_FUZZ_SEEDS", "16").getIntValue();
        std::vector<int> seeds;

        for (int i = 1; i <= numSeeds; ++i)
            seeds.push_back (i);

        // Seeds that have found bugs, so they're always checked
        for (int seed : { 21, 30, 34, 37, 40, 41, 43, 44, 45, 49, 57, 59 })
            if (seed > numSeeds)
                seeds.push_back (seed);

        return seeds;
    }

    /** Collects a seed's failures to report them together. */
    struct Report
    {
        void add (const juce::String& check, const std::vector<juce::String>& failures)
        {
            if (failures.empty())
                return;

            message << "  " << check << ":\n";

            for (size_t i = 0; i < std::min<size_t> (failures.size(), 6); ++i)
                message << "    " << failures[i] << "\n";

            if (failures.size() > 6)
                message << "    ... and " << (int) (failures.size() - 6) << " more\n";
        }

        void fail (int seed, const Scenario& sc, Variant v, const Result& result)
        {
            if (message.isNotEmpty())
                FAIL_CHECK (("seed " + juce::String (seed) + ":\n" + message + describe (sc, v, result.stepTimes)).toStdString());
        }

        juce::String message;
    };


    inline void checkVariant (Variant variant)
    {
        auto& engine = *Engine::getEngines()[0];

        for (auto seed : getSeeds())
        {
            const auto sc = createScenario (seed, variant);
            const auto result = run (engine, sc, variant);
            const auto segments = createModel (variant, result);
            REQUIRE (! segments.empty());

            const auto editMappings = getEditMappings (segments, result);
            const auto launchedMappings = getLaunchedMappings (segments, result);
            Report rep;

            rep.add ("playhead", checkSections (result, segments));
            rep.add ("loop", checkLoopRanges (sc, result));
            rep.add ("tempo maps", checkTempoMaps (result, result.probeTempoMaps));
            rep.add ("clip positions", result.clipsOffTheirBeats);

            std::vector<Note> notes;

            for (auto& c : sc.midiClips)
                for (auto n : getNotes (c))
                    notes.push_back (n);

            for (auto t : { midiTrack, beatMidiTrack })
            {
                const auto name = t == midiTrack ? juce::String ("arrangement MIDI") : juce::String ("beat-based arrangement MIDI");

                if (auto at = juce::SystemStats::getEnvironmentVariable ("TE_TEMPO_CHANGE_FUZZ_DUMP_AT", {}); at.isNotEmpty())
                {
                    const auto o = at.getLargeIntValue();

                    for (auto& sec : result.sections)
                        if (sec.outputStart >= o - blockSize && sec.outputStart <= o + blockSize)
                        {
                            auto& m = result.probeTempoMaps.at (sec.tempoMapHash);
                            std::cout << "  section out " << sec.outputStart << " n " << sec.numSamples
                                      << " beats " << juce::String (m.toBeats (sec.editTime.getStart()).inBeats(), 8)
                                      << " - " << juce::String (m.toBeats (sec.editTime.getStart() + TimeDuration::fromSamples (sec.numSamples, sampleRate)).inBeats(), 8) << "\n";
                        }

                    for (auto& ev : result.events[(size_t) t])
                        if (ev.outputSample >= (double) (o - blockSize) && ev.outputSample <= (double) (o + blockSize))
                            std::cout << "  track " << t << " event " << (ev.isNoteOn ? "on " : "off ") << ev.noteNumber << " at " << ev.outputSample << "\n";
                }
                rep.add (name, compareEvents (getExpectedEvents (notes, editMappings), result.events[(size_t) t],
                                                               result.endOutputSample));
                rep.add (name + " note balance", checkNoteBalance (result.events[(size_t) t]));
            }

            {
                MidiClipSpec launched = sc.launcherMidi;
                launched.start = 0.0;
                launched.length = launchedMappings.empty() ? 0.0 : std::ceil (launchedMappings.back().getEnd()) + 1.0;
                launched.loopLength = sc.launcherMidi.length;
                rep.add ("launched MIDI",
                        compareEvents (getExpectedEvents (getNotes (launched), launchedMappings), result.events[launcherMidiTrack],
                                       result.endOutputSample));
                rep.add ("launched MIDI note balance", checkNoteBalance (result.events[launcherMidiTrack]));
            }

            for (size_t c = 0; c < sc.audioClips.size(); ++c)
            {
                const auto& spec = sc.audioClips[c];
                const AudioRegion region { spec.start, spec.start + spec.length, spec.fadeSeconds, std::nullopt };
                const auto name = "audio clip " + juce::String ((int) c);
                rep.add (name + " source position", checkClipPlayheads (result, audioTrack, (int) c, region, editMappings));
                rep.add (name + " output", checkToneContinuity (result.audio[audioTrack], region, editMappings,
                                                                                 result.endOutputSample));
            }

            {
                const AudioRegion region { 0.0, 1.0e6, 0.0, sc.launcherAudioLength };
                rep.add ("launched audio source position",
                        checkClipPlayheads (result, launcherAudioTrack, (int) sc.audioClips.size(), region, launchedMappings));
                rep.add ("launched audio output",
                        checkToneContinuity (result.audio[launcherAudioTrack], region, launchedMappings, result.endOutputSample));
            }

            rep.fail (seed, sc, variant, result);
        }
    }

    //==============================================================================
    /** An Edit with a probe on each track and a player, for the regression tests. */
    struct ProbedEdit
    {
        ProbedEdit (Engine& e, int numTracks = 1)
            : player (e, getPlayerParams()),
              edit (test_utilities::createTestEdit (e, numTracks, Edit::EditRole::forEditing))
        {
            for (auto t : getAudioTracks (*edit))
                probes.push_back (&addTempoProbePlugin (*t));
        }

        AudioTrack& getTrack (int index)        { return *getAudioTracks (*edit)[index]; }
        TempoSequence& getTempoSequence()       { return edit->tempoSequence; }
        TransportControl& getTransport()        { return edit->getTransport(); }

        void process (TimeDuration duration)
        {
            for (auto numBlocks = (int) std::ceil (duration.inSeconds() * sampleRate / blockSize); --numBlocks >= 0;)
                player.process (blockSize);
        }

        /** Returns the output time of an Edit time, as it played. */
        double getOutputSeconds (int track, TimePosition editTime) const
        {
            for (auto& s : probes[(size_t) track]->sections)
            {
                const auto sectionLength = TimeDuration::fromSamples (s.numSamples, sampleRate);

                if (s.isPlaying && s.editTime.getStart() <= editTime && editTime < s.editTime.getStart() + sectionLength)
                    return ((double) s.outputStart + (editTime - s.editTime.getStart()).inSeconds() * sampleRate) / sampleRate;
            }

            return -1.0;
        }

        /** Returns the Edit beat an output sample played, with the tempo map it was played with. */
        std::optional<double> getEditBeat (int track, double outputSample) const
        {
            auto& probe = *probes[(size_t) track];

            for (auto& s : probe.sections)
                if (s.isPlaying && (double) s.outputStart <= outputSample && outputSample < (double) (s.outputStart + s.numSamples))
                    return probe.tempoMaps.at (s.tempoMapHash)
                                .toBeats (s.editTime.getStart() + TimeDuration::fromSeconds ((outputSample - (double) s.outputStart) / sampleRate))
                                .inBeats();

            return std::nullopt;
        }

        std::vector<Event> getNoteOns (int track, int noteNumber) const
        {
            std::vector<Event> ons;

            for (auto& e : probes[(size_t) track]->events)
                if (e.isNoteOn && e.noteNumber == noteNumber)
                    ons.push_back (e);

            return ons;
        }

        /** Sets a tempo the way TempoCurveEditor does, keeping the Edit's positions on their beats. */
        void setBpm (int index, double bpm)
        {
            auto tempo = getTempoSequence().getTempo (index);
            tempo->set (tempo->getStartBeat(), bpm, tempo->getCurve(), true);
        }

        test_utilities::EnginePlayer player;
        std::vector<std::unique_ptr<MemoryAudioFile>> files;    // N.B. declared before the Edit to outlive it
        std::unique_ptr<Edit> edit;
        std::vector<TempoProbePlugin*> probes;
    };

    /** Returns the RMS level of a channel of a probe's audio over a range of output seconds. */
    inline double getLevel (const TempoProbePlugin& probe, double startSeconds, double endSeconds)
    {
        auto& audio = probe.audio[0];
        const auto start = (size_t) (startSeconds * sampleRate), end = std::min (audio.size(), (size_t) (endSeconds * sampleRate));
        double sum = 0.0;

        for (auto i = start; i < end; ++i)
            sum += (double) audio[i] * audio[i];

        return end > start ? std::sqrt (sum / (double) (end - start)) : 0.0;
    }
}

//==============================================================================
//==============================================================================
TEST_SUITE ("tracktion_engine")
{
    using namespace tempo_change_tests;

    //==============================================================================
    TEST_CASE ("Tempo change: a block is split at a tempo change in it")
    {
        auto& engine = *Engine::getEngines()[0];
        ProbedEdit e (engine);
        auto& ts = e.getTempoSequence();

        // 60bpm then 180bpm from beat 2, which is 2s, 136 samples in to a block, and another
        // change after it, so the next change after the block isn't the one in it
        ts.getTempo (0)->set (0_bp, 60.0, 1.0f, false);
        ts.insertTempo (2_bp, 180.0, 1.0f);
        ts.insertTempo (8_bp, 120.0, 1.0f);
        ts.updateTempoData();

        // A note just after the change, in the same block
        auto clip = insertMIDIClip (e.getTrack (0), { 0_tp, 4_tp });
        clip->setUsesProxy (false);
        const auto noteBeat = BeatPosition::fromBeats (2.02);
        clip->getSequence().addNote (60, noteBeat, 0.5_bd, 100, 0, nullptr);

        e.getTransport().play (false);
        e.process (3s);

        const auto ons = e.getNoteOns (0, 60);
        REQUIRE (ons.size() == 1);
        CHECK (std::abs (ons[0].outputSample - e.getOutputSeconds (0, ts.toTime (noteBeat)) * sampleRate) <= 1.5);
    }

    TEST_CASE ("Tempo change: a clip far along the timeline plays straight after a big tempo change")
    {
        auto& engine = *Engine::getEngines()[0];
        ProbedEdit e (engine);
        auto& ts = e.getTempoSequence();
        ts.getTempo (0)->set (0_bp, 120.0, 1.0f, false);
        ts.updateTempoData();

        // A clip at beat 64, 32s in at 120bpm, played from beat 60
        auto clip = insertMIDIClip (e.getTrack (0), { ts.toTime (64_bp), ts.toTime (72_bp) });
        clip->setUsesProxy (false);
        clip->getSequence().addNote (60, 1_bp, 1_bd, 100, 0, nullptr);

        e.getTransport().setPosition (ts.toTime (60_bp));
        e.getTransport().play (false);
        e.process (0.2s);

        // Halving the tempo moves the clip to 64s, without a graph rebuild to catch up
        e.setBpm (0, 60.0);
        e.process (6s);

        const auto ons = e.getNoteOns (0, 60);
        REQUIRE (ons.size() == 1);
        const auto beat = e.getEditBeat (0, ons[0].outputSample);
        REQUIRE (beat);
        CHECK (*beat == doctest::Approx (65.0).epsilon (0.00001));
    }

    TEST_CASE ("Tempo change: arrangement MIDI clips stay on the beat")
    {
        auto& engine = *Engine::getEngines()[0];

        for (bool beatBased : { false, true })
        {
            for (bool rebuild : { false, true })
            {
                CAPTURE (beatBased);
                CAPTURE (rebuild);
                ProbedEdit e (engine);
                auto& ts = e.getTempoSequence();
                ts.getTempo (0)->set (0_bp, 60.0, 1.0f, false);
                ts.updateTempoData();

                // A note on every beat from 2 to 11
                auto clip = insertMIDIClip (e.getTrack (0), { 0_tp, ts.toTime (12_bp) });
                clip->setUsesProxy (! beatBased);

                for (int beat = 2; beat < 12; ++beat)
                    clip->getSequence().addNote (60, BeatPosition::fromBeats (beat), 0.5_bd, 100, 0, nullptr);

                e.getTransport().play (false);
                e.process (1.5s);

                // Speed up then slow down, between the notes and over them
                e.setBpm (0, 120.0);

                if (rebuild)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (5);

                e.process (1.6s);
                e.setBpm (0, 90.0);

                if (rebuild)
                    juce::MessageManager::getInstance()->runDispatchLoopUntil (5);

                e.process (6s);

                const auto ons = e.getNoteOns (0, 60);
                CHECK (ons.size() == 10);

                for (size_t i = 0; i < ons.size(); ++i)
                {
                    CAPTURE (i);
                    const auto beat = e.getEditBeat (0, ons[i].outputSample);
                    REQUIRE (beat);
                    CHECK (*beat == doctest::Approx (2.0 + (double) i).epsilon (0.00001));
                }

                CHECK (checkNoteBalance (e.probes[0]->events).empty());
            }
        }
    }

    TEST_CASE ("Tempo change: a beat-based audio clip with fades isn't cut short at its old end")
    {
        auto& engine = *Engine::getEngines()[0];
        ProbedEdit e (engine);
        auto& ts = e.getTempoSequence();
        ts.getTempo (0)->set (0_bp, 120.0, 1.0f, false);
        ts.updateTempoData();

        // Beats 0-8, which is 0-4s at 120bpm
        e.files.push_back (createToneFile (engine, 8.0));
        auto clip = insertWaveClip (e.getTrack (0), {}, e.files.back()->getFile(), { { 0_tp, 4_tp } }, DeleteExistingClips::no);
        makeBeatBased (*clip, 8.0);
        clip->setPosition ({ { 0_tp, 4_tp } });
        clip->setFadeIn (0.01s);
        clip->setFadeOut (0.01s);

        e.getTransport().play (false);
        e.process (1s);

        // Halving the tempo at beat 2 moves the clip's end to 7s
        e.setBpm (0, 60.0);
        e.process (6.5s);

        const auto start = e.getOutputSeconds (0, 4_tp);
        CHECK (getLevel (*e.probes[0], start + 0.5, start + 2.5) == doctest::Approx (toneLevel / std::sqrt (2.0)).epsilon (0.1));
    }

    TEST_CASE ("Tempo change: dragging a tempo in small steps keeps clips on their beats")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine, 1, Edit::EditRole::forEditing);
        auto& ts = edit->tempoSequence;
        ts.getTempo (0)->set (0_bp, 120.0, 1.0f, false);
        ts.insertTempo (8_bp, 100.0, 0.3f);    // A curve to beat 24
        ts.insertTempo (24_bp, 110.0, 1.0f);
        ts.updateTempoData();

        auto& track = *getAudioTracks (*edit)[0];
        auto midiClip = insertMIDIClip (track, { ts.toTime (2_bp), ts.toTime (6_bp) });
        auto curvedClip = insertMIDIClip (track, { ts.toTime (10_bp), ts.toTime (19_bp) });

        // As dragging the tempo curve does, a little at a time, so each step moves the clips only a bit
        for (int i = 1; i <= 100; ++i)
        {
            auto tempo = ts.getTempo (0);
            tempo->set (tempo->getStartBeat(), 120.0 + i * 0.01, tempo->getCurve(), true);
        }

        for (int i = 1; i <= 100; ++i)
        {
            auto tempo = ts.getTempo (1);
            tempo->set (tempo->getStartBeat(), 100.0 - i * 0.02, tempo->getCurve(), true);
        }

        CHECK (ts.toBeats (midiClip->getEditTimeRange()).getStart().inBeats() == doctest::Approx (2.0).epsilon (1.0e-9));
        CHECK (ts.toBeats (midiClip->getEditTimeRange()).getEnd().inBeats() == doctest::Approx (6.0).epsilon (1.0e-9));
        CHECK (ts.toBeats (curvedClip->getEditTimeRange()).getStart().inBeats() == doctest::Approx (10.0).epsilon (1.0e-9));
        CHECK (ts.toBeats (curvedClip->getEditTimeRange()).getEnd().inBeats() == doctest::Approx (19.0).epsilon (1.0e-9));
    }

    TEST_CASE ("Tempo change: notes where the playhead's put back on the beat are played once each")
    {
        auto& engine = *Engine::getEngines()[0];

        // The playhead can only be put on a whole sample, so these put it a little either side of the beat
        for (double bpm : { 61.0, 77.7, 103.1, 140.3, 199.9 })
        {
            for (bool beatBased : { false, true })
            {
                CAPTURE (bpm);
                CAPTURE (beatBased);
                ProbedEdit e (engine);
                auto& ts = e.getTempoSequence();
                ts.getTempo (0)->set (0_bp, 120.0, 1.0f, false);
                ts.updateTempoData();

                auto clip = insertMIDIClip (e.getTrack (0), { 0_tp, ts.toTime (16_bp) });
                clip->setUsesProxy (! beatBased);

                auto& transport = e.getTransport();
                transport.play (false);
                e.process (1s);

                // Notes over the next few samples from the beat the playhead's got to
                const auto beat = transport.getCurrentPlaybackContext()->getSyncPoint()->beat.inBeats();
                const auto oneSample = 2.0 / sampleRate;

                for (int i = 0; i < 24; ++i)
                    clip->getSequence().addNote (30 + i, BeatPosition::fromBeats (beat + oneSample * i / 8.0), 0.25_bd, 100, 0, nullptr);

                transport.ensureContextAllocated (true);
                e.setBpm (0, bpm);
                e.process (2s);

                for (int i = 0; i < 24; ++i)
                {
                    CAPTURE (i);
                    CHECK (e.getNoteOns (0, 30 + i).size() == 1);
                }

                CHECK (checkNoteBalance (e.probes[0]->events).empty());
            }
        }
    }

    TEST_CASE ("Tempo change: the loop stays on its beats to the sample, from a fast tempo to a slow one")
    {
        auto& engine = *Engine::getEngines()[0];

        for (double loopEnd : { 7.0, 9.0, 13.0, 15.0 })
        {
            CAPTURE (loopEnd);
            ProbedEdit e (engine);
            auto& ts = e.getTempoSequence();
            ts.getTempo (0)->set (0_bp, 197.0, 1.0f, false);
            ts.updateTempoData();

            auto& transport = e.getTransport();
            transport.setLoopRange ({ ts.toTime (3_bp), ts.toTime (BeatPosition::fromBeats (loopEnd)) });
            transport.looping = true;
            transport.setPosition (ts.toTime (4_bp));
            transport.play (false);
            e.process (0.1s);

            // A quarter the tempo makes a sample out at the start four at the end
            for (double bpm = 190.0; bpm >= 49.0; bpm -= 7.0)
            {
                e.setBpm (0, bpm);
                e.process (0.02s);
            }

            const auto loop = transport.getCurrentPlaybackContext()->getNodePlayHead()->getLoopRange();
            CHECK (loop.getStart() == toSamples (ts.toTime (3_bp), sampleRate));
            CHECK (loop.getEnd() == toSamples (ts.toTime (BeatPosition::fromBeats (loopEnd)), sampleRate));
        }
    }

    TEST_CASE ("Tempo change: step clips stay on the beat")
    {
        auto& engine = *Engine::getEngines()[0];
        ProbedEdit e (engine);
        auto& ts = e.getTempoSequence();
        ts.getTempo (0)->set (0_bp, 60.0, 1.0f, false);
        ts.updateTempoData();

        auto clip = dynamic_cast<StepClip*> (insertNewClip (e.getTrack (0), TrackItem::Type::step, { 0_tp, 8_tp }));
        REQUIRE (clip != nullptr);
        REQUIRE (! clip->getChannels().isEmpty());
        auto pattern = clip->getPattern (0);
        pattern.setNoteLength (0.25_bd);

        // A note on each beat
        for (int step = 0; step < pattern.getNumNotes(); step += 4)
            pattern.setNote (0, step, true);

        const auto noteNumber = clip->getChannels()[0]->noteNumber.get();

        e.getTransport().play (false);
        e.process (1.5s);
        e.setBpm (0, 120.0);
        e.process (1.6s);
        e.setBpm (0, 90.0);
        e.process (4s);

        const auto ons = e.getNoteOns (0, noteNumber);
        REQUIRE (ons.size() >= 6);

        for (size_t i = 0; i < ons.size(); ++i)
        {
            CAPTURE (i);
            const auto beat = e.getEditBeat (0, ons[i].outputSample);
            REQUIRE (beat);
            CHECK (*beat == doctest::Approx (std::round (*beat)).epsilon (0.00001));
        }

        CHECK (checkNoteBalance (e.probes[0]->events).empty());
    }

    TEST_CASE ("PlayHead: a loop range change that leaves the playhead where it is isn't a jump")
    {
        graph::PlayHead playHead;
        graph::PlayHeadState playHeadState (playHead);
        juce::Range<int64_t> referenceRange (0, 0);

        auto processBlock = [&]
        {
            referenceRange = juce::Range<int64_t>::withStartAndLength (referenceRange.getEnd(), blockSize);
            playHead.setReferenceSampleRange (referenceRange);
            playHeadState.update (referenceRange);
        };

        processBlock();
        playHead.play ({ 0, 4410 }, true);

        // Past the end a couple of times
        for (int i = 0; i < 20; ++i)
            processBlock();

        // As the transport does when the loop's moved with its beats after a tempo change
        // (and the playhead's already done it). Being a sample different still mustn't move the playhead
        const auto position = playHead.getPosition();
        playHead.setLoopRange (true, { 0, 4411 });
        CHECK (playHead.getPosition() == position);

        processBlock();
        CHECK (playHead.getPosition() == position + blockSize);
        CHECK (playHeadState.isContiguousWithPreviousBlock());
    }

    TEST_CASE ("Tempo change fuzz: playback stays on the beat")
    {
        checkVariant ({ .looping = false, .rebuilds = false });
    }

    TEST_CASE ("Tempo change fuzz: playback stays on the beat when looping")
    {
        checkVariant ({ .looping = true, .rebuilds = false });
    }

    TEST_CASE ("Tempo change fuzz: playback stays on the beat with graph rebuilds")
    {
        checkVariant ({ .looping = false, .rebuilds = true });
    }

    TEST_CASE ("Tempo change fuzz: playback stays on the beat when looping with graph rebuilds")
    {
        checkVariant ({ .looping = true, .rebuilds = true });
    }
}

} // namespace tracktion::inline engine

#endif //TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_PLAYBACK && ENGINE_UNIT_TESTS_CLIP_LAUNCHER
