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

#ifndef CHOC_AUDIOFILEFORMAT_HEADER_INCLUDED
#define CHOC_AUDIOFILEFORMAT_HEADER_INCLUDED

#include <vector>
#include <fstream>
#include "choc_SampleBuffers.h"
#include "choc_SincInterpolator.h"
#include "../text/choc_StringUtilities.h"
#include "../text/choc_JSON.h"

namespace choc::audio
{

//==============================================================================
/// List of integer or float sample types that a file format may be able to support.
enum class BitDepth
{
    unknown = 0,
    int8,
    int16,
    int24,
    int32,
    float32,
    float64
};

constexpr bool isFloat (BitDepth);
constexpr uint32_t getBytesPerSample (BitDepth);
std::string_view getBitDepthDescription (BitDepth);
constexpr BitDepth getIntegerBitDepth (uint16_t numBits);
constexpr BitDepth getFloatBitDepth (uint16_t numBits);

//==============================================================================
/// List of speaker types.
enum class Speaker
{
    unknown,
    front_left,
    front_right,
    front_center,
    low_frequency,
    back_left,
    back_right,
    front_left_of_center,
    front_right_of_center,
    back_center,
    side_left,
    side_right,
    top_center,
    top_front_left,
    top_front_center,
    top_front_right,
    top_back_left,
    top_back_center,
    top_back_right
};

std::string_view getSpeakerName (Speaker);

//==============================================================================
/// A set of properties that were either read from an audio file, or which define
/// the settings you'd like an AudioFileWriter to use.
struct AudioFileProperties
{
    std::string formatName;
    double sampleRate = 0;
    uint64_t numFrames = 0;
    uint32_t numChannels = 0;
    BitDepth bitDepth = BitDepth::int16;
    std::vector<Speaker> speakers;
    std::string quality;

    /// This is a JSON-structured object containing metadata that is read
    /// from or written to the file. The exact format of the data in here will
    /// vary wildly depending on the file format. To see the names that might
    /// be used, have a look in the individual format classes.
    choc::value::Value metadata;

    std::string getDescription() const;
};

//==============================================================================
/// This is a container for a chunk of audio data with an associated sample rate
struct AudioFileData
{
    choc::buffer::ChannelArrayBuffer<float> frames;
    double sampleRate = 0;

    /// Attempts to resample the data in this object to match the given target rate.
    /// If the ratio is too high or some other error occurs, this will
    /// throw a std::exception with an error message.
    /// If maxNumFrames is not 0, then any attempts to generate a larger buffer
    /// will cause a failure.
    void resample (double requiredRate,
                   uint64_t maxNumFrames = 0);
};

//==============================================================================
/// A base class for streams that will read audio frames from a stream.
/// See AudioFileFormat::createReader()
class AudioFileReader
{
public:
    virtual ~AudioFileReader() = default;

    /// Returns the properties of this file or stream.
    virtual const AudioFileProperties& getProperties() = 0;

    /// Reads a chunk of single-precision frames into the buffer provided, starting
    /// from the given frame index in the file.
    /// The buffer supplied must contain exactly the same number of channels
    /// as the stream - you can find out the source channel count from getProperties().
    /// If the chunk being read goes beyond the end of the stream, this will pad
    /// the end of the buffer with zeros.
    /// Returns true if the buffer was read successfully, or false if there was
    /// some kind of read error.
    virtual bool readFrames (uint64_t frameIndex, choc::buffer::ChannelArrayView<float> result) = 0;

    /// Reads a chunk of frames with double precision. See the other readFrames()
    /// method for details.
    virtual bool readFrames (uint64_t frameIndex, choc::buffer::ChannelArrayView<double> result) = 0;

    /// Attempts to read the stream, returning a buffer with the appropriate
    /// number of channels. On failure, returns an empty buffer. The template
    /// parameter lets you specify whether you want a float or double buffer.
    template <typename SampleType>
    choc::buffer::ChannelArrayBuffer<SampleType> readEntireStream();

