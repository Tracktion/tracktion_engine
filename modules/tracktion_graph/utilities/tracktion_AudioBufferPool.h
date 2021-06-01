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
    */
    void reserve (size_t numBuffers);

    /** Reserves a number of buffers of a given size, preallocating them. */
    void reserve (size_t numBuffers, choc::buffer::Size);

    /** Returns the currently allocated size of all the buffers in bytes. */
    size_t getAllocatedSize();

private:
    choc::fifo::MultipleReaderMultipleWriterFIFO<choc::buffer::ChannelArrayBuffer<float>> fifo;
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
    :AudioBufferPool (0)
{
}

inline AudioBufferPool::AudioBufferPool (size_t numBuffers)
{
    reserve (numBuffers);
}

inline choc::buffer::ChannelArrayBuffer<float> AudioBufferPool::allocate (choc::buffer::Size size)
{
    choc::buffer::ChannelArrayBuffer<float> buffer;
    
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
        buffer.resize (size);
    }
        
    return buffer;
}

inline bool AudioBufferPool::release (choc::buffer::ChannelArrayBuffer<float>&& buffer)
{
    return fifo.push (std::move (buffer));
}

//==============================================================================
inline void AudioBufferPool::reset()
{
    fifo.reset (0);
}

inline void AudioBufferPool::reserve (size_t numBuffers)
{
    fifo.reset ((size_t) numBuffers);
}

inline void AudioBufferPool::reserve (size_t numBuffers, choc::buffer::Size size)
{
    std::vector<choc::buffer::ChannelArrayBuffer<float>> buffers;
    
    // Remove all the buffers
    for (uint32_t i = 0; i < fifo.getFreeSlots(); ++i)
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
    const int numToAdd = static_cast<int> (buffers.size()) - static_cast<int> (numBuffers);
    fifo.reset ((size_t) numBuffers);

    // Push the temp buffers back
    for (auto& b : buffers)
    {
        [[ maybe_unused ]] bool succeeded = fifo.push (std::move (b));
        assert (succeeded);
    }
    
    // Push any additional buffers
    for (int i = 0; i < numToAdd; ++i)
        fifo.push (choc::buffer::ChannelArrayBuffer<float> (size));
}

inline size_t AudioBufferPool::getAllocatedSize()
{
    return 0;
}

} // namespace tracktion_graph
