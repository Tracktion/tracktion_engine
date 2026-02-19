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

#ifndef CHOC_MIDIFILE_HEADER_INCLUDED
#define CHOC_MIDIFILE_HEADER_INCLUDED

#include <stdexcept>
#include <functional>
#include "choc_MIDISequence.h"

namespace choc::midi
{

//==============================================================================
/**
    A reader for MIDI (.mid) file data.
*/
struct File
{
    File() = default;
    ~File() = default;

    void clear();

    /// Attempts to load the given data as a MIDI file. If errors occur,
    /// a std::runtime_error exception will be thrown.
    void load (const void* midiFileData, size_t dataSize);

    /// Attempts to save the current state to a block of data which can be written to a file.
    /// If errors occur, a std::runtime_error exception will be thrown.
    std::vector<uint8_t> save() const;

    /// Creates a MIDI file from a sequence.
    File (const choc::midi::Sequence& sequence);

    struct Event
    {
        LongMessage message;
        uint32_t tickPosition = 0;
    };

    struct Track
    {
        std::vector<Event> events;
    };

    /// Iterates all the events on all tracks, returning each one with its playback time in seconds.
    void iterateEvents (const std::function<void(const LongMessage&, double timeInSeconds)>&) const;

    /// Merges all the events from this file into a single MIDI Sequence object.
    choc::midi::Sequence toSequence() const;

    //==============================================================================
    std::vector<Track> tracks;

    /// This is the standard MIDI file time format:
    ///  If positive, this is the number of ticks per quarter-note.
    ///  If negative, this is a SMPTE timecode type
    int16_t timeFormat = 60;
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

namespace
{
    struct Reader
    {
        const uint8_t* data;
        size_t size;

        void expectSize (size_t num)
        {
            if (size < num)
                throw std::runtime_error ("Unexpected end-of-file");
        }

        void skip (size_t num)
        {
            data += num;
            size -= num;
        }

        template <typename Type>
        Type read()
        {
            uint32_t n = 0;
            expectSize (sizeof (Type));

            for (size_t i = 0; i < sizeof (Type); ++i)
                n = (n << 8) | data[i];

            skip (sizeof (Type));
            return static_cast<Type> (n);
        }

        std::string_view readString (size_t length)
        {
            expectSize (length);
            std::string_view s (reinterpret_cast<const char*> (data), length);
            skip (length);
            return s;
        }

        uint32_t readVariableLength()
        {
            uint32_t n = 0, numUsed = 0;

            for (;;)
            {
                auto byte = read<uint8_t>();
                n = (n << 7) | (byte & 0x7fu);

                if (byte < 0x80)
                    return n;

                if (++numUsed == 4)
                    throw std::runtime_error ("Error in variable-length integer");
            }
        }
    };

    //==============================================================================
    struct Header
    {
        uint16_t fileType = 0, numTracks = 0, timeFormat = 0;
    };

    inline static Header readHeader (Reader& reader)
    {
        auto chunkName = reader.readString (4);

        if (chunkName == "RIFF")
        {
            for (int i = 8; --i >= 0;)
            {
                chunkName = reader.readString (4);

                if (chunkName == "MThd")
                    break;
            }
        }

        if (chunkName != "MThd")
            throw std::runtime_error ("Unknown chunk type");

        auto length = reader.read<uint32_t>();
        reader.expectSize (length);

        Header header;
        header.fileType   = reader.read<uint16_t>();
        header.numTracks  = reader.read<uint16_t>();
        header.timeFormat = reader.read<uint16_t>();

        if (header.fileType > 2)
            throw std::runtime_error ("Unknown file type");

        if (header.fileType == 0 && header.numTracks != 1)
            throw std::runtime_error ("Unsupported number of tracks");

        return header;
    }