    /// Attempts to load the contents of an audio file into a buffer, optionally
    /// resampling the data to match a target sample rate.
    /// Any problems will throw a std::exception with a suitable error message.
    AudioFileData loadFileContent (double targetSampleRate = 0,
                                   uint64_t maxNumFrames = 48000 * 100,
                                   uint32_t maxNumChannels = 16);
};

//==============================================================================
/// Writes audio data to a stream
/// See AudioFileFormat::createWriter()
class AudioFileWriter
{
public:
    virtual ~AudioFileWriter() = default;

    virtual const AudioFileProperties& getProperties() = 0;

    /// Appends a chunk of frames to the stream.
    /// The buffer provided must contain exactly the same number of channels that
    /// was specified when creating the stream - you can get this number from
    /// the getProperties() method.
    /// Returns true if the buffer was written successfully, or false if there was
    /// some kind of i/o error.
    /// After appending some frames, the content that has been written to the
    /// stream may be temporarily in an invalid state, and can be made valid
    /// by either calling flush() or deleting the writer object.
    virtual bool appendFrames (choc::buffer::ChannelArrayView<const float> frames) = 0;

    /// Writes a chunk of frames with double precision. See the other writeFrames()
    /// method for details.
    virtual bool appendFrames (choc::buffer::ChannelArrayView<const double> frames) = 0;

    /// Flushes the stream so that the content is in a consistent state that forms a
    /// valid audio file. This may involve seeking within the stream and re-writing
    /// header blocks, so could be a very slow operation. If you're writing a very
    /// long file, you may want to call this periodically so that the file being
    /// created is left in a failrly up-to-date and readable state if the process is
    /// interrupted.
    virtual bool flush() = 0;
};

//==============================================================================
/// Base class for an audio file format.
/// Sub-classes of AudioFileFormat are available for different types of file.
class AudioFileFormat
{
public:
    AudioFileFormat();
    virtual ~AudioFileFormat();

    /// Returns a space-separated list of file suffixes (without the dots)
    virtual std::string getFileSuffixes() = 0;

    /// Checks a filename to see whether its suffix is one that might be
    /// supported by this format
    bool filenameSuffixMatches (std::string_view filename);

    virtual uint32_t getMaximumNumChannels() = 0;
    virtual std::vector<double> getSupportedSampleRates() = 0;
    virtual std::vector<BitDepth> getSupportedBitDepths() = 0;
    virtual std::vector<std::string> getQualityLevels() = 0;

    /// Some formats may be able to read streams, but not create one.
    virtual bool supportsWriting() const = 0;

    /// Attempts to create a reader for the given stream. If it fails, the stream can
    /// be re-used by the caller, but if it succeeds, the caller should allow the reader
    /// object to take control of the stream.
    /// Note that the stream provided must support seeking.
    virtual std::unique_ptr<AudioFileReader> createReader (std::shared_ptr<std::istream>) = 0;

    /// Attempts to create a writer for the given stream, and the given properties.
    /// If you give it invalid properties, or an invalid stream, it'll return a nullptr.
    /// Some formats may not actually support writing, so these will also return a nullptr.
    /// The writer can keep a shared pointer to the stream, so if you need to do something like
    /// use a std::ostringstream to write to memory, you can keep a pointer to the result, but
    /// its content will only be valid when the writer has been deleted or immediately after
    /// a call to AudioFileWriter::flush().
    /// The nature of audio files means that the stream is expected to be able to seek backwards
    /// to overwrite sections as it grows.
    virtual std::unique_ptr<AudioFileWriter> createWriter (std::shared_ptr<std::ostream>,
                                                           AudioFileProperties properties) = 0;

    /// Helper function that tries to create a reader for a file
    std::unique_ptr<AudioFileReader> createReader (const std::string& filePath);

