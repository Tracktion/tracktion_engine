//
//    ██████ ██   ██  ██████   ██████
//   ██      ██   ██ ██    ██ ██            ** Classy Header-Only Classes **
//   ██      ███████ ██    ██ ██
//   ██      ██   ██ ██    ██ ██           https://github.com/Tracktion/choc
//    ██████ ██   ██  ██████   ██████
//
//   CHOC is (C)2022 Tracktion Corporation, and is offered under the terms of the ISC license:
//
//   Permission to use, copy, modify, and/or distribute this software for any purpose with or
//   without fee is hereby granted, provided that the above copyright notice and this permission
//   notice appear in all copies. THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
//   WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
//   AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
//   CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
//   WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
//   CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#ifndef CHOC_ALIGNEDMEMORYBLOCK_HEADER_INCLUDED
#define CHOC_ALIGNEDMEMORYBLOCK_HEADER_INCLUDED

#include <cstddef>

namespace choc
{

/**
    A very simple container that manages a heap-allocated block of memory, ensuring
    that its address is aligned to the size provided in the class template argument.

    The size of the alignment must be a power-of-two.
*/
template <size_t alignmentBytes>
struct AlignedMemoryBlock
{
    AlignedMemoryBlock() = default;
    AlignedMemoryBlock (size_t initialSize);
    AlignedMemoryBlock (const AlignedMemoryBlock&);
    AlignedMemoryBlock (AlignedMemoryBlock&&) noexcept;
    ~AlignedMemoryBlock();

    AlignedMemoryBlock& operator= (const AlignedMemoryBlock&);
    AlignedMemoryBlock& operator= (AlignedMemoryBlock&&) noexcept;

    /// Returns the aligned memory address, or nullptr if the block's size hasn't been set.
    void* data() noexcept               { return alignedPointer; }
    /// Returns the available space in bytes.
    size_t size() const noexcept        { return availableSize; }

    /// Re-initialises the memory with a new size.
    /// This method invalidates any currently allocated data, and the newly allocated
    /// block's contents will be in an undefined state.
    void resize (size_t newSize);

    /// Zeroes any allocated memory.
    void clear() noexcept;

    /// Releases any allocated memory.
    void reset();

private:
    void* alignedPointer = nullptr;
    char* allocatedData = nullptr;
    size_t availableSize = 0;
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

template <size_t alignmentBytes>
AlignedMemoryBlock<alignmentBytes>::AlignedMemoryBlock (size_t initialSize)
{
    resize (initialSize);
}

template <size_t alignmentBytes>
AlignedMemoryBlock<alignmentBytes>::AlignedMemoryBlock (const AlignedMemoryBlock& other)
{
    resize (other.availableSize);
    memcpy (alignedPointer, other.alignedPointer, availableSize);
}

template <size_t alignmentBytes>
AlignedMemoryBlock<alignmentBytes>::AlignedMemoryBlock (AlignedMemoryBlock&& other) noexcept
   : alignedPointer (other.alignedPointer), allocatedData (other.allocatedData),
     availableSize (other.availableSize)
{
    other.alignedPointer = nullptr;
    other.allocatedData = nullptr;
    other.availableSize = 0;
}

template <size_t alignmentBytes>
AlignedMemoryBlock<alignmentBytes>& AlignedMemoryBlock<alignmentBytes>::operator= (const AlignedMemoryBlock& other)
{
    resize (other.availableSize);
    memcpy (alignedPointer, other.alignedPointer, availableSize);
    return *this;
}

template <size_t alignmentBytes>
AlignedMemoryBlock<alignmentBytes>& AlignedMemoryBlock<alignmentBytes>::operator= (AlignedMemoryBlock&& other) noexcept
{
    alignedPointer = other.alignedPointer;
    allocatedData = other.allocatedData;
    availableSize = other.availableSize;
    other.alignedPointer = nullptr;
    other.allocatedData = nullptr;
    other.availableSize = 0;
    return *this;
}

template <size_t alignmentBytes>
AlignedMemoryBlock<alignmentBytes>::~AlignedMemoryBlock()
{
    static_assert (alignmentBytes > 0 && (alignmentBytes & (alignmentBytes - 1)) == 0,
                   "choc::AlignedMemoryBlock requires the alignment value to be a power of 2");

    reset();
}

template <size_t alignmentBytes>
void AlignedMemoryBlock<alignmentBytes>::reset()
{
    delete[] allocatedData;
    allocatedData = nullptr;
    alignedPointer = nullptr;
    availableSize = 0;
}

template <size_t alignmentBytes>
void AlignedMemoryBlock<alignmentBytes>::resize (size_t newSize)
{
    if (newSize != size())
    {
        reset();

        if (newSize != 0)
        {
            availableSize = newSize;
            allocatedData = new char[newSize + alignmentBytes];
            auto address = reinterpret_cast<std::uintptr_t> (allocatedData);
            alignedPointer = reinterpret_cast<void*> ((address + (alignmentBytes - 1u))
                                                        & ~static_cast<std::uintptr_t> (alignmentBytes - 1u));
        }
    }
}

template <size_t alignmentBytes>
void AlignedMemoryBlock<alignmentBytes>::clear() noexcept
{
    if (size() != 0)
        std::memset (data(), 0, size());
}

} // namespace choc

#endif // CHOC_ALIGNEDMEMORYBLOCK_HEADER_INCLUDED
