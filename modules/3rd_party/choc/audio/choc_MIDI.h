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

#ifndef CHOC_MIDI_HEADER_INCLUDED
#define CHOC_MIDI_HEADER_INCLUDED

#include <string>
#include <cmath>
#include "../platform/choc_Assert.h"

namespace choc::midi
{

static constexpr float  A440_frequency   = 440.0f;
static constexpr int    A440_noteNumber  = 69;

/// Converts a MIDI note (usually in the range 0-127) to a frequency in Hz.
inline float noteNumberToFrequency (int note)          { return A440_frequency * std::pow (2.0f, (static_cast<float> (note) - A440_noteNumber) * (1.0f / 12.0f)); }
/// Converts a MIDI note (usually in the range 0-127) to a frequency in Hz.
inline float noteNumberToFrequency (float note)        { return A440_frequency * std::pow (2.0f, (note - static_cast<float> (A440_noteNumber)) * (1.0f / 12.0f)); }
/// Converts a frequency in Hz to an equivalent MIDI note number.
inline float frequencyToNoteNumber (float frequency)   { return static_cast<float> (A440_noteNumber) + (12.0f / std::log (2.0f)) * std::log (frequency * (1.0f / A440_frequency)); }

/// Returns the name for a MIDI controller number.
inline std::string getControllerName (uint8_t controllerNumber);

/// Returns a space-separated string of hex digits in a fomrat that's appropriate for a MIDI data dump.
inline std::string printHexMIDIData (const uint8_t* data, size_t numBytes);

//==============================================================================
/**
    A class to hold a 0-127 MIDI note number, which provides some helpful methods.
*/
struct NoteNumber
{
    /// The MIDI note number, which must be in the range 0-127.
    uint8_t note;

    /// A NoteNumber can be cast to an integer to get the raw MIDI note number.
    operator uint8_t() const                                    { return note; }

    /// Returns this note's position within an octave, 0-11, where C is 0.
    uint8_t getChromaticScaleIndex() const                      { return note % 12; }

    /// Returns the note's octave number.
    int getOctaveNumber (int octaveForMiddleC = 3) const        { return note / 12 + (octaveForMiddleC - 5); }

    /// Returns the note as a frequency in Hertz.
    float getFrequency() const                                  { return noteNumberToFrequency (static_cast<int> (note)); }

    /// Returns the note name, adding sharps and flats where necessary.
    std::string_view getName() const                            { return std::addressof ("C\0\0C#\0D\0\0Eb\0E\0\0F\0\0F#\0G\0\0G#\0A\0\0Bb\0B"[3 * getChromaticScaleIndex()]); }
    /// Returns the note name, adding sharps where necessary.
    std::string_view getNameWithSharps() const                  { return std::addressof ("C\0\0C#\0D\0\0D#\0E\0\0F\0\0F#\0G\0\0G#\0A\0\0A#\0B"[3 * getChromaticScaleIndex()]); }
    /// Returns the note name, adding flats where necessary.
    std::string_view getNameWithFlats() const                   { return std::addressof ("C\0\0Db\0D\0\0Eb\0E\0\0F\0\0Gb\0G\0\0Ab\0A\0\0Bb\0B"[3 * getChromaticScaleIndex()]); }
    /// Returns the note name and octave number (using default choices for things like sharp/flat/octave number).
    std::string getNameWithOctaveNumber() const                 { return std::string (getName()) + std::to_string (getOctaveNumber()); }

    /// Returns true if this is a natural note in the C major scale.
    bool isNatural() const                                      { return (0b101010110101 & (1 << getChromaticScaleIndex())) != 0; }
    /// Returns true if this is an accidental note, i.e. a sharp or flat.
    bool isAccidental() const                                   { return ! isNatural(); }
};

//==============================================================================
/**
    A MIDI message.

    The storage type template parameter lets this class use different policies for
    storing the actual MIDI data. For short messages up to 3-bytes, you can use a
    `Message<ShortMIDIMessageStorage>` (aka `ShortMessage`), and for longer ones, a
    `Message<StringMIDIDataStorage>` (aka `LongMessage`). The `MessageView` type
    lets you create messages that do not own their data, but just point to some
    memory.
*/
template <typename StorageType>
struct Message
{
    Message() = default;
    Message (uint8_t byte0, uint8_t byte1, uint8_t byte2)  : midiData { byte0, byte1, byte2 } {}
    Message (const void* sourceData, size_t numBytes)      : midiData (sourceData, numBytes) {}

