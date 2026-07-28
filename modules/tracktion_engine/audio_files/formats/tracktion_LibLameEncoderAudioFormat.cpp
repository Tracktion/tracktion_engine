/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine {

#if TRACKTION_ENABLE_LIBLAME

namespace liblame
{
    /** Stands in for LAME's lame_global_flags. It stays incomplete on purpose - we only
        ever hold the pointer and hand it back to the library.
    */
    struct Flags;
    using lame_t = Flags*;

    // From MPEG_mode in lame.h (STEREO = 0 and DUAL_CHANNEL = 2 aren't used here)
    static constexpr int modeJointStereo = 1;
    static constexpr int modeMono        = 3;

    // From vbr_mode in lame.h. vbr_mtrh is what vbr_default resolves to in 3.100
    static constexpr int vbrOff  = 0;
    static constexpr int vbrMtrh = 4;

    /** LAME wants at least this much room to flush its final frames. */
    static constexpr size_t flushBufferSize = 7200;

    /** LAME's documented worst case output size for a block of samples. */
    static size_t getEncodeBufferSize (int numSamples)
    {
        return (size_t) (1.25 * numSamples) + flushBufferSize;
    }

    /** True if the string has anything that ISO-8859-1 can't represent. ID3v2.3 text
        frames are latin-1 unless they're explicitly flagged as UTF-16.
    */
    static bool needsUnicode (const juce::String& s)
    {
        for (auto c = s.getCharPointer(); ! c.isEmpty();)
            if (c.getAndAdvance() > 255)
                return true;

        return false;
    }

    /** Encodes to latin-1 for LAME's normal id3 setters, dropping anything that
        doesn't fit. Callers should prefer the UTF-16 entry points where possible.
    */
    static std::string toLatin1 (const juce::String& s)
    {
        std::string result;

        for (auto c = s.getCharPointer(); ! c.isEmpty();)
            if (auto character = c.getAndAdvance(); character < 256)
                result += (char) character;

        return result;
    }

    /** LAME's UTF-16 id3 entry points want a byte-order-marked, null terminated string. */
    static std::vector<unsigned short> toUTF16WithBOM (const juce::String& s)
    {
        std::vector<unsigned short> result { 0xfeff };

        for (auto c = s.getCharPointer(); ! c.isEmpty();)
        {
            auto character = c.getAndAdvance();

            if (character >= 0x10000)
            {
                character -= 0x10000;
                result.push_back ((unsigned short) (0xd800 + (character >> 10)));
                result.push_back ((unsigned short) (0xdc00 + (character & 0x3ff)));
            }
            else
            {
                result.push_back ((unsigned short) character);
            }
        }

        result.push_back (0);
        return result;
    }

    /** LAME reports through vfprintf-style callbacks and defaults to stderr, which is
        no use in a GUI app, so everything gets routed through here instead.
    */
    static void logCallback (const char* format, va_list args)
    {
        char buffer[1024] = {};
        std::vsnprintf (buffer, sizeof (buffer) - 1, format, args);

        if (juce::String message (buffer); message.trim().isNotEmpty())
            TRACKTION_LOG ("LAME: " + message.trimEnd());
    }
}

//==============================================================================
//==============================================================================
/** The part of the LAME C API that we use, resolved by name at runtime.

    The members deliberately keep LAME's own snake_case names so that the loading
    below can be driven from the symbol name, and so it's obvious which C function
    each one is.
*/
struct LibLameEncoderAudioFormat::LameLibrary
{
    using lame_t = liblame::lame_t;

    explicit LameLibrary (const juce::String& libraryName)
    {
        if (! library.open (libraryName))
        {
            TRACKTION_LOG ("LAME: couldn't open shared library: " + libraryName);
            return;
        }

       #define TRACKTION_LOAD_LAME_FN(name)   name = reinterpret_cast<decltype (name)> (library.getFunction (#name));

        // Encoding won't work without these
        TRACKTION_LOAD_LAME_FN (lame_init)
        TRACKTION_LOAD_LAME_FN (lame_init_params)
        TRACKTION_LOAD_LAME_FN (lame_close)
        TRACKTION_LOAD_LAME_FN (lame_set_in_samplerate)
        TRACKTION_LOAD_LAME_FN (lame_set_out_samplerate)
        TRACKTION_LOAD_LAME_FN (lame_set_num_channels)
        TRACKTION_LOAD_LAME_FN (lame_set_mode)
        TRACKTION_LOAD_LAME_FN (lame_set_quality)
        TRACKTION_LOAD_LAME_FN (lame_set_brate)
        TRACKTION_LOAD_LAME_FN (lame_set_VBR)
        TRACKTION_LOAD_LAME_FN (lame_set_VBR_q)
        TRACKTION_LOAD_LAME_FN (lame_encode_buffer_ieee_float)
        TRACKTION_LOAD_LAME_FN (lame_encode_flush)

        // These are all optional. Distro builds vary in what they export, and everything
        // they're used for degrades gracefully if they're missing
        TRACKTION_LOAD_LAME_FN (get_lame_version)
        TRACKTION_LOAD_LAME_FN (lame_set_bWriteVbrTag)
        TRACKTION_LOAD_LAME_FN (lame_set_write_id3tag_automatic)
        TRACKTION_LOAD_LAME_FN (lame_set_errorf)
        TRACKTION_LOAD_LAME_FN (lame_set_msgf)
        TRACKTION_LOAD_LAME_FN (lame_set_debugf)
        TRACKTION_LOAD_LAME_FN (lame_get_id3v1_tag)
        TRACKTION_LOAD_LAME_FN (lame_get_id3v2_tag)
        TRACKTION_LOAD_LAME_FN (lame_get_lametag_frame)
        TRACKTION_LOAD_LAME_FN (id3tag_init)
        TRACKTION_LOAD_LAME_FN (id3tag_add_v2)
        TRACKTION_LOAD_LAME_FN (id3tag_set_title)
        TRACKTION_LOAD_LAME_FN (id3tag_set_artist)
        TRACKTION_LOAD_LAME_FN (id3tag_set_album)
        TRACKTION_LOAD_LAME_FN (id3tag_set_year)
        TRACKTION_LOAD_LAME_FN (id3tag_set_comment)
        TRACKTION_LOAD_LAME_FN (id3tag_set_track)
        TRACKTION_LOAD_LAME_FN (id3tag_set_genre)
        TRACKTION_LOAD_LAME_FN (id3tag_set_textinfo_utf16)
        TRACKTION_LOAD_LAME_FN (id3tag_set_comment_utf16)

       #undef TRACKTION_LOAD_LAME_FN

        isLoaded = lame_init != nullptr
                && lame_init_params != nullptr
                && lame_close != nullptr
                && lame_set_in_samplerate != nullptr
                && lame_set_out_samplerate != nullptr
                && lame_set_num_channels != nullptr
                && lame_set_mode != nullptr
                && lame_set_quality != nullptr
                && lame_set_brate != nullptr
                && lame_set_VBR != nullptr
                && lame_set_VBR_q != nullptr
                && lame_encode_buffer_ieee_float != nullptr
                && lame_encode_flush != nullptr;

        if (! isLoaded)
            TRACKTION_LOG ("LAME: shared library is missing required symbols: " + libraryName);
    }

    //==============================================================================
    /** Points LAME's message callbacks at the Tracktion log rather than stderr. */
    void redirectLogging (lame_t gfp) const
    {
        if (lame_set_errorf != nullptr)  lame_set_errorf (gfp, liblame::logCallback);
        if (lame_set_msgf != nullptr)    lame_set_msgf   (gfp, liblame::logCallback);
        if (lame_set_debugf != nullptr)  lame_set_debugf (gfp, liblame::logCallback);
    }

    /** Applies the standard JUCE id3 metadata values to the tag spec.

        Must be called before lame_init_params. Values that fit in latin-1 go through
        LAME's normal setters, which also lets it write an ID3v1 tag; anything else
        needs the UTF-16 entry points, as the plain setters are latin-1 only.
    */
    void applyMetadata (lame_t gfp, const juce::StringPairArray& metadata) const
    {
        if (id3tag_init == nullptr || id3tag_add_v2 == nullptr)
            return;

        id3tag_init (gfp);
        id3tag_add_v2 (gfp);

        auto setTextFrame = [this, gfp] (const char* frameID, const juce::String& value, auto&& latin1Setter)
        {
            if (value.isEmpty())
                return;

            if (liblame::needsUnicode (value) && id3tag_set_textinfo_utf16 != nullptr)
                if (id3tag_set_textinfo_utf16 (gfp, frameID, liblame::toUTF16WithBOM (value).data()) == 0)
                    return;

            if (latin1Setter != nullptr)
                latin1Setter (gfp, liblame::toLatin1 (value).c_str());
        };

        setTextFrame ("TIT2", metadata.getValue ("id3title", {}),       id3tag_set_title);
        setTextFrame ("TPE1", metadata.getValue ("id3artist", {}),      id3tag_set_artist);
        setTextFrame ("TALB", metadata.getValue ("id3album", {}),       id3tag_set_album);
        setTextFrame ("TYER", metadata.getValue ("id3date", {}),        id3tag_set_year);
        setTextFrame ("TCON", metadata.getValue ("id3genre", {}),       id3tag_set_genre);
        setTextFrame ("TRCK", metadata.getValue ("id3trackNumber", {}), id3tag_set_track);

        applyComment (gfp, metadata.getValue ("id3comment", {}));
    }

    void applyComment (lame_t gfp, const juce::String& value) const
    {
        if (value.isEmpty())
            return;

        if (liblame::needsUnicode (value) && id3tag_set_comment_utf16 != nullptr)
        {
            static const unsigned short emptyDescription[] = { 0xfeff, 0 };

            if (id3tag_set_comment_utf16 (gfp, "eng", emptyDescription,
                                          liblame::toUTF16WithBOM (value).data()) == 0)
                return;
        }

        if (id3tag_set_comment != nullptr)
            id3tag_set_comment (gfp, liblame::toLatin1 (value).c_str());
    }

    //==============================================================================
    juce::DynamicLibrary library;
    bool isLoaded = false;

    lame_t (*lame_init)() = nullptr;
    int (*lame_init_params) (lame_t) = nullptr;
    int (*lame_close) (lame_t) = nullptr;
    const char* (*get_lame_version)() = nullptr;

    int (*lame_set_in_samplerate) (lame_t, int) = nullptr;
    int (*lame_set_out_samplerate) (lame_t, int) = nullptr;
    int (*lame_set_num_channels) (lame_t, int) = nullptr;
    int (*lame_set_mode) (lame_t, int) = nullptr;
    int (*lame_set_quality) (lame_t, int) = nullptr;
    int (*lame_set_brate) (lame_t, int) = nullptr;
    int (*lame_set_VBR) (lame_t, int) = nullptr;
    int (*lame_set_VBR_q) (lame_t, int) = nullptr;
    int (*lame_set_bWriteVbrTag) (lame_t, int) = nullptr;
    void (*lame_set_write_id3tag_automatic) (lame_t, int) = nullptr;

    int (*lame_set_errorf) (lame_t, void (*) (const char*, va_list)) = nullptr;
    int (*lame_set_msgf) (lame_t, void (*) (const char*, va_list)) = nullptr;
    int (*lame_set_debugf) (lame_t, void (*) (const char*, va_list)) = nullptr;

    int (*lame_encode_buffer_ieee_float) (lame_t, const float*, const float*, int, unsigned char*, int) = nullptr;
    int (*lame_encode_flush) (lame_t, unsigned char*, int) = nullptr;

    size_t (*lame_get_id3v1_tag) (lame_t, unsigned char*, size_t) = nullptr;
    size_t (*lame_get_id3v2_tag) (lame_t, unsigned char*, size_t) = nullptr;
    size_t (*lame_get_lametag_frame) (lame_t, unsigned char*, size_t) = nullptr;

    void (*id3tag_init) (lame_t) = nullptr;
    void (*id3tag_add_v2) (lame_t) = nullptr;
    void (*id3tag_set_title) (lame_t, const char*) = nullptr;
    void (*id3tag_set_artist) (lame_t, const char*) = nullptr;
    void (*id3tag_set_album) (lame_t, const char*) = nullptr;
    void (*id3tag_set_year) (lame_t, const char*) = nullptr;
    void (*id3tag_set_comment) (lame_t, const char*) = nullptr;
    int (*id3tag_set_track) (lame_t, const char*) = nullptr;
    int (*id3tag_set_genre) (lame_t, const char*) = nullptr;
    int (*id3tag_set_textinfo_utf16) (lame_t, const char*, const unsigned short*) = nullptr;
    int (*id3tag_set_comment_utf16) (lame_t, const char*, const unsigned short*, const unsigned short*) = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LameLibrary)
};