    //==============================================================================
    inline static std::vector<File::Event> readTrack (Reader& reader)
    {
        std::vector<File::Event> result;
        uint32_t tickPosition = 0;
        uint8_t statusByte = 0;

        while (reader.size > 0)
        {
            auto interval = reader.readVariableLength();
            tickPosition += interval;

            if (reader.data[0] >= 0x80)
            {
                statusByte = reader.data[0];
                reader.skip(1);
            }

            if (statusByte < 0x80)
                throw std::runtime_error ("Error in MIDI bytes");

            if (statusByte == 0xff) // meta-event
            {
                auto start = reader.data;
                reader.skip (1); // skip the type
                auto length = reader.readVariableLength();
                reader.skip (length);

                LongMessage meta (std::addressof (statusByte), 1);
                meta.midiData.storage.append (reinterpret_cast<const char*> (start), static_cast<size_t> (reader.data - start));
                result.push_back ({ std::move (meta), tickPosition });
            }
            else if (statusByte == 0xf0) // sysex
            {
                LongMessage sysex (std::addressof (statusByte), 1);
                auto start = reader.data;

                while (reader.read<uint8_t>() < 0x80)
                {}

                sysex.midiData.storage.append (reinterpret_cast<const char*> (start), static_cast<size_t> (reader.data - start));
                result.push_back ({ std::move (sysex), tickPosition });
            }
            else
            {
                ShortMessage m (statusByte, 0, 0);
                auto length = m.length();

                if (length > 1)  m.midiData.bytes[1] = reader.read<uint8_t>();
                if (length > 2)  m.midiData.bytes[2] = reader.read<uint8_t>();

                result.push_back ({ LongMessage (m), tickPosition });
            }
        }

        return result;
    }
}

inline void File::clear()
{
    tracks.clear();
}

inline void File::load (const void* midiFileData, size_t dataSize)
{
    clear();

    if (dataSize == 0)
        return;

    if (midiFileData == nullptr)
        throw std::runtime_error ("No data supplied");

    Reader reader { static_cast<const uint8_t*> (midiFileData), dataSize };

    auto header = readHeader (reader);
    timeFormat = static_cast<int16_t> (header.timeFormat);

    for (uint16_t track = 0; track < header.numTracks; ++track)
    {
        auto chunkType = reader.readString (4);
        auto chunkSize = reader.read<uint32_t>();
        reader.expectSize (chunkSize);

        if (chunkType == "MTrk")
        {
            Reader chunkReader { reader.data, chunkSize };
            tracks.push_back ({ readTrack (chunkReader) });
        }

        reader.skip (chunkSize);
    }
}

inline void File::iterateEvents (const std::function<void(const LongMessage&, double timeInSeconds)>& handleEvent) const
{
    std::vector<Event> allEvents;

    for (auto& t : tracks)
        allEvents.insert (allEvents.end(), t.events.begin(), t.events.end());

    std::stable_sort (allEvents.begin(), allEvents.end(),
                      [] (const Event& e1, const Event& e2) { return e1.tickPosition < e2.tickPosition; });

    uint32_t lastTempoChangeTick = 0;
    double lastTempoChangeSeconds = 0, secondsPerTick = 0;

    if (timeFormat < 0)
        secondsPerTick = 1.0 / (-(timeFormat >> 8) * (timeFormat & 0xff));
    else
        secondsPerTick = 0.5 / (timeFormat & 0x7fff);

    for (auto& event : allEvents)
    {
        CHOC_ASSERT (event.tickPosition >= lastTempoChangeTick);
        auto eventTimeSeconds = lastTempoChangeSeconds + secondsPerTick * (event.tickPosition - lastTempoChangeTick);

        if (event.message.isMetaEventOfType (0x51)) // tempo meta-event
        {
            auto content = event.message.getMetaEventData();

            if (content.length() != 3)
                throw std::runtime_error ("Error in meta-event data");

            uint32_t microsecondsPerQuarterNote = (uint8_t) content[0];
            microsecondsPerQuarterNote = (microsecondsPerQuarterNote << 8) | (uint8_t) content[1];
            microsecondsPerQuarterNote = (microsecondsPerQuarterNote << 8) | (uint8_t) content[2];

            if (timeFormat > 0)
            {
                lastTempoChangeTick = event.tickPosition;
                lastTempoChangeSeconds = eventTimeSeconds;
                auto secondsPerQuarterNote = microsecondsPerQuarterNote / 1000000.0;
                secondsPerTick = secondsPerQuarterNote / (timeFormat & 0x7fff);
            }
        }
        else
        {
            handleEvent (event.message, eventTimeSeconds);
        }
    }
}

inline choc::midi::Sequence File::toSequence() const
{
    choc::midi::Sequence sequence;

    iterateEvents ([&] (const LongMessage& m, double time)
    {
        sequence.events.push_back ({ time, m });
    });

    return sequence;
}

inline File::File (const choc::midi::Sequence& sequence)
{
    timeFormat = 1000; // use a timebase of 1000 ticks per quarter-note
    tracks.resize (1);
    auto& track = tracks.front();

    for (auto& e : sequence.events)
        track.events.push_back ({ e.message, static_cast<uint32_t> (e.timeStamp * timeFormat * 2.0) });
}

namespace
{
    struct Writer
    {
        std::vector<uint8_t> data;