    template <typename OtherStorage> Message (const Message<OtherStorage>&);
    template <typename OtherStorage> Message& operator= (const Message<OtherStorage>&);

    const uint8_t* data() const                         { return midiData.data(); }

    /// Returns true if this is an empty, uninitialised message.
    bool isNull() const                                 { return midiData.empty(); }

    /// Returns the size of the message in bytes.
    uint32_t length() const                             { return midiData.size(); }
    /// Returns the size of the message in bytes.
    uint32_t size() const                               { return midiData.size(); }

    uint8_t getChannel0to15() const                     { return midiData[0] & 0x0f; }
    uint8_t getChannel1to16() const                     { return static_cast<uint8_t> (getChannel0to15() + 1u); }

    bool isNoteOn() const                               { return isVoiceMessage (0x90) && getVelocity() != 0; }
    bool isNoteOff() const                              { return isVoiceMessage (0x80) || (getVelocity() == 0 && isVoiceMessage (0x90)); }
    NoteNumber getNoteNumber() const                    { return { midiData[1] }; }
    uint8_t getVelocity() const                         { return midiData[2]; }

    bool isProgramChange() const                        { return isVoiceMessage (0xc0); }
    uint8_t getProgramChangeNumber() const              { return midiData[1]; }

    bool isPitchWheel() const                           { return isVoiceMessage (0xe0); }
    uint32_t getPitchWheelValue() const                 { return get14BitValue(); }
    bool isAftertouch() const                           { return isVoiceMessage (0xa0); }
    uint8_t getAfterTouchValue() const                  { return midiData[2]; }

    bool isChannelPressure() const                      { return isVoiceMessage (0xd0); }
    uint8_t getChannelPressureValue() const             { return midiData[1]; }

    bool isController() const                           { return isVoiceMessage (0xb0); }
    uint8_t getControllerNumber() const                 { return midiData[1]; }
    uint8_t getControllerValue() const                  { return midiData[2]; }
    bool isControllerNumber (uint8_t number) const      { return midiData[1] == number && isController(); }
    bool isAllNotesOff() const                          { return isControllerNumber (123); }
    bool isAllSoundOff() const                          { return isControllerNumber (120); }

    bool isQuarterFrame() const                         { return midiData[0] == 0xf1; }
    bool isClock() const                                { return midiData[0] == 0xf8; }
    bool isStart() const                                { return midiData[0] == 0xfa; }
    bool isContinue() const                             { return midiData[0] == 0xfb; }
    bool isStop() const                                 { return midiData[0] == 0xfc; }
    bool isActiveSense() const                          { return midiData[0] == 0xfe; }

    bool isSongPositionPointer() const                  { return midiData[0] == 0xf2; }
    uint32_t getSongPositionPointerValue() const        { return get14BitValue(); }

    /// Returns true if this is a short message (up to 3 bytes).
    bool isShortMessage() const;
    /// Returns true if this is a sysex.
    bool isSysex() const;

    /// Returns true if this is a meta-event.
    bool isMetaEvent() const;
    /// Returns true if this is a meta-event with the given type.
    bool isMetaEventOfType (uint8_t type) const;
    /// If this is a meta-event, this will return its meta-type byte. If it isn't
    /// a meta-event, this will trigger an assertion, so you should check beforehand.
    uint8_t getMetaEventType() const;
    /// If this is a meta-event, this will return a description of its type. If it isn't
    /// a meta-event, this will trigger an assertion, so you should check beforehand.
    std::string getMetaEventTypeName() const;
    /// If this is a meta-event, this will return the payload data (i.e. the chunk of
    /// variable-length data after the type and length fields). If it isn't a meta-event,
    /// this will trigger an assertion, so you should check beforehand. If the message
    /// data is malformed, this may return an empty value.
    std::string_view getMetaEventData() const;

