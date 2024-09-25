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

using MPESourceID = uint32_t;


struct MidiMessageWithSource  : public juce::MidiMessage
{
    MidiMessageWithSource (const juce::MidiMessage& m, MPESourceID source) : juce::MidiMessage (m), mpeSourceID (source) {}
    MidiMessageWithSource (juce::MidiMessage&& m, MPESourceID source) : juce::MidiMessage (std::move (m)), mpeSourceID (source) {}

    MidiMessageWithSource (const MidiMessageWithSource&) = default;
    MidiMessageWithSource (MidiMessageWithSource&&) = default;
    MidiMessageWithSource& operator= (const MidiMessageWithSource&) = default;
    MidiMessageWithSource& operator= (MidiMessageWithSource&&) = default;

    MPESourceID mpeSourceID = {};
};

inline static MPESourceID createUniqueMPESourceID() noexcept
{
    static MPESourceID i = 0;
    return ++i;
}

[[ deprecated ]]
static constexpr MPESourceID notMPE = 0;


}} // namespace tracktion
