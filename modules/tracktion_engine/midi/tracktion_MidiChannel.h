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

/**
    Represents a MIDI channel 1-16, and also contains extra ID bits to encode
    info about the event's origin.
*/
struct MidiChannel
{
    MidiChannel() = default;
    MidiChannel (const MidiChannel&) = default;
    MidiChannel& operator= (const MidiChannel&) = default;

    // Takes a number 1-16
    explicit MidiChannel (int channelNumber1to16) noexcept  : channel ((uint8_t) channelNumber1to16)
    {
        jassert (channelNumber1to16 > 0 && channelNumber1to16 <= 16);
    }

    explicit MidiChannel (const juce::var& storedChannel) noexcept
        : channel ((uint8_t) static_cast<int> (storedChannel))
    {
        if (! isValid())
            channel = 1;
    }

    static MidiChannel fromChannelOrZero (int channel) noexcept
    {
        jassert (channel >= 0 && channel <= 16);
        MidiChannel m;
        m.channel = (uint8_t) channel;
        return m;
    }

    // Returns 1-16 or 0 for invalid
    int getChannelNumber() const noexcept                       { return (int) channel; }

    bool isValid() const noexcept                               { return channel != 0; }

    bool operator== (const MidiChannel& other) const noexcept   { return channel == other.channel; }
    bool operator!= (const MidiChannel& other) const noexcept   { return channel != other.channel; }

    operator juce::var() const                                  { return juce::var (getChannelNumber()); }

private:
    uint8_t channel = 0;
};

}} // namespace tracktion { inline namespace engine
