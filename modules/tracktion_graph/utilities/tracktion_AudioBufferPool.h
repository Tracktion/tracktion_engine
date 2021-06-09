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

//==============================================================================
/**
*/
class AudioBufferPool
{
public:
    /** Create an empty pool.
        You won't be able to use this pool right away, you must reserve some a
        number of buffers for it to use first.
        @see reserve
    */
    AudioBufferPool();

    /** Create an empty pool capabale of holding the given number of buffers. */
    AudioBufferPool (size_t numBuffers);

    ~AudioBufferPool()
    {
        if (numReserved < maxInUse)
        {
            DBG("Reserved: " << numReserved.load());
            DBG("Used: " << maxInUse.load());
        }
    }

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
    
    /** Releases an allocated buffer back to the pool.
        If the pool didn't have enough storage to add this buffer, it will simply be
        deallocated and this method will return false.
    */
    bool release (choc::buffer::ChannelArrayBuffer<float>&&);

    //==============================================================================
    /** Releases all the internal allocated storage. */
    void reset();

    /** Reserves space for a number of buffers.
        This is the maximum number of buffers the pool can use.
        This doesn't actually allocate any though, use reserve for that.
    */
    void reset (size_t numBuffers);

    /** Reserves a number of buffers of a given size, preallocating them. */
    void reserve (size_t numBuffers, choc::buffer::Size, size_t maxCapacity);

    /** Returns the currently allocated size of all the buffers in bytes. */
    size_t getAllocatedSize();

private:
    choc::fifo::MultipleReaderMultipleWriterFIFO<choc::buffer::ChannelArrayBuffer<float>> fifo;
    std::atomic<int> counter { 0 }, numInUse { 0 }, maxInUse { 0 }, numReserved { 0 };
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
inline AudioBufferPool::AudioBufferPool()
    : AudioBufferPool (0)
{
}

inline AudioBufferPool::AudioBufferPool (size_t numBuffers)
{
    reset (numBuffers);
}

inline choc::buffer::ChannelArrayBuffer<float> AudioBufferPool::allocate (choc::buffer::Size size)
{
    choc::buffer::ChannelArrayBuffer<float> buffer;

    --counter;
    ++numInUse;
    
    while (maxInUse < numInUse)
        maxInUse = numInUse.load();
    
    if (fifo.pop (buffer))
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
        DBG("allocating: " << (int)size.numChannels << " - " << (int)size.numFrames);
        //assert (false && "Buffer requested not reserved. Allocating inline");
        buffer.resize (size);
    }
        
    return buffer;
}

inline bool AudioBufferPool::release (choc::buffer::ChannelArrayBuffer<float>&& buffer)
{
    --numInUse;
    ++counter;
    return fifo.push (std::move (buffer));
}

//==============================================================================
inline void AudioBufferPool::reset()
{
    fifo.reset (0);
}

inline void AudioBufferPool::reset (size_t numBuffers)
{
    fifo.reset ((size_t) numBuffers);
}

inline void AudioBufferPool::reserve (size_t numBuffers, choc::buffer::Size size, size_t maxCapacity)
{
    assert (numBuffers <= maxCapacity);
    DBG("reserve: " << (int)numBuffers << " - " << (int)size.numChannels << " - " << (int)size.numFrames);
    std::vector<choc::buffer::ChannelArrayBuffer<float>> buffers;
    
    // Remove all the buffers
    for (uint32_t i = 0; i < fifo.getUsedSlots(); ++i)
    {
        choc::buffer::ChannelArrayBuffer<float> tempBuffer;
        [[ maybe_unused ]] bool succeeded = fifo.pop (tempBuffer);
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
    fifo.reset (maxCapacity);

    // Push the temp buffers back
    for (auto& b : buffers)
    {
        [[ maybe_unused ]] bool succeeded = fifo.push (std::move (b));
        assert (succeeded);
    }
    
    // Push any additional buffers
    for (int i = 0; i < numToAdd; ++i)
        fifo.push (choc::buffer::ChannelArrayBuffer<float> (size));
    
    assert (fifo.getUsedSlots() == (uint32_t) numBuffers);
    assert (fifo.getFreeSlots() == (maxCapacity - numBuffers + 1));
    counter = (int) numBuffers;
    numReserved = (int) numBuffers;
    maxInUse = 0;
    numInUse = 0;
}

inline size_t AudioBufferPool::getAllocatedSize()
{
    return 0;
}

} // namespace tracktion_graph