//==============================================================================
//==============================================================================
class LibLameEncoderAudioFormat::Writer  : public juce::AudioFormatWriter
{
public:
    /** The flags must already have been through a successful lame_init_params - see
        LibLameEncoderAudioFormat::createWriterFor. Ownership of them passes to this.
    */
    Writer (std::unique_ptr<juce::OutputStream>& destStream,
            const juce::String& formatName,
            std::shared_ptr<LameLibrary> libraryToUse,
            liblame::lame_t flagsToUse,
            const juce::AudioFormatWriterOptions& options)
        : AudioFormatWriter (destStream.release(), formatName,
                             options.getSampleRate(),
                             (unsigned int) options.getNumChannels(),
                             (unsigned int) options.getBitsPerSample()),
          lame (std::move (libraryToUse)),
          gfp (flagsToUse)
    {
        writeID3v2Tag();

        // The encoder emits a placeholder Xing/LAME frame before any audio, so this is
        // where the real one has to go once the final frame count is known - see finish()
        lameTagPosition = output->getPosition();
    }

    ~Writer() override
    {
        finish();
    }

    bool write (const int** samplesToWrite, int numSamples) override
    {
        if (gfp == nullptr)
            return false;

        if (numSamples <= 0)
            return true;

        auto numChans = (int) numChannels;
        scratch.setSize (numChans, numSamples, false, false, true);

        for (int i = 0; i < numChans; ++i)
        {
            auto dest = scratch.getWritePointer (i);

            // JUCE always hands over 32-bit ints here, whatever the nominal bit depth
            if (auto src = samplesToWrite != nullptr ? samplesToWrite[i] : nullptr)
                juce::FloatVectorOperations::convertFixedToFloat (dest, src, intToFloatScale, numSamples);
            else
                juce::FloatVectorOperations::clear (dest, numSamples);
        }

        auto left  = scratch.getReadPointer (0);
        auto right = numChans > 1 ? scratch.getReadPointer (1) : left;

        auto dest = prepareEncodeBuffer (numSamples);

        return writeEncodedBytes (lame->lame_encode_buffer_ieee_float (gfp, left, right, numSamples,
                                                                       dest, (int) mp3Buffer.getSize()));
    }