    /// Returns a human-readable description of the message.
    std::string getDescription() const;

    /// Returns a hex string dump of the message.
    std::string toHexString() const;

    template <typename OtherStorage> bool operator== (const Message<OtherStorage>&) const;
    template <typename OtherStorage> bool operator!= (const Message<OtherStorage>&) const;

    StorageType midiData;

private:
    bool isVoiceMessage (uint8_t type) const            { return (midiData[0] & 0xf0) == type; }
    uint32_t get14BitValue() const                      { return midiData[1] | (static_cast<uint32_t> (midiData[2]) << 7); }
};


//==============================================================================
/// A storage type for `choc::midi::Message` which holds only short MIDI messages,
/// up to 3 bytes in length.
/// If you're dealing only with short MIDI messages, then a
/// `Message<ShortMIDIMessageStorage>` is allocation-free and very fast, but will
/// assert if you try to create one with over 3 bytes of data.
/// For longer messages, see other classes such as `StringMIDIDataStorage`.
/// @see ShortMessage
struct ShortMIDIMessageStorage
{
    ShortMIDIMessageStorage() = default;
    ShortMIDIMessageStorage (const void* data, size_t size);
    ShortMIDIMessageStorage (uint8_t byte0, uint8_t byte1, uint8_t byte2);

    uint8_t* data()                         { return bytes; }
    const uint8_t* data() const             { return bytes; }
    uint8_t operator[] (uint32_t i) const   { return bytes[i]; }
    uint32_t size() const;
    bool empty() const                      { return bytes[0] == 0; }

    uint8_t bytes[3] = {};
    static constexpr bool isShortMessage = true;
};

//==============================================================================
/// A storage type for `choc::midi::Message` which stores the data in a std::string.
/// This will have to heap-allocate for longer messages, but most std::string
/// implementations use a short-string optimisation so that most typical short
/// MIDI messages will be stored efficiently without allocation being needed.
struct StringMIDIDataStorage
{
    StringMIDIDataStorage() = default;
    StringMIDIDataStorage (const void* data, size_t size);
    StringMIDIDataStorage (uint8_t byte0, uint8_t byte1, uint8_t byte2);

    uint8_t* data()                                 { return reinterpret_cast<uint8_t*> (storage.data()); }
    const uint8_t* data() const                     { return reinterpret_cast<const uint8_t*> (storage.data()); }
    uint8_t operator[] (uint32_t i) const           { CHOC_ASSERT (i < storage.length()); return static_cast<uint8_t> (storage[i]); }
    uint32_t size() const                           { return static_cast<uint32_t> (storage.size()); }
    bool empty() const                              { return storage.empty(); }

    std::string storage;
    static constexpr bool isShortMessage = false;
};

//==============================================================================
/// A storage type for `choc::midi::Message` which simply points to a block of
/// const data that is owned by something else.
/// If you want to create a `choc::midi::Message` that is simply a view onto a
/// chunk of data that is managed externally, then you can do that with a
/// `Message<MIDIMessageStorageView>`.
/// If you want the Message object to own its own storage, then use one of the
/// other types such as `ShortMIDIMessageStorage` or `StringMIDIDataStorage`.
struct MIDIMessageStorageView
{
    MIDIMessageStorageView() = default;
    MIDIMessageStorageView (const void* data, size_t size)
        : midiData (static_cast<const uint8_t*> (data)), dataSize (static_cast<uint32_t> (size)) {}

    const uint8_t* data() const             { return midiData; }
    uint8_t operator[] (uint32_t i) const   { return midiData[i]; }
    uint32_t size() const                   { return dataSize; }
    bool empty() const                      { return dataSize == 0; }

