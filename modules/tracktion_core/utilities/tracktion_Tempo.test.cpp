/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS_TIME

#include "tracktion_Tempo.h"
#include "../../3rd_party/choc/text/choc_StringUtilities.h"

namespace tracktion { inline namespace core
{

//==============================================================================
//==============================================================================
class TempoTests    : public juce::UnitTest
{
public:
    TempoTests()
        : juce::UnitTest ("Tempo", "tracktion_core")
    {
    }
    
    void runTest() override
    {
        using namespace Tempo;

        beginTest ("60bpm 4/4");
        {
            Sequence seq ({{ BeatPosition(), 60.0, 0.0f }},
                          {{ BeatPosition(), 4, 4, false }});

            expect (seq, {}, {});
            expect (seq, BeatPosition::fromBeats (1), 1s);
            expect (seq, BeatPosition::fromBeats (4), 4s);
            expect (seq, BeatPosition::fromBeats (60), 1min);
            expect (seq, BeatPosition::fromBeats (120), 2min);
        }

        beginTest ("60bpm 4/8");
        {
            Sequence seq ({{ BeatPosition(), 60.0, 0.0f }},
                          {{ BeatPosition(), 4, 8, false }});

            expect (seq, {}, {});
            expect (seq, BeatPosition::fromBeats (1), 0.5s);
            expect (seq, BeatPosition::fromBeats (4), 2s);
            expect (seq, BeatPosition::fromBeats (120), 1min);
            expect (seq, BeatPosition::fromBeats (240), 2min);
        }

        beginTest ("120bpm 4/4");
        {
            Sequence seq ({{ BeatPosition(), 120.0, 0.0f }},
                          {{ BeatPosition(), 4, 4, false }});

            expect (seq, {}, {});
            expect (seq, BeatPosition::fromBeats (1), 0.5s);
            expect (seq, BeatPosition::fromBeats (4), 2s);
            expect (seq, BeatPosition::fromBeats (8), 4s);
            expect (seq, BeatPosition::fromBeats (120), 1min);
            expect (seq, BeatPosition::fromBeats (240), 2min);
        }

        beginTest ("120bpm 8/8");
        {
            Sequence seq ({{ BeatPosition(), 120.0, 0.0f }},
                          {{ BeatPosition(), 8, 8, false }});

            expect (seq, {}, {});
            expect (seq, BeatPosition::fromBeats (1), 0.25s);
            expect (seq, BeatPosition::fromBeats (4), 1s);
            expect (seq, BeatPosition::fromBeats (8), 2s);
            expect (seq, BeatPosition::fromBeats (120), 0.5min);
            expect (seq, BeatPosition::fromBeats (240), 1min);
        }

        beginTest ("0b: 120bpm 4/4 c=0, 4b: 60bpm 4/4");
        {
            Sequence seq ({{ BeatPosition(), 120.0, 0.0f },
                           { BeatPosition::fromBeats (4), 60.0, 0.0f } },
                          {{ BeatPosition(), 4, 4, false }});

            expect (seq, {}, {});
            expect (seq, BeatPosition::fromBeats (1), 0.525s);
            expect (seq, BeatPosition::fromBeats (4), 2.711s);
            expect (seq, BeatPosition::fromBeats (8), 6.711s);
        }

        beginTest ("0b: 120bpm 4/4 c=-1, 4b: 60bpm 4/4");
        {
            Sequence seq ({{ BeatPosition(), 120.0, -1.0f },
                           { BeatPosition::fromBeats (4), 60.0, 0.0f } },
                          {{ BeatPosition(), 4, 4, false }});

            expect (seq, {}, {});
            expect (seq, BeatPosition::fromBeats (1), 1s);
            expect (seq, BeatPosition::fromBeats (4), 4s);
            expect (seq, BeatPosition::fromBeats (60), 1min);
            expect (seq, BeatPosition::fromBeats (120), 2min);
        }
    }