    bool flush() override
    {
        if (output == nullptr)
            return false;

        // The encoder can't be flushed mid-stream without ending it, but MP3 is a
        // sequence of independent frames, so pushing what's written so far is enough
        // to leave a readable file behind
        output->flush();
        return true;
    }

private:
    //==============================================================================
    unsigned char* prepareEncodeBuffer (int numSamples)
    {
        mp3Buffer.ensureSize (liblame::getEncodeBufferSize (numSamples), false);
        return static_cast<unsigned char*> (mp3Buffer.getData());
    }

    bool writeEncodedBytes (int numBytes)
    {
        if (numBytes < 0)
        {
            TRACKTION_LOG ("LAME: encoder returned error " + juce::String (numBytes));
            return false;
        }

        return numBytes == 0 || output->write (mp3Buffer.getData(), (size_t) numBytes);
    }

    void finish()
    {
        if (gfp == nullptr)
            return;

        writeEncodedBytes (lame->lame_encode_flush (gfp, prepareEncodeBuffer (0),
                                                    (int) mp3Buffer.getSize()));

        writeID3v1Tag();
        writeLameTag();
        output->flush();

        lame->lame_close (gfp);
        gfp = nullptr;
    }

    //==============================================================================
    /** Reads a tag out of LAME, which reports the size it needs when given a null buffer. */
    juce::MemoryBlock readTag (size_t (*getTag) (liblame::lame_t, unsigned char*, size_t)) const
    {
        if (getTag == nullptr)
            return {};

        auto numBytes = getTag (gfp, nullptr, 0);

        if (numBytes == 0)
            return {};

        juce::MemoryBlock tag (numBytes);

        if (getTag (gfp, static_cast<unsigned char*> (tag.getData()), numBytes) != numBytes)
            return {};

        return tag;
    }