    const uint8_t* midiData = {};
    uint32_t dataSize = 0;
    static constexpr bool isShortMessage = false;
};

//==============================================================================
/// This type can hold MIDI messages up to 3 bytes in length.
/// @see Message, ShortMIDIMessageStorage
using ShortMessage = Message<ShortMIDIMessageStorage>;

/// This type can hold MIDI messages of any length, using a std::string to
/// allocate and manage the storage.
/// @see Message, StringMIDIDataStorage
using LongMessage = Message<StringMIDIDataStorage>;

/// This type holds MIDI messages that point to midi data that is owned by
/// some other object.
/// @see Message, MIDIMessageStorageView
using MessageView = Message<MIDIMessageStorageView>;






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

static constexpr uint8_t sysexStartByte      = 0xf0;
static constexpr uint8_t metaEventStartByte  = 0xff;

inline ShortMIDIMessageStorage::ShortMIDIMessageStorage (uint8_t byte0, uint8_t byte1, uint8_t byte2)
{
    bytes[0] = byte0;
    bytes[1] = byte1;
    bytes[2] = byte2;
}

inline ShortMIDIMessageStorage::ShortMIDIMessageStorage (const void* sourceData, size_t numBytes)
{
    if (numBytes > 0)
    {
        auto source = static_cast<const uint8_t*> (sourceData);
        bytes[0] = source[0];

        if (numBytes > 1)
        {
            bytes[1] = source[1];

            if (numBytes > 2)
            {
                bytes[2] = source[2];

                // This storage type can only hold short messages! For longer ones, use
                // something like StringMIDIDataStorage instead.
                CHOC_ASSERT (numBytes == 3);
            }
        }
    }
}

inline uint32_t ShortMIDIMessageStorage::size() const
{
    constexpr uint32_t mainGroupLengths = (3u << 0) | (3u << 2) | (3u << 4) | (3u << 6)
                                        | (2u << 8) | (2u << 10) | (3u << 12);

    constexpr uint32_t lastGroupLengths = (1u <<  0) | (2u <<  2) | (3u <<  4) | (2u <<  6)
                                        | (1u <<  8) | (1u << 10) | (1u << 12) | (1u << 14)
                                        | (1u << 16) | (1u << 18) | (1u << 20) | (1u << 22)
                                        | (1u << 24) | (1u << 26) | (1u << 28) | (1u << 30);

    auto firstByte = bytes[0];
    auto group = static_cast<uint8_t> ((firstByte >> 4) & 7);

    return (group != 7u ? (mainGroupLengths >> (2u * group))
                        : (lastGroupLengths >> (2u * (firstByte & 15u)))) & 3u;
}

inline StringMIDIDataStorage::StringMIDIDataStorage (const void* data, size_t size)
    : storage (static_cast<const char*> (data), size)
{}

inline StringMIDIDataStorage::StringMIDIDataStorage (uint8_t byte0, uint8_t byte1, uint8_t byte2)
{
    ShortMIDIMessageStorage m (byte0, byte1, byte2);
    storage.append (reinterpret_cast<const char*> (m.data()), m.size());
}


template <typename StorageType>
template <typename OtherStorage>
Message<StorageType>::Message (const Message<OtherStorage>& other) : Message (other.data(), other.size()) {}

template <typename StorageType>
template <typename OtherStorage>
Message<StorageType>& Message<StorageType>::operator= (const Message<OtherStorage>& other)
{
    midiData = StorageType (other.data(), other.size());
    return *this;
}

template <typename StorageType>
template <typename OtherStorage>
bool Message<StorageType>::operator== (const Message<OtherStorage>& other) const
{
    auto len1 = size();
    auto len2 = other.size();
    return len1 == len2 && memcmp (data(), other.data(), len1) == 0;
}

template <typename StorageType>
template <typename OtherStorage>
bool Message<StorageType>::operator!= (const Message<OtherStorage>& other) const
{
    return ! operator== (other);
}

template <typename StorageType>
inline std::string Message<StorageType>::getDescription() const
{
    auto channelText = " Channel " + std::to_string (getChannel1to16());
    auto getNote = [this] { auto s = getNoteNumber().getNameWithOctaveNumber(); return s.length() < 4 ? s + " " : s; };

    if (isNoteOn())                return "Note-On:  "   + getNote() + channelText + "  Velocity " + std::to_string (getVelocity());
    if (isNoteOff())               return "Note-Off: "   + getNote() + channelText + "  Velocity " + std::to_string (getVelocity());
    if (isAftertouch())            return "Aftertouch: " + getNote() + channelText +  ": " + std::to_string (getAfterTouchValue());
    if (isPitchWheel())            return "Pitch wheel: " + std::to_string (getPitchWheelValue()) + ' ' + channelText;
    if (isChannelPressure())       return "Channel pressure: " + std::to_string (getChannelPressureValue()) + ' ' + channelText;
    if (isController())            return "Controller:" + channelText + ": " + getControllerName (getControllerNumber()) + " = " + std::to_string (getControllerValue());
    if (isProgramChange())         return "Program change: " + std::to_string (getProgramChangeNumber()) + ' ' + channelText;
    if (isAllNotesOff())           return "All notes off:" + channelText;
    if (isAllSoundOff())           return "All sound off:" + channelText;
    if (isQuarterFrame())          return "Quarter-frame";
    if (isClock())                 return "Clock";
    if (isStart())                 return "Start";
    if (isContinue())              return "Continue";
    if (isStop())                  return "Stop";
    if (isMetaEvent())             return "Meta-event: type " + std::to_string (midiData[1]);
    if (isSongPositionPointer())   return "Song Position: " + std::to_string (getSongPositionPointerValue());
    if (isSysex())                 return "Sysex: " + toHexString();

    if (isMetaEvent())
    {
        auto metadataContent = getMetaEventData();

        return "Meta-event: " + getMetaEventTypeName()
                 + ", length: " + std::to_string (metadataContent.length())
                 + ", data: " + printHexMIDIData (reinterpret_cast<const uint8_t*> (metadataContent.data()),
                                                  metadataContent.length());
    }

    return toHexString();
}

inline std::string printHexMIDIData (const uint8_t* data, size_t numBytes)
{
    if (numBytes == 0)
        return "[empty]";

    std::string s;
    s.reserve (3 * numBytes);

    for (size_t i = 0; i < numBytes; ++i)
    {
        if (i != 0)  s += ' ';

        s += "0123456789abcdef"[data[i] >> 4];
        s += "0123456789abcdef"[data[i] & 15];
    }

    return s;
}

inline std::string getControllerName (uint8_t controllerNumber)
{
    if (controllerNumber < 128)
    {
        static constexpr const char* controllerNames[128] =
        {
            "Bank Select",                  "Modulation Wheel (coarse)",      "Breath controller (coarse)",       nullptr,
            "Foot Pedal (coarse)",          "Portamento Time (coarse)",       "Data Entry (coarse)",              "Volume (coarse)",
            "Balance (coarse)",             nullptr,                          "Pan position (coarse)",            "Expression (coarse)",
            "Effect Control 1 (coarse)",    "Effect Control 2 (coarse)",      nullptr,                            nullptr,
            "General Purpose Slider 1",     "General Purpose Slider 2",       "General Purpose Slider 3",         "General Purpose Slider 4",
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            "Bank Select (fine)",           "Modulation Wheel (fine)",        "Breath controller (fine)",         nullptr,
            "Foot Pedal (fine)",            "Portamento Time (fine)",         "Data Entry (fine)",                "Volume (fine)",
            "Balance (fine)",               nullptr,                          "Pan position (fine)",              "Expression (fine)",
            "Effect Control 1 (fine)",      "Effect Control 2 (fine)",        nullptr,                            nullptr,
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            "Hold Pedal",                   "Portamento",                     "Sustenuto Pedal",                  "Soft Pedal",
            "Legato Pedal",                 "Hold 2 Pedal",                   "Sound Variation",                  "Sound Timbre",
            "Sound Release Time",           "Sound Attack Time",              "Sound Brightness",                 "Sound Control 6",
            "Sound Control 7",              "Sound Control 8",                "Sound Control 9",                  "Sound Control 10",
            "General Purpose Button 1",     "General Purpose Button 2",       "General Purpose Button 3",         "General Purpose Button 4",
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            nullptr,                        nullptr,                          nullptr,                            "Reverb Level",
            "Tremolo Level",                "Chorus Level",                   "Celeste Level",                    "Phaser Level",
            "Data Button increment",        "Data Button decrement",          "Non-registered Parameter (fine)",  "Non-registered Parameter (coarse)",
            "Registered Parameter (fine)",  "Registered Parameter (coarse)",  nullptr,                            nullptr,
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            nullptr,                        nullptr,                          nullptr,                            nullptr,
            "All Sound Off",                "All Controllers Off",            "Local Keyboard",                   "All Notes Off",
            "Omni Mode Off",                "Omni Mode On",                   "Mono Operation",                   "Poly Operation"
        };

        if (auto name = controllerNames[controllerNumber])
            return name;
    }

    return std::to_string (controllerNumber);
}

//==============================================================================
template <typename StorageType>
bool Message<StorageType>::isShortMessage() const
{
    if constexpr (StorageType::isShortMessage)
    {
        return true;
    }
    else
    {
        auto len = midiData.size();

        if (len == 0 || len > 3)
            return false;

        auto firstByte = midiData[0];
        return firstByte != sysexStartByte && firstByte != metaEventStartByte;
    }
}

template <typename StorageType>
bool Message<StorageType>::isSysex() const                         { return midiData.size() > 1 && midiData[0] == sysexStartByte; }
template <typename StorageType>
bool Message<StorageType>::isMetaEvent() const                     { return midiData.size() > 2 && midiData[0] == metaEventStartByte; }
template <typename StorageType>
bool Message<StorageType>::isMetaEventOfType (uint8_t type) const  { return midiData.size() > 2 && midiData[1] == type && midiData[0] == metaEventStartByte; }

template <typename StorageType>
uint8_t Message<StorageType>::getMetaEventType() const
{
    CHOC_ASSERT (isMetaEvent()); // You must check that this is a meta-event before calling this method
    return data()[1];
}

template <typename StorageType>
std::string Message<StorageType>::getMetaEventTypeName() const
{
    auto type = getMetaEventType();
    const char* result = nullptr;

    switch (type)
    {
        case 0x00:  result = "Sequence number";     break;
        case 0x01:  result = "Text";                break;
        case 0x02:  result = "Copyright notice";    break;
        case 0x03:  result = "Track name";          break;
        case 0x04:  result = "Instrument name";     break;
        case 0x05:  result = "Lyrics";              break;
        case 0x06:  result = "Marker";              break;
        case 0x07:  result = "Cue point";           break;
        case 0x20:  result = "Channel prefix";      break;
        case 0x2F:  result = "End of track";        break;
        case 0x51:  result = "Set tempo";           break;
        case 0x54:  result = "SMPTE offset";        break;
        case 0x58:  result = "Time signature";      break;
        case 0x59:  result = "Key signature";       break;
        case 0x7F:  result = "Sequencer specific";  break;
        default:    return std::to_string (type);
    }

    return result;
}

template <typename StorageType>
std::string_view Message<StorageType>::getMetaEventData() const
{
    CHOC_ASSERT (isMetaEvent()); // You must check that this is a meta-event before calling this method

    auto totalLength = length();

    if (totalLength < 4)
        return {}; // malformed data

    uint32_t contentLength = 0, lengthBytes = 0;
    auto d = data() + 2; // skip to the length field

    for (;;)
    {
        auto byte = *d++;
        ++lengthBytes;
        contentLength = (contentLength << 7) | (byte & 0x7fu);

        if (byte < 0x80)
            break;

        if (lengthBytes == 4 || lengthBytes + 2 == totalLength)
            return {}; // malformed data
    }

    auto contentStart = lengthBytes + 2;

    if (contentStart + contentLength > totalLength)
        return {}; // malformed data

    return std::string_view (reinterpret_cast<const char*> (data()) + contentStart, contentLength);
}

template <typename StorageType>
std::string Message<StorageType>::toHexString() const     { return printHexMIDIData (data(), length()); }


} // namespace choc::midi

#endif