        void write (uint32_t value)
        {
            data.push_back (static_cast<uint8_t> (value >> 24));
            data.push_back (static_cast<uint8_t> (value >> 16));
            data.push_back (static_cast<uint8_t> (value >> 8));
            data.push_back (static_cast<uint8_t> (value));
        }

        void write (uint16_t value)
        {
            data.push_back (static_cast<uint8_t> (value >> 8));
            data.push_back (static_cast<uint8_t> (value));
        }

        void write (const void* d, size_t size)
        {
            auto p = static_cast<const uint8_t*> (d);
            data.insert (data.end(), p, p + size);
        }

        void writeVariableLength (uint32_t n)
        {
            uint8_t buffer[4];
            uint32_t numBytes = 0;

            do
            {
                buffer[numBytes++] = static_cast<uint8_t> (n & 0x7fu);
                n >>= 7;
            }
            while (n != 0);

            while (numBytes != 0)
            {
                auto byte = buffer[--numBytes];

                if (numBytes != 0)
                    byte |= 0x80;

                data.push_back (byte);
            }
        }
    };
}

inline std::vector<uint8_t> File::save() const
{
    Writer writer;
    writer.data.reserve (8192);

    writer.write (static_cast<uint32_t> (0x4d546864)); // MThd
    writer.write (static_cast<uint32_t> (6));
    writer.write (static_cast<uint16_t> (tracks.size() > 1 ? 1 : 0));
    writer.write (static_cast<uint16_t> (tracks.size()));
    writer.write (static_cast<uint16_t> (timeFormat));

    for (auto& track : tracks)
    {
        writer.write (static_cast<uint32_t> (0x4d54726b)); // MTrk
        auto trackSizePos = writer.data.size();
        writer.write (static_cast<uint32_t> (0)); // placeholder size

        auto trackStartPos = writer.data.size();
        uint32_t lastTick = 0;
        uint8_t lastStatusByte = 0;

        for (auto& ev : track.events)
        {
            writer.writeVariableLength (ev.tickPosition - lastTick);
            lastTick = ev.tickPosition;

            auto messageData = ev.message.data();
            auto messageSize = ev.message.size();
            auto statusByte = messageData[0];

            if (statusByte != lastStatusByte)
            {
                writer.write (messageData, 1);
                lastStatusByte = statusByte;
            }

            if (messageSize > 1)
                writer.write (messageData + 1, messageSize - 1);
        }

        auto trackLength = static_cast<uint32_t> (writer.data.size() - trackStartPos);
        auto p = writer.data.data() + trackSizePos;
        p[0] = static_cast<uint8_t> (trackLength >> 24);
        p[1] = static_cast<uint8_t> (trackLength >> 16);
        p[2] = static_cast<uint8_t> (trackLength >> 8);
        p[3] = static_cast<uint8_t> (trackLength);
    }

    return writer.data;
}



} // namespace choc::midi''

#endif // CHOC_MIDIFILE_HEADER_INCLUDED