    void writeID3v2Tag()
    {
        if (auto tag = readTag (lame->lame_get_id3v2_tag); ! tag.isEmpty())
            output->write (tag.getData(), tag.getSize());
    }

    void writeID3v1Tag()
    {
        if (auto tag = readTag (lame->lame_get_id3v1_tag); ! tag.isEmpty())
            output->write (tag.getData(), tag.getSize());
    }

    /** Overwrites the placeholder frame the encoder wrote at the start of the stream
        with the real Xing/LAME info frame. Streams that can't seek keep the
        placeholder, which is still valid, just uninformative.
    */
    void writeLameTag()
    {
        auto frame = readTag (lame->lame_get_lametag_frame);

        if (frame.isEmpty())
            return;

        auto endPosition = output->getPosition();

        if (! output->setPosition (lameTagPosition))
        {
            TRACKTION_LOG ("LAME: couldn't seek to write the VBR info tag");
            return;
        }

        output->write (frame.getData(), frame.getSize());
        output->setPosition (endPosition);
    }

    //==============================================================================
    static constexpr float intToFloatScale = 1.0f / (float) 0x7fffffff;

    const std::shared_ptr<LameLibrary> lame;
    liblame::lame_t gfp = nullptr;
    juce::int64 lameTagPosition = 0;
    juce::AudioBuffer<float> scratch;
    juce::MemoryBlock mp3Buffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Writer)
};