    void expect (const Tempo::Sequence& seq, BeatPosition b, TimePosition t)
    {
        {
            const auto beats = seq.toBeats (t);
            const auto diff = std::abs (beats.inBeats() - b.inBeats());
            juce::UnitTest::expect (diff <= 0.001,
                                    juce::String ("Converting TTTs to beats. Expected EEE beats, calculated CCC beats")
                                        .replace ("TTT", juce::String (t.inSeconds())).replace ("EEE", juce::String (b.inBeats())).replace ("CCC", juce::String (beats.inBeats())));
        }

        {
            const auto time = seq.toTime (b);
            const auto diff = std::abs (time.inSeconds() - t.inSeconds());
            juce::UnitTest::expect (diff <= 0.001,
                                    juce::String ("Converting BBB beats to time. Expected EEEs, calculated CCCs")
                                        .replace ("BBB", juce::String (b.inBeats())).replace ("EEE", juce::String (t.inSeconds())).replace ("CCC", juce::String (time.inSeconds())));
        }
    }
};

static TempoTests tempoTests;

}} // namespace tracktion

#endif //TRACKTION_UNIT_TESTS_TIME


//==============================================================================
//==============================================================================
#if TRACKTION_BENCHMARKS

namespace tracktion { inline namespace engine
{

//==============================================================================
//==============================================================================
class TempoBenchmarks   : public juce::UnitTest
{
public:
    TempoBenchmarks()
        : juce::UnitTest ("Tempo", "tracktion_core")
    {}

    void runTest() override
    {
        beginTest ("Benchmark: Tempo conversion");
        {
            benchmarkTempos (0.0f);
            benchmarkTempos (-1.0f);
            benchmarkTempos (1.0f);
            benchmarkTempos (-0.5f);
            benchmarkTempos (0.5f);
        }
    }

    void benchmarkTempos (float curve)
    {
        // Create a sequence of T tempos and time sigs over N beats
        // Convert from beats to time:
        //  - At the first minute
        //  - The last minute
        //  - Randomly throughout

        constexpr int numIterations = 100;
        constexpr int numBeats = 100;
        constexpr int numConversions = 10'000;
        juce::Random r (4200);

        using choc::text::replace;
        Benchmark bm1 (createBenchmarkDescription ("Tempo", replace ("Create sequence 4/4, curve = CCC", "CCC", std::to_string (curve)), "100 tempos, every beat"));
        Benchmark bm2 (createBenchmarkDescription ("Tempo", "Convert 10'000", replace ("100'000, first quarter beats (4/4, curve = CCC)", "CCC", std::to_string (curve))));
        Benchmark bm3 (createBenchmarkDescription ("Tempo", "Convert 10'000", replace ("100'000, last quarter beats (4/4, curve = CCC)", "CCC", std::to_string (curve))));
        Benchmark bm4 (createBenchmarkDescription ("Tempo", "Convert 10'000", replace ("100'000, random beats (4/4, curve = CCC)", "CCC", std::to_string (curve))));

        // Create the tempos first as we don't want to profile that
        std::vector<Tempo::Setting> tempos;

        for (int b = 0; b < numBeats; ++b)
            tempos.push_back ({ BeatPosition::fromBeats (b), (double) r.nextInt ({ 60, 180 }), 0.0f });

        for (int i = 0; i < numIterations; ++i)
        {
            // Create a copy outside of the profile loop
            auto tempTempos = tempos;
            std::vector<TimeSig::Setting> timeSigs {{ BeatPosition(), 4, 4, false }};

            // Create the sequence
            bm1.start();
            Tempo::Sequence seq (std::move (tempTempos), timeSigs);
            bm1.stop();

            // Profile the first 25 beats
            for (int c = 0; c < numConversions; ++c)
            {
                const auto b = BeatPosition::fromBeats (r.nextInt (numBeats / 4));

                bm2.start();
                [[ maybe_unused ]] const volatile auto beats = seq.toTime (b);
                bm2.stop();
            }

            // Profile the last 25 beats
            for (int c = 0; c < numConversions; ++c)
            {
                const auto b = BeatPosition::fromBeats (r.nextInt ({ (numBeats / 4) * 3, numBeats }));

                bm3.start();
                [[ maybe_unused ]] const volatile auto beats = seq.toTime (b);
                bm3.stop();
            }

            // Profile the whole range
            for (int c = 0; c < numConversions; ++c)
            {
                const auto b = BeatPosition::fromBeats (r.nextInt (numBeats));

                bm4.start();
                [[ maybe_unused ]] const volatile auto beats = seq.toTime (b);
                bm4.stop();
            }
        }

        for (auto bm : { &bm1, &bm2, &bm3, &bm4 })
            BenchmarkList::getInstance().addResult (bm->getResult());
    }
};

static TempoBenchmarks tempoBenchmarks;

}} // namespace tracktion { inline namespace engine

#endif //TRACKTION_BENCHMARKS
