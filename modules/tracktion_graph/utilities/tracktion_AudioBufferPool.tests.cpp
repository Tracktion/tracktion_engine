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
                expectEquals<size_t> (pool.getNumBuffers(), 0);
                pool.release (ChannelArrayBuffer<float> (size));
                expectEquals<size_t> (pool.getNumBuffers(), 1);
                expectEquals<size_t> (pool.getAllocatedSize(), SeparateChannelLayout<float>::getBytesNeeded (size));
            }

            {
                AudioBufferPool pool;
                pool.reserve (1, size);
                
                pool.release (ChannelArrayBuffer<float> (size));

                auto buffer = pool.allocate (size);
                const auto bufferSize = buffer.getSize();
                expectGreaterOrEqual ((int) bufferSize.numFrames, (int) size.numFrames);
                expectGreaterOrEqual ((int) bufferSize.numChannels, (int) size.numChannels);
                expectEquals<size_t> (pool.getAllocatedSize(), SeparateChannelLayout<float>::getBytesNeeded (size));

                expectEquals<size_t> (pool.getNumBuffers(), 1);
                pool.release (std::move (buffer));
                expectEquals<size_t> (pool.getNumBuffers(), 2);
            }
        }
    }
};

static AudioBufferPoolTests audioBufferPoolTests;

#endif

} // namespace tracktion_engine