    /// Helper function that tries to create a writer that will overwrite a file
    std::unique_ptr<AudioFileWriter> createWriter (const std::string& filePath,
                                                   AudioFileProperties properties);
};


//==============================================================================
/// Copies a given number of frames of audio data from an AudioFormatReader to
/// an AudioFormatWriter.
/// If maxNumFramesToCopy == 0, it will copy as many frames as the reader can
/// supply.
/// Returns the number of frames successfully copied.
///
uint64_t copyAudioData (AudioFileWriter& destWriter,
                        AudioFileReader& sourceReader,
                        uint64_t maxNumFramesToCopy = 0);


//==============================================================================
/// Holds a list of audio formats, and can try each one in turn when to find
/// one that'll open a stream.
struct AudioFileFormatList
{
    AudioFileFormatList() = default;
    AudioFileFormatList (AudioFileFormatList&&) = default;
    AudioFileFormatList& operator= (AudioFileFormatList&&) = default;
    ~AudioFileFormatList() = default;

    /// Adds an instance of a format
    void addFormat (std::unique_ptr<AudioFileFormat> formatToAdd);

    /// Adds an instance of the format that's named by the template
    template <typename AudioFormatType>
    void addFormat();

    /// Searches for the first format in this list that can load the
    /// given stream. If none of them can read it, returns nullptr.
    std::unique_ptr<AudioFileReader> createReader (std::shared_ptr<std::istream>) const;

    /// Searches for the first format in this list that can load a file
    /// from the given filename. If none of them can read it, returns nullptr.
    std::unique_ptr<AudioFileReader> createReader (const std::string& filePath) const;

    /// Attempts to load the contents of an audio file into a buffer, optionally
    /// resampling the data to match a target sample rate.
    /// Any problems will throw a std::exception with a suitable error message.
    AudioFileData loadFileContent (std::shared_ptr<std::istream> fileReader,
                                   double targetSampleRate = 0,
                                   uint64_t maxNumFrames = 48000 * 100,
                                   uint32_t maxNumChannels = 16) const;

