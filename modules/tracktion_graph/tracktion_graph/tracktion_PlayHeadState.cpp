/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion { inline namespace graph
{

#if GRAPH_UNIT_TESTS_PLAYHEADSTATE

//==============================================================================
//==============================================================================
class PlayHeadStateTests : public juce::UnitTest
{
public:
    PlayHeadStateTests()
        : juce::UnitTest ("PlayHeadStateTests", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        constexpr int64_t blockSize = 44'100;
        
        beginTest ("Loop edges");
        {
            PlayHead playHead;
            PlayHeadState playHeadState (playHead);
            
            playHead.play ({ 44'100, 176'400 }, true); // 1-4s
            auto referenceRange = juce::Range<int64_t>::withStartAndLength (0, blockSize);
            playHeadState.update (referenceRange); // This is reference samples

            expect (! playHeadState.isContiguousWithPreviousBlock());
            expect (playHeadState.didPlayheadJump());
            expect (playHeadState.isFirstBlockOfLoop());
            expect (! playHeadState.isLastBlockOfLoop());

            referenceRange += blockSize;
            playHead.setReferenceSampleRange (referenceRange);
            playHeadState.update (referenceRange);

            expect (playHeadState.isContiguousWithPreviousBlock());
            expect (! playHeadState.didPlayheadJump());
            expect (! playHeadState.isFirstBlockOfLoop());
            expect (! playHeadState.isLastBlockOfLoop());

            referenceRange += blockSize;
            playHead.setReferenceSampleRange (referenceRange);
            playHeadState.update (referenceRange);

            expect (playHeadState.isContiguousWithPreviousBlock());
            expect (! playHeadState.didPlayheadJump());
            expect (! playHeadState.isFirstBlockOfLoop());
            expect (playHeadState.isLastBlockOfLoop());
        }
        
        beginTest ("Not looping");
        {
            PlayHead playHead;
            PlayHeadState playHeadState (playHead);
            
            playHead.play ({ 44'100, 176'400 }, false); // 1-4s
            auto referenceRange = juce::Range<int64_t>::withStartAndLength (0, blockSize);
            playHeadState.update (referenceRange); // This is reference samples

            expect (! playHeadState.isContiguousWithPreviousBlock());
            expect (playHeadState.didPlayheadJump());
            expect (! playHeadState.isFirstBlockOfLoop());
            expect (! playHeadState.isLastBlockOfLoop());

            referenceRange += blockSize;
            playHead.setReferenceSampleRange (referenceRange);
            playHeadState.update (referenceRange);

            expect (playHeadState.isContiguousWithPreviousBlock());
            expect (! playHeadState.didPlayheadJump());
            expect (! playHeadState.isFirstBlockOfLoop());
            expect (! playHeadState.isLastBlockOfLoop());

            referenceRange += blockSize;
            playHead.setReferenceSampleRange (referenceRange);
            playHeadState.update (referenceRange);

            expect (playHeadState.isContiguousWithPreviousBlock());
            expect (! playHeadState.didPlayheadJump());
            expect (! playHeadState.isFirstBlockOfLoop());
            expect (! playHeadState.isLastBlockOfLoop());
        }
    }
};

static PlayHeadStateTests playHeadStateTests;

#endif

}}
