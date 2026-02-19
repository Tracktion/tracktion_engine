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

#ifndef CHOC_BASE64_HEADER_INCLUDED
#define CHOC_BASE64_HEADER_INCLUDED

#include <string>

namespace choc::base64
{

//==============================================================================
/// Encodes a chunk of binary data as base-64.
/// The lambda function provided for the output callback must take 4 char parameters,
/// and it will be called for each set of 4 base-64 characters that are produced
/// (the length of a base-64 string is always a multiple of 4).
///
/// Also see choc::base64::encodeToString() for an easier way to get the result as a string.
template <typename WriteFrameFn>
void encodeToFrames (const void* inputBinaryData,
                     size_t numInputBytes,
                     WriteFrameFn&& writeFrame);

/// Returns a base-64 string for the given chunk of binary data.
std::string encodeToString (const void* inputBinaryData,
                            size_t numInputBytes);

/// Returns a base-64 string for a chunk of binary data provided in some kind
/// of container, e.g. a std::vector<uint8_t> or std::string
template <typename Container>
std::string encodeToString (const Container&);

/// Attempts to decode a base64 string, calling a lambda to handle each byte
/// that is produced.
/// The lambda should have one parameter which is a uint8_t.
/// Returns false if any errors were encountered before finishing.
template <typename HandleOutputByteFn>
bool decode (std::string_view base64,
             HandleOutputByteFn&& handleOutputByte);

/// Attempts to decode a base64 string, writing the results to a container such
/// as a std::vector.
/// The container passed in must have reserve() and a push_back() method that
/// will be called with a uint8_t argument.
/// Returns a bool to indicate success - it'll fail if the data wasn't valid base64
template <typename OutputContainer>
bool decodeToContainer (OutputContainer& outputContainer,
                        std::string_view base64);



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

template <typename WriteFrameFn>
void encodeToFrames (const void* inputBinaryData, size_t numInputBytes, WriteFrameFn&& writeFrame)
{
    static constexpr const char encoding[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    auto input = static_cast<const uint8_t*> (inputBinaryData);

    while (numInputBytes >= 3u)
    {
        auto frame = static_cast<uint32_t> (*input++) << 16;
        frame     += static_cast<uint32_t> (*input++) << 8;
        frame     += static_cast<uint32_t> (*input++);

        writeFrame (encoding[(frame >> 18) & 63u],
                    encoding[(frame >> 12) & 63u],
                    encoding[(frame >> 6)  & 63u],
                    encoding[frame         & 63u]);

        numInputBytes -= 3u;
    }

    if (numInputBytes != 0)
    {
        auto frame = static_cast<uint32_t> (*input++) << 16;

        if (numInputBytes == 2)
        {
            frame += static_cast<uint32_t> (*input) << 8;

            writeFrame (encoding[(frame >> 18) & 63u],
                        encoding[(frame >> 12) & 63u],
                        encoding[(frame >> 6)  & 63u],
                        '=');
        }
        else
        {
            writeFrame (encoding[(frame >> 18) & 63u],
                        encoding[(frame >> 12) & 63u],
                        '=',
                        '=');
        }
    }
}

inline std::string encodeToString (const void* inputBinaryData, size_t numInputBytes)
{
    std::string s;
    s.reserve (((numInputBytes / 3u) + (numInputBytes % 3u)) * 4u);

    encodeToFrames (inputBinaryData, numInputBytes, [&] (char c1, char c2, char c3, char c4)
    {
        s += c1; s += c2; s += c3; s += c4;
    });

    return s;
}

template <typename Container>
std::string encodeToString (const Container& c)
{
    auto size = c.size();
    return encodeToString (size == 0 ? nullptr : std::addressof (c[0]), size);
}

template <typename HandleOutputByteFn>
bool decode (std::string_view base64, HandleOutputByteFn&& handleOutputByte)
{
    auto length = base64.length();

    if ((length & 3u) != 0)
        return false;

    auto inputChars = base64.data();

    for (size_t index = 0; index < length;)
    {
        uint32_t frame = 0;

        for (uint32_t i = 0; i < 4; ++i)
        {
            frame <<= 6;
            auto c = static_cast<uint8_t> (inputChars[index++]);

            if (c >= 'A' && c <= 'Z')    { frame += (c - static_cast<uint32_t> ('A'));       continue; }
            if (c >= 'a' && c <= 'z')    { frame += (c - static_cast<uint32_t> ('a' - 26));  continue; }
            if (c >= '0' && c <= '9')    { frame += (c + static_cast<uint32_t> (52 - '0'));  continue; }
            if (c == '+')                { frame += 62; continue; }
            if (c == '/')                { frame += 63; continue; }

            if (c == '=' && i > 1)
            {
                auto remainingChars = length - index;

                if (remainingChars == 0)
                {
                    handleOutputByte (static_cast<uint8_t> (frame >> 16));
                    handleOutputByte (static_cast<uint8_t> (frame >> 8));
                    return true;
                }

                if (remainingChars == 1 && inputChars[index] == '=')
                {
                    handleOutputByte (static_cast<uint8_t> (frame >> 10));
                    return true;
                }
            }

            return false;
        }

        handleOutputByte (static_cast<uint8_t> (frame >> 16));
        handleOutputByte (static_cast<uint8_t> (frame >> 8));
        handleOutputByte (static_cast<uint8_t> (frame));
    }

    return true;
}

template <typename OutputContainer>
bool decodeToContainer (OutputContainer& outputContainer, std::string_view base64)
{
    outputContainer.reserve ((base64.length() / 4u) * 3u + 4u);
    return decode (base64, [&] (uint8_t b) { outputContainer.push_back (static_cast<typename OutputContainer::value_type> (b)); });
}


} // namespace choc::base64

#endif // CHOC_BASE64_HEADER_INCLUDED
