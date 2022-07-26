/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

struct ActiveNoteList
{
    juce::uint16 activeChannels[128] = {};

    void reset() noexcept
    {
        std::memset (activeChannels, 0, sizeof (activeChannels));
    }

    bool isNoteActive (int channel, int note) const noexcept
    {
        return isValidIndex (channel, note)
                && (activeChannels[note] & (1u << (channel - 1))) != 0;
    }

    void startNote (int channel, int note) noexcept
    {
        if (isValidIndex (channel, note))
            activeChannels[note] |= (1u << (channel - 1));
    }

    void clearNote (int channel, int note) noexcept
    {
        if (isValidIndex (channel, note))
            activeChannels[note] &= ~(1u << (channel - 1));
    }

    bool areAnyNotesActive() const noexcept
    {
        juce::uint16 result = 0;

        for (auto a : activeChannels)
            result |= a;

        return result > 0;
    }

    template <typename Visitor>
    void iterate (Visitor&& v) const noexcept
    {
        for (int note = 0; note < 128; ++note)
            for (int chan = 0; chan < 16; ++chan)
                if ((activeChannels[note] & (1u << chan)) != 0)
                    v (chan + 1, note);
    }

    static bool isValidIndex (int channel, int note) noexcept
    {
        return juce::isPositiveAndBelow (note, 128) && channel > 0 && channel <= 16;
    }
};

}} // namespace tracktion { inline namespace engine
