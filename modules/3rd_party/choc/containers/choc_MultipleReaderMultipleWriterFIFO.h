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

#ifndef CHOC_MULTI_READER_MULTI_WRITER_FIFO_HEADER_INCLUDED
#define CHOC_MULTI_READER_MULTI_WRITER_FIFO_HEADER_INCLUDED

#include "choc_SingleReaderMultipleWriterFIFO.h"

namespace choc::fifo
{

//==============================================================================
/**
    A simple atomic multiple-reader, multiple-writer FIFO.

    This does use some spin-locks, so it's not technically lock-free, although in
    practice it's very unlikely to cause any issues if used on a realtime thread.
*/
template <typename Item>
struct MultipleReaderMultipleWriterFIFO
{
    MultipleReaderMultipleWriterFIFO() = default;
    ~MultipleReaderMultipleWriterFIFO() = default;

    /// Clears the FIFO and allocates a size for it.
    /// Note that this is not thread-safe with respect to the other methods - it must
    /// only be called when nothing else is modifying the FIFO.
    void reset (size_t numItems)                                { fifo.reset (numItems); }

    /// Clears the FIFO and allocates a size for it, filling the slots with
    /// copies of the given object.
    void reset (size_t numItems, const Item& itemInitialiser)   { fifo.reset (numItems, itemInitialiser); }

    /// Resets the FIFO, keeping the current size.
    void reset()                                                { fifo.reset(); }

    /// Returns the number of items in the FIFO.
    uint32_t getUsedSlots() const                               { return fifo.getUsedSlots(); }
    /// Returns the number of free slots in the FIFO.
    uint32_t getFreeSlots() const                               { return fifo.getFreeSlots(); }

    /// Attempts to push an into into the FIFO, returning false if no space was available.
    bool push (const Item& item)                                { return fifo.push (item); }

    /// Attempts to push an into into the FIFO, returning false if no space was available.
    bool push (Item&& item)                                     { return fifo.push (std::move (item)); }

    /// If any items are available, this copies the first into the given target, and returns true.
    bool pop (Item&);

private:
    choc::fifo::SingleReaderMultipleWriterFIFO<Item> fifo;
    choc::threading::SpinLock readLock;

    MultipleReaderMultipleWriterFIFO (const MultipleReaderMultipleWriterFIFO&) = delete;
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

template <typename Item> bool MultipleReaderMultipleWriterFIFO<Item>::pop (Item& result)
{
    const std::scoped_lock lock (readLock);
    return fifo.pop (result);
}

} // choc::fifo

#endif