//==============================================================================
//==============================================================================
LibLameEncoderAudioFormat::LibLameEncoderAudioFormat (const juce::String& lameSharedLibraryToUse)
    : AudioFormat ("MP3 file", ".mp3"),
      lame (std::make_shared<LameLibrary> (lameSharedLibraryToUse))
{
}

LibLameEncoderAudioFormat::~LibLameEncoderAudioFormat()
{
}

juce::String LibLameEncoderAudioFormat::getDefaultLibraryName()
{
   #if JUCE_WINDOWS
    return "libmp3lame.dll";
   #elif JUCE_MAC || JUCE_IOS
    return "libmp3lame.dylib";
   #else
    // The bare .so is only there with the -dev package installed, so use the soname
    return "libmp3lame.so.0";
   #endif
}

bool LibLameEncoderAudioFormat::isLibraryLoaded() const
{
    return lame->isLoaded;
}

juce::String LibLameEncoderAudioFormat::getLibraryVersion() const
{
    if (! isLibraryLoaded() || lame->get_lame_version == nullptr)
        return {};

    return juce::String (lame->get_lame_version());
}

bool LibLameEncoderAudioFormat::canHandleFile (const juce::File&)
{
    return false;
}

juce::Array<int> LibLameEncoderAudioFormat::getPossibleSampleRates()
{
    // Deliberately the same set as LAMEEncoderAudioFormat and FFmpegEncoderAudioFormat
    // so this is a drop-in replacement for them. MP3 itself also allows the MPEG-2 and
    // 2.5 rates, and passing one of those in will still work, but nothing in the render
    // UI offers them
    return { 32000, 44100, 48000 };
}

