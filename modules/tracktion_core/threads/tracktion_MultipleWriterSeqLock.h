/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "../../3rd_party/crill/seqlock_object.h"
#include "../../tracktion_graph/utilities/tracktion_RealTimeSpinLock.h"

namespace tracktion { inline namespace core
{

/** Wraps a seqlock to allow a thread-safe object with wait-free reads with respect to each other.
    This version also allows multiple threads to write to the object but they will block each other.
*/
template<typename T>
class MultipleWriterSeqLock
{
public:
    // Creates a seqlock_object with a default-constructed value.
    MultipleWriterSeqLock() = default;

    // Creates a seqlock_object with the given value.
    MultipleWriterSeqLock(T t)
    {
        store(t);
    }

    // Reads and returns the current value.
    // Non-blocking guarantees: wait-free if there are no concurrent writes,
    // otherwise none.
    T load() const noexcept
    {
        return object.load();
    }

    // Attempts to read the current value and write it into the passed-in object.
    // Returns: true if the read succeeded, false otherwise.
    // Non-blocking guarantees: wait-free.
    bool try_load(T& t) const noexcept
    {
        return object.try_load (t);
    }

    // Updates the current value to the value passed in.
    // Non-blocking guarantees: wait-free for a single writer, blocking otherwise
    void store(T t) noexcept
    {
        std::unique_lock lock (mutex);
        object.store (t);
    }

private:
    RealTimeSpinLock mutex;
    crill::seqlock_object<T> object;
};

}} // namespace tracktion { inline namespace core
