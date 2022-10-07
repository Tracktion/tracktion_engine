/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


namespace tracktion { inline namespace graph
{
#include "../3rd_party/farbot/include/farbot/fifo.hpp"
}}

namespace tracktion { inline namespace graph
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
        You won't be able to use this pool right away, you must set a capacity and
        reserve some a number of buffers for it to use first.
        @see setCapacity, reserve
    */
    AudioBufferPool() = default;

    /** Creates pool with a given max capacity.
        @see reserve
    */
    AudioBufferPool (size_t maxCapacity);

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
     
        [[ thread_safe ]]
    */
    choc::buffer::ChannelArrayBuffer<float> allocate (choc::buffer::Size);
    
    /** Releases an allocated buffer back to the pool.
        @returns true if the buffer was able to fit in the pool, false if it
        couldn't and had to be deallocated in place
     
        [[ thread_safe ]]
    */
    bool release (choc::buffer::ChannelArrayBuffer<float>&&);

    //==============================================================================
    /** Releases all the internal allocated storage. */
    void reset();

    /** Sets the maximum number of buffers this can store. */
    void setCapacity (size_t);

    /** Returns the current maximum number of buffers this can store. */
    size_t getCapacity() const                      { return capacity; }

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
    std::unique_ptr<farbot::fifo<choc::buffer::ChannelArrayBuffer<float>>> fifo;
    size_t capacity = 0;
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
inline AudioBufferPool::AudioBufferPool (size_t maxCapacity)
{
    setCapacity (maxCapacity);
}

inline choc::buffer::ChannelArrayBuffer<float> AudioBufferPool::allocate (choc::buffer::Size size)
{
    choc::buffer::ChannelArrayBuffer<float> buffer;

    if (fifo->pop (buffer))
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
    return fifo->push (std::move (buffer));
}

//==============================================================================
inline void  AudioBufferPool::setCapacity (size_t maxCapacity)
{
    maxCapacity = (size_t) juce::nextPowerOfTwo ((int) maxCapacity);
    
    if (maxCapacity <= capacity)
        return;
    
    fifo = std::make_unique<farbot::fifo<choc::buffer::ChannelArrayBuffer<float>>> ((int) maxCapacity);
    capacity = maxCapacity;
}

inline void AudioBufferPool::reserve (size_t numBuffers, choc::buffer::Size size)
{
    std::vector<choc::buffer::ChannelArrayBuffer<float>> buffers;
    
    // Remove all the buffers
    for (;;)
    {
        choc::buffer::ChannelArrayBuffer<float> tempBuffer;

        if (! fifo->pop (tempBuffer))
            break;

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

    // Push the temp buffers back
    for (auto& b : buffers)
    {
        [[ maybe_unused ]] bool succeeded = release (std::move (b));
        assert (succeeded); // Capacity too small?
    }
    
    // Push any additional buffers
    for (int i = 0; i < numToAdd; ++i)
    {
        [[ maybe_unused ]] bool succeeded = release (choc::buffer::ChannelArrayBuffer<float> (size));
        assert (succeeded); // Capacity too small?
    }
}

inline size_t AudioBufferPool::getNumBuffers()
{
    std::vector<choc::buffer::ChannelArrayBuffer<float>> buffers;

    // Remove all the buffers
    for (;;)
    {
        choc::buffer::ChannelArrayBuffer<float> tempBuffer;

        if (! fifo->pop (tempBuffer))
            break;

        buffers.emplace_back (std::move (tempBuffer));
    }

    const auto numBuffers = buffers.size();
    
    // Push the temp buffers back
    for (auto& b : buffers)
    {
        [[ maybe_unused ]] bool succeeded = release (std::move (b));
        assert (succeeded);
    }

    return numBuffers;
}

inline size_t AudioBufferPool::getAllocatedSize()
{
    size_t size = 0;
    std::vector<choc::buffer::ChannelArrayBuffer<float>> buffers;

    // Remove all the buffers
    for (;;)
    {
        choc::buffer::ChannelArrayBuffer<float> tempBuffer;
        
        if (! fifo->pop (tempBuffer))
            break;
        
        buffers.emplace_back (std::move (tempBuffer));
    }

    // Calculate the size of them
    for (auto& b : buffers)
        size +=  b.getView().data.getBytesNeeded (b.getSize());
    
    // Then put them back in the fifo
    for (auto& b : buffers)
        fifo->push (std::move (b));

    return size;
}

}} // namespace tracktion
