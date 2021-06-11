/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#ifdef _MSC_VER
 #pragma warning (push)
 #pragma warning (disable: 4127)
#endif

#include "../3rd_party/concurrentqueue.h"

#ifdef _MSC_VER
 #pragma warning (pop)
#endif

namespace tracktion_graph
{

//==============================================================================
/**
    A lock-free pool of audio buffers.
 
    If you need to quickly create and then return some audio buffers this class
    enables you to do that in a lock free way.
 
    Note that the buffers can be pre-allocated but if you ask for a buffer which
    isn't in the pool, it will either resize an existing one or allocate a new one.
    
    After processing a constant audio graph for a while though this should be
    completely allocation and lock-free.
*/
class AudioBufferPool
{
public:
    /** Create an empty pool.
        You won't be able to use this pool right away, you must reserve some a
        number of buffers for it to use first.
        @see reserve
    */
    AudioBufferPool() = default;

    /** Returns an allocated buffer for a given size from the pool.
        This will attempt to get a buffer from the pre-allocated pool that can
        fit the size but if one can't easily be found, it will allocate one with
        new and return it.
        
        Additionally, the returned buffer may be bigger than the size asked for
        so be sure to only use a view of it but retain ownership of the whole buffer.
        You can release it back to the pool later.
        It will also contain junk so be sure to clear or initialise the view
        required before use.
        @see release
    */
    choc::buffer::ChannelArrayBuffer<float> allocate (choc::buffer::Size);
    
    /** Releases an allocated buffer back to the pool. */
    void release (choc::buffer::ChannelArrayBuffer<float>&&);

    //==============================================================================
    /** Releases all the internal allocated storage. */
    void reset();

    /** Reserves a number of buffers of a given size, preallocating them. */
    void reserve (size_t numBuffers, choc::buffer::Size);

    //==============================================================================
    /** Returns the current number of buffers in the pool.
        N.B. This isn't safe to call concurrently with any other methods.
    */
    size_t getNumBuffers();

    /** Returns the currently allocated size of all the buffers in bytes.
        N.B. This isn't safe to call concurrently with any other methods as it needs
        to pop and then push all the elements to examine their size.
    */
    size_t getAllocatedSize();

private:
    moodycamel::ConcurrentQueue<choc::buffer::ChannelArrayBuffer<float>> fifo;
};


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================
inline choc::buffer::ChannelArrayBuffer<float> AudioBufferPool::allocate (choc::buffer::Size size)
{
    choc::buffer::ChannelArrayBuffer<float> buffer;

    if (fifo.try_dequeue (buffer))
    {
        if (auto bufferSize = buffer.getSize();
            bufferSize.numChannels < size.numChannels
            || bufferSize.numFrames < size.numFrames)
        {
            buffer.resize (size);
        }
    }
    else
    {
        buffer.resize (size);
    }
        
    return buffer;
}

inline void AudioBufferPool::release (choc::buffer::ChannelArrayBuffer<float>&& buffer)
{
    fifo.enqueue (std::move (buffer));
}

//==============================================================================
inline void AudioBufferPool::reserve (size_t numBuffers, choc::buffer::Size size)
{
    std::vector<choc::buffer::ChannelArrayBuffer<float>> buffers;
    
    // Remove all the buffers
    for (uint32_t i = 0; i < std::min ((uint32_t) numBuffers, (uint32_t) fifo.size_approx()); ++i)
    {
        choc::buffer::ChannelArrayBuffer<float> tempBuffer;
        [[ maybe_unused ]] bool succeeded = fifo.try_dequeue (tempBuffer);
        assert (succeeded);

        buffers.emplace_back (std::move (tempBuffer));
    }
    
    // Ensure their size is as big as required
    for (auto& b : buffers)
    {
        if (auto bufferSize = b.getSize();
            bufferSize.numChannels < size.numChannels
            || bufferSize.numFrames < size.numFrames)
        {
            b.resize (size);
        }
    }
    
    // Reset the fifo storage to hold the new number of buffers
    const int numToAdd = static_cast<int> (numBuffers) - static_cast<int> (buffers.size());
    assert (numToAdd >= 0);

    // Push the temp buffers back
    for (auto& b : buffers)
    {
        [[ maybe_unused ]] bool succeeded = fifo.try_enqueue (std::move (b));
        assert (succeeded);
    }
    
    // Push any additional buffers
    for (int i = 0; i < numToAdd; ++i)
        fifo.enqueue (choc::buffer::ChannelArrayBuffer<float> (size));
}

inline size_t AudioBufferPool::getNumBuffers()
{
    return fifo.size_approx();
}

inline size_t AudioBufferPool::getAllocatedSize()
{
    size_t size = 0;
    std::vector<choc::buffer::ChannelArrayBuffer<float>> buffers;

    // Remove all the buffers
    for (;;)
    {
        choc::buffer::ChannelArrayBuffer<float> tempBuffer;
        
        if (! fifo.try_dequeue (tempBuffer))
            break;
        
        buffers.emplace_back (std::move (tempBuffer));
    }

    // Calculate the size of them
    for (auto& b : buffers)
        size +=  b.getView().data.getBytesNeeded (b.getSize());
    
    // Then put them back in the fifo
    for (auto& b : buffers)
        fifo.enqueue (std::move (b));

    return size;
}

} // namespace tracktion_graph
