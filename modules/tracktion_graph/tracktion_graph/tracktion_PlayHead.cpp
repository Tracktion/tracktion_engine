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

#if GRAPH_UNIT_TESTS_PLAYHEAD

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
                expectEquals<int64_t> (tr.timelineRange1.getStart(), 250);
                expectEquals<int64_t> (tr.timelineRange1.getEnd(), 750);
                expectEquals<int64_t> (tr.timelineRange2.getStart(), 0);
                expectEquals<int64_t> (tr.timelineRange2.getEnd(), 0);
            }

            {
                const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { 750, 1250 });
                expect (tr.isSplit);
                expectEquals<int64_t> (tr.timelineRange1.getStart(), 750);
                expectEquals<int64_t> (tr.timelineRange1.getEnd(), 1000);
                expectEquals<int64_t> (tr.timelineRange2.getStart(), 0);
                expectEquals<int64_t> (tr.timelineRange2.getEnd(), 250);
            }

            {
                playHead.play ({ 0, 1500 }, false);
                const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { 500, 1500 });
                expect (! tr.isSplit);
                expectEquals<int64_t> (tr.timelineRange1.getStart(), 500);
                expectEquals<int64_t> (tr.timelineRange1.getEnd(), 1500);
                expectEquals<int64_t> (tr.timelineRange2.getStart(), 0);
                expectEquals<int64_t> (tr.timelineRange2.getEnd(), 0);
            }
        }
        
        beginTest ("PlayHead playing");
        {
            {
                PlayHead playHead;
                playHead.play ({ 0, 10'000 }, false);
                juce::Range<int64_t> referenceRange;

                expectEquals<int64_t> (playHead.getPosition(), 0);
                expectEquals<int64_t> (playHead.referenceSamplePositionToTimelinePosition (0), 0);

                referenceRange += 500;
                playHead.setReferenceSampleRange (referenceRange);
                expectEquals<int64_t> (playHead.getPosition(), 500);
                expectEquals<int64_t> (playHead.referenceSamplePositionToTimelinePosition (0), 0);

                referenceRange += 1000;
                playHead.setReferenceSampleRange (referenceRange);
                expectEquals<int64_t> (playHead.getPosition(), 1500);
                expectEquals<int64_t> (playHead.referenceSamplePositionToTimelinePosition (0), 0);

                playHead.stop();
                referenceRange += 500;
                playHead.setReferenceSampleRange (referenceRange);
                expectEquals<int64_t> (playHead.getPosition(), 1500); // timeline position hasn't moved
                expectEquals<int64_t> (playHead.referenceSamplePositionToTimelinePosition (0), 1500); // ref position is at the previous timeline pos

                playHead.play();
                expectEquals<int64_t> (playHead.getPosition(), 1500);
                // ref position is now synced to the last timeline pos (2000)
                expectEquals<int64_t> (playHead.referenceSamplePositionToTimelinePosition (0), -500);
                expectEquals<int64_t> (playHead.getPosition(), 1500);
                referenceRange += 500;
                playHead.setReferenceSampleRange (referenceRange);
                expectEquals<int64_t> (playHead.getPosition(), 2000);
                expectEquals<int64_t> (playHead.referenceSamplePositionToTimelinePosition (0), -500);
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
                
                expectEquals<int64_t> (playHead.getPosition(), 0);
                incrementReferencePos (1000);
                expectEquals<int64_t> (playHead.getPosition(), 1000);
                expectEquals<int64_t> (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 500 });

                    expect (! tr.isSplit);
                    expectEquals<int64_t> (tr.timelineRange1.getStart(), 1000);
                    expectEquals<int64_t> (tr.timelineRange1.getEnd(), 1500);
                    expectEquals<int64_t> (tr.timelineRange2.getStart(), 0);
                    expectEquals<int64_t> (tr.timelineRange2.getEnd(), 0);
                }

                incrementReferencePos (500);
                expectEquals<int64_t> (playHead.getPosition(), 1500);
                expectEquals<int64_t> (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 1000 });

                    expect (tr.isSplit);
                    expectEquals<int64_t> (tr.timelineRange1.getStart(), 1500);
                    expectEquals<int64_t> (tr.timelineRange1.getEnd(), 2000);
                    expectEquals<int64_t> (tr.timelineRange2.getStart(), 0);
                    expectEquals<int64_t> (tr.timelineRange2.getEnd(), 500);
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
                
                expectEquals<int64_t> (playHead.getPosition(), 1'000);
                incrementReferencePos (1'000);
                expectEquals<int64_t> (playHead.getPosition(), 2'000);
                expectEquals<int64_t> (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 500 });

                    expect (! tr.isSplit);
                    expectEquals<int64_t> (tr.timelineRange1.getStart(), 2'000);
                    expectEquals<int64_t> (tr.timelineRange1.getEnd(), 2'500);
                    expectEquals<int64_t> (tr.timelineRange2.getStart(), 0);
                    expectEquals<int64_t> (tr.timelineRange2.getEnd(), 0);
                }

                incrementReferencePos (500);
                expectEquals<int64_t> (playHead.getPosition(), 2'500);
                expectEquals<int64_t> (playHead.referenceSamplePositionToTimelinePosition (referencePos), playHead.getPosition());

                {
                    const auto tr = referenceSampleRangeToSplitTimelineRange (playHead, { referencePos, referencePos + 1'000 });

                    expect (tr.isSplit);
                    expectEquals<int64_t> (tr.timelineRange1.getStart(), 2'500);
                    expectEquals<int64_t> (tr.timelineRange1.getEnd(), 3'000);
                    expectEquals<int64_t> (tr.timelineRange2.getStart(), 1'000);
                    expectEquals<int64_t> (tr.timelineRange2.getEnd(), 1'500);
                }
            }
            
            {
                PlayHead playHead;
                playHead.play ({ 1'000, 3'000 }, true);
                playHead.setRollInToLoop (500);
                expect (playHead.isPlaying());
                expect (playHead.isLooping());
                expect (playHead.isRollingIntoLoop());

                expectEquals<int64_t> (playHead.getPosition(), 500);
                playHead.setReferenceSampleRange ({ 500, 500 });
                expectEquals<int64_t> (playHead.getPosition(), 1'000);
                expect (! playHead.isRollingIntoLoop());
            }
        }
    }
};

static PlayHeadTests playHeadTests;

#endif

}}