    /// The list of registered formats
    std::vector<std::unique_ptr<AudioFileFormat>> formats;
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

inline std::string_view getBitDepthDescription (BitDepth b)
{
    switch (b)
    {
        case BitDepth::int8:    return "8-bit int";
        case BitDepth::int16:   return "16-bit int";
        case BitDepth::int24:   return "24-bit int";
        case BitDepth::int32:   return "32-bit int";
        case BitDepth::float32: return "32-bit float";
        case BitDepth::float64: return "64-bit float";
        case BitDepth::unknown:
        default:                return "unknown";
    }
}

inline constexpr bool isFloat (BitDepth b)
{
    return b == BitDepth::float32 || b == BitDepth::float64;
}

inline constexpr uint32_t getBytesPerSample (BitDepth b)
{
    switch (b)
    {
        case BitDepth::int8:    return 1;
        case BitDepth::int16:   return 2;
        case BitDepth::int24:   return 3;
        case BitDepth::int32:   return 4;
        case BitDepth::float32: return 4;
        case BitDepth::float64: return 8;
        case BitDepth::unknown:
        default:                return 0;
    }
}

inline constexpr BitDepth getIntegerBitDepth (uint16_t numBits)
{
    switch (numBits)
    {
        case 8:     return BitDepth::int8;
        case 16:    return BitDepth::int16;
        case 24:    return BitDepth::int24;
        case 32:    return BitDepth::int32;
        default:    return BitDepth::unknown;
    }
}

inline constexpr BitDepth getFloatBitDepth (uint16_t numBits)
{
    switch (numBits)
    {
        case 32:    return BitDepth::float32;
        case 64:    return BitDepth::float64;
        default:    return BitDepth::unknown;
    }
}

inline std::string_view getSpeakerName (Speaker s)
{
    switch (s)
    {
        case Speaker::front_left:             return "front-left";
        case Speaker::front_right:            return "front-right";
        case Speaker::front_center:           return "front-center";
        case Speaker::low_frequency:          return "low-frequency";
        case Speaker::back_left:              return "back-left";
        case Speaker::back_right:             return "back-right";
        case Speaker::front_left_of_center:   return "front-left-of-center";
        case Speaker::front_right_of_center:  return "front-right-of-center";
        case Speaker::back_center:            return "back-center";
        case Speaker::side_left:              return "side-left";
        case Speaker::side_right:             return "side-right";
        case Speaker::top_center:             return "top-center";
        case Speaker::top_front_left:         return "top-front-left";
        case Speaker::top_front_center:       return "top-front-center";
        case Speaker::top_front_right:        return "top-front-right";
        case Speaker::top_back_left:          return "top-back-left";
        case Speaker::top_back_center:        return "top-back-center";
        case Speaker::top_back_right:         return "top-back-right";
        case Speaker::unknown:
        default:                              return "unknown";
    }
}

//==============================================================================
inline std::string AudioFileProperties::getDescription() const
{
    std::string desc;

    if (! formatName.empty())
        desc += "Format: " + formatName + "\n";

    if (sampleRate > 0)
        desc += "Rate: " + std::to_string (static_cast<uint32_t> (sampleRate)) + "Hz\n";

    if (bitDepth != BitDepth::unknown)
        desc += "Bit depth: " + std::string (getBitDepthDescription (bitDepth)) + "\n";

    desc += "Frame count: " + std::to_string (numFrames) + "\n";

    if (sampleRate > 0)
    {
        auto millisecs = (numFrames * 1000u) / static_cast<uint64_t> (sampleRate);
        desc += "Length: " + choc::text::getDurationDescription (std::chrono::milliseconds (millisecs)) + "\n";
    }

    if (numChannels != 0)
        desc += "Channels: " + std::to_string (numChannels) + "\n";

    if (! speakers.empty())
    {
        std::vector<std::string> names;

        for (auto& s : speakers)
            names.push_back (std::string (getSpeakerName (s)));

        desc += "Speakers: " + choc::text::joinStrings (names, ", ") + "\n";
    }

    if (! quality.empty())
        desc += "Quality: " + quality + "\n";

    if (! metadata.isVoid())
        desc += "Metadata: " + choc::json::toString (metadata, true) + "\n";

    return desc;
}

inline AudioFileFormat::AudioFileFormat() = default;
inline AudioFileFormat::~AudioFileFormat() = default;

inline bool AudioFileFormat::filenameSuffixMatches (std::string_view filename)
{
    for (const auto& suffix : choc::text::splitAtWhitespace (getFileSuffixes()))
        if (choc::text::endsWith (filename, "." + choc::text::trim (suffix)))
            return true;

    return false;
}

inline std::unique_ptr<AudioFileReader> AudioFileFormat::createReader (const std::string& p)
{
    return createReader (std::make_shared<std::ifstream> (p, std::ios::binary | std::ios::in));
}

inline std::unique_ptr<AudioFileWriter> AudioFileFormat::createWriter (const std::string& file, AudioFileProperties p)
{
    return createWriter (std::make_shared<std::ofstream> (file, std::ios::binary | std::ios::out | std::ios::trunc), std::move (p));
}

template <typename SampleType>
choc::buffer::ChannelArrayBuffer<SampleType> AudioFileReader::readEntireStream()
{
    auto& props = getProperties();

    if (static_cast<choc::buffer::FrameCount> (props.numFrames) == props.numFrames)
    {
        choc::buffer::ChannelArrayBuffer<SampleType> buffer (props.numChannels, static_cast<choc::buffer::FrameCount> (props.numFrames));

        if (readFrames (0, buffer))
            return buffer;
    }

    return {};
}

inline AudioFileData AudioFileReader::loadFileContent (double targetSampleRate,
                                                       uint64_t maxNumFrames,
                                                       uint32_t maxNumChannels)
{
    const auto& props = getProperties();

    auto rate = props.sampleRate;
    auto numFrames = props.numFrames;
    auto numChannels = props.numChannels;

    if (rate <= 0)                      throw std::runtime_error ("Cannot open file");
    if (numFrames > maxNumFrames)       throw std::runtime_error ("File is too long");
    if (numChannels > maxNumChannels)   throw std::runtime_error ("File contains too many channels");

    if (numFrames == 0)
        return {};

    AudioFileData data;
    data.frames.resize ({ numChannels, static_cast<choc::buffer::FrameCount> (numFrames) });
    data.sampleRate = rate;

    if (! readFrames (0, data.frames.getView()))
        throw std::runtime_error ("Failed to read from file");

    if (targetSampleRate > 0)
        data.resample (targetSampleRate, maxNumFrames);

    return data;
}

inline uint64_t copyAudioData (AudioFileWriter& dest, AudioFileReader& source, uint64_t maxNumFramesToCopy)
{
    constexpr uint32_t bufferSize = 2048;

    auto& readerProperties = source.getProperties();
    auto framesToDo = maxNumFramesToCopy != 0 ? std::min (maxNumFramesToCopy, readerProperties.numFrames) : readerProperties.numFrames;
    auto tempBuffer = choc::buffer::ChannelArrayBuffer<double> (readerProperties.numChannels, bufferSize);
    uint64_t readIndex = 0;

    while (framesToDo != 0)
    {
        auto blockSize = static_cast<uint32_t> (std::min (static_cast<uint64_t> (bufferSize), framesToDo));
        auto block = tempBuffer.getStart (blockSize);

        if (! source.readFrames (readIndex, block))
            break;

        dest.appendFrames (block);

        readIndex += blockSize;
        framesToDo -= blockSize;
    }

    return readIndex;
}

//==============================================================================
template <typename AudioFormatType>
void AudioFileFormatList::addFormat()   { addFormat (std::make_unique<AudioFormatType>()); }

inline void AudioFileFormatList::addFormat (std::unique_ptr<AudioFileFormat> formatToAdd)
{
    formats.push_back (std::move (formatToAdd));
}

inline std::unique_ptr<AudioFileReader> AudioFileFormatList::createReader (std::shared_ptr<std::istream> stream) const
{
    try
    {
        auto originalStreamPos = stream->tellg();

        for (auto& f : formats)
        {
            if (auto r = f->createReader (stream))
                return r;

            stream->seekg (originalStreamPos);
        }
    }
    catch (...) {}

    return {};
}

inline std::unique_ptr<AudioFileReader> AudioFileFormatList::createReader (const std::string& p) const
{
    return createReader (std::make_shared<std::ifstream> (p, std::ios::binary | std::ios::in));
}

inline void AudioFileData::resample (double targetSampleRate, uint64_t maxNumFrames)
{
    if (targetSampleRate != 0 && std::abs (targetSampleRate - sampleRate) > 1.0)
    {
        static constexpr double maxRatio = 64.0;

        if (targetSampleRate > sampleRate * maxRatio || targetSampleRate < sampleRate / maxRatio)
            throw std::runtime_error ("Resampling ratio is out-of-range");

        auto ratio = targetSampleRate / sampleRate;
        auto newFrameCount = static_cast<uint64_t> (ratio * static_cast<double> (frames.getNumFrames()) + 0.5);

        if (maxNumFrames != 0 && newFrameCount > maxNumFrames)
            throw std::runtime_error ("File too long");

        if (newFrameCount != frames.getNumFrames())
        {
            using BufferType = decltype (frames);
            BufferType resampled (frames.getNumChannels(), static_cast<uint32_t> (newFrameCount));
            choc::interpolation::sincInterpolate<BufferType&, BufferType, 10> (resampled, frames);
            frames = std::move (resampled);
            sampleRate = targetSampleRate;
        }
    }
}

inline AudioFileData AudioFileFormatList::loadFileContent (std::shared_ptr<std::istream> fileReader,
                                                           double targetSampleRate,
                                                           uint64_t maxNumFrames,
                                                           uint32_t maxNumChannels) const
{
    auto reader = createReader (fileReader);

    if (reader == nullptr)
        throw std::runtime_error ("Cannot open file");

    return reader->loadFileContent (targetSampleRate, maxNumFrames, maxNumChannels);
}

} // namespace choc::audio

#endif // CHOC_AUDIOFILEFORMAT_HEADER_INCLUDED