juce::Array<int> LibLameEncoderAudioFormat::getPossibleBitDepths()
{
    return { 16 };
}

bool LibLameEncoderAudioFormat::canDoStereo()      { return true; }
bool LibLameEncoderAudioFormat::canDoMono()        { return true; }
bool LibLameEncoderAudioFormat::isCompressed()     { return true; }

juce::StringArray LibLameEncoderAudioFormat::getQualityOptions()
{
    static const char* vbrOptions[] = { "VBR quality 0 (best)", "VBR quality 1", "VBR quality 2", "VBR quality 3",
                                        "VBR quality 4 (normal)", "VBR quality 5", "VBR quality 6", "VBR quality 7",
                                        "VBR quality 8", "VBR quality 9 (smallest)", nullptr };

    juce::StringArray opts (vbrOptions);

    const int cbrRates[] = { 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };

    for (int i = 0; i < std::ssize (cbrRates); ++i)
        opts.add (juce::String (cbrRates[i]) + " Kb/s CBR");

    return opts;
}

juce::AudioFormatReader* LibLameEncoderAudioFormat::createReaderFor (juce::InputStream*, const bool)
{
    return nullptr;
}

std::unique_ptr<juce::AudioFormatWriter> LibLameEncoderAudioFormat::createWriterFor (std::unique_ptr<juce::OutputStream>& streamToWriteTo,
                                                                                     const juce::AudioFormatWriterOptions& options)
{
    if (streamToWriteTo == nullptr || ! isLibraryLoaded())
        return {};

    auto numChans = options.getNumChannels();

    if (numChans < 1 || numChans > 2)   // MP3 is mono or stereo only
        return {};

    auto sampleRate = juce::roundToInt (options.getSampleRate());

    if (sampleRate <= 0)
        return {};

    auto gfp = lame->lame_init();

    if (gfp == nullptr)
        return {};

    lame->redirectLogging (gfp);

    lame->lame_set_in_samplerate (gfp, sampleRate);
    lame->lame_set_out_samplerate (gfp, sampleRate);
    lame->lame_set_num_channels (gfp, numChans);
    lame->lame_set_mode (gfp, numChans == 1 ? liblame::modeMono : liblame::modeJointStereo);
    lame->lame_set_quality (gfp, 2);

    int vbr = 4, cbr = 0;
    const juce::String qual (getQualityOptions()[options.getQualityOptionIndex()]);

    if (qual.contains ("VBR"))
        vbr = qual.retainCharacters ("0123456789").getIntValue();
    else
        cbr = qual.getIntValue();

    if (cbr > 0)
    {
        lame->lame_set_VBR (gfp, liblame::vbrOff);
        lame->lame_set_brate (gfp, cbr);
    }
    else
    {
        lame->lame_set_VBR (gfp, liblame::vbrMtrh);
        lame->lame_set_VBR_q (gfp, vbr);
    }

    // LAME's automatic tag writing assumes it owns a FILE*, which we don't have, so the
    // ID3 and Xing tags get written to the stream by the Writer instead
    if (lame->lame_set_write_id3tag_automatic != nullptr)
        lame->lame_set_write_id3tag_automatic (gfp, 0);

    if (lame->lame_set_bWriteVbrTag != nullptr)
        lame->lame_set_bWriteVbrTag (gfp, lame->lame_get_lametag_frame != nullptr ? 1 : 0);

    juce::StringPairArray metadata;
    metadata.addUnorderedMap (options.getMetadataValues());

    if (! metadata.getAllKeys().isEmpty())
        lame->applyMetadata (gfp, metadata);

    if (lame->lame_init_params (gfp) < 0)
    {
        TRACKTION_LOG ("LAME: couldn't initialise the encoder");
        lame->lame_close (gfp);
        return {};
    }

    return std::make_unique<Writer> (streamToWriteTo, getFormatName(), lame, gfp, options);
}

#endif

} // namespace tracktion::inline engine
