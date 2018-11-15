/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

struct MidiMessageArray
{
    using MPESourceID = juce::uint32;

    static MPESourceID createUniqueMPESourceID() noexcept
    {
        static MPESourceID i = 0;
        return ++i;
    }

    static constexpr MPESourceID notMPE = 0;

    struct MidiMessageWithSource  : public juce::MidiMessage
    {
        MidiMessageWithSource (const juce::MidiMessage& m, MPESourceID source) : juce::MidiMessage (m), mpeSourceID (source) {}
        MidiMessageWithSource (juce::MidiMessage&& m, MPESourceID source) : juce::MidiMessage (std::move (m)), mpeSourceID (source) {}

        MidiMessageWithSource (const MidiMessageWithSource&) = default;
        MidiMessageWithSource (MidiMessageWithSource&&) = default;
        MidiMessageWithSource& operator= (const MidiMessageWithSource&) = default;
        MidiMessageWithSource& operator= (MidiMessageWithSource&&) = default;

        MPESourceID mpeSourceID = 0;
    };

    bool isEmpty() const noexcept                                   { return messages.isEmpty(); }
    bool isNotEmpty() const noexcept                                { return ! messages.isEmpty(); }

    int size() const noexcept                                       { return messages.size(); }
    MidiMessageWithSource& operator[] (int i) const noexcept        { return messages.getReference(i); }

    MidiMessageWithSource* begin() const noexcept                   { return messages.begin(); }
    MidiMessageWithSource* end() const noexcept                     { return messages.end(); }

    void remove (int index) noexcept                                { messages.remove (index); }

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

    void mergeFrom (const MidiMessageArray& source)
    {
        if (source.isEmpty())
            return;

        isAllNotesOff = isAllNotesOff || source.isAllNotesOff;
        messages.ensureStorageAllocated (messages.size() + source.size());

        for (auto& m : source)
            messages.add (m);
    }

    void mergeFromAndClear (MidiMessageArray& source)
    {
        if (isEmpty())
        {
            swapWith (source);
        }
        else
        {
            isAllNotesOff = isAllNotesOff || source.isAllNotesOff;
            messages.ensureStorageAllocated (messages.size() + source.size());

            for (auto& m : source)
                messages.add (std::move (m));

            source.clear();
        }
    }

    void mergeFromAndClearWithOffset (MidiMessageArray& source, double delta)
    {
        if (isEmpty())
        {
            swapWith (source);
            addToTimestamps (delta);
        }
        else
        {
            isAllNotesOff = isAllNotesOff || source.isAllNotesOff;
            messages.ensureStorageAllocated (messages.size() + source.size());

            for (auto& m : source)
            {
                messages.add (std::move (m));
                messages.getReference (messages.size() - 1).addToTimeStamp (delta);
            }

            source.clear();
        }
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
            if (messages.getReference(i).isNoteOnOrOff())
                messages.remove (i);
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

    void sortByTimestamp() noexcept
    {
        struct MidiMessageSorter
        {
            static int compareElements (const juce::MidiMessage& first, const juce::MidiMessage& second) noexcept
            {
                return first.getTimeStamp() >= second.getTimeStamp() ? 1 : -1;
            }
        };

        MidiMessageSorter sorter;
        messages.sort (sorter);
    }

    void reserve (int size)
    {
        messages.ensureStorageAllocated (size);
    }

    bool isAllNotesOff = false;

private:
    juce::Array<MidiMessageWithSource> messages;
};

} // namespace tracktion_engine
