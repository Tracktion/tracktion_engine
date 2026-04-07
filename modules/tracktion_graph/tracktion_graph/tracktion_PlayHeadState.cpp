/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


#if TRACKTION_UNIT_TESTS && GRAPH_UNIT_TESTS_PLAYHEADSTATE

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline graph {

TEST_SUITE ("tracktion_graph")
{
    TEST_CASE ("PlayHeadStateTests")
    {
        constexpr int64_t blockSize = 44'100;

        SUBCASE ("Loop edges")
        {
            PlayHead playHead;
            PlayHeadState playHeadState (playHead);

            playHead.play ({ 44'100, 176'400 }, true); // 1-4s
            auto referenceRange = juce::Range<int64_t>::withStartAndLength (0, blockSize);
            playHeadState.update (referenceRange); // This is reference samples

            CHECK (! playHeadState.isContiguousWithPreviousBlock());
            CHECK (playHeadState.didPlayheadJump());
            CHECK (playHeadState.isFirstBlockOfLoop());
            CHECK (! playHeadState.isLastBlockOfLoop());

            referenceRange += blockSize;
            playHead.setReferenceSampleRange (referenceRange);
            playHeadState.update (referenceRange);

            CHECK (playHeadState.isContiguousWithPreviousBlock());
            CHECK (! playHeadState.didPlayheadJump());
            CHECK (! playHeadState.isFirstBlockOfLoop());
            CHECK (! playHeadState.isLastBlockOfLoop());

            referenceRange += blockSize;
            playHead.setReferenceSampleRange (referenceRange);
            playHeadState.update (referenceRange);

            CHECK (playHeadState.isContiguousWithPreviousBlock());
            CHECK (! playHeadState.didPlayheadJump());
            CHECK (! playHeadState.isFirstBlockOfLoop());
            CHECK (playHeadState.isLastBlockOfLoop());
        }

        SUBCASE ("Not looping")
        {
            PlayHead playHead;
            PlayHeadState playHeadState (playHead);

            playHead.play ({ 44'100, 176'400 }, false); // 1-4s
            auto referenceRange = juce::Range<int64_t>::withStartAndLength (0, blockSize);
            playHeadState.update (referenceRange); // This is reference samples

            CHECK (! playHeadState.isContiguousWithPreviousBlock());
            CHECK (playHeadState.didPlayheadJump());
            CHECK (! playHeadState.isFirstBlockOfLoop());
            CHECK (! playHeadState.isLastBlockOfLoop());

            referenceRange += blockSize;
            playHead.setReferenceSampleRange (referenceRange);
            playHeadState.update (referenceRange);

            CHECK (playHeadState.isContiguousWithPreviousBlock());
            CHECK (! playHeadState.didPlayheadJump());
            CHECK (! playHeadState.isFirstBlockOfLoop());
            CHECK (! playHeadState.isLastBlockOfLoop());

            referenceRange += blockSize;
            playHead.setReferenceSampleRange (referenceRange);
            playHeadState.update (referenceRange);

            CHECK (playHeadState.isContiguousWithPreviousBlock());
            CHECK (! playHeadState.didPlayheadJump());
            CHECK (! playHeadState.isFirstBlockOfLoop());
            CHECK (! playHeadState.isLastBlockOfLoop());
        }
    }
}

} // namespace tracktion::inline graph

#endif
