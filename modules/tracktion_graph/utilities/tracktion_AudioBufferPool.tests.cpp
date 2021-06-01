/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_graph
{

#if GRAPH_UNIT_TESTS_AUDIOBUFFERPOOL

class AudioBufferPoolTests  : public juce::UnitTest
{
public:
    AudioBufferPoolTests()
        : juce::UnitTest ("AudioBufferPool", "tracktion_graph") {}

    //==============================================================================
    void runTest() override
    {
        runAllocationTests();
    }
    
private:
    void runAllocationTests()
    {
        using namespace choc::buffer;
        
        beginTest ("Allocation");
        {
            const auto size = Size::create (2, 128);
            
            {
                AudioBufferPool pool;
                expect (! pool.release (ChannelArrayBuffer<float> (size)));
                expectEquals<size_t> (pool.getAllocatedSize(), 0);
            }

            {
                AudioBufferPool pool (1);
                expect (pool.release (ChannelArrayBuffer<float> (size)));
                expect (! pool.release (ChannelArrayBuffer<float> (size)));

                auto buffer = pool.allocate (size);
                const auto bufferSize = buffer.getSize();
                expectGreaterOrEqual ((int) bufferSize.numFrames, (int) size.numFrames);
                expectGreaterOrEqual ((int) bufferSize.numChannels, (int) size.numChannels);
                //expectEquals<size_t> (pool.getAllocatedSize(), SeparateChannelLayout<float>::getBytesNeeded (size)));

                expect (pool.release (std::move (buffer)));
                expect (! pool.release (ChannelArrayBuffer<float> (size)));
            }
        }
    }
};

static AudioBufferPoolTests audioBufferPoolTests;

#endif

} // namespace tracktion_engine
