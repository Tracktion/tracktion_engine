/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


#if TRACKTION_UNIT_TESTS && GRAPH_UNIT_TESTS_PLAYHEAD

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline graph {

TEST_SUITE ("tracktion_graph")
{
    TEST_CASE ("PlayHead")
    {
        // SplitTimelineRange
        {
            PlayHead playHead;
            playHead.play ({ 0, 1000 }, true);

            {
                const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { 250, 750 });
                CHECK (! tr.isSplit);
                CHECK_EQ (tr.timelineRange1.getStart(), (int64_t) 250);
                CHECK_EQ (tr.timelineRange1.getEnd(), (int64_t) 750);
                CHECK_EQ (tr.timelineRange2.getStart(), (int64_t) 0);
                CHECK_EQ (tr.timelineRange2.getEnd(), (int64_t) 0);
            }

            {
                const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { 750, 1250 });
                CHECK (tr.isSplit);
                CHECK_EQ (tr.timelineRange1.getStart(), (int64_t) 750);
                CHECK_EQ (tr.timelineRange1.getEnd(), (int64_t) 1000);
                CHECK_EQ (tr.timelineRange2.getStart(), (int64_t) 0);
                CHECK_EQ (tr.timelineRange2.getEnd(), (int64_t) 250);
            }

            {
                playHead.play ({ 0, 1500 }, false);
                const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { 500, 1500 });
                CHECK (! tr.isSplit);
                CHECK_EQ (tr.timelineRange1.getStart(), (int64_t) 500);
                CHECK_EQ (tr.timelineRange1.getEnd(), (int64_t) 1500);
                CHECK_EQ (tr.timelineRange2.getStart(), (int64_t) 0);
                CHECK_EQ (tr.timelineRange2.getEnd(), (int64_t) 0);
            }
        }

        // PlayHead playing
        {
            {
                PlayHead playHead;
                playHead.play ({ 0, 10'000 }, false);
                juce::Range<int64_t> referenceRange;

                CHECK_EQ (playHead.getPosition(), (int64_t) 0);
                CHECK_EQ (playHead.referenceSamplePositionToTimelinePosition (0), (int64_t) 0);

                referenceRange += 500;
                playHead.setReferenceSampleRange (referenceRange);
                CHECK_EQ (playHead.getPosition(), (int64_t) 500);
                CHECK_EQ (playHead.referenceSamplePositionToTimelinePosition (0), (int64_t) 0);

                referenceRange += 1000;
                playHead.setReferenceSampleRange (referenceRange);
                CHECK_EQ (playHead.getPosition(), (int64_t) 1500);
                CHECK_EQ (playHead.referenceSamplePositionToTimelinePosition (0), (int64_t) 0);

                playHead.stop();
                referenceRange += 500;
                playHead.setReferenceSampleRange (referenceRange);
                CHECK_EQ (playHead.getPosition(), (int64_t) 1500);
                CHECK_EQ (playHead.referenceSamplePositionToTimelinePosition (0), (int64_t) 1500);

                playHead.play();
                CHECK_EQ (playHead.getPosition(), (int64_t) 1500);
                CHECK_EQ (playHead.referenceSamplePositionToTimelinePosition (0), (int64_t) -500);
                CHECK_EQ (playHead.getPosition(), (int64_t) 1500);
                referenceRange += 500;
                playHead.setReferenceSampleRange (referenceRange);
                CHECK_EQ (playHead.getPosition(), (int64_t) 2000);
                CHECK_EQ (playHead.referenceSamplePositionToTimelinePosition (0), (int64_t) -500);
            }

            {
                PlayHead playHead;
                playHead.play ({ 0, 2'000 }, true);
                int64_t referencePos = 0;

                auto incrementReferencePos = [&] (int64_t numSamples)
                {
                    referencePos += numSamples;
                    playHead.setReferenceSampleRange ({ referencePos, referencePos });
                };

                CHECK_EQ (playHead.getPosition(), (int64_t) 0);
                incrementReferencePos (1000);
                CHECK_EQ (playHead.getPosition(), (int64_t) 1000);
                CHECK_EQ (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 500 });

                    CHECK (! tr.isSplit);
                    CHECK_EQ (tr.timelineRange1.getStart(), (int64_t) 1000);
                    CHECK_EQ (tr.timelineRange1.getEnd(), (int64_t) 1500);
                    CHECK_EQ (tr.timelineRange2.getStart(), (int64_t) 0);
                    CHECK_EQ (tr.timelineRange2.getEnd(), (int64_t) 0);
                }

                incrementReferencePos (500);
                CHECK_EQ (playHead.getPosition(), (int64_t) 1500);
                CHECK_EQ (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 1000 });

                    CHECK (tr.isSplit);
                    CHECK_EQ (tr.timelineRange1.getStart(), (int64_t) 1500);
                    CHECK_EQ (tr.timelineRange1.getEnd(), (int64_t) 2000);
                    CHECK_EQ (tr.timelineRange2.getStart(), (int64_t) 0);
                    CHECK_EQ (tr.timelineRange2.getEnd(), (int64_t) 500);
                }
            }

            {
                PlayHead playHead;
                playHead.play ({ 1'000, 3'000 }, true);
                int64_t referencePos = 0;

                auto incrementReferencePos = [&] (int64_t numSamples)
                {
                    referencePos += numSamples;
                    playHead.setReferenceSampleRange ({ referencePos, referencePos });
                };

                CHECK_EQ (playHead.getPosition(), (int64_t) 1'000);
                incrementReferencePos (1'000);
                CHECK_EQ (playHead.getPosition(), (int64_t) 2'000);
                CHECK_EQ (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 500 });

                    CHECK (! tr.isSplit);
                    CHECK_EQ (tr.timelineRange1.getStart(), (int64_t) 2'000);
                    CHECK_EQ (tr.timelineRange1.getEnd(), (int64_t) 2'500);
                    CHECK_EQ (tr.timelineRange2.getStart(), (int64_t) 0);
                    CHECK_EQ (tr.timelineRange2.getEnd(), (int64_t) 0);
                }

                incrementReferencePos (500);
                CHECK_EQ (playHead.getPosition(), (int64_t) 2'500);
                CHECK_EQ (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 1'000 });

                    CHECK (tr.isSplit);
                    CHECK_EQ (tr.timelineRange1.getStart(), (int64_t) 2'500);
                    CHECK_EQ (tr.timelineRange1.getEnd(), (int64_t) 3'000);
                    CHECK_EQ (tr.timelineRange2.getStart(), (int64_t) 1'000);
                    CHECK_EQ (tr.timelineRange2.getEnd(), (int64_t) 1'500);
                }
            }

            {
                PlayHead playHead;
                playHead.play ({ 1'000, 3'000 }, true);
                playHead.setRollInToLoop (500);
                CHECK (playHead.isPlaying());
                CHECK (playHead.isLooping());
                CHECK (playHead.isRollingIntoLoop());

                CHECK_EQ (playHead.getPosition(), (int64_t) 500);
                playHead.setReferenceSampleRange ({ 500, 500 });
                CHECK_EQ (playHead.getPosition(), (int64_t) 1'000);
                CHECK (! playHead.isRollingIntoLoop());
            }
        }
    }
}

} // namespace tracktion::inline graph

#endif
