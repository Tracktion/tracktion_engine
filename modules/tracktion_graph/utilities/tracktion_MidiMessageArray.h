/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

struct MidiMessageArray
{
    bool isEmpty() const noexcept                                   { return messages.isEmpty(); }
    bool isNotEmpty() const noexcept                                { return ! messages.isEmpty(); }

    int size() const noexcept                                       { return messages.size(); }
    MidiMessageWithSource& operator[] (int i)                       { return messages.getReference (i); }
    const MidiMessageWithSource& operator[] (int i) const           { return messages.getReference (i); }

    MidiMessageWithSource* begin() noexcept                         { return messages.begin(); }
    const MidiMessageWithSource* begin() const noexcept             { return messages.begin(); }
    MidiMessageWithSource* end() noexcept                           { return messages.end(); }
    const MidiMessageWithSource* end() const noexcept               { return messages.end(); }

    void remove (int index)                                         { messages.remove (index); }

    void swapWith (MidiMessageArray& other) noexcept
    {
        std::swap (isAllNotesOff, other.isAllNotesOff);
        messages.swapWith (other.messages);
    }

    void clear() noexcept
    {
        isAllNotesOff = false;
        messages.clearQuick();
    }

    void addMidiMessage (const juce::MidiMessage& m, MPESourceID mpeSourceID)
    {
        messages.add ({ m, mpeSourceID });
    }

    void addMidiMessage (juce::MidiMessage&& m, MPESourceID mpeSourceID)
    {
        messages.add ({ std::move (m), mpeSourceID });
    }

    void addMidiMessage (const juce::MidiMessage& m, double time, MPESourceID mpeSourceID)
    {
        messages.add ({ m, mpeSourceID });
        messages.getReference (messages.size() - 1).setTimeStamp (time);
    }

    void addMidiMessage (juce::MidiMessage&& m, double time, MPESourceID mpeSourceID)
    {
        messages.add ({ std::move (m), mpeSourceID });
        messages.getReference (messages.size() - 1).setTimeStamp (time);
    }

    void add (const MidiMessageWithSource& m)
    {
        messages.add (m);
    }

    void add (MidiMessageWithSource&& m)
    {
        messages.add (std::move (m));
    }

    void add (const MidiMessageWithSource& m, double time)
    {
        messages.add (m);
        messages.getReference (messages.size() - 1).setTimeStamp (time);
    }

    void add (MidiMessageWithSource&& m, double time)
    {
        messages.add (std::move (m));
        messages.getReference (messages.size() - 1).setTimeStamp (time);
    }

    void copyFrom (const MidiMessageArray& source)
    {
        clear();
        mergeFrom (source);
    }

    void mergeFrom (const MidiMessageArray& source)
    {
        isAllNotesOff = isAllNotesOff || source.isAllNotesOff;

        if (source.isEmpty())
            return;

        messages.ensureStorageAllocated (messages.size() + source.size());

        for (auto& m : source)
            messages.add (m);
    }

    void mergeFromWithOffset (const MidiMessageArray& source, double delta)
    {
        isAllNotesOff = isAllNotesOff || source.isAllNotesOff;

        if (source.isEmpty())
            return;

        messages.ensureStorageAllocated (messages.size() + source.size());

        for (const auto& m : source)
        {
            auto copy = tracktion::engine::MidiMessageWithSource (m);
            copy.addToTimeStamp (delta);
            messages.add (std::move (copy));
        }
    }

    void mergeFromAndClear (MidiMessageArray& source)
    {
        isAllNotesOff = isAllNotesOff || source.isAllNotesOff;

        if (isEmpty())
        {
            swapWith (source);
        }
        else
        {
            messages.ensureStorageAllocated (messages.size() + source.size());

            for (auto& m : source)
                messages.add (std::move (m));
        }

        source.clear();
    }

    void mergeFromAndClearWithOffset (MidiMessageArray& source, double delta)
    {
        isAllNotesOff = isAllNotesOff || source.isAllNotesOff;

        if (isEmpty())
        {
            swapWith (source);
            addToTimestamps (delta);
        }
        else
        {
            messages.ensureStorageAllocated (messages.size() + source.size());

            for (auto& m : source)
            {
                messages.add (std::move (m));
                messages.getReference (messages.size() - 1).addToTimeStamp (delta);
            }
        }

        source.clear();
    }

    void mergeFromAndClearWithOffsetAndLimit (MidiMessageArray& source, double delta, int numItemsToTake)
    {
        if (numItemsToTake >= source.size())
            return mergeFromAndClearWithOffset (source, delta);

        isAllNotesOff = isAllNotesOff || source.isAllNotesOff;
        messages.ensureStorageAllocated (messages.size() + numItemsToTake);

        for (int i = 0; i < numItemsToTake; ++i)
        {
            messages.add (std::move (source.messages.getReference(i)));
            messages.getReference (messages.size() - 1).addToTimeStamp (delta);
        }

        source.messages.removeRange (0, numItemsToTake);
    }

    void mergeFromAndClear (juce::Array<juce::MidiMessage>& source, MPESourceID mpeSourceID)
    {
        messages.ensureStorageAllocated (messages.size() + source.size());

        for (auto& m : source)
            addMidiMessage (m, mpeSourceID);

        source.clear();
    }

    void removeNoteOnsAndOffs()
    {
        for (int i = messages.size(); --i >= 0;)
            if (messages.getReference (i).isNoteOnOrOff())
                messages.remove (i);
    }

    /// Removes any notes that match the given predicate
    template <typename Predicate>
    void removeIf (Predicate&& pred)
    {
        messages.removeIf (pred);
    }

    void addToTimestamps (double delta) noexcept
    {
        for (auto& m : messages)
            m.addToTimeStamp (delta);
    }

    void addToNoteNumbers (int delta) noexcept
    {
        for (auto& m : messages)
            m.setNoteNumber (m.getNoteNumber() + delta);
    }

    void multiplyVelocities (float factor) noexcept
    {
        for (auto& m : messages)
            m.multiplyVelocity (factor);
    }

    void sortByTimestamp()
    {
        choc::sorting::stable_sort (messages.begin(), messages.end(), [] (const juce::MidiMessage& a, const juce::MidiMessage& b)
        {
            auto t1 = a.getTimeStamp();
            auto t2 = b.getTimeStamp();

            if (t1 == t2)
            {
                if (a.isNoteOff() && b.isNoteOn()) return true;
                if (a.isNoteOn() && b.isNoteOff()) return false;
            }
            return t1 < t2;
        });
    }

    void reserve (int size)
    {
        messages.ensureStorageAllocated (size);
    }

    bool isAllNotesOff = false;


    //==============================================================================
   #if ! JUCE_GCC
    using MidiMessageWithSource [[ deprecated("This type has moved into the parent namespace") ]] = tracktion::engine::MidiMessageWithSource;
   #endif
    using MPESourceID [[ deprecated("This type has moved into the parent namespace") ]] = tracktion::engine::MPESourceID;
    [[ deprecated("This function has moved into the parent namespace") ]] static tracktion::engine::MPESourceID createUniqueMPESourceID() { return tracktion::engine::createUniqueMPESourceID(); }
    [[ deprecated("Just use a default-constructed MPESourceID instead of this") ]] static constexpr tracktion::engine::MPESourceID notMPE = {};

private:
    juce::Array<tracktion::engine::MidiMessageWithSource> messages;
};

}} // namespace tracktion
