/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion_graph
{

//==============================================================================
//==============================================================================
class PlayHeadTests : public juce::UnitTest
{
public:
    PlayHeadTests()
        : juce::UnitTest ("PlayHead", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
        runBasicTests();
    }

private:
    //==============================================================================
    //==============================================================================
    void runBasicTests()
    {
        beginTest ("SplitTimelineRange");
        {
            PlayHead playHead;
            playHead.play ({ 0, 1000 }, true);

            {
                const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { 250, 750 });
                expect (! tr.isSplit);
                expectEquals<> (tr.timelineRange1.getStart(), 250LL);
                expectEquals (tr.timelineRange1.getEnd(), 750LL);
                expectEquals (tr.timelineRange2.getStart(), 0LL);
                expectEquals (tr.timelineRange2.getEnd(), 0LL);
            }

            {
                const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { 500, 1500 });
                expect (tr.isSplit);
                expectEquals (tr.timelineRange1.getStart(), 500LL);
                expectEquals (tr.timelineRange1.getEnd(), 1000LL);
                expectEquals (tr.timelineRange2.getStart(), 0LL);
                expectEquals (tr.timelineRange2.getEnd(), 500LL);
            }
            
            {
                playHead.play ({ 0, 1500 }, false);
                const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { 500, 1500 });
                expect (! tr.isSplit);
                expectEquals (tr.timelineRange1.getStart(), 500LL);
                expectEquals (tr.timelineRange1.getEnd(), 1500LL);
                expectEquals (tr.timelineRange2.getStart(), 0LL);
                expectEquals (tr.timelineRange2.getEnd(), 0LL);
            }
        }
        
        beginTest ("PlayHead playing");
        {
            {
                PlayHead playHead;
                playHead.play ({ 0, 10'000 }, false);
                expectEquals (playHead.getPosition(), 0LL);
                expectEquals (playHead.referenceSamplePositionToTimelinePosition (0), 0LL);

                playHead.incrementReferenceSampleCount (500);
                expectEquals (playHead.getPosition(), 500LL);
                expectEquals (playHead.referenceSamplePositionToTimelinePosition (0), 0LL);

                playHead.incrementReferenceSampleCount (1000);
                expectEquals (playHead.getPosition(), 1500LL);
                expectEquals (playHead.referenceSamplePositionToTimelinePosition (0), 0LL);

                playHead.stop();
                playHead.incrementReferenceSampleCount (500);
                expectEquals (playHead.getPosition(), 1500LL); // timeline position hasn't moved
                expectEquals (playHead.referenceSamplePositionToTimelinePosition (0), 1500LL); // ref position is at the previous timeline pos

                playHead.play();
                expectEquals (playHead.getPosition(), 1500LL);
                // ref position is now synced to the last timeline pos (2000)
                expectEquals (playHead.referenceSamplePositionToTimelinePosition (0), -500LL);
                expectEquals (playHead.getPosition(), 1500LL);
                playHead.incrementReferenceSampleCount (500);
                expectEquals (playHead.getPosition(), 2000LL);
                expectEquals (playHead.referenceSamplePositionToTimelinePosition (0), -500LL);
            }
            
            {
                PlayHead playHead;
                playHead.play ({ 0, 2'000 }, true);
                int64_t referencePos = 0;
                
                auto incrementReferencePos = [&] (int64_t numSamples)
                {
                    playHead.incrementReferenceSampleCount (numSamples);
                    referencePos += numSamples;
                };
                
                expectEquals (playHead.getPosition(), 0LL);
                incrementReferencePos (1000);
                expectEquals (playHead.getPosition(), 1000LL);
                expectEquals (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 500 });

                    expect (! tr.isSplit);
                    expectEquals<> (tr.timelineRange1.getStart(), 1000LL);
                    expectEquals (tr.timelineRange1.getEnd(), 1500LL);
                    expectEquals (tr.timelineRange2.getStart(), 0LL);
                    expectEquals (tr.timelineRange2.getEnd(), 0LL);
                }

                incrementReferencePos (500);
                expectEquals (playHead.getPosition(), 1500LL);
                expectEquals (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 1000 });

                    expect (tr.isSplit);
                    expectEquals<> (tr.timelineRange1.getStart(), 1500LL);
                    expectEquals (tr.timelineRange1.getEnd(), 2000LL);
                    expectEquals (tr.timelineRange2.getStart(), 0LL);
                    expectEquals (tr.timelineRange2.getEnd(), 500LL);
                }
            }

            {
                PlayHead playHead;
                playHead.play ({ 1'000, 3'000 }, true);
                int64_t referencePos = 0;
                
                auto incrementReferencePos = [&] (int64_t numSamples)
                {
                    playHead.incrementReferenceSampleCount (numSamples);
                    referencePos += numSamples;
                };
                
                expectEquals (playHead.getPosition(), 1'000LL);
                incrementReferencePos (1'000);
                expectEquals (playHead.getPosition(), 2'000LL);
                expectEquals (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 500 });

                    expect (! tr.isSplit);
                    expectEquals<> (tr.timelineRange1.getStart(), 2'000LL);
                    expectEquals (tr.timelineRange1.getEnd(), 2'500LL);
                    expectEquals (tr.timelineRange2.getStart(), 0LL);
                    expectEquals (tr.timelineRange2.getEnd(), 0LL);
                }

                incrementReferencePos (500);
                expectEquals (playHead.getPosition(), 2'500LL);
                expectEquals (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 1'000 });

                    expect (tr.isSplit);
                    expectEquals<> (tr.timelineRange1.getStart(), 2'500LL);
                    expectEquals (tr.timelineRange1.getEnd(), 3'000LL);
                    expectEquals (tr.timelineRange2.getStart(), 1'000LL);
                    expectEquals (tr.timelineRange2.getEnd(), 1'500LL);
                }
            }
            
            {
                PlayHead playHead;
                playHead.play ({ 1'000, 3'000 }, true);
                playHead.setRollInToLoop (500);
                expect (playHead.isPlaying());
                expect (playHead.isLooping());
                expect (playHead.isRollingIntoLoop());

                expectEquals (playHead.getPosition(), 500LL);
                playHead.incrementReferenceSampleCount (500);
                expectEquals (playHead.getPosition(), 1'000LL);
                expect (! playHead.isRollingIntoLoop());
            }
        }
    }
};

static PlayHeadTests playHeadTests;

}
