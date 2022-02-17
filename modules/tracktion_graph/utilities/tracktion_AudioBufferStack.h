/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#include <stack>

namespace tracktion { inline namespace graph
{

//==============================================================================
/**
    A stack of audio buffers.
 
    If you need to quickly create and then return some audio buffers this class
    enables you to do that.
 
    Note that the buffers can be pre-allocated but if you ask for a buffer which
    isn't in the pool, it will either resize an existing one or allocate a new one.
    
    After processing a constant audio graph for a while though this should be
    completely allocation and lock-free.
    
    However, it is not thread safe so you should use a different stack for each thread!
*/
class AudioBufferStack
{
public:
    /** Create an empty stack.
        You won't be able to use this pool right away, you must reserve some a
        number of buffers for it to use first.
        @see reserve
    */
    AudioBufferStack() = default;

    /** Returns an allocated buffer for a given size from the stack.
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
    
    /** Releases an allocated buffer back to the stack. */
    void release (choc::buffer::ChannelArrayBuffer<float>&&);

    //==============================================================================
    /** Releases all the internal allocated storage. */
    void reset();

    /** Reserves a number of buffers of a given size, preallocating them. */
    void reserve (size_t numBuffers, choc::buffer::Size);

    //==============================================================================
    /** Returns the current number of buffers in the stack. */
    size_t getNumBuffers();

    /** Returns the currently allocated size of all the buffers in bytes. */
    size_t getAllocatedSize();

private:
    std::stack<choc::buffer::ChannelArrayBuffer<float>> stack;
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
inline choc::buffer::ChannelArrayBuffer<float> AudioBufferStack::allocate (choc::buffer::Size size)
{
    choc::buffer::ChannelArrayBuffer<float> buffer;

    if (stack.size() > 0)
    {
        buffer = std::move (stack.top());
        stack.pop();
        
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

inline void AudioBufferStack::release (choc::buffer::ChannelArrayBuffer<float>&& buffer)
{
    stack.push (std::move (buffer));
}

//==============================================================================
inline void AudioBufferStack::reserve (size_t numBuffers, choc::buffer::Size size)
{
    std::vector<choc::buffer::ChannelArrayBuffer<float>> buffers;
    
    // Remove all the buffers
    for (size_t i = 0; i < std::min (numBuffers, stack.size()); ++i)
    {
        buffers.emplace_back (std::move (stack.top()));
        stack.pop();
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
        stack.push (std::move (b));
    
    // Push any additional buffers
    for (int i = 0; i < numToAdd; ++i)
        stack.push (choc::buffer::ChannelArrayBuffer<float> (size));
}

inline size_t AudioBufferStack::getNumBuffers()
{
    return stack.size();
}

inline size_t AudioBufferStack::getAllocatedSize()
{
    if (stack.size() == 0)
        return 0;
    
    auto& b = stack.top();
    return b.getView().data.getBytesNeeded (b.getSize());
}

}} // namespace tracktion
