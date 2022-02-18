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
