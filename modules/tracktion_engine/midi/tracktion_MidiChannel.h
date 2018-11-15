/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


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
    explicit MidiChannel (int channelNumber1to16) noexcept  : channel ((juce::uint8) channelNumber1to16)
    {
        jassert (channelNumber1to16 > 0 && channelNumber1to16 <= 16);
    }

    explicit MidiChannel (const juce::var& storedChannel) noexcept
        : channel ((juce::uint8) static_cast<int> (storedChannel))
    {
        if (! isValid())
            channel = 1;
    }

    static MidiChannel fromChannelOrZero (int channel) noexcept
    {
        jassert (channel >= 0 && channel <= 16);
        MidiChannel m;
        m.channel = (juce::uint8) channel;
        return m;
    }

    // Returns 1-16 or 0 for invalid
    int getChannelNumber() const noexcept                       { return (int) channel; }

    bool isValid() const noexcept                               { return channel != 0; }

    bool operator== (const MidiChannel& other) const noexcept   { return channel == other.channel; }
    bool operator!= (const MidiChannel& other) const noexcept   { return channel != other.channel; }

    operator juce::var() const                                  { return juce::var (getChannelNumber()); }

private:
    juce::uint8 channel = 0;
};
