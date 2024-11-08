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

#ifndef CHOC_AUDIOFILEFORMAT_FLAC_HEADER_INCLUDED
#define CHOC_AUDIOFILEFORMAT_FLAC_HEADER_INCLUDED

#include "../platform/choc_Assert.h"
#include "choc_AudioFileFormat.h"
#include "choc_AudioSampleData.h"

namespace choc::audio
{

//==============================================================================
/**
    An AudioFormat class which can read and write FLAC files.

    The template parameter lets you choose whether the format can create
    writers or not - if you only need to read and not write, then using a
    FLACAudioFileFormat<false> will avoid bloating your binary with (quite
    a significant amount of) unused code.
*/
template <bool supportWriting>
class FLACAudioFileFormat  : public AudioFileFormat
{
public:
    FLACAudioFileFormat() = default;
    ~FLACAudioFileFormat() override {}

    std::string getFileSuffixes() override;
    uint32_t getMaximumNumChannels() override;
    std::vector<double> getSupportedSampleRates() override;
    std::vector<BitDepth> getSupportedBitDepths() override;
    std::vector<std::string> getQualityLevels() override;
    bool supportsWriting() const override;

    std::unique_ptr<AudioFileReader> createReader (std::shared_ptr<std::istream>) override;
    std::unique_ptr<AudioFileWriter> createWriter (std::shared_ptr<std::ostream>, AudioFileProperties) override;
    using AudioFileFormat::createReader;
    using AudioFileFormat::createWriter;

private:
    struct Implementation;
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

} // namespace choc::audio

#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <stddef.h>

#if (defined (_WIN32) || defined (_WIN64)) && (defined (_M_X64) || defined(__x86_64__))
 #include <intrin.h>
#endif

#ifdef __linux__
 #include <signal.h>
#endif

#ifdef __APPLE__
 #include <sys/sysctl.h>
#endif

#if _MSC_VER
 #include <excpt.h>
#endif

namespace choc::audio
{

#include "../platform/choc_DisableAllWarnings.h"

namespace flac
{

/*
    The massive chunk of code inside this flac namespace is the content
    of most of the files from libflac 1.3.1, copy-pasted into a big lump
    so we can all live the single-header-file dream.

    The original code can be found here:
    https://github.com/xiph/flac

    The original BSD-like license is available here:
    https://github.com/xiph/flac/blob/master/COPYING.Xiph

    I've made a few hacks to it to make it fit this format better:
    I've either removed or renamed most of the macros to avoid polluting the
    global namespace with them. I've also deleted some chunks of code that weren't
    needed for the platforms choc runs on, to keep the bloat down. And to make
    it possible to add this header to multiple compile units, I marked the
    functions as 'inline', which was a bit of a pain to do.

    If you skip to the bottom of this file, you'll find the few hundred
    lines of choc wrapper code that actually uses the FLAC library to
    implement the AudioFileFormat classes...
*/

#ifdef CHOC_REGISTER_OPEN_SOURCE_LICENCE
 CHOC_REGISTER_OPEN_SOURCE_LICENCE (FLAC, R"(
==============================================================================
FLAC license:

Copyright (C) 2000-2009  Josh Coalson
Copyright (C) 2011-2022  Xiph.Org Foundation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

- Neither the name of the Xiph.Org Foundation nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
)")
#endif

#ifndef M_PI
 static constexpr double M_PI = 3.141592653589793238;
#endif

#ifndef M_LN2
 static constexpr double M_LN2 = 0.69314718055994530942;
#endif

#if ! (defined (__ARM_ARCH) || defined (__arm__) || defined (__arm64__) || defined(__aarch64__) || defined(_M_ARM64) || defined (__EMSCRIPTEN__))
 #define FLAC__HAS_X86INTRIN 1

 #if defined (_WIN64) || defined (_WIN64)
  #define FLAC__CPU_X86_64 1
 #else
  #define FLAC__CPU_IA32 1
 #endif
#endif

#undef FLAC_VERSION
#define FLAC_VERSION "1.3.1"

#define FLAC_API inline

/* For MSVC 2010 and everything else which provides <stdint.h>. */

typedef int8_t FLAC__int8;
typedef uint8_t FLAC__uint8;
typedef int16_t FLAC__int16;
typedef int32_t FLAC__int32;
typedef int64_t FLAC__int64;
typedef uint16_t FLAC__uint16;
typedef uint32_t FLAC__uint32;
typedef uint64_t FLAC__uint64;

typedef int FLAC__bool;
typedef FLAC__uint8 FLAC__byte;

#ifdef true
#undef true
#endif
#ifdef false
#undef false
#endif

static int32_t signedLeftShift (int32_t n, int bits)  { return static_cast<int32_t> (static_cast<uint32_t> (n) << bits); }

/** The largest legal metadata type code. */
static constexpr uint32_t FLAC__MAX_METADATA_TYPE_CODE = (126u);

/** The minimum block size, in samples, permitted by the format. */
static constexpr uint32_t FLAC__MIN_BLOCK_SIZE = (16u);

/** The maximum block size, in samples, permitted by the format. */
static constexpr uint32_t FLAC__MAX_BLOCK_SIZE = (65535u);

/** The maximum block size, in samples, permitted by the FLAC subset for
 *  sample rates up to 48kHz. */
static constexpr uint32_t FLAC__SUBSET_MAX_BLOCK_SIZE_48000HZ = (4608u);

/** The maximum number of channels permitted by the format. */
static constexpr uint32_t FLAC__MAX_CHANNELS = (8u);

/** The minimum sample resolution permitted by the format. */
static constexpr uint32_t FLAC__MIN_BITS_PER_SAMPLE = (4u);

/** The maximum sample resolution permitted by the format. */
static constexpr uint32_t FLAC__MAX_BITS_PER_SAMPLE = (32u);

/** The maximum sample resolution permitted by libFLAC.
 *
 * \warning
 * FLAC__MAX_BITS_PER_SAMPLE is the limit of the FLAC format.  However,
 * the reference encoder/decoder is currently limited to 24 bits because
 * of prevalent 32-bit math, so make sure and use this value when
 * appropriate.
 */
static constexpr uint32_t FLAC__REFERENCE_CODEC_MAX_BITS_PER_SAMPLE = (24u);

/** The maximum sample rate permitted by the format.  The value is
 *  ((2 ^ 16) - 1) * 10; see <A HREF="../format.html">FLAC format</A>
 *  as to why.
 */
static constexpr uint32_t FLAC__MAX_SAMPLE_RATE = (655350u);

/** The maximum LPC order permitted by the format. */
static constexpr uint32_t FLAC__MAX_LPC_ORDER = (32u);

/** The maximum LPC order permitted by the FLAC subset for sample rates
 *  up to 48kHz. */
static constexpr uint32_t FLAC__SUBSET_MAX_LPC_ORDER_48000HZ = (12u);

/** The minimum quantized linear predictor coefficient precision
 *  permitted by the format.
 */
static constexpr uint32_t FLAC__MIN_QLP_COEFF_PRECISION = (5u);

/** The maximum quantized linear predictor coefficient precision
 *  permitted by the format.
 */
static constexpr uint32_t FLAC__MAX_QLP_COEFF_PRECISION = (15u);

/** The maximum order of the fixed predictors permitted by the format. */
static constexpr uint32_t FLAC__MAX_FIXED_ORDER = (4u);

/** The maximum Rice partition order permitted by the format. */
static constexpr uint32_t FLAC__MAX_RICE_PARTITION_ORDER = (15u);

/** The maximum Rice partition order permitted by the FLAC Subset. */
static constexpr uint32_t FLAC__SUBSET_MAX_RICE_PARTITION_ORDER = (8u);

/** The length of the FLAC signature in bytes. */
static constexpr uint32_t FLAC__STREAM_SYNC_LENGTH = (4u);


/*****************************************************************************
 *
 * Subframe structures
 *
 *****************************************************************************/

/*****************************************************************************/

/** An enumeration of the available entropy coding methods. */
typedef enum {
    FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE = 0,
    /**< Residual is coded by partitioning into contexts, each with it's own
     * 4-bit Rice parameter. */

    FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2 = 1
    /**< Residual is coded by partitioning into contexts, each with it's own
     * 5-bit Rice parameter. */
} FLAC__EntropyCodingMethodType;

/** Contents of a Rice partitioned residual
 */
typedef struct {

    unsigned *parameters;
    /**< The Rice parameters for each context. */

    unsigned *raw_bits;
    /**< Widths for escape-coded partitions.  Will be non-zero for escaped
     * partitions and zero for unescaped partitions.
     */

    unsigned capacity_by_order;
    /**< The capacity of the \a parameters and \a raw_bits arrays
     * specified as an order, i.e. the number of array elements
     * allocated is 2 ^ \a capacity_by_order.
     */
} FLAC__EntropyCodingMethod_PartitionedRiceContents;

/** Header for a Rice partitioned residual.  (c.f. <A HREF="../format.html#partitioned_rice">format specification</A>)
 */
typedef struct {

    unsigned order;
    /**< The partition order, i.e. # of contexts = 2 ^ \a order. */

    const FLAC__EntropyCodingMethod_PartitionedRiceContents *contents;
    /**< The context's Rice parameters and/or raw bits. */

} FLAC__EntropyCodingMethod_PartitionedRice;

/** Header for the entropy coding method.  (c.f. <A HREF="../format.html#residual">format specification</A>)
 */
typedef struct {
    FLAC__EntropyCodingMethodType type;
    union {
        FLAC__EntropyCodingMethod_PartitionedRice partitioned_rice;
    } data;
} FLAC__EntropyCodingMethod;


/*****************************************************************************/

/** An enumeration of the available subframe types. */
typedef enum {
    FLAC__SUBFRAME_TYPE_CONSTANT = 0, /**< constant signal */
    FLAC__SUBFRAME_TYPE_VERBATIM = 1, /**< uncompressed signal */
    FLAC__SUBFRAME_TYPE_FIXED = 2, /**< fixed polynomial prediction */
    FLAC__SUBFRAME_TYPE_LPC = 3 /**< linear prediction */
} FLAC__SubframeType;


/** CONSTANT subframe.  (c.f. <A HREF="../format.html#subframe_constant">format specification</A>)
 */
typedef struct {
    FLAC__int32 value; /**< The constant signal value. */
} FLAC__Subframe_Constant;


/** VERBATIM subframe.  (c.f. <A HREF="../format.html#subframe_verbatim">format specification</A>)
 */
typedef struct {
    const FLAC__int32 *data; /**< A pointer to verbatim signal. */
} FLAC__Subframe_Verbatim;


/** FIXED subframe.  (c.f. <A HREF="../format.html#subframe_fixed">format specification</A>)
 */
typedef struct {
    FLAC__EntropyCodingMethod entropy_coding_method;
    /**< The residual coding method. */

    unsigned order;
    /**< The polynomial order. */

    FLAC__int32 warmup[FLAC__MAX_FIXED_ORDER];
    /**< Warmup samples to prime the predictor, length == order. */

    const FLAC__int32 *residual;
    /**< The residual signal, length == (blocksize minus order) samples. */
} FLAC__Subframe_Fixed;


/** LPC subframe.  (c.f. <A HREF="../format.html#subframe_lpc">format specification</A>)
 */
typedef struct {
    FLAC__EntropyCodingMethod entropy_coding_method;
    /**< The residual coding method. */

    unsigned order;
    /**< The FIR order. */

    unsigned qlp_coeff_precision;
    /**< Quantized FIR filter coefficient precision in bits. */

    int quantization_level;
    /**< The qlp coeff shift needed. */

    FLAC__int32 qlp_coeff[FLAC__MAX_LPC_ORDER];
    /**< FIR filter coefficients. */

    FLAC__int32 warmup[FLAC__MAX_LPC_ORDER];
    /**< Warmup samples to prime the predictor, length == order. */

    const FLAC__int32 *residual;
    /**< The residual signal, length == (blocksize minus order) samples. */
} FLAC__Subframe_LPC;

/** FLAC subframe structure.  (c.f. <A HREF="../format.html#subframe">format specification</A>)
 */
typedef struct {
    FLAC__SubframeType type;
    union {
        FLAC__Subframe_Constant constant;
        FLAC__Subframe_Fixed fixed;
        FLAC__Subframe_LPC lpc;
        FLAC__Subframe_Verbatim verbatim;
    } data;
    unsigned wasted_bits;
} FLAC__Subframe;


/*****************************************************************************
 *
 * Frame structures
 *
 *****************************************************************************/

/** An enumeration of the available channel assignments. */
typedef enum {
    FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT = 0, /**< independent channels */
    FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE = 1, /**< left+side stereo */
    FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE = 2, /**< right+side stereo */
    FLAC__CHANNEL_ASSIGNMENT_MID_SIDE = 3 /**< mid+side stereo */
} FLAC__ChannelAssignment;

/** An enumeration of the possible frame numbering methods. */
typedef enum {
    FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER, /**< number contains the frame number */
    FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER /**< number contains the sample number of first sample in frame */
} FLAC__FrameNumberType;


/** FLAC frame header structure.  (c.f. <A HREF="../format.html#frame_header">format specification</A>)
 */
typedef struct {
    unsigned blocksize;
    /**< The number of samples per subframe. */

    unsigned sample_rate;
    /**< The sample rate in Hz. */

    unsigned channels;
    /**< The number of channels (== number of subframes). */

    FLAC__ChannelAssignment channel_assignment;
    /**< The channel assignment for the frame. */

    unsigned bits_per_sample;
    /**< The sample resolution. */

    FLAC__FrameNumberType number_type;
    /**< The numbering scheme used for the frame.  As a convenience, the
     * decoder will always convert a frame number to a sample number because
     * the rules are complex. */

    union {
        FLAC__uint32 frame_number;
        FLAC__uint64 sample_number;
    } number;
    /**< The frame number or sample number of first sample in frame;
     * use the \a number_type value to determine which to use. */

    FLAC__uint8 crc;
    /**< CRC-8 (polynomial = x^8 + x^2 + x^1 + x^0, initialized with 0)
     * of the raw frame header bytes, meaning everything before the CRC byte
     * including the sync code.
     */
} FLAC__FrameHeader;

/** FLAC frame footer structure.  (c.f. <A HREF="../format.html#frame_footer">format specification</A>)
 */
typedef struct {
    FLAC__uint16 crc;
    /**< CRC-16 (polynomial = x^16 + x^15 + x^2 + x^0, initialized with
     * 0) of the bytes before the crc, back to and including the frame header
     * sync code.
     */
} FLAC__FrameFooter;

/** FLAC frame structure.  (c.f. <A HREF="../format.html#frame">format specification</A>)
 */
typedef struct {
    FLAC__FrameHeader header;
    FLAC__Subframe subframes[FLAC__MAX_CHANNELS];
    FLAC__FrameFooter footer;
} FLAC__Frame;

/*****************************************************************************/


/*****************************************************************************
 *
 * Meta-data structures
 *
 *****************************************************************************/

/** An enumeration of the available metadata block types. */
typedef enum {

    FLAC__METADATA_TYPE_STREAMINFO = 0,
    /**< <A HREF="../format.html#metadata_block_streaminfo">STREAMINFO</A> block */

    FLAC__METADATA_TYPE_PADDING = 1,
    /**< <A HREF="../format.html#metadata_block_padding">PADDING</A> block */

    FLAC__METADATA_TYPE_APPLICATION = 2,
    /**< <A HREF="../format.html#metadata_block_application">APPLICATION</A> block */

    FLAC__METADATA_TYPE_SEEKTABLE = 3,
    /**< <A HREF="../format.html#metadata_block_seektable">SEEKTABLE</A> block */

    FLAC__METADATA_TYPE_VORBIS_COMMENT = 4,
    /**< <A HREF="../format.html#metadata_block_vorbis_comment">VORBISCOMMENT</A> block (a.k.a. FLAC tags) */

    FLAC__METADATA_TYPE_CUESHEET = 5,
    /**< <A HREF="../format.html#metadata_block_cuesheet">CUESHEET</A> block */

    FLAC__METADATA_TYPE_PICTURE = 6,
    /**< <A HREF="../format.html#metadata_block_picture">PICTURE</A> block */

    FLAC__METADATA_TYPE_UNDEFINED = 7,
    /**< marker to denote beginning of undefined type range; this number will increase as new metadata types are added */

    FLAC__MAX_METADATA_TYPE = FLAC__MAX_METADATA_TYPE_CODE,
    /**< No type will ever be greater than this. There is not enough room in the protocol block. */
} FLAC__MetadataType;


/** FLAC STREAMINFO structure.  (c.f. <A HREF="../format.html#metadata_block_streaminfo">format specification</A>)
 */
typedef struct {
    unsigned min_blocksize, max_blocksize;
    unsigned min_framesize, max_framesize;
    unsigned sample_rate;
    unsigned channels;
    unsigned bits_per_sample;
    FLAC__uint64 total_samples;
    FLAC__byte md5sum[16];
} FLAC__StreamMetadata_StreamInfo;


/** The total stream length of the STREAMINFO block in bytes. */
static constexpr uint32_t FLAC__STREAM_METADATA_STREAMINFO_LENGTH = 34u;

/** FLAC PADDING structure.  (c.f. <A HREF="../format.html#metadata_block_padding">format specification</A>)
 */
typedef struct {
    int dummy;
    /**< Conceptually this is an empty struct since we don't store the
     * padding bytes.  Empty structs are not allowed by some C compilers,
     * hence the dummy.
     */
} FLAC__StreamMetadata_Padding;


/** FLAC APPLICATION structure.  (c.f. <A HREF="../format.html#metadata_block_application">format specification</A>)
 */
typedef struct {
    FLAC__byte id[4];
    FLAC__byte *data;
} FLAC__StreamMetadata_Application;

/** SeekPoint structure used in SEEKTABLE blocks.  (c.f. <A HREF="../format.html#seekpoint">format specification</A>)
 */
typedef struct {
    FLAC__uint64 sample_number;
    /**<  The sample number of the target frame. */

    FLAC__uint64 stream_offset;
    /**< The offset, in bytes, of the target frame with respect to
     * beginning of the first frame. */

    unsigned frame_samples;
    /**< The number of samples in the target frame. */
} FLAC__StreamMetadata_SeekPoint;

/** The total stream length of a seek point in bytes. */
static constexpr uint32_t FLAC__STREAM_METADATA_SEEKPOINT_LENGTH = (18u);


/** FLAC SEEKTABLE structure.  (c.f. <A HREF="../format.html#metadata_block_seektable">format specification</A>)
 *
 * \note From the format specification:
 * - The seek points must be sorted by ascending sample number.
 * - Each seek point's sample number must be the first sample of the
 *   target frame.
 * - Each seek point's sample number must be unique within the table.
 * - Existence of a SEEKTABLE block implies a correct setting of
 *   total_samples in the stream_info block.
 * - Behavior is undefined when more than one SEEKTABLE block is
 *   present in a stream.
 */
typedef struct {
    unsigned num_points;
    FLAC__StreamMetadata_SeekPoint *points;
} FLAC__StreamMetadata_SeekTable;


/** Vorbis comment entry structure used in VORBIS_COMMENT blocks.  (c.f. <A HREF="../format.html#metadata_block_vorbis_comment">format specification</A>)
 *
 *  For convenience, the APIs maintain a trailing NUL character at the end of
 *  \a entry which is not counted toward \a length, i.e.
 *  \code strlen(entry) == length \endcode
 */
typedef struct {
    FLAC__uint32 length;
    FLAC__byte *entry;
} FLAC__StreamMetadata_VorbisComment_Entry;


/** FLAC VORBIS_COMMENT structure.  (c.f. <A HREF="../format.html#metadata_block_vorbis_comment">format specification</A>)
 */
typedef struct {
    FLAC__StreamMetadata_VorbisComment_Entry vendor_string;
    FLAC__uint32 num_comments;
    FLAC__StreamMetadata_VorbisComment_Entry *comments;
} FLAC__StreamMetadata_VorbisComment;


/** FLAC CUESHEET track index structure.  (See the
 * <A HREF="../format.html#cuesheet_track_index">format specification</A> for
 * the full description of each field.)
 */
typedef struct {
    FLAC__uint64 offset;
    /**< Offset in samples, relative to the track offset, of the index
     * point.
     */

    FLAC__byte number;
    /**< The index point number. */
} FLAC__StreamMetadata_CueSheet_Index;


/** FLAC CUESHEET track structure.  (See the
 * <A HREF="../format.html#cuesheet_track">format specification</A> for
 * the full description of each field.)
 */
typedef struct {
    FLAC__uint64 offset;
    /**< Track offset in samples, relative to the beginning of the FLAC audio stream. */

    FLAC__byte number;
    /**< The track number. */

    char isrc[13];
    /**< Track ISRC.  This is a 12-digit alphanumeric code plus a trailing \c NUL byte */

    unsigned type:1;
    /**< The track type: 0 for audio, 1 for non-audio. */

    unsigned pre_emphasis:1;
    /**< The pre-emphasis flag: 0 for no pre-emphasis, 1 for pre-emphasis. */

    FLAC__byte num_indices;
    /**< The number of track index points. */

    FLAC__StreamMetadata_CueSheet_Index *indices;
    /**< NULL if num_indices == 0, else pointer to array of index points. */

} FLAC__StreamMetadata_CueSheet_Track;


/** FLAC CUESHEET structure.  (See the
 * <A HREF="../format.html#metadata_block_cuesheet">format specification</A>
 * for the full description of each field.)
 */
typedef struct {
    char media_catalog_number[129];
    /**< Media catalog number, in ASCII printable characters 0x20-0x7e.  In
     * general, the media catalog number may be 0 to 128 bytes long; any
     * unused characters should be right-padded with NUL characters.
     */

    FLAC__uint64 lead_in;
    /**< The number of lead-in samples. */

    FLAC__bool is_cd;
    /**< \c true if CUESHEET corresponds to a Compact Disc, else \c false. */

    unsigned num_tracks;
    /**< The number of tracks. */

    FLAC__StreamMetadata_CueSheet_Track *tracks;
    /**< NULL if num_tracks == 0, else pointer to array of tracks. */

} FLAC__StreamMetadata_CueSheet;


/** An enumeration of the PICTURE types (see FLAC__StreamMetadataPicture and id3 v2.4 APIC tag). */
typedef enum {
    FLAC__STREAM_METADATA_PICTURE_TYPE_OTHER = 0, /**< Other */
    FLAC__STREAM_METADATA_PICTURE_TYPE_FILE_ICON_STANDARD = 1, /**< 32x32 pixels 'file icon' (PNG only) */
    FLAC__STREAM_METADATA_PICTURE_TYPE_FILE_ICON = 2, /**< Other file icon */
    FLAC__STREAM_METADATA_PICTURE_TYPE_FRONT_COVER = 3, /**< Cover (front) */
    FLAC__STREAM_METADATA_PICTURE_TYPE_BACK_COVER = 4, /**< Cover (back) */
    FLAC__STREAM_METADATA_PICTURE_TYPE_LEAFLET_PAGE = 5, /**< Leaflet page */
    FLAC__STREAM_METADATA_PICTURE_TYPE_MEDIA = 6, /**< Media (e.g. label side of CD) */
    FLAC__STREAM_METADATA_PICTURE_TYPE_LEAD_ARTIST = 7, /**< Lead artist/lead performer/soloist */
    FLAC__STREAM_METADATA_PICTURE_TYPE_ARTIST = 8, /**< Artist/performer */
    FLAC__STREAM_METADATA_PICTURE_TYPE_CONDUCTOR = 9, /**< Conductor */
    FLAC__STREAM_METADATA_PICTURE_TYPE_BAND = 10, /**< Band/Orchestra */
    FLAC__STREAM_METADATA_PICTURE_TYPE_COMPOSER = 11, /**< Composer */
    FLAC__STREAM_METADATA_PICTURE_TYPE_LYRICIST = 12, /**< Lyricist/text writer */
    FLAC__STREAM_METADATA_PICTURE_TYPE_RECORDING_LOCATION = 13, /**< Recording Location */
    FLAC__STREAM_METADATA_PICTURE_TYPE_DURING_RECORDING = 14, /**< During recording */
    FLAC__STREAM_METADATA_PICTURE_TYPE_DURING_PERFORMANCE = 15, /**< During performance */
    FLAC__STREAM_METADATA_PICTURE_TYPE_VIDEO_SCREEN_CAPTURE = 16, /**< Movie/video screen capture */
    FLAC__STREAM_METADATA_PICTURE_TYPE_FISH = 17, /**< A bright coloured fish */
    FLAC__STREAM_METADATA_PICTURE_TYPE_ILLUSTRATION = 18, /**< Illustration */
    FLAC__STREAM_METADATA_PICTURE_TYPE_BAND_LOGOTYPE = 19, /**< Band/artist logotype */
    FLAC__STREAM_METADATA_PICTURE_TYPE_PUBLISHER_LOGOTYPE = 20, /**< Publisher/Studio logotype */
    FLAC__STREAM_METADATA_PICTURE_TYPE_UNDEFINED
} FLAC__StreamMetadata_Picture_Type;

/** FLAC PICTURE structure.  (See the
 * <A HREF="../format.html#metadata_block_picture">format specification</A>
 * for the full description of each field.)
 */
typedef struct {
    FLAC__StreamMetadata_Picture_Type type;
    /**< The kind of picture stored. */

    char *mime_type;
    /**< Picture data's MIME type, in ASCII printable characters
     * 0x20-0x7e, NUL terminated.  For best compatibility with players,
     * use picture data of MIME type \c image/jpeg or \c image/png.  A
     * MIME type of '-->' is also allowed, in which case the picture
     * data should be a complete URL.  In file storage, the MIME type is
     * stored as a 32-bit length followed by the ASCII string with no NUL
     * terminator, but is converted to a plain C string in this structure
     * for convenience.
     */

    FLAC__byte *description;
    /**< Picture's description in UTF-8, NUL terminated.  In file storage,
     * the description is stored as a 32-bit length followed by the UTF-8
     * string with no NUL terminator, but is converted to a plain C string
     * in this structure for convenience.
     */

    FLAC__uint32 width;
    /**< Picture's width in pixels. */

    FLAC__uint32 height;
    /**< Picture's height in pixels. */

    FLAC__uint32 depth;
    /**< Picture's color depth in bits-per-pixel. */

    FLAC__uint32 colors;
    /**< For indexed palettes (like GIF), picture's number of colors (the
     * number of palette entries), or \c 0 for non-indexed (i.e. 2^depth).
     */

    FLAC__uint32 data_length;
    /**< Length of binary picture data in bytes. */

    FLAC__byte *data;
    /**< Binary picture data. */

} FLAC__StreamMetadata_Picture;


/** Structure that is used when a metadata block of unknown type is loaded.
 *  The contents are opaque.  The structure is used only internally to
 *  correctly handle unknown metadata.
 */
typedef struct {
    FLAC__byte *data;
} FLAC__StreamMetadata_Unknown;


/** FLAC metadata block structure.  (c.f. <A HREF="../format.html#metadata_block">format specification</A>)
 */
typedef struct {
    FLAC__MetadataType type;
    /**< The type of the metadata block; used determine which member of the
     * \a data union to dereference.  If type >= FLAC__METADATA_TYPE_UNDEFINED
     * then \a data.unknown must be used. */

    FLAC__bool is_last;
    /**< \c true if this metadata block is the last, else \a false */

    unsigned length;
    /**< Length, in bytes, of the block data as it appears in the stream. */

    union {
        FLAC__StreamMetadata_StreamInfo stream_info;
        FLAC__StreamMetadata_Padding padding;
        FLAC__StreamMetadata_Application application;
        FLAC__StreamMetadata_SeekTable seek_table;
        FLAC__StreamMetadata_VorbisComment vorbis_comment;
        FLAC__StreamMetadata_CueSheet cue_sheet;
        FLAC__StreamMetadata_Picture picture;
        FLAC__StreamMetadata_Unknown unknown;
    } data;
    /**< Polymorphic block data; use the \a type value to determine which
     * to use. */
} FLAC__StreamMetadata;

/** The total stream length of a metadata block header in bytes. */
static constexpr uint32_t FLAC__STREAM_METADATA_HEADER_LENGTH = (4u);

/*****************************************************************************/


/*****************************************************************************
 *
 * Utility functions
 *
 *****************************************************************************/

/** Tests that a sample rate is valid for FLAC.
 *
 * \param sample_rate  The sample rate to test for compliance.
 * \retval FLAC__bool
 *    \c true if the given sample rate conforms to the specification, else
 *    \c false.
 */
FLAC_API FLAC__bool FLAC__format_sample_rate_is_valid(unsigned sample_rate);

/** Tests that a blocksize at the given sample rate is valid for the FLAC
 *  subset.
 *
 * \param blocksize    The blocksize to test for compliance.
 * \param sample_rate  The sample rate is needed, since the valid subset
 *                     blocksize depends on the sample rate.
 * \retval FLAC__bool
 *    \c true if the given blocksize conforms to the specification for the
 *    subset at the given sample rate, else \c false.
 */
FLAC_API FLAC__bool FLAC__format_blocksize_is_subset(unsigned blocksize, unsigned sample_rate);

/** Tests that a sample rate is valid for the FLAC subset.  The subset rules
 *  for valid sample rates are slightly more complex since the rate has to
 *  be expressible completely in the frame header.
 *
 * \param sample_rate  The sample rate to test for compliance.
 * \retval FLAC__bool
 *    \c true if the given sample rate conforms to the specification for the
 *    subset, else \c false.
 */
FLAC_API FLAC__bool FLAC__format_sample_rate_is_subset(unsigned sample_rate);

/** Check a Vorbis comment entry name to see if it conforms to the Vorbis
 *  comment specification.
 *
 *  Vorbis comment names must be composed only of characters from
 *  [0x20-0x3C,0x3E-0x7D].
 *
 * \param name       A NUL-terminated string to be checked.
 * \assert
 *    \code name != NULL \endcode
 * \retval FLAC__bool
 *    \c false if entry name is illegal, else \c true.
 */
FLAC_API FLAC__bool FLAC__format_vorbiscomment_entry_name_is_legal(const char *name);

/** Check a Vorbis comment entry value to see if it conforms to the Vorbis
 *  comment specification.
 *
 *  Vorbis comment values must be valid UTF-8 sequences.
 *
 * \param value      A string to be checked.
 * \param length     A the length of \a value in bytes.  May be
 *                   \c (unsigned)(-1) to indicate that \a value is a plain
 *                   UTF-8 NUL-terminated string.
 * \assert
 *    \code value != NULL \endcode
 * \retval FLAC__bool
 *    \c false if entry name is illegal, else \c true.
 */
FLAC_API FLAC__bool FLAC__format_vorbiscomment_entry_value_is_legal(const FLAC__byte *value, unsigned length);

/** Check a Vorbis comment entry to see if it conforms to the Vorbis
 *  comment specification.
 *
 *  Vorbis comment entries must be of the form 'name=value', and 'name' and
 *  'value' must be legal according to
 *  FLAC__format_vorbiscomment_entry_name_is_legal() and
 *  FLAC__format_vorbiscomment_entry_value_is_legal() respectively.
 *
 * \param entry      An entry to be checked.
 * \param length     The length of \a entry in bytes.
 * \assert
 *    \code value != NULL \endcode
 * \retval FLAC__bool
 *    \c false if entry name is illegal, else \c true.
 */
FLAC_API FLAC__bool FLAC__format_vorbiscomment_entry_is_legal(const FLAC__byte *entry, unsigned length);

/** Check a seek table to see if it conforms to the FLAC specification.
 *  See the format specification for limits on the contents of the
 *  seek table.
 *
 * \param seek_table  A pointer to a seek table to be checked.
 * \assert
 *    \code seek_table != NULL \endcode
 * \retval FLAC__bool
 *    \c false if seek table is illegal, else \c true.
 */
FLAC_API FLAC__bool FLAC__format_seektable_is_legal(const FLAC__StreamMetadata_SeekTable *seek_table);

/** Sort a seek table's seek points according to the format specification.
 *  This includes a "unique-ification" step to remove duplicates, i.e.
 *  seek points with identical \a sample_number values.  Duplicate seek
 *  points are converted into placeholder points and sorted to the end of
 *  the table.
 *
 * \param seek_table  A pointer to a seek table to be sorted.
 * \assert
 *    \code seek_table != NULL \endcode
 * \retval unsigned
 *    The number of duplicate seek points converted into placeholders.
 */
FLAC_API unsigned FLAC__format_seektable_sort(FLAC__StreamMetadata_SeekTable *seek_table);

/** Check a cue sheet to see if it conforms to the FLAC specification.
 *  See the format specification for limits on the contents of the
 *  cue sheet.
 *
 * \param cue_sheet  A pointer to an existing cue sheet to be checked.
 * \param check_cd_da_subset  If \c true, check CUESHEET against more
 *                   stringent requirements for a CD-DA (audio) disc.
 * \param violation  Address of a pointer to a string.  If there is a
 *                   violation, a pointer to a string explanation of the
 *                   violation will be returned here. \a violation may be
 *                   \c NULL if you don't need the returned string.  Do not
 *                   free the returned string; it will always point to static
 *                   data.
 * \assert
 *    \code cue_sheet != NULL \endcode
 * \retval FLAC__bool
 *    \c false if cue sheet is illegal, else \c true.
 */
FLAC_API FLAC__bool FLAC__format_cuesheet_is_legal(const FLAC__StreamMetadata_CueSheet *cue_sheet, FLAC__bool check_cd_da_subset, const char **violation);

/** Check picture data to see if it conforms to the FLAC specification.
 *  See the format specification for limits on the contents of the
 *  PICTURE block.
 *
 * \param picture    A pointer to existing picture data to be checked.
 * \param violation  Address of a pointer to a string.  If there is a
 *                   violation, a pointer to a string explanation of the
 *                   violation will be returned here. \a violation may be
 *                   \c NULL if you don't need the returned string.  Do not
 *                   free the returned string; it will always point to static
 *                   data.
 * \assert
 *    \code picture != NULL \endcode
 * \retval FLAC__bool
 *    \c false if picture data is illegal, else \c true.
 */
FLAC_API FLAC__bool FLAC__format_picture_is_legal(const FLAC__StreamMetadata_Picture *picture, const char **violation);


/** State values for a FLAC__StreamDecoder
 *
 * The decoder's state can be obtained by calling FLAC__stream_decoder_get_state().
 */
typedef enum {

    FLAC__STREAM_DECODER_SEARCH_FOR_METADATA = 0,
    /**< The decoder is ready to search for metadata. */

    FLAC__STREAM_DECODER_READ_METADATA,
    /**< The decoder is ready to or is in the process of reading metadata. */

    FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC,
    /**< The decoder is ready to or is in the process of searching for the
     * frame sync code.
     */

    FLAC__STREAM_DECODER_READ_FRAME,
    /**< The decoder is ready to or is in the process of reading a frame. */

    FLAC__STREAM_DECODER_END_OF_STREAM,
    /**< The decoder has reached the end of the stream. */

    FLAC__STREAM_DECODER_OGG_ERROR,
    /**< An error occurred in the underlying Ogg layer.  */

    FLAC__STREAM_DECODER_SEEK_ERROR,
    /**< An error occurred while seeking.  The decoder must be flushed
     * with FLAC__stream_decoder_flush() or reset with
     * FLAC__stream_decoder_reset() before decoding can continue.
     */

    FLAC__STREAM_DECODER_ABORTED,
    /**< The decoder was aborted by the read callback. */

    FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR,
    /**< An error occurred allocating memory.  The decoder is in an invalid
     * state and can no longer be used.
     */

    FLAC__STREAM_DECODER_UNINITIALIZED
    /**< The decoder is in the uninitialized state; one of the
     * FLAC__stream_decoder_init_*() functions must be called before samples
     * can be processed.
     */

} FLAC__StreamDecoderState;


/** Possible return values for the FLAC__stream_decoder_init_*() functions.
 */
typedef enum {

    FLAC__STREAM_DECODER_INIT_STATUS_OK = 0,
    /**< Initialization was successful. */

    FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER,
    /**< The library was not compiled with support for the given container
     * format.
     */

    FLAC__STREAM_DECODER_INIT_STATUS_INVALID_CALLBACKS,
    /**< A required callback was not supplied. */

    FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR,
    /**< An error occurred allocating memory. */

    FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE,
    /**< fopen() failed in FLAC__stream_decoder_init_file() or
     * FLAC__stream_decoder_init_ogg_file(). */

    FLAC__STREAM_DECODER_INIT_STATUS_ALREADY_INITIALIZED
    /**< FLAC__stream_decoder_init_*() was called when the decoder was
     * already initialized, usually because
     * FLAC__stream_decoder_finish() was not called.
     */

} FLAC__StreamDecoderInitStatus;


/** Return values for the FLAC__StreamDecoder read callback.
 */
typedef enum {

    FLAC__STREAM_DECODER_READ_STATUS_CONTINUE,
    /**< The read was OK and decoding can continue. */

    FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM,
    /**< The read was attempted while at the end of the stream.  Note that
     * the client must only return this value when the read callback was
     * called when already at the end of the stream.  Otherwise, if the read
     * itself moves to the end of the stream, the client should still return
     * the data and \c FLAC__STREAM_DECODER_READ_STATUS_CONTINUE, and then on
     * the next read callback it should return
     * \c FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM with a byte count
     * of \c 0.
     */

    FLAC__STREAM_DECODER_READ_STATUS_ABORT
    /**< An unrecoverable error occurred.  The decoder will return from the process call. */

} FLAC__StreamDecoderReadStatus;


/** Return values for the FLAC__StreamDecoder seek callback.
 */
typedef enum {

    FLAC__STREAM_DECODER_SEEK_STATUS_OK,
    /**< The seek was OK and decoding can continue. */

    FLAC__STREAM_DECODER_SEEK_STATUS_ERROR,
    /**< An unrecoverable error occurred.  The decoder will return from the process call. */

    FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED
    /**< Client does not support seeking. */

} FLAC__StreamDecoderSeekStatus;


/** Return values for the FLAC__StreamDecoder tell callback.
 */
typedef enum {

    FLAC__STREAM_DECODER_TELL_STATUS_OK,
    /**< The tell was OK and decoding can continue. */

    FLAC__STREAM_DECODER_TELL_STATUS_ERROR,
    /**< An unrecoverable error occurred.  The decoder will return from the process call. */

    FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED
    /**< Client does not support telling the position. */

} FLAC__StreamDecoderTellStatus;


/** Return values for the FLAC__StreamDecoder length callback.
 */
typedef enum {

    FLAC__STREAM_DECODER_LENGTH_STATUS_OK,
    /**< The length call was OK and decoding can continue. */

    FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR,
    /**< An unrecoverable error occurred.  The decoder will return from the process call. */

    FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED
    /**< Client does not support reporting the length. */

} FLAC__StreamDecoderLengthStatus;


/** Return values for the FLAC__StreamDecoder write callback.
 */
typedef enum {

    FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE,
    /**< The write was OK and decoding can continue. */

    FLAC__STREAM_DECODER_WRITE_STATUS_ABORT
    /**< An unrecoverable error occurred.  The decoder will return from the process call. */

} FLAC__StreamDecoderWriteStatus;


/** Possible values passed back to the FLAC__StreamDecoder error callback.
 *  \c FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC is the generic catch-
 *  all.  The rest could be caused by bad sync (false synchronization on
 *  data that is not the start of a frame) or corrupted data.  The error
 *  itself is the decoder's best guess at what happened assuming a correct
 *  sync.  For example \c FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER
 *  could be caused by a correct sync on the start of a frame, but some
 *  data in the frame header was corrupted.  Or it could be the result of
 *  syncing on a point the stream that looked like the starting of a frame
 *  but was not.  \c FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM
 *  could be because the decoder encountered a valid frame made by a future
 *  version of the encoder which it cannot parse, or because of a false
 *  sync making it appear as though an encountered frame was generated by
 *  a future encoder.
 */
typedef enum {

    FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC,
    /**< An error in the stream caused the decoder to lose synchronization. */

    FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER,
    /**< The decoder encountered a corrupted frame header. */

    FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH,
    /**< The frame's data did not match the CRC in the footer. */

    FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM
    /**< The decoder encountered reserved fields in use in the stream. */

} FLAC__StreamDecoderErrorStatus;


/***********************************************************************
 *
 * class FLAC__StreamDecoder
 *
 ***********************************************************************/

struct FLAC__StreamDecoderProtected;
struct FLAC__StreamDecoderPrivate;
/** The opaque structure definition for the stream decoder type.
 *  See the \link flac_stream_decoder stream decoder module \endlink
 *  for a detailed description.
 */
typedef struct {
    struct FLAC__StreamDecoderProtected *protected_; /* avoid the C++ keyword 'protected' */
    struct FLAC__StreamDecoderPrivate *private_; /* avoid the C++ keyword 'private' */
} FLAC__StreamDecoder;

/** Signature for the read callback.
 *
 *  A function pointer matching this signature must be passed to
 *  FLAC__stream_decoder_init*_stream(). The supplied function will be
 *  called when the decoder needs more input data.  The address of the
 *  buffer to be filled is supplied, along with the number of bytes the
 *  buffer can hold.  The callback may choose to supply less data and
 *  modify the byte count but must be careful not to overflow the buffer.
 *  The callback then returns a status code chosen from
 *  FLAC__StreamDecoderReadStatus.
 *
 * Here is an example of a read callback for stdio streams:
 * \code
 * FLAC__StreamDecoderReadStatus read_cb(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
 * {
 *   FILE *file = ((MyClientData*)client_data)->file;
 *   if(*bytes > 0) {
 *     *bytes = fread(buffer, sizeof(FLAC__byte), *bytes, file);
 *     if(ferror(file))
 *       return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
 *     else if(*bytes == 0)
 *       return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
 *     else
 *       return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
 *   }
 *   else
 *     return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
 * }
 * \endcode
 *
 * \note In general, FLAC__StreamDecoder functions which change the
 * state should not be called on the \a decoder while in the callback.
 *
 * \param  decoder  The decoder instance calling the callback.
 * \param  buffer   A pointer to a location for the callee to store
 *                  data to be decoded.
 * \param  bytes    A pointer to the size of the buffer.  On entry
 *                  to the callback, it contains the maximum number
 *                  of bytes that may be stored in \a buffer.  The
 *                  callee must set it to the actual number of bytes
 *                  stored (0 in case of error or end-of-stream) before
 *                  returning.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_decoder_init_*().
 * \retval FLAC__StreamDecoderReadStatus
 *    The callee's return status.  Note that the callback should return
 *    \c FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM if and only if
 *    zero bytes were read and there is no more data to be read.
 */
typedef FLAC__StreamDecoderReadStatus (*FLAC__StreamDecoderReadCallback)(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);

/** Signature for the seek callback.
 *
 *  A function pointer matching this signature may be passed to
 *  FLAC__stream_decoder_init*_stream().  The supplied function will be
 *  called when the decoder needs to seek the input stream.  The decoder
 *  will pass the absolute byte offset to seek to, 0 meaning the
 *  beginning of the stream.
 *
 * Here is an example of a seek callback for stdio streams:
 * \code
 * FLAC__StreamDecoderSeekStatus seek_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
 * {
 *   FILE *file = ((MyClientData*)client_data)->file;
 *   if(file == stdin)
 *     return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;
 *   else if(fseeko(file, (off_t)absolute_byte_offset, SEEK_SET) < 0)
 *     return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
 *   else
 *     return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
 * }
 * \endcode
 *
 * \note In general, FLAC__StreamDecoder functions which change the
 * state should not be called on the \a decoder while in the callback.
 *
 * \param  decoder  The decoder instance calling the callback.
 * \param  absolute_byte_offset  The offset from the beginning of the stream
 *                               to seek to.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_decoder_init_*().
 * \retval FLAC__StreamDecoderSeekStatus
 *    The callee's return status.
 */
typedef FLAC__StreamDecoderSeekStatus (*FLAC__StreamDecoderSeekCallback)(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);

/** Signature for the tell callback.
 *
 *  A function pointer matching this signature may be passed to
 *  FLAC__stream_decoder_init*_stream().  The supplied function will be
 *  called when the decoder wants to know the current position of the
 *  stream.  The callback should return the byte offset from the
 *  beginning of the stream.
 *
 * Here is an example of a tell callback for stdio streams:
 * \code
 * FLAC__StreamDecoderTellStatus tell_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
 * {
 *   FILE *file = ((MyClientData*)client_data)->file;
 *   off_t pos;
 *   if(file == stdin)
 *     return FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED;
 *   else if((pos = ftello(file)) < 0)
 *     return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;
 *   else {
 *     *absolute_byte_offset = (FLAC__uint64)pos;
 *     return FLAC__STREAM_DECODER_TELL_STATUS_OK;
 *   }
 * }
 * \endcode
 *
 * \note In general, FLAC__StreamDecoder functions which change the
 * state should not be called on the \a decoder while in the callback.
 *
 * \param  decoder  The decoder instance calling the callback.
 * \param  absolute_byte_offset  A pointer to storage for the current offset
 *                               from the beginning of the stream.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_decoder_init_*().
 * \retval FLAC__StreamDecoderTellStatus
 *    The callee's return status.
 */
typedef FLAC__StreamDecoderTellStatus (*FLAC__StreamDecoderTellCallback)(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);

/** Signature for the length callback.
 *
 *  A function pointer matching this signature may be passed to
 *  FLAC__stream_decoder_init*_stream().  The supplied function will be
 *  called when the decoder wants to know the total length of the stream
 *  in bytes.
 *
 * Here is an example of a length callback for stdio streams:
 * \code
 * FLAC__StreamDecoderLengthStatus length_cb(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
 * {
 *   FILE *file = ((MyClientData*)client_data)->file;
 *   struct stat filestats;
 *
 *   if(file == stdin)
 *     return FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED;
 *   else if(fstat(fileno(file), &filestats) != 0)
 *     return FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
 *   else {
 *     *stream_length = (FLAC__uint64)filestats.st_size;
 *     return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
 *   }
 * }
 * \endcode
 *
 * \note In general, FLAC__StreamDecoder functions which change the
 * state should not be called on the \a decoder while in the callback.
 *
 * \param  decoder  The decoder instance calling the callback.
 * \param  stream_length  A pointer to storage for the length of the stream
 *                        in bytes.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_decoder_init_*().
 * \retval FLAC__StreamDecoderLengthStatus
 *    The callee's return status.
 */
typedef FLAC__StreamDecoderLengthStatus (*FLAC__StreamDecoderLengthCallback)(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);

/** Signature for the EOF callback.
 *
 *  A function pointer matching this signature may be passed to
 *  FLAC__stream_decoder_init*_stream().  The supplied function will be
 *  called when the decoder needs to know if the end of the stream has
 *  been reached.
 *
 * Here is an example of a EOF callback for stdio streams:
 * FLAC__bool eof_cb(const FLAC__StreamDecoder *decoder, void *client_data)
 * \code
 * {
 *   FILE *file = ((MyClientData*)client_data)->file;
 *   return feof(file)? true : false;
 * }
 * \endcode
 *
 * \note In general, FLAC__StreamDecoder functions which change the
 * state should not be called on the \a decoder while in the callback.
 *
 * \param  decoder  The decoder instance calling the callback.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_decoder_init_*().
 * \retval FLAC__bool
 *    \c true if the currently at the end of the stream, else \c false.
 */
typedef FLAC__bool (*FLAC__StreamDecoderEofCallback)(const FLAC__StreamDecoder *decoder, void *client_data);

/** Signature for the write callback.
 *
 *  A function pointer matching this signature must be passed to one of
 *  the FLAC__stream_decoder_init_*() functions.
 *  The supplied function will be called when the decoder has decoded a
 *  single audio frame.  The decoder will pass the frame metadata as well
 *  as an array of pointers (one for each channel) pointing to the
 *  decoded audio.
 *
 * \note In general, FLAC__StreamDecoder functions which change the
 * state should not be called on the \a decoder while in the callback.
 *
 * \param  decoder  The decoder instance calling the callback.
 * \param  frame    The description of the decoded frame.  See
 *                  FLAC__Frame.
 * \param  buffer   An array of pointers to decoded channels of data.
 *                  Each pointer will point to an array of signed
 *                  samples of length \a frame->header.blocksize.
 *                  Channels will be ordered according to the FLAC
 *                  specification; see the documentation for the
 *                  <A HREF="../format.html#frame_header">frame header</A>.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_decoder_init_*().
 * \retval FLAC__StreamDecoderWriteStatus
 *    The callee's return status.
 */
typedef FLAC__StreamDecoderWriteStatus (*FLAC__StreamDecoderWriteCallback)(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);

/** Signature for the metadata callback.
 *
 *  A function pointer matching this signature must be passed to one of
 *  the FLAC__stream_decoder_init_*() functions.
 *  The supplied function will be called when the decoder has decoded a
 *  metadata block.  In a valid FLAC file there will always be one
 *  \c STREAMINFO block, followed by zero or more other metadata blocks.
 *  These will be supplied by the decoder in the same order as they
 *  appear in the stream and always before the first audio frame (i.e.
 *  write callback).  The metadata block that is passed in must not be
 *  modified, and it doesn't live beyond the callback, so you should make
 *  a copy of it with FLAC__metadata_object_clone() if you will need it
 *  elsewhere.  Since metadata blocks can potentially be large, by
 *  default the decoder only calls the metadata callback for the
 *  \c STREAMINFO block; you can instruct the decoder to pass or filter
 *  other blocks with FLAC__stream_decoder_set_metadata_*() calls.
 *
 * \note In general, FLAC__StreamDecoder functions which change the
 * state should not be called on the \a decoder while in the callback.
 *
 * \param  decoder  The decoder instance calling the callback.
 * \param  metadata The decoded metadata block.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_decoder_init_*().
 */
typedef void (*FLAC__StreamDecoderMetadataCallback)(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);

/** Signature for the error callback.
 *
 *  A function pointer matching this signature must be passed to one of
 *  the FLAC__stream_decoder_init_*() functions.
 *  The supplied function will be called whenever an error occurs during
 *  decoding.
 *
 * \note In general, FLAC__StreamDecoder functions which change the
 * state should not be called on the \a decoder while in the callback.
 *
 * \param  decoder  The decoder instance calling the callback.
 * \param  status   The error encountered by the decoder.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_decoder_init_*().
 */
typedef void (*FLAC__StreamDecoderErrorCallback)(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);


/***********************************************************************
 *
 * Class constructor/destructor
 *
 ***********************************************************************/

/** Create a new stream decoder instance.  The instance is created with
 *  default settings; see the individual FLAC__stream_decoder_set_*()
 *  functions for each setting's default.
 *
 * \retval FLAC__StreamDecoder*
 *    \c NULL if there was an error allocating memory, else the new instance.
 */
FLAC_API FLAC__StreamDecoder *FLAC__stream_decoder_new(void);

/** Free a decoder instance.  Deletes the object pointed to by \a decoder.
 *
 * \param decoder  A pointer to an existing decoder.
 * \assert
 *    \code decoder != NULL \endcode
 */
FLAC_API void FLAC__stream_decoder_delete(FLAC__StreamDecoder *decoder);


/***********************************************************************
 *
 * Public class method prototypes
 *
 ***********************************************************************/

/** Set the serial number for the FLAC stream within the Ogg container.
 *  The default behavior is to use the serial number of the first Ogg
 *  page.  Setting a serial number here will explicitly specify which
 *  stream is to be decoded.
 *
 * \note
 * This does not need to be set for native FLAC decoding.
 *
 * \default \c use serial number of first page
 * \param  decoder        A decoder instance to set.
 * \param  serial_number  See above.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the decoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_set_ogg_serial_number(FLAC__StreamDecoder *decoder, long serial_number);

/** Set the "MD5 signature checking" flag.  If \c true, the decoder will
 *  compute the MD5 signature of the unencoded audio data while decoding
 *  and compare it to the signature from the STREAMINFO block, if it
 *  exists, during FLAC__stream_decoder_finish().
 *
 *  MD5 signature checking will be turned off (until the next
 *  FLAC__stream_decoder_reset()) if there is no signature in the
 *  STREAMINFO block or when a seek is attempted.
 *
 *  Clients that do not use the MD5 check should leave this off to speed
 *  up decoding.
 *
 * \default \c false
 * \param  decoder  A decoder instance to set.
 * \param  value    Flag value (see above).
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the decoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_set_md5_checking(FLAC__StreamDecoder *decoder, FLAC__bool value);

/** Direct the decoder to pass on all metadata blocks of type \a type.
 *
 * \default By default, only the \c STREAMINFO block is returned via the
 *          metadata callback.
 * \param  decoder  A decoder instance to set.
 * \param  type     See above.
 * \assert
 *    \code decoder != NULL \endcode
 *    \a type is valid
 * \retval FLAC__bool
 *    \c false if the decoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_respond(FLAC__StreamDecoder *decoder, FLAC__MetadataType type);

/** Direct the decoder to pass on all APPLICATION metadata blocks of the
 *  given \a id.
 *
 * \default By default, only the \c STREAMINFO block is returned via the
 *          metadata callback.
 * \param  decoder  A decoder instance to set.
 * \param  id       See above.
 * \assert
 *    \code decoder != NULL \endcode
 *    \code id != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the decoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_respond_application(FLAC__StreamDecoder *decoder, const FLAC__byte id[4]);

/** Direct the decoder to pass on all metadata blocks of any type.
 *
 * \default By default, only the \c STREAMINFO block is returned via the
 *          metadata callback.
 * \param  decoder  A decoder instance to set.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the decoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_respond_all(FLAC__StreamDecoder *decoder);

/** Direct the decoder to filter out all metadata blocks of type \a type.
 *
 * \default By default, only the \c STREAMINFO block is returned via the
 *          metadata callback.
 * \param  decoder  A decoder instance to set.
 * \param  type     See above.
 * \assert
 *    \code decoder != NULL \endcode
 *    \a type is valid
 * \retval FLAC__bool
 *    \c false if the decoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_ignore(FLAC__StreamDecoder *decoder, FLAC__MetadataType type);

/** Direct the decoder to filter out all APPLICATION metadata blocks of
 *  the given \a id.
 *
 * \default By default, only the \c STREAMINFO block is returned via the
 *          metadata callback.
 * \param  decoder  A decoder instance to set.
 * \param  id       See above.
 * \assert
 *    \code decoder != NULL \endcode
 *    \code id != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the decoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_ignore_application(FLAC__StreamDecoder *decoder, const FLAC__byte id[4]);

/** Direct the decoder to filter out all metadata blocks of any type.
 *
 * \default By default, only the \c STREAMINFO block is returned via the
 *          metadata callback.
 * \param  decoder  A decoder instance to set.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the decoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_ignore_all(FLAC__StreamDecoder *decoder);

/** Get the current decoder state.
 *
 * \param  decoder  A decoder instance to query.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__StreamDecoderState
 *    The current decoder state.
 */
FLAC_API FLAC__StreamDecoderState FLAC__stream_decoder_get_state(const FLAC__StreamDecoder *decoder);

/** Get the current decoder state as a C string.
 *
 * \param  decoder  A decoder instance to query.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval const char *
 *    The decoder state as a C string.  Do not modify the contents.
 */
FLAC_API const char *FLAC__stream_decoder_get_resolved_state_string(const FLAC__StreamDecoder *decoder);

/** Get the "MD5 signature checking" flag.
 *  This is the value of the setting, not whether or not the decoder is
 *  currently checking the MD5 (remember, it can be turned off automatically
 *  by a seek).  When the decoder is reset the flag will be restored to the
 *  value returned by this function.
 *
 * \param  decoder  A decoder instance to query.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    See above.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_get_md5_checking(const FLAC__StreamDecoder *decoder);

/** Get the total number of samples in the stream being decoded.
 *  Will only be valid after decoding has started and will contain the
 *  value from the \c STREAMINFO block.  A value of \c 0 means "unknown".
 *
 * \param  decoder  A decoder instance to query.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval unsigned
 *    See above.
 */
FLAC_API FLAC__uint64 FLAC__stream_decoder_get_total_samples(const FLAC__StreamDecoder *decoder);

/** Get the current number of channels in the stream being decoded.
 *  Will only be valid after decoding has started and will contain the
 *  value from the most recently decoded frame header.
 *
 * \param  decoder  A decoder instance to query.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval unsigned
 *    See above.
 */
FLAC_API unsigned FLAC__stream_decoder_get_channels(const FLAC__StreamDecoder *decoder);

/** Get the current channel assignment in the stream being decoded.
 *  Will only be valid after decoding has started and will contain the
 *  value from the most recently decoded frame header.
 *
 * \param  decoder  A decoder instance to query.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__ChannelAssignment
 *    See above.
 */
FLAC_API FLAC__ChannelAssignment FLAC__stream_decoder_get_channel_assignment(const FLAC__StreamDecoder *decoder);

/** Get the current sample resolution in the stream being decoded.
 *  Will only be valid after decoding has started and will contain the
 *  value from the most recently decoded frame header.
 *
 * \param  decoder  A decoder instance to query.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval unsigned
 *    See above.
 */
FLAC_API unsigned FLAC__stream_decoder_get_bits_per_sample(const FLAC__StreamDecoder *decoder);

/** Get the current sample rate in Hz of the stream being decoded.
 *  Will only be valid after decoding has started and will contain the
 *  value from the most recently decoded frame header.
 *
 * \param  decoder  A decoder instance to query.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval unsigned
 *    See above.
 */
FLAC_API unsigned FLAC__stream_decoder_get_sample_rate(const FLAC__StreamDecoder *decoder);

/** Get the current blocksize of the stream being decoded.
 *  Will only be valid after decoding has started and will contain the
 *  value from the most recently decoded frame header.
 *
 * \param  decoder  A decoder instance to query.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval unsigned
 *    See above.
 */
FLAC_API unsigned FLAC__stream_decoder_get_blocksize(const FLAC__StreamDecoder *decoder);

/** Returns the decoder's current read position within the stream.
 *  The position is the byte offset from the start of the stream.
 *  Bytes before this position have been fully decoded.  Note that
 *  there may still be undecoded bytes in the decoder's read FIFO.
 *  The returned position is correct even after a seek.
 *
 *  \warning This function currently only works for native FLAC,
 *           not Ogg FLAC streams.
 *
 * \param  decoder   A decoder instance to query.
 * \param  position  Address at which to return the desired position.
 * \assert
 *    \code decoder != NULL \endcode
 *    \code position != NULL \endcode
 * \retval FLAC__bool
 *    \c true if successful, \c false if the stream is not native FLAC,
 *    or there was an error from the 'tell' callback or it returned
 *    \c FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_get_decode_position(const FLAC__StreamDecoder *decoder, FLAC__uint64 *position);

/** Initialize the decoder instance to decode native FLAC streams.
 *
 *  This flavor of initialization sets up the decoder to decode from a
 *  native FLAC stream. I/O is performed via callbacks to the client.
 *  For decoding from a plain file via filename or open FILE*,
 *  FLAC__stream_decoder_init_file() and FLAC__stream_decoder_init_FILE()
 *  provide a simpler interface.
 *
 *  This function should be called after FLAC__stream_decoder_new() and
 *  FLAC__stream_decoder_set_*() but before any of the
 *  FLAC__stream_decoder_process_*() functions.  Will set and return the
 *  decoder state, which will be FLAC__STREAM_DECODER_SEARCH_FOR_METADATA
 *  if initialization succeeded.
 *
 * \param  decoder            An uninitialized decoder instance.
 * \param  read_callback      See FLAC__StreamDecoderReadCallback.  This
 *                            pointer must not be \c NULL.
 * \param  seek_callback      See FLAC__StreamDecoderSeekCallback.  This
 *                            pointer may be \c NULL if seeking is not
 *                            supported.  If \a seek_callback is not \c NULL then a
 *                            \a tell_callback, \a length_callback, and \a eof_callback must also be supplied.
 *                            Alternatively, a dummy seek callback that just
 *                            returns \c FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the decoder.
 * \param  tell_callback      See FLAC__StreamDecoderTellCallback.  This
 *                            pointer may be \c NULL if not supported by the client.  If
 *                            \a seek_callback is not \c NULL then a
 *                            \a tell_callback must also be supplied.
 *                            Alternatively, a dummy tell callback that just
 *                            returns \c FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the decoder.
 * \param  length_callback    See FLAC__StreamDecoderLengthCallback.  This
 *                            pointer may be \c NULL if not supported by the client.  If
 *                            \a seek_callback is not \c NULL then a
 *                            \a length_callback must also be supplied.
 *                            Alternatively, a dummy length callback that just
 *                            returns \c FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the decoder.
 * \param  eof_callback       See FLAC__StreamDecoderEofCallback.  This
 *                            pointer may be \c NULL if not supported by the client.  If
 *                            \a seek_callback is not \c NULL then a
 *                            \a eof_callback must also be supplied.
 *                            Alternatively, a dummy length callback that just
 *                            returns \c false
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the decoder.
 * \param  write_callback     See FLAC__StreamDecoderWriteCallback.  This
 *                            pointer must not be \c NULL.
 * \param  metadata_callback  See FLAC__StreamDecoderMetadataCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.
 * \param  error_callback     See FLAC__StreamDecoderErrorCallback.  This
 *                            pointer must not be \c NULL.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__StreamDecoderInitStatus
 *    \c FLAC__STREAM_DECODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamDecoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamDecoderInitStatus FLAC__stream_decoder_init_stream(
    FLAC__StreamDecoder *decoder,
    FLAC__StreamDecoderReadCallback read_callback,
    FLAC__StreamDecoderSeekCallback seek_callback,
    FLAC__StreamDecoderTellCallback tell_callback,
    FLAC__StreamDecoderLengthCallback length_callback,
    FLAC__StreamDecoderEofCallback eof_callback,
    FLAC__StreamDecoderWriteCallback write_callback,
    FLAC__StreamDecoderMetadataCallback metadata_callback,
    FLAC__StreamDecoderErrorCallback error_callback,
    void *client_data
);

/** Initialize the decoder instance to decode Ogg FLAC streams.
 *
 *  This flavor of initialization sets up the decoder to decode from a
 *  FLAC stream in an Ogg container. I/O is performed via callbacks to the
 *  client.  For decoding from a plain file via filename or open FILE*,
 *  FLAC__stream_decoder_init_ogg_file() and FLAC__stream_decoder_init_ogg_FILE()
 *  provide a simpler interface.
 *
 *  This function should be called after FLAC__stream_decoder_new() and
 *  FLAC__stream_decoder_set_*() but before any of the
 *  FLAC__stream_decoder_process_*() functions.  Will set and return the
 *  decoder state, which will be FLAC__STREAM_DECODER_SEARCH_FOR_METADATA
 *  if initialization succeeded.
 *
 *  \note Support for Ogg FLAC in the library is optional.  If this
 *  library has been built without support for Ogg FLAC, this function
 *  will return \c FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER.
 *
 * \param  decoder            An uninitialized decoder instance.
 * \param  read_callback      See FLAC__StreamDecoderReadCallback.  This
 *                            pointer must not be \c NULL.
 * \param  seek_callback      See FLAC__StreamDecoderSeekCallback.  This
 *                            pointer may be \c NULL if seeking is not
 *                            supported.  If \a seek_callback is not \c NULL then a
 *                            \a tell_callback, \a length_callback, and \a eof_callback must also be supplied.
 *                            Alternatively, a dummy seek callback that just
 *                            returns \c FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the decoder.
 * \param  tell_callback      See FLAC__StreamDecoderTellCallback.  This
 *                            pointer may be \c NULL if not supported by the client.  If
 *                            \a seek_callback is not \c NULL then a
 *                            \a tell_callback must also be supplied.
 *                            Alternatively, a dummy tell callback that just
 *                            returns \c FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the decoder.
 * \param  length_callback    See FLAC__StreamDecoderLengthCallback.  This
 *                            pointer may be \c NULL if not supported by the client.  If
 *                            \a seek_callback is not \c NULL then a
 *                            \a length_callback must also be supplied.
 *                            Alternatively, a dummy length callback that just
 *                            returns \c FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the decoder.
 * \param  eof_callback       See FLAC__StreamDecoderEofCallback.  This
 *                            pointer may be \c NULL if not supported by the client.  If
 *                            \a seek_callback is not \c NULL then a
 *                            \a eof_callback must also be supplied.
 *                            Alternatively, a dummy length callback that just
 *                            returns \c false
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the decoder.
 * \param  write_callback     See FLAC__StreamDecoderWriteCallback.  This
 *                            pointer must not be \c NULL.
 * \param  metadata_callback  See FLAC__StreamDecoderMetadataCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.
 * \param  error_callback     See FLAC__StreamDecoderErrorCallback.  This
 *                            pointer must not be \c NULL.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__StreamDecoderInitStatus
 *    \c FLAC__STREAM_DECODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamDecoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamDecoderInitStatus FLAC__stream_decoder_init_ogg_stream(
    FLAC__StreamDecoder *decoder,
    FLAC__StreamDecoderReadCallback read_callback,
    FLAC__StreamDecoderSeekCallback seek_callback,
    FLAC__StreamDecoderTellCallback tell_callback,
    FLAC__StreamDecoderLengthCallback length_callback,
    FLAC__StreamDecoderEofCallback eof_callback,
    FLAC__StreamDecoderWriteCallback write_callback,
    FLAC__StreamDecoderMetadataCallback metadata_callback,
    FLAC__StreamDecoderErrorCallback error_callback,
    void *client_data
);

/** Initialize the decoder instance to decode native FLAC files.
 *
 *  This flavor of initialization sets up the decoder to decode from a
 *  plain native FLAC file.  For non-stdio streams, you must use
 *  FLAC__stream_decoder_init_stream() and provide callbacks for the I/O.
 *
 *  This function should be called after FLAC__stream_decoder_new() and
 *  FLAC__stream_decoder_set_*() but before any of the
 *  FLAC__stream_decoder_process_*() functions.  Will set and return the
 *  decoder state, which will be FLAC__STREAM_DECODER_SEARCH_FOR_METADATA
 *  if initialization succeeded.
 *
 * \param  decoder            An uninitialized decoder instance.
 * \param  file               An open FLAC file.  The file should have been
 *                            opened with mode \c "rb" and rewound.  The file
 *                            becomes owned by the decoder and should not be
 *                            manipulated by the client while decoding.
 *                            Unless \a file is \c stdin, it will be closed
 *                            when FLAC__stream_decoder_finish() is called.
 *                            Note however that seeking will not work when
 *                            decoding from \c stdout since it is not seekable.
 * \param  write_callback     See FLAC__StreamDecoderWriteCallback.  This
 *                            pointer must not be \c NULL.
 * \param  metadata_callback  See FLAC__StreamDecoderMetadataCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.
 * \param  error_callback     See FLAC__StreamDecoderErrorCallback.  This
 *                            pointer must not be \c NULL.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code decoder != NULL \endcode
 *    \code file != NULL \endcode
 * \retval FLAC__StreamDecoderInitStatus
 *    \c FLAC__STREAM_DECODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamDecoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamDecoderInitStatus FLAC__stream_decoder_init_FILE(
    FLAC__StreamDecoder *decoder,
    FILE *file,
    FLAC__StreamDecoderWriteCallback write_callback,
    FLAC__StreamDecoderMetadataCallback metadata_callback,
    FLAC__StreamDecoderErrorCallback error_callback,
    void *client_data
);

/** Initialize the decoder instance to decode Ogg FLAC files.
 *
 *  This flavor of initialization sets up the decoder to decode from a
 *  plain Ogg FLAC file.  For non-stdio streams, you must use
 *  FLAC__stream_decoder_init_ogg_stream() and provide callbacks for the I/O.
 *
 *  This function should be called after FLAC__stream_decoder_new() and
 *  FLAC__stream_decoder_set_*() but before any of the
 *  FLAC__stream_decoder_process_*() functions.  Will set and return the
 *  decoder state, which will be FLAC__STREAM_DECODER_SEARCH_FOR_METADATA
 *  if initialization succeeded.
 *
 *  \note Support for Ogg FLAC in the library is optional.  If this
 *  library has been built without support for Ogg FLAC, this function
 *  will return \c FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER.
 *
 * \param  decoder            An uninitialized decoder instance.
 * \param  file               An open FLAC file.  The file should have been
 *                            opened with mode \c "rb" and rewound.  The file
 *                            becomes owned by the decoder and should not be
 *                            manipulated by the client while decoding.
 *                            Unless \a file is \c stdin, it will be closed
 *                            when FLAC__stream_decoder_finish() is called.
 *                            Note however that seeking will not work when
 *                            decoding from \c stdout since it is not seekable.
 * \param  write_callback     See FLAC__StreamDecoderWriteCallback.  This
 *                            pointer must not be \c NULL.
 * \param  metadata_callback  See FLAC__StreamDecoderMetadataCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.
 * \param  error_callback     See FLAC__StreamDecoderErrorCallback.  This
 *                            pointer must not be \c NULL.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code decoder != NULL \endcode
 *    \code file != NULL \endcode
 * \retval FLAC__StreamDecoderInitStatus
 *    \c FLAC__STREAM_DECODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamDecoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamDecoderInitStatus FLAC__stream_decoder_init_ogg_FILE(
    FLAC__StreamDecoder *decoder,
    FILE *file,
    FLAC__StreamDecoderWriteCallback write_callback,
    FLAC__StreamDecoderMetadataCallback metadata_callback,
    FLAC__StreamDecoderErrorCallback error_callback,
    void *client_data
);

/** Initialize the decoder instance to decode native FLAC files.
 *
 *  This flavor of initialization sets up the decoder to decode from a plain
 *  native FLAC file.  If POSIX fopen() semantics are not sufficient, (for
 *  example, with Unicode filenames on Windows), you must use
 *  FLAC__stream_decoder_init_FILE(), or FLAC__stream_decoder_init_stream()
 *  and provide callbacks for the I/O.
 *
 *  This function should be called after FLAC__stream_decoder_new() and
 *  FLAC__stream_decoder_set_*() but before any of the
 *  FLAC__stream_decoder_process_*() functions.  Will set and return the
 *  decoder state, which will be FLAC__STREAM_DECODER_SEARCH_FOR_METADATA
 *  if initialization succeeded.
 *
 * \param  decoder            An uninitialized decoder instance.
 * \param  filename           The name of the file to decode from.  The file will
 *                            be opened with fopen().  Use \c NULL to decode from
 *                            \c stdin.  Note that \c stdin is not seekable.
 * \param  write_callback     See FLAC__StreamDecoderWriteCallback.  This
 *                            pointer must not be \c NULL.
 * \param  metadata_callback  See FLAC__StreamDecoderMetadataCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.
 * \param  error_callback     See FLAC__StreamDecoderErrorCallback.  This
 *                            pointer must not be \c NULL.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__StreamDecoderInitStatus
 *    \c FLAC__STREAM_DECODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamDecoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamDecoderInitStatus FLAC__stream_decoder_init_file(
    FLAC__StreamDecoder *decoder,
    const char *filename,
    FLAC__StreamDecoderWriteCallback write_callback,
    FLAC__StreamDecoderMetadataCallback metadata_callback,
    FLAC__StreamDecoderErrorCallback error_callback,
    void *client_data
);

/** Initialize the decoder instance to decode Ogg FLAC files.
 *
 *  This flavor of initialization sets up the decoder to decode from a plain
 *  Ogg FLAC file.  If POSIX fopen() semantics are not sufficient, (for
 *  example, with Unicode filenames on Windows), you must use
 *  FLAC__stream_decoder_init_ogg_FILE(), or FLAC__stream_decoder_init_ogg_stream()
 *  and provide callbacks for the I/O.
 *
 *  This function should be called after FLAC__stream_decoder_new() and
 *  FLAC__stream_decoder_set_*() but before any of the
 *  FLAC__stream_decoder_process_*() functions.  Will set and return the
 *  decoder state, which will be FLAC__STREAM_DECODER_SEARCH_FOR_METADATA
 *  if initialization succeeded.
 *
 *  \note Support for Ogg FLAC in the library is optional.  If this
 *  library has been built without support for Ogg FLAC, this function
 *  will return \c FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER.
 *
 * \param  decoder            An uninitialized decoder instance.
 * \param  filename           The name of the file to decode from.  The file will
 *                            be opened with fopen().  Use \c NULL to decode from
 *                            \c stdin.  Note that \c stdin is not seekable.
 * \param  write_callback     See FLAC__StreamDecoderWriteCallback.  This
 *                            pointer must not be \c NULL.
 * \param  metadata_callback  See FLAC__StreamDecoderMetadataCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.
 * \param  error_callback     See FLAC__StreamDecoderErrorCallback.  This
 *                            pointer must not be \c NULL.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__StreamDecoderInitStatus
 *    \c FLAC__STREAM_DECODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamDecoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamDecoderInitStatus FLAC__stream_decoder_init_ogg_file(
    FLAC__StreamDecoder *decoder,
    const char *filename,
    FLAC__StreamDecoderWriteCallback write_callback,
    FLAC__StreamDecoderMetadataCallback metadata_callback,
    FLAC__StreamDecoderErrorCallback error_callback,
    void *client_data
);

/** Finish the decoding process.
 *  Flushes the decoding buffer, releases resources, resets the decoder
 *  settings to their defaults, and returns the decoder state to
 *  FLAC__STREAM_DECODER_UNINITIALIZED.
 *
 *  In the event of a prematurely-terminated decode, it is not strictly
 *  necessary to call this immediately before FLAC__stream_decoder_delete()
 *  but it is good practice to match every FLAC__stream_decoder_init_*()
 *  with a FLAC__stream_decoder_finish().
 *
 * \param  decoder  An uninitialized decoder instance.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if MD5 checking is on AND a STREAMINFO block was available
 *    AND the MD5 signature in the STREAMINFO block was non-zero AND the
 *    signature does not match the one computed by the decoder; else
 *    \c true.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_finish(FLAC__StreamDecoder *decoder);

/** Flush the stream input.
 *  The decoder's input buffer will be cleared and the state set to
 *  \c FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC.  This will also turn
 *  off MD5 checking.
 *
 * \param  decoder  A decoder instance.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c true if successful, else \c false if a memory allocation
 *    error occurs (in which case the state will be set to
 *    \c FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR).
 */
FLAC_API FLAC__bool FLAC__stream_decoder_flush(FLAC__StreamDecoder *decoder);

/** Reset the decoding process.
 *  The decoder's input buffer will be cleared and the state set to
 *  \c FLAC__STREAM_DECODER_SEARCH_FOR_METADATA.  This is similar to
 *  FLAC__stream_decoder_finish() except that the settings are
 *  preserved; there is no need to call FLAC__stream_decoder_init_*()
 *  before decoding again.  MD5 checking will be restored to its original
 *  setting.
 *
 *  If the decoder is seekable, or was initialized with
 *  FLAC__stream_decoder_init*_FILE() or FLAC__stream_decoder_init*_file(),
 *  the decoder will also attempt to seek to the beginning of the file.
 *  If this rewind fails, this function will return \c false.  It follows
 *  that FLAC__stream_decoder_reset() cannot be used when decoding from
 *  \c stdin.
 *
 *  If the decoder was initialized with FLAC__stream_encoder_init*_stream()
 *  and is not seekable (i.e. no seek callback was provided or the seek
 *  callback returns \c FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED), it
 *  is the duty of the client to start feeding data from the beginning of
 *  the stream on the next FLAC__stream_decoder_process() or
 *  FLAC__stream_decoder_process_interleaved() call.
 *
 * \param  decoder  A decoder instance.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c true if successful, else \c false if a memory allocation occurs
 *    (in which case the state will be set to
 *    \c FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR) or a seek error
 *    occurs (the state will be unchanged).
 */
FLAC_API FLAC__bool FLAC__stream_decoder_reset(FLAC__StreamDecoder *decoder);

/** Decode one metadata block or audio frame.
 *  This version instructs the decoder to decode a either a single metadata
 *  block or a single frame and stop, unless the callbacks return a fatal
 *  error or the read callback returns
 *  \c FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM.
 *
 *  As the decoder needs more input it will call the read callback.
 *  Depending on what was decoded, the metadata or write callback will be
 *  called with the decoded metadata block or audio frame.
 *
 *  Unless there is a fatal read error or end of stream, this function
 *  will return once one whole frame is decoded.  In other words, if the
 *  stream is not synchronized or points to a corrupt frame header, the
 *  decoder will continue to try and resync until it gets to a valid
 *  frame, then decode one frame, then return.  If the decoder points to
 *  a frame whose frame CRC in the frame footer does not match the
 *  computed frame CRC, this function will issue a
 *  FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH error to the
 *  error callback, and return, having decoded one complete, although
 *  corrupt, frame.  (Such corrupted frames are sent as silence of the
 *  correct length to the write callback.)
 *
 * \param  decoder  An initialized decoder instance.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if any fatal read, write, or memory allocation error
 *    occurred (meaning decoding must stop), else \c true; for more
 *    information about the decoder, check the decoder state with
 *    FLAC__stream_decoder_get_state().
 */
FLAC_API FLAC__bool FLAC__stream_decoder_process_single(FLAC__StreamDecoder *decoder);

/** Decode until the end of the metadata.
 *  This version instructs the decoder to decode from the current position
 *  and continue until all the metadata has been read, or until the
 *  callbacks return a fatal error or the read callback returns
 *  \c FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM.
 *
 *  As the decoder needs more input it will call the read callback.
 *  As each metadata block is decoded, the metadata callback will be called
 *  with the decoded metadata.
 *
 * \param  decoder  An initialized decoder instance.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if any fatal read, write, or memory allocation error
 *    occurred (meaning decoding must stop), else \c true; for more
 *    information about the decoder, check the decoder state with
 *    FLAC__stream_decoder_get_state().
 */
FLAC_API FLAC__bool FLAC__stream_decoder_process_until_end_of_metadata(FLAC__StreamDecoder *decoder);

/** Decode until the end of the stream.
 *  This version instructs the decoder to decode from the current position
 *  and continue until the end of stream (the read callback returns
 *  \c FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM), or until the
 *  callbacks return a fatal error.
 *
 *  As the decoder needs more input it will call the read callback.
 *  As each metadata block and frame is decoded, the metadata or write
 *  callback will be called with the decoded metadata or frame.
 *
 * \param  decoder  An initialized decoder instance.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if any fatal read, write, or memory allocation error
 *    occurred (meaning decoding must stop), else \c true; for more
 *    information about the decoder, check the decoder state with
 *    FLAC__stream_decoder_get_state().
 */
FLAC_API FLAC__bool FLAC__stream_decoder_process_until_end_of_stream(FLAC__StreamDecoder *decoder);

/** Skip one audio frame.
 *  This version instructs the decoder to 'skip' a single frame and stop,
 *  unless the callbacks return a fatal error or the read callback returns
 *  \c FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM.
 *
 *  The decoding flow is the same as what occurs when
 *  FLAC__stream_decoder_process_single() is called to process an audio
 *  frame, except that this function does not decode the parsed data into
 *  PCM or call the write callback.  The integrity of the frame is still
 *  checked the same way as in the other process functions.
 *
 *  This function will return once one whole frame is skipped, in the
 *  same way that FLAC__stream_decoder_process_single() will return once
 *  one whole frame is decoded.
 *
 *  This function can be used in more quickly determining FLAC frame
 *  boundaries when decoding of the actual data is not needed, for
 *  example when an application is separating a FLAC stream into frames
 *  for editing or storing in a container.  To do this, the application
 *  can use FLAC__stream_decoder_skip_single_frame() to quickly advance
 *  to the next frame, then use
 *  FLAC__stream_decoder_get_decode_position() to find the new frame
 *  boundary.
 *
 *  This function should only be called when the stream has advanced
 *  past all the metadata, otherwise it will return \c false.
 *
 * \param  decoder  An initialized decoder instance not in a metadata
 *                  state.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if any fatal read, write, or memory allocation error
 *    occurred (meaning decoding must stop), or if the decoder
 *    is in the FLAC__STREAM_DECODER_SEARCH_FOR_METADATA or
 *    FLAC__STREAM_DECODER_READ_METADATA state, else \c true; for more
 *    information about the decoder, check the decoder state with
 *    FLAC__stream_decoder_get_state().
 */
FLAC_API FLAC__bool FLAC__stream_decoder_skip_single_frame(FLAC__StreamDecoder *decoder);

/** Flush the input and seek to an absolute sample.
 *  Decoding will resume at the given sample.  Note that because of
 *  this, the next write callback may contain a partial block.  The
 *  client must support seeking the input or this function will fail
 *  and return \c false.  Furthermore, if the decoder state is
 *  \c FLAC__STREAM_DECODER_SEEK_ERROR, then the decoder must be flushed
 *  with FLAC__stream_decoder_flush() or reset with
 *  FLAC__stream_decoder_reset() before decoding can continue.
 *
 * \param  decoder  A decoder instance.
 * \param  sample   The target sample number to seek to.
 * \assert
 *    \code decoder != NULL \endcode
 * \retval FLAC__bool
 *    \c true if successful, else \c false.
 */
FLAC_API FLAC__bool FLAC__stream_decoder_seek_absolute(FLAC__StreamDecoder *decoder, FLAC__uint64 sample);



/** State values for a FLAC__StreamEncoder.
 *
 * The encoder's state can be obtained by calling FLAC__stream_encoder_get_state().
 *
 * If the encoder gets into any other state besides \c FLAC__STREAM_ENCODER_OK
 * or \c FLAC__STREAM_ENCODER_UNINITIALIZED, it becomes invalid for encoding and
 * must be deleted with FLAC__stream_encoder_delete().
 */
typedef enum {

    FLAC__STREAM_ENCODER_OK = 0,
    /**< The encoder is in the normal OK state and samples can be processed. */

    FLAC__STREAM_ENCODER_UNINITIALIZED,
    /**< The encoder is in the uninitialized state; one of the
     * FLAC__stream_encoder_init_*() functions must be called before samples
     * can be processed.
     */

    FLAC__STREAM_ENCODER_OGG_ERROR,
    /**< An error occurred in the underlying Ogg layer.  */

    FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR,
    /**< An error occurred in the underlying verify stream decoder;
     * check FLAC__stream_encoder_get_verify_decoder_state().
     */

    FLAC__STREAM_ENCODER_VERIFY_MISMATCH_IN_AUDIO_DATA,
    /**< The verify decoder detected a mismatch between the original
     * audio signal and the decoded audio signal.
     */

    FLAC__STREAM_ENCODER_CLIENT_ERROR,
    /**< One of the callbacks returned a fatal error. */

    FLAC__STREAM_ENCODER_IO_ERROR,
    /**< An I/O error occurred while opening/reading/writing a file.
     * Check \c errno.
     */

    FLAC__STREAM_ENCODER_FRAMING_ERROR,
    /**< An error occurred while writing the stream; usually, the
     * write_callback returned an error.
     */

    FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR
    /**< Memory allocation failed. */

} FLAC__StreamEncoderState;

/** Possible return values for the FLAC__stream_encoder_init_*() functions.
 */
typedef enum {

    FLAC__STREAM_ENCODER_INIT_STATUS_OK = 0,
    /**< Initialization was successful. */

    FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR,
    /**< General failure to set up encoder; call FLAC__stream_encoder_get_state() for cause. */

    FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER,
    /**< The library was not compiled with support for the given container
     * format.
     */

    FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS,
    /**< A required callback was not supplied. */

    FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS,
    /**< The encoder has an invalid setting for number of channels. */

    FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE,
    /**< The encoder has an invalid setting for bits-per-sample.
     * FLAC supports 4-32 bps but the reference encoder currently supports
     * only up to 24 bps.
     */

    FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE,
    /**< The encoder has an invalid setting for the input sample rate. */

    FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE,
    /**< The encoder has an invalid setting for the block size. */

    FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER,
    /**< The encoder has an invalid setting for the maximum LPC order. */

    FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION,
    /**< The encoder has an invalid setting for the precision of the quantized linear predictor coefficients. */

    FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER,
    /**< The specified block size is less than the maximum LPC order. */

    FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE,
    /**< The encoder is bound to the <A HREF="../format.html#subset">Subset</A> but other settings violate it. */

    FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA,
    /**< The metadata input to the encoder is invalid, in one of the following ways:
     * - FLAC__stream_encoder_set_metadata() was called with a null pointer but a block count > 0
     * - One of the metadata blocks contains an undefined type
     * - It contains an illegal CUESHEET as checked by FLAC__format_cuesheet_is_legal()
     * - It contains an illegal SEEKTABLE as checked by FLAC__format_seektable_is_legal()
     * - It contains more than one SEEKTABLE block or more than one VORBIS_COMMENT block
     */

    FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED
    /**< FLAC__stream_encoder_init_*() was called when the encoder was
     * already initialized, usually because
     * FLAC__stream_encoder_finish() was not called.
     */

} FLAC__StreamEncoderInitStatus;


/** Return values for the FLAC__StreamEncoder read callback.
 */
typedef enum {

    FLAC__STREAM_ENCODER_READ_STATUS_CONTINUE,
    /**< The read was OK and decoding can continue. */

    FLAC__STREAM_ENCODER_READ_STATUS_END_OF_STREAM,
    /**< The read was attempted at the end of the stream. */

    FLAC__STREAM_ENCODER_READ_STATUS_ABORT,
    /**< An unrecoverable error occurred. */

    FLAC__STREAM_ENCODER_READ_STATUS_UNSUPPORTED
    /**< Client does not support reading back from the output. */

} FLAC__StreamEncoderReadStatus;


/** Return values for the FLAC__StreamEncoder write callback.
 */
typedef enum {

    FLAC__STREAM_ENCODER_WRITE_STATUS_OK = 0,
    /**< The write was OK and encoding can continue. */

    FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR
    /**< An unrecoverable error occurred.  The encoder will return from the process call. */

} FLAC__StreamEncoderWriteStatus;

/** Return values for the FLAC__StreamEncoder seek callback.
 */
typedef enum {

    FLAC__STREAM_ENCODER_SEEK_STATUS_OK,
    /**< The seek was OK and encoding can continue. */

    FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR,
    /**< An unrecoverable error occurred. */

    FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED
    /**< Client does not support seeking. */

} FLAC__StreamEncoderSeekStatus;


/** Return values for the FLAC__StreamEncoder tell callback.
 */
typedef enum {

    FLAC__STREAM_ENCODER_TELL_STATUS_OK,
    /**< The tell was OK and encoding can continue. */

    FLAC__STREAM_ENCODER_TELL_STATUS_ERROR,
    /**< An unrecoverable error occurred. */

    FLAC__STREAM_ENCODER_TELL_STATUS_UNSUPPORTED
    /**< Client does not support seeking. */

} FLAC__StreamEncoderTellStatus;


/***********************************************************************
 *
 * class FLAC__StreamEncoder
 *
 ***********************************************************************/

struct FLAC__StreamEncoderProtected;
struct FLAC__StreamEncoderPrivate;
/** The opaque structure definition for the stream encoder type.
 *  See the \link flac_stream_encoder stream encoder module \endlink
 *  for a detailed description.
 */
typedef struct {
    struct FLAC__StreamEncoderProtected *protected_; /* avoid the C++ keyword 'protected' */
    struct FLAC__StreamEncoderPrivate *private_; /* avoid the C++ keyword 'private' */
} FLAC__StreamEncoder;

/** Signature for the read callback.
 *
 *  A function pointer matching this signature must be passed to
 *  FLAC__stream_encoder_init_ogg_stream() if seeking is supported.
 *  The supplied function will be called when the encoder needs to read back
 *  encoded data.  This happens during the metadata callback, when the encoder
 *  has to read, modify, and rewrite the metadata (e.g. seekpoints) gathered
 *  while encoding.  The address of the buffer to be filled is supplied, along
 *  with the number of bytes the buffer can hold.  The callback may choose to
 *  supply less data and modify the byte count but must be careful not to
 *  overflow the buffer.  The callback then returns a status code chosen from
 *  FLAC__StreamEncoderReadStatus.
 *
 * Here is an example of a read callback for stdio streams:
 * \code
 * FLAC__StreamEncoderReadStatus read_cb(const FLAC__StreamEncoder *encoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
 * {
 *   FILE *file = ((MyClientData*)client_data)->file;
 *   if(*bytes > 0) {
 *     *bytes = fread(buffer, sizeof(FLAC__byte), *bytes, file);
 *     if(ferror(file))
 *       return FLAC__STREAM_ENCODER_READ_STATUS_ABORT;
 *     else if(*bytes == 0)
 *       return FLAC__STREAM_ENCODER_READ_STATUS_END_OF_STREAM;
 *     else
 *       return FLAC__STREAM_ENCODER_READ_STATUS_CONTINUE;
 *   }
 *   else
 *     return FLAC__STREAM_ENCODER_READ_STATUS_ABORT;
 * }
 * \endcode
 *
 * \note In general, FLAC__StreamEncoder functions which change the
 * state should not be called on the \a encoder while in the callback.
 *
 * \param  encoder  The encoder instance calling the callback.
 * \param  buffer   A pointer to a location for the callee to store
 *                  data to be encoded.
 * \param  bytes    A pointer to the size of the buffer.  On entry
 *                  to the callback, it contains the maximum number
 *                  of bytes that may be stored in \a buffer.  The
 *                  callee must set it to the actual number of bytes
 *                  stored (0 in case of error or end-of-stream) before
 *                  returning.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_encoder_set_client_data().
 * \retval FLAC__StreamEncoderReadStatus
 *    The callee's return status.
 */
typedef FLAC__StreamEncoderReadStatus (*FLAC__StreamEncoderReadCallback)(const FLAC__StreamEncoder *encoder, FLAC__byte buffer[], size_t *bytes, void *client_data);

/** Signature for the write callback.
 *
 *  A function pointer matching this signature must be passed to
 *  FLAC__stream_encoder_init*_stream().  The supplied function will be called
 *  by the encoder anytime there is raw encoded data ready to write.  It may
 *  include metadata mixed with encoded audio frames and the data is not
 *  guaranteed to be aligned on frame or metadata block boundaries.
 *
 *  The only duty of the callback is to write out the \a bytes worth of data
 *  in \a buffer to the current position in the output stream.  The arguments
 *  \a samples and \a current_frame are purely informational.  If \a samples
 *  is greater than \c 0, then \a current_frame will hold the current frame
 *  number that is being written; otherwise it indicates that the write
 *  callback is being called to write metadata.
 *
 * \note
 * Unlike when writing to native FLAC, when writing to Ogg FLAC the
 * write callback will be called twice when writing each audio
 * frame; once for the page header, and once for the page body.
 * When writing the page header, the \a samples argument to the
 * write callback will be \c 0.
 *
 * \note In general, FLAC__StreamEncoder functions which change the
 * state should not be called on the \a encoder while in the callback.
 *
 * \param  encoder  The encoder instance calling the callback.
 * \param  buffer   An array of encoded data of length \a bytes.
 * \param  bytes    The byte length of \a buffer.
 * \param  samples  The number of samples encoded by \a buffer.
 *                  \c 0 has a special meaning; see above.
 * \param  current_frame  The number of the current frame being encoded.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_encoder_init_*().
 * \retval FLAC__StreamEncoderWriteStatus
 *    The callee's return status.
 */
typedef FLAC__StreamEncoderWriteStatus (*FLAC__StreamEncoderWriteCallback)(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, unsigned current_frame, void *client_data);

/** Signature for the seek callback.
 *
 *  A function pointer matching this signature may be passed to
 *  FLAC__stream_encoder_init*_stream().  The supplied function will be called
 *  when the encoder needs to seek the output stream.  The encoder will pass
 *  the absolute byte offset to seek to, 0 meaning the beginning of the stream.
 *
 * Here is an example of a seek callback for stdio streams:
 * \code
 * FLAC__StreamEncoderSeekStatus seek_cb(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data)
 * {
 *   FILE *file = ((MyClientData*)client_data)->file;
 *   if(file == stdin)
 *     return FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED;
 *   else if(fseeko(file, (off_t)absolute_byte_offset, SEEK_SET) < 0)
 *     return FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR;
 *   else
 *     return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
 * }
 * \endcode
 *
 * \note In general, FLAC__StreamEncoder functions which change the
 * state should not be called on the \a encoder while in the callback.
 *
 * \param  encoder  The encoder instance calling the callback.
 * \param  absolute_byte_offset  The offset from the beginning of the stream
 *                               to seek to.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_encoder_init_*().
 * \retval FLAC__StreamEncoderSeekStatus
 *    The callee's return status.
 */
typedef FLAC__StreamEncoderSeekStatus (*FLAC__StreamEncoderSeekCallback)(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data);

/** Signature for the tell callback.
 *
 *  A function pointer matching this signature may be passed to
 *  FLAC__stream_encoder_init*_stream().  The supplied function will be called
 *  when the encoder needs to know the current position of the output stream.
 *
 * \warning
 * The callback must return the true current byte offset of the output to
 * which the encoder is writing.  If you are buffering the output, make
 * sure and take this into account.  If you are writing directly to a
 * FILE* from your write callback, ftell() is sufficient.  If you are
 * writing directly to a file descriptor from your write callback, you
 * can use lseek(fd, SEEK_CUR, 0).  The encoder may later seek back to
 * these points to rewrite metadata after encoding.
 *
 * Here is an example of a tell callback for stdio streams:
 * \code
 * FLAC__StreamEncoderTellStatus tell_cb(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
 * {
 *   FILE *file = ((MyClientData*)client_data)->file;
 *   off_t pos;
 *   if(file == stdin)
 *     return FLAC__STREAM_ENCODER_TELL_STATUS_UNSUPPORTED;
 *   else if((pos = ftello(file)) < 0)
 *     return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;
 *   else {
 *     *absolute_byte_offset = (FLAC__uint64)pos;
 *     return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
 *   }
 * }
 * \endcode
 *
 * \note In general, FLAC__StreamEncoder functions which change the
 * state should not be called on the \a encoder while in the callback.
 *
 * \param  encoder  The encoder instance calling the callback.
 * \param  absolute_byte_offset  The address at which to store the current
 *                               position of the output.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_encoder_init_*().
 * \retval FLAC__StreamEncoderTellStatus
 *    The callee's return status.
 */
typedef FLAC__StreamEncoderTellStatus (*FLAC__StreamEncoderTellCallback)(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data);

/** Signature for the metadata callback.
 *
 *  A function pointer matching this signature may be passed to
 *  FLAC__stream_encoder_init*_stream().  The supplied function will be called
 *  once at the end of encoding with the populated STREAMINFO structure.  This
 *  is so the client can seek back to the beginning of the file and write the
 *  STREAMINFO block with the correct statistics after encoding (like
 *  minimum/maximum frame size and total samples).
 *
 * \note In general, FLAC__StreamEncoder functions which change the
 * state should not be called on the \a encoder while in the callback.
 *
 * \param  encoder      The encoder instance calling the callback.
 * \param  metadata     The final populated STREAMINFO block.
 * \param  client_data  The callee's client data set through
 *                      FLAC__stream_encoder_init_*().
 */
typedef void (*FLAC__StreamEncoderMetadataCallback)(const FLAC__StreamEncoder *encoder, const FLAC__StreamMetadata *metadata, void *client_data);

/** Signature for the progress callback.
 *
 *  A function pointer matching this signature may be passed to
 *  FLAC__stream_encoder_init*_file() or FLAC__stream_encoder_init*_FILE().
 *  The supplied function will be called when the encoder has finished
 *  writing a frame.  The \c total_frames_estimate argument to the
 *  callback will be based on the value from
 *  FLAC__stream_encoder_set_total_samples_estimate().
 *
 * \note In general, FLAC__StreamEncoder functions which change the
 * state should not be called on the \a encoder while in the callback.
 *
 * \param  encoder          The encoder instance calling the callback.
 * \param  bytes_written    Bytes written so far.
 * \param  samples_written  Samples written so far.
 * \param  frames_written   Frames written so far.
 * \param  total_frames_estimate  The estimate of the total number of
 *                                frames to be written.
 * \param  client_data      The callee's client data set through
 *                          FLAC__stream_encoder_init_*().
 */
typedef void (*FLAC__StreamEncoderProgressCallback)(const FLAC__StreamEncoder *encoder, FLAC__uint64 bytes_written, FLAC__uint64 samples_written, unsigned frames_written, unsigned total_frames_estimate, void *client_data);


/***********************************************************************
 *
 * Class constructor/destructor
 *
 ***********************************************************************/

/** Create a new stream encoder instance.  The instance is created with
 *  default settings; see the individual FLAC__stream_encoder_set_*()
 *  functions for each setting's default.
 *
 * \retval FLAC__StreamEncoder*
 *    \c NULL if there was an error allocating memory, else the new instance.
 */
FLAC_API FLAC__StreamEncoder *FLAC__stream_encoder_new(void);

/** Free an encoder instance.  Deletes the object pointed to by \a encoder.
 *
 * \param encoder  A pointer to an existing encoder.
 * \assert
 *    \code encoder != NULL \endcode
 */
FLAC_API void FLAC__stream_encoder_delete(FLAC__StreamEncoder *encoder);


/***********************************************************************
 *
 * Public class method prototypes
 *
 ***********************************************************************/

/** Set the serial number for the FLAC stream to use in the Ogg container.
 *
 * \note
 * This does not need to be set for native FLAC encoding.
 *
 * \note
 * It is recommended to set a serial number explicitly as the default of '0'
 * may collide with other streams.
 *
 * \default \c 0
 * \param  encoder        An encoder instance to set.
 * \param  serial_number  See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_ogg_serial_number(FLAC__StreamEncoder *encoder, long serial_number);

/** Set the "verify" flag.  If \c true, the encoder will verify it's own
 *  encoded output by feeding it through an internal decoder and comparing
 *  the original signal against the decoded signal.  If a mismatch occurs,
 *  the process call will return \c false.  Note that this will slow the
 *  encoding process by the extra time required for decoding and comparison.
 *
 * \default \c false
 * \param  encoder  An encoder instance to set.
 * \param  value    Flag value (see above).
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_verify(FLAC__StreamEncoder *encoder, FLAC__bool value);

/** Set the <A HREF="../format.html#subset">Subset</A> flag.  If \c true,
 *  the encoder will comply with the Subset and will check the
 *  settings during FLAC__stream_encoder_init_*() to see if all settings
 *  comply.  If \c false, the settings may take advantage of the full
 *  range that the format allows.
 *
 *  Make sure you know what it entails before setting this to \c false.
 *
 * \default \c true
 * \param  encoder  An encoder instance to set.
 * \param  value    Flag value (see above).
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_streamable_subset(FLAC__StreamEncoder *encoder, FLAC__bool value);

/** Set the number of channels to be encoded.
 *
 * \default \c 2
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_channels(FLAC__StreamEncoder *encoder, unsigned value);

/** Set the sample resolution of the input to be encoded.
 *
 * \warning
 * Do not feed the encoder data that is wider than the value you
 * set here or you will generate an invalid stream.
 *
 * \default \c 16
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_bits_per_sample(FLAC__StreamEncoder *encoder, unsigned value);

/** Set the sample rate (in Hz) of the input to be encoded.
 *
 * \default \c 44100
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_sample_rate(FLAC__StreamEncoder *encoder, unsigned value);

/** Set the compression level
 *
 * The compression level is roughly proportional to the amount of effort
 * the encoder expends to compress the file.  A higher level usually
 * means more computation but higher compression.  The default level is
 * suitable for most applications.
 *
 * Currently the levels range from \c 0 (fastest, least compression) to
 * \c 8 (slowest, most compression).  A value larger than \c 8 will be
 * treated as \c 8.
 *
 * This function automatically calls the following other \c _set_
 * functions with appropriate values, so the client does not need to
 * unless it specifically wants to override them:
 * - FLAC__stream_encoder_set_do_mid_side_stereo()
 * - FLAC__stream_encoder_set_loose_mid_side_stereo()
 * - FLAC__stream_encoder_set_apodization()
 * - FLAC__stream_encoder_set_max_lpc_order()
 * - FLAC__stream_encoder_set_qlp_coeff_precision()
 * - FLAC__stream_encoder_set_do_qlp_coeff_prec_search()
 * - FLAC__stream_encoder_set_do_escape_coding()
 * - FLAC__stream_encoder_set_do_exhaustive_model_search()
 * - FLAC__stream_encoder_set_min_residual_partition_order()
 * - FLAC__stream_encoder_set_max_residual_partition_order()
 * - FLAC__stream_encoder_set_rice_parameter_search_dist()
 *
 * The actual values set for each level are:
 * <table>
 * <tr>
 *  <td><b>level</b></td>
 *  <td>do mid-side stereo</td>
 *  <td>loose mid-side stereo</td>
 *  <td>apodization</td>
 *  <td>max lpc order</td>
 *  <td>qlp coeff precision</td>
 *  <td>qlp coeff prec search</td>
 *  <td>escape coding</td>
 *  <td>exhaustive model search</td>
 *  <td>min residual partition order</td>
 *  <td>max residual partition order</td>
 *  <td>rice parameter search dist</td>
 * </tr>
 * <tr>  <td><b>0</b></td> <td>false</td> <td>false</td> <td>tukey(0.5)<td>                                     <td>0</td>  <td>0</td> <td>false</td> <td>false</td> <td>false</td> <td>0</td> <td>3</td> <td>0</td> </tr>
 * <tr>  <td><b>1</b></td> <td>true</td>  <td>true</td>  <td>tukey(0.5)<td>                                     <td>0</td>  <td>0</td> <td>false</td> <td>false</td> <td>false</td> <td>0</td> <td>3</td> <td>0</td> </tr>
 * <tr>  <td><b>2</b></td> <td>true</td>  <td>false</td> <td>tukey(0.5)<td>                                     <td>0</td>  <td>0</td> <td>false</td> <td>false</td> <td>false</td> <td>0</td> <td>3</td> <td>0</td> </tr>
 * <tr>  <td><b>3</b></td> <td>false</td> <td>false</td> <td>tukey(0.5)<td>                                     <td>6</td>  <td>0</td> <td>false</td> <td>false</td> <td>false</td> <td>0</td> <td>4</td> <td>0</td> </tr>
 * <tr>  <td><b>4</b></td> <td>true</td>  <td>true</td>  <td>tukey(0.5)<td>                                     <td>8</td>  <td>0</td> <td>false</td> <td>false</td> <td>false</td> <td>0</td> <td>4</td> <td>0</td> </tr>
 * <tr>  <td><b>5</b></td> <td>true</td>  <td>false</td> <td>tukey(0.5)<td>                                     <td>8</td>  <td>0</td> <td>false</td> <td>false</td> <td>false</td> <td>0</td> <td>5</td> <td>0</td> </tr>
 * <tr>  <td><b>6</b></td> <td>true</td>  <td>false</td> <td>tukey(0.5);partial_tukey(2)<td>                    <td>8</td>  <td>0</td> <td>false</td> <td>false</td> <td>false</td> <td>0</td> <td>6</td> <td>0</td> </tr>
 * <tr>  <td><b>7</b></td> <td>true</td>  <td>false</td> <td>tukey(0.5);partial_tukey(2)<td>                    <td>12</td> <td>0</td> <td>false</td> <td>false</td> <td>false</td> <td>0</td> <td>6</td> <td>0</td> </tr>
 * <tr>  <td><b>8</b></td> <td>true</td>  <td>false</td> <td>tukey(0.5);partial_tukey(2);punchout_tukey(3)</td> <td>12</td> <td>0</td> <td>false</td> <td>false</td> <td>false</td> <td>0</td> <td>6</td> <td>0</td> </tr>
 * </table>
 *
 * \default \c 5
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_compression_level(FLAC__StreamEncoder *encoder, unsigned value);

/** Set the blocksize to use while encoding.
 *
 * The number of samples to use per frame.  Use \c 0 to let the encoder
 * estimate a blocksize; this is usually best.
 *
 * \default \c 0
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_blocksize(FLAC__StreamEncoder *encoder, unsigned value);

/** Set to \c true to enable mid-side encoding on stereo input.  The
 *  number of channels must be 2 for this to have any effect.  Set to
 *  \c false to use only independent channel coding.
 *
 * \default \c false
 * \param  encoder  An encoder instance to set.
 * \param  value    Flag value (see above).
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_do_mid_side_stereo(FLAC__StreamEncoder *encoder, FLAC__bool value);

/** Set to \c true to enable adaptive switching between mid-side and
 *  left-right encoding on stereo input.  Set to \c false to use
 *  exhaustive searching.  Setting this to \c true requires
 *  FLAC__stream_encoder_set_do_mid_side_stereo() to also be set to
 *  \c true in order to have any effect.
 *
 * \default \c false
 * \param  encoder  An encoder instance to set.
 * \param  value    Flag value (see above).
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_loose_mid_side_stereo(FLAC__StreamEncoder *encoder, FLAC__bool value);

/** Sets the apodization function(s) the encoder will use when windowing
 *  audio data for LPC analysis.
 *
 * The \a specification is a plain ASCII string which specifies exactly
 * which functions to use.  There may be more than one (up to 32),
 * separated by \c ';' characters.  Some functions take one or more
 * comma-separated arguments in parentheses.
 *
 * The available functions are \c bartlett, \c bartlett_hann,
 * \c blackman, \c blackman_harris_4term_92db, \c connes, \c flattop,
 * \c gauss(STDDEV), \c hamming, \c hann, \c kaiser_bessel, \c nuttall,
 * \c rectangle, \c triangle, \c tukey(P), \c partial_tukey(n[/ov[/P]]),
 * \c punchout_tukey(n[/ov[/P]]), \c welch.
 *
 * For \c gauss(STDDEV), STDDEV specifies the standard deviation
 * (0<STDDEV<=0.5).
 *
 * For \c tukey(P), P specifies the fraction of the window that is
 * tapered (0<=P<=1).  P=0 corresponds to \c rectangle and P=1
 * corresponds to \c hann.
 *
 * Specifying \c partial_tukey or \c punchout_tukey works a little
 * different. These do not specify a single apodization function, but
 * a series of them with some overlap. partial_tukey specifies a series
 * of small windows (all treated separately) while punchout_tukey
 * specifies a series of windows that have a hole in them. In this way,
 * the predictor is constructed with only a part of the block, which
 * helps in case a block consists of dissimilar parts.
 *
 * The three parameters that can be specified for the functions are
 * n, ov and P. n is the number of functions to add, ov is the overlap
 * of the windows in case of partial_tukey and the overlap in the gaps
 * in case of punchout_tukey. P is the fraction of the window that is
 * tapered, like with a regular tukey window. The function can be
 * specified with only a number, a number and an overlap, or a number
 * an overlap and a P, for example, partial_tukey(3), partial_tukey(3/0.3)
 * and partial_tukey(3/0.3/0.5) are all valid. ov should be smaller than 1
 * and can be negative.
 *
 * Example specifications are \c "blackman" or
 * \c "hann;triangle;tukey(0.5);tukey(0.25);tukey(0.125)"
 *
 * Any function that is specified erroneously is silently dropped.  Up
 * to 32 functions are kept, the rest are dropped.  If the specification
 * is empty the encoder defaults to \c "tukey(0.5)".
 *
 * When more than one function is specified, then for every subframe the
 * encoder will try each of them separately and choose the window that
 * results in the smallest compressed subframe.
 *
 * Note that each function specified causes the encoder to occupy a
 * floating point array in which to store the window. Also note that the
 * values of P, STDDEV and ov are locale-specific, so if the comma
 * separator specified by the locale is a comma, a comma should be used.
 *
 * \default \c "tukey(0.5)"
 * \param  encoder        An encoder instance to set.
 * \param  specification  See above.
 * \assert
 *    \code encoder != NULL \endcode
 *    \code specification != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_apodization(FLAC__StreamEncoder *encoder, const char *specification);

/** Set the maximum LPC order, or \c 0 to use only the fixed predictors.
 *
 * \default \c 0
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_max_lpc_order(FLAC__StreamEncoder *encoder, unsigned value);

/** Set the precision, in bits, of the quantized linear predictor
 *  coefficients, or \c 0 to let the encoder select it based on the
 *  blocksize.
 *
 * \note
 * In the current implementation, qlp_coeff_precision + bits_per_sample must
 * be less than 32.
 *
 * \default \c 0
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_qlp_coeff_precision(FLAC__StreamEncoder *encoder, unsigned value);

/** Set to \c false to use only the specified quantized linear predictor
 *  coefficient precision, or \c true to search neighboring precision
 *  values and use the best one.
 *
 * \default \c false
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_do_qlp_coeff_prec_search(FLAC__StreamEncoder *encoder, FLAC__bool value);

/** Deprecated.  Setting this value has no effect.
 *
 * \default \c false
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_do_escape_coding(FLAC__StreamEncoder *encoder, FLAC__bool value);

/** Set to \c false to let the encoder estimate the best model order
 *  based on the residual signal energy, or \c true to force the
 *  encoder to evaluate all order models and select the best.
 *
 * \default \c false
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_do_exhaustive_model_search(FLAC__StreamEncoder *encoder, FLAC__bool value);

/** Set the minimum partition order to search when coding the residual.
 *  This is used in tandem with
 *  FLAC__stream_encoder_set_max_residual_partition_order().
 *
 *  The partition order determines the context size in the residual.
 *  The context size will be approximately <tt>blocksize / (2 ^ order)</tt>.
 *
 *  Set both min and max values to \c 0 to force a single context,
 *  whose Rice parameter is based on the residual signal variance.
 *  Otherwise, set a min and max order, and the encoder will search
 *  all orders, using the mean of each context for its Rice parameter,
 *  and use the best.
 *
 * \default \c 0
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_min_residual_partition_order(FLAC__StreamEncoder *encoder, unsigned value);

/** Set the maximum partition order to search when coding the residual.
 *  This is used in tandem with
 *  FLAC__stream_encoder_set_min_residual_partition_order().
 *
 *  The partition order determines the context size in the residual.
 *  The context size will be approximately <tt>blocksize / (2 ^ order)</tt>.
 *
 *  Set both min and max values to \c 0 to force a single context,
 *  whose Rice parameter is based on the residual signal variance.
 *  Otherwise, set a min and max order, and the encoder will search
 *  all orders, using the mean of each context for its Rice parameter,
 *  and use the best.
 *
 * \default \c 0
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_max_residual_partition_order(FLAC__StreamEncoder *encoder, unsigned value);

/** Deprecated.  Setting this value has no effect.
 *
 * \default \c 0
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_rice_parameter_search_dist(FLAC__StreamEncoder *encoder, unsigned value);

/** Set an estimate of the total samples that will be encoded.
 *  This is merely an estimate and may be set to \c 0 if unknown.
 *  This value will be written to the STREAMINFO block before encoding,
 *  and can remove the need for the caller to rewrite the value later
 *  if the value is known before encoding.
 *
 * \default \c 0
 * \param  encoder  An encoder instance to set.
 * \param  value    See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_total_samples_estimate(FLAC__StreamEncoder *encoder, FLAC__uint64 value);

/** Set the metadata blocks to be emitted to the stream before encoding.
 *  A value of \c NULL, \c 0 implies no metadata; otherwise, supply an
 *  array of pointers to metadata blocks.  The array is non-const since
 *  the encoder may need to change the \a is_last flag inside them, and
 *  in some cases update seek point offsets.  Otherwise, the encoder will
 *  not modify or free the blocks.  It is up to the caller to free the
 *  metadata blocks after encoding finishes.
 *
 * \note
 * The encoder stores only copies of the pointers in the \a metadata array;
 * the metadata blocks themselves must survive at least until after
 * FLAC__stream_encoder_finish() returns.  Do not free the blocks until then.
 *
 * \note
 * The STREAMINFO block is always written and no STREAMINFO block may
 * occur in the supplied array.
 *
 * \note
 * By default the encoder does not create a SEEKTABLE.  If one is supplied
 * in the \a metadata array, but the client has specified that it does not
 * support seeking, then the SEEKTABLE will be written verbatim.  However
 * by itself this is not very useful as the client will not know the stream
 * offsets for the seekpoints ahead of time.  In order to get a proper
 * seektable the client must support seeking.  See next note.
 *
 * \note
 * SEEKTABLE blocks are handled specially.  Since you will not know
 * the values for the seek point stream offsets, you should pass in
 * a SEEKTABLE 'template', that is, a SEEKTABLE object with the
 * required sample numbers (or placeholder points), with \c 0 for the
 * \a frame_samples and \a stream_offset fields for each point.  If the
 * client has specified that it supports seeking by providing a seek
 * callback to FLAC__stream_encoder_init_stream() or both seek AND read
 * callback to FLAC__stream_encoder_init_ogg_stream() (or by using
 * FLAC__stream_encoder_init*_file() or FLAC__stream_encoder_init*_FILE()),
 * then while it is encoding the encoder will fill the stream offsets in
 * for you and when encoding is finished, it will seek back and write the
 * real values into the SEEKTABLE block in the stream.  There are helper
 * routines for manipulating seektable template blocks; see metadata.h:
 * FLAC__metadata_object_seektable_template_*().  If the client does
 * not support seeking, the SEEKTABLE will have inaccurate offsets which
 * will slow down or remove the ability to seek in the FLAC stream.
 *
 * \note
 * The encoder instance \b will modify the first \c SEEKTABLE block
 * as it transforms the template to a valid seektable while encoding,
 * but it is still up to the caller to free all metadata blocks after
 * encoding.
 *
 * \note
 * A VORBIS_COMMENT block may be supplied.  The vendor string in it
 * will be ignored.  libFLAC will use it's own vendor string. libFLAC
 * will not modify the passed-in VORBIS_COMMENT's vendor string, it
 * will simply write it's own into the stream.  If no VORBIS_COMMENT
 * block is present in the \a metadata array, libFLAC will write an
 * empty one, containing only the vendor string.
 *
 * \note The Ogg FLAC mapping requires that the VORBIS_COMMENT block be
 * the second metadata block of the stream.  The encoder already supplies
 * the STREAMINFO block automatically.  If \a metadata does not contain a
 * VORBIS_COMMENT block, the encoder will supply that too.  Otherwise, if
 * \a metadata does contain a VORBIS_COMMENT block and it is not the
 * first, the init function will reorder \a metadata by moving the
 * VORBIS_COMMENT block to the front; the relative ordering of the other
 * blocks will remain as they were.
 *
 * \note The Ogg FLAC mapping limits the number of metadata blocks per
 * stream to \c 65535.  If \a num_blocks exceeds this the function will
 * return \c false.
 *
 * \default \c NULL, 0
 * \param  encoder     An encoder instance to set.
 * \param  metadata    See above.
 * \param  num_blocks  See above.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if the encoder is already initialized, else \c true.
 *    \c false if the encoder is already initialized, or if
 *    \a num_blocks > 65535 if encoding to Ogg FLAC, else \c true.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_set_metadata(FLAC__StreamEncoder *encoder, FLAC__StreamMetadata **metadata, unsigned num_blocks);

/** Get the current encoder state.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__StreamEncoderState
 *    The current encoder state.
 */
FLAC_API FLAC__StreamEncoderState FLAC__stream_encoder_get_state(const FLAC__StreamEncoder *encoder);

/** Get the state of the verify stream decoder.
 *  Useful when the stream encoder state is
 *  \c FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__StreamDecoderState
 *    The verify stream decoder state.
 */
FLAC_API FLAC__StreamDecoderState FLAC__stream_encoder_get_verify_decoder_state(const FLAC__StreamEncoder *encoder);

/** Get the current encoder state as a C string.
 *  This version automatically resolves
 *  \c FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR by getting the
 *  verify decoder's state.
 *
 * \param  encoder  A encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval const char *
 *    The encoder state as a C string.  Do not modify the contents.
 */
FLAC_API const char *FLAC__stream_encoder_get_resolved_state_string(const FLAC__StreamEncoder *encoder);

/** Get relevant values about the nature of a verify decoder error.
 *  Useful when the stream encoder state is
 *  \c FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR.  The arguments should
 *  be addresses in which the stats will be returned, or NULL if value
 *  is not desired.
 *
 * \param  encoder  An encoder instance to query.
 * \param  absolute_sample  The absolute sample number of the mismatch.
 * \param  frame_number  The number of the frame in which the mismatch occurred.
 * \param  channel       The channel in which the mismatch occurred.
 * \param  sample        The number of the sample (relative to the frame) in
 *                       which the mismatch occurred.
 * \param  expected      The expected value for the sample in question.
 * \param  got           The actual value returned by the decoder.
 * \assert
 *    \code encoder != NULL \endcode
 */
FLAC_API void FLAC__stream_encoder_get_verify_decoder_error_stats(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_sample, unsigned *frame_number, unsigned *channel, unsigned *sample, FLAC__int32 *expected, FLAC__int32 *got);

/** Get the "verify" flag.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    See FLAC__stream_encoder_set_verify().
 */
FLAC_API FLAC__bool FLAC__stream_encoder_get_verify(const FLAC__StreamEncoder *encoder);

/** Get the <A HREF="../format.html#subset>Subset</A> flag.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    See FLAC__stream_encoder_set_streamable_subset().
 */
FLAC_API FLAC__bool FLAC__stream_encoder_get_streamable_subset(const FLAC__StreamEncoder *encoder);

/** Get the number of input channels being processed.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval unsigned
 *    See FLAC__stream_encoder_set_channels().
 */
FLAC_API unsigned FLAC__stream_encoder_get_channels(const FLAC__StreamEncoder *encoder);

/** Get the input sample resolution setting.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval unsigned
 *    See FLAC__stream_encoder_set_bits_per_sample().
 */
FLAC_API unsigned FLAC__stream_encoder_get_bits_per_sample(const FLAC__StreamEncoder *encoder);

/** Get the input sample rate setting.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval unsigned
 *    See FLAC__stream_encoder_set_sample_rate().
 */
FLAC_API unsigned FLAC__stream_encoder_get_sample_rate(const FLAC__StreamEncoder *encoder);

/** Get the blocksize setting.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval unsigned
 *    See FLAC__stream_encoder_set_blocksize().
 */
FLAC_API unsigned FLAC__stream_encoder_get_blocksize(const FLAC__StreamEncoder *encoder);

/** Get the "mid/side stereo coding" flag.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    See FLAC__stream_encoder_get_do_mid_side_stereo().
 */
FLAC_API FLAC__bool FLAC__stream_encoder_get_do_mid_side_stereo(const FLAC__StreamEncoder *encoder);

/** Get the "adaptive mid/side switching" flag.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    See FLAC__stream_encoder_set_loose_mid_side_stereo().
 */
FLAC_API FLAC__bool FLAC__stream_encoder_get_loose_mid_side_stereo(const FLAC__StreamEncoder *encoder);

/** Get the maximum LPC order setting.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval unsigned
 *    See FLAC__stream_encoder_set_max_lpc_order().
 */
FLAC_API unsigned FLAC__stream_encoder_get_max_lpc_order(const FLAC__StreamEncoder *encoder);

/** Get the quantized linear predictor coefficient precision setting.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval unsigned
 *    See FLAC__stream_encoder_set_qlp_coeff_precision().
 */
FLAC_API unsigned FLAC__stream_encoder_get_qlp_coeff_precision(const FLAC__StreamEncoder *encoder);

/** Get the qlp coefficient precision search flag.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    See FLAC__stream_encoder_set_do_qlp_coeff_prec_search().
 */
FLAC_API FLAC__bool FLAC__stream_encoder_get_do_qlp_coeff_prec_search(const FLAC__StreamEncoder *encoder);

/** Get the "escape coding" flag.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    See FLAC__stream_encoder_set_do_escape_coding().
 */
FLAC_API FLAC__bool FLAC__stream_encoder_get_do_escape_coding(const FLAC__StreamEncoder *encoder);

/** Get the exhaustive model search flag.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    See FLAC__stream_encoder_set_do_exhaustive_model_search().
 */
FLAC_API FLAC__bool FLAC__stream_encoder_get_do_exhaustive_model_search(const FLAC__StreamEncoder *encoder);

/** Get the minimum residual partition order setting.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval unsigned
 *    See FLAC__stream_encoder_set_min_residual_partition_order().
 */
FLAC_API unsigned FLAC__stream_encoder_get_min_residual_partition_order(const FLAC__StreamEncoder *encoder);

/** Get maximum residual partition order setting.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval unsigned
 *    See FLAC__stream_encoder_set_max_residual_partition_order().
 */
FLAC_API unsigned FLAC__stream_encoder_get_max_residual_partition_order(const FLAC__StreamEncoder *encoder);

/** Get the Rice parameter search distance setting.
 *
 * \param  encoder  An encoder instance to query.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval unsigned
 *    See FLAC__stream_encoder_set_rice_parameter_search_dist().
 */
FLAC_API unsigned FLAC__stream_encoder_get_rice_parameter_search_dist(const FLAC__StreamEncoder *encoder);

/** Get the previously set estimate of the total samples to be encoded.
 *  The encoder merely mimics back the value given to
 *  FLAC__stream_encoder_set_total_samples_estimate() since it has no
 *  other way of knowing how many samples the client will encode.
 *
 * \param  encoder  An encoder instance to set.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__uint64
 *    See FLAC__stream_encoder_get_total_samples_estimate().
 */
FLAC_API FLAC__uint64 FLAC__stream_encoder_get_total_samples_estimate(const FLAC__StreamEncoder *encoder);

/** Initialize the encoder instance to encode native FLAC streams.
 *
 *  This flavor of initialization sets up the encoder to encode to a
 *  native FLAC stream. I/O is performed via callbacks to the client.
 *  For encoding to a plain file via filename or open \c FILE*,
 *  FLAC__stream_encoder_init_file() and FLAC__stream_encoder_init_FILE()
 *  provide a simpler interface.
 *
 *  This function should be called after FLAC__stream_encoder_new() and
 *  FLAC__stream_encoder_set_*() but before FLAC__stream_encoder_process()
 *  or FLAC__stream_encoder_process_interleaved().
 *  initialization succeeded.
 *
 *  The call to FLAC__stream_encoder_init_stream() currently will also
 *  immediately call the write callback several times, once with the \c fLaC
 *  signature, and once for each encoded metadata block.
 *
 * \param  encoder            An uninitialized encoder instance.
 * \param  write_callback     See FLAC__StreamEncoderWriteCallback.  This
 *                            pointer must not be \c NULL.
 * \param  seek_callback      See FLAC__StreamEncoderSeekCallback.  This
 *                            pointer may be \c NULL if seeking is not
 *                            supported.  The encoder uses seeking to go back
 *                            and write some some stream statistics to the
 *                            STREAMINFO block; this is recommended but not
 *                            necessary to create a valid FLAC stream.  If
 *                            \a seek_callback is not \c NULL then a
 *                            \a tell_callback must also be supplied.
 *                            Alternatively, a dummy seek callback that just
 *                            returns \c FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the encoder.
 * \param  tell_callback      See FLAC__StreamEncoderTellCallback.  This
 *                            pointer may be \c NULL if seeking is not
 *                            supported.  If \a seek_callback is \c NULL then
 *                            this argument will be ignored.  If
 *                            \a seek_callback is not \c NULL then a
 *                            \a tell_callback must also be supplied.
 *                            Alternatively, a dummy tell callback that just
 *                            returns \c FLAC__STREAM_ENCODER_TELL_STATUS_UNSUPPORTED
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the encoder.
 * \param  metadata_callback  See FLAC__StreamEncoderMetadataCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.  If the client provides a seek callback,
 *                            this function is not necessary as the encoder
 *                            will automatically seek back and update the
 *                            STREAMINFO block.  It may also be \c NULL if the
 *                            client does not support seeking, since it will
 *                            have no way of going back to update the
 *                            STREAMINFO.  However the client can still supply
 *                            a callback if it would like to know the details
 *                            from the STREAMINFO.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__StreamEncoderInitStatus
 *    \c FLAC__STREAM_ENCODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamEncoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamEncoderInitStatus FLAC__stream_encoder_init_stream(FLAC__StreamEncoder *encoder, FLAC__StreamEncoderWriteCallback write_callback, FLAC__StreamEncoderSeekCallback seek_callback, FLAC__StreamEncoderTellCallback tell_callback, FLAC__StreamEncoderMetadataCallback metadata_callback, void *client_data);

/** Initialize the encoder instance to encode Ogg FLAC streams.
 *
 *  This flavor of initialization sets up the encoder to encode to a FLAC
 *  stream in an Ogg container.  I/O is performed via callbacks to the
 *  client.  For encoding to a plain file via filename or open \c FILE*,
 *  FLAC__stream_encoder_init_ogg_file() and FLAC__stream_encoder_init_ogg_FILE()
 *  provide a simpler interface.
 *
 *  This function should be called after FLAC__stream_encoder_new() and
 *  FLAC__stream_encoder_set_*() but before FLAC__stream_encoder_process()
 *  or FLAC__stream_encoder_process_interleaved().
 *  initialization succeeded.
 *
 *  The call to FLAC__stream_encoder_init_ogg_stream() currently will also
 *  immediately call the write callback several times to write the metadata
 *  packets.
 *
 * \param  encoder            An uninitialized encoder instance.
 * \param  read_callback      See FLAC__StreamEncoderReadCallback.  This
 *                            pointer must not be \c NULL if \a seek_callback
 *                            is non-NULL since they are both needed to be
 *                            able to write data back to the Ogg FLAC stream
 *                            in the post-encode phase.
 * \param  write_callback     See FLAC__StreamEncoderWriteCallback.  This
 *                            pointer must not be \c NULL.
 * \param  seek_callback      See FLAC__StreamEncoderSeekCallback.  This
 *                            pointer may be \c NULL if seeking is not
 *                            supported.  The encoder uses seeking to go back
 *                            and write some some stream statistics to the
 *                            STREAMINFO block; this is recommended but not
 *                            necessary to create a valid FLAC stream.  If
 *                            \a seek_callback is not \c NULL then a
 *                            \a tell_callback must also be supplied.
 *                            Alternatively, a dummy seek callback that just
 *                            returns \c FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the encoder.
 * \param  tell_callback      See FLAC__StreamEncoderTellCallback.  This
 *                            pointer may be \c NULL if seeking is not
 *                            supported.  If \a seek_callback is \c NULL then
 *                            this argument will be ignored.  If
 *                            \a seek_callback is not \c NULL then a
 *                            \a tell_callback must also be supplied.
 *                            Alternatively, a dummy tell callback that just
 *                            returns \c FLAC__STREAM_ENCODER_TELL_STATUS_UNSUPPORTED
 *                            may also be supplied, all though this is slightly
 *                            less efficient for the encoder.
 * \param  metadata_callback  See FLAC__StreamEncoderMetadataCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.  If the client provides a seek callback,
 *                            this function is not necessary as the encoder
 *                            will automatically seek back and update the
 *                            STREAMINFO block.  It may also be \c NULL if the
 *                            client does not support seeking, since it will
 *                            have no way of going back to update the
 *                            STREAMINFO.  However the client can still supply
 *                            a callback if it would like to know the details
 *                            from the STREAMINFO.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__StreamEncoderInitStatus
 *    \c FLAC__STREAM_ENCODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamEncoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamEncoderInitStatus FLAC__stream_encoder_init_ogg_stream(FLAC__StreamEncoder *encoder, FLAC__StreamEncoderReadCallback read_callback, FLAC__StreamEncoderWriteCallback write_callback, FLAC__StreamEncoderSeekCallback seek_callback, FLAC__StreamEncoderTellCallback tell_callback, FLAC__StreamEncoderMetadataCallback metadata_callback, void *client_data);

/** Initialize the encoder instance to encode native FLAC files.
 *
 *  This flavor of initialization sets up the encoder to encode to a
 *  plain native FLAC file.  For non-stdio streams, you must use
 *  FLAC__stream_encoder_init_stream() and provide callbacks for the I/O.
 *
 *  This function should be called after FLAC__stream_encoder_new() and
 *  FLAC__stream_encoder_set_*() but before FLAC__stream_encoder_process()
 *  or FLAC__stream_encoder_process_interleaved().
 *  initialization succeeded.
 *
 * \param  encoder            An uninitialized encoder instance.
 * \param  file               An open file.  The file should have been opened
 *                            with mode \c "w+b" and rewound.  The file
 *                            becomes owned by the encoder and should not be
 *                            manipulated by the client while encoding.
 *                            Unless \a file is \c stdout, it will be closed
 *                            when FLAC__stream_encoder_finish() is called.
 *                            Note however that a proper SEEKTABLE cannot be
 *                            created when encoding to \c stdout since it is
 *                            not seekable.
 * \param  progress_callback  See FLAC__StreamEncoderProgressCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code encoder != NULL \endcode
 *    \code file != NULL \endcode
 * \retval FLAC__StreamEncoderInitStatus
 *    \c FLAC__STREAM_ENCODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamEncoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamEncoderInitStatus FLAC__stream_encoder_init_FILE(FLAC__StreamEncoder *encoder, FILE *file, FLAC__StreamEncoderProgressCallback progress_callback, void *client_data);

/** Initialize the encoder instance to encode Ogg FLAC files.
 *
 *  This flavor of initialization sets up the encoder to encode to a
 *  plain Ogg FLAC file.  For non-stdio streams, you must use
 *  FLAC__stream_encoder_init_ogg_stream() and provide callbacks for the I/O.
 *
 *  This function should be called after FLAC__stream_encoder_new() and
 *  FLAC__stream_encoder_set_*() but before FLAC__stream_encoder_process()
 *  or FLAC__stream_encoder_process_interleaved().
 *  initialization succeeded.
 *
 * \param  encoder            An uninitialized encoder instance.
 * \param  file               An open file.  The file should have been opened
 *                            with mode \c "w+b" and rewound.  The file
 *                            becomes owned by the encoder and should not be
 *                            manipulated by the client while encoding.
 *                            Unless \a file is \c stdout, it will be closed
 *                            when FLAC__stream_encoder_finish() is called.
 *                            Note however that a proper SEEKTABLE cannot be
 *                            created when encoding to \c stdout since it is
 *                            not seekable.
 * \param  progress_callback  See FLAC__StreamEncoderProgressCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code encoder != NULL \endcode
 *    \code file != NULL \endcode
 * \retval FLAC__StreamEncoderInitStatus
 *    \c FLAC__STREAM_ENCODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamEncoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamEncoderInitStatus FLAC__stream_encoder_init_ogg_FILE(FLAC__StreamEncoder *encoder, FILE *file, FLAC__StreamEncoderProgressCallback progress_callback, void *client_data);

/** Initialize the encoder instance to encode native FLAC files.
 *
 *  This flavor of initialization sets up the encoder to encode to a plain
 *  FLAC file.  If POSIX fopen() semantics are not sufficient (for example,
 *  with Unicode filenames on Windows), you must use
 *  FLAC__stream_encoder_init_FILE(), or FLAC__stream_encoder_init_stream()
 *  and provide callbacks for the I/O.
 *
 *  This function should be called after FLAC__stream_encoder_new() and
 *  FLAC__stream_encoder_set_*() but before FLAC__stream_encoder_process()
 *  or FLAC__stream_encoder_process_interleaved().
 *  initialization succeeded.
 *
 * \param  encoder            An uninitialized encoder instance.
 * \param  filename           The name of the file to encode to.  The file will
 *                            be opened with fopen().  Use \c NULL to encode to
 *                            \c stdout.  Note however that a proper SEEKTABLE
 *                            cannot be created when encoding to \c stdout since
 *                            it is not seekable.
 * \param  progress_callback  See FLAC__StreamEncoderProgressCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__StreamEncoderInitStatus
 *    \c FLAC__STREAM_ENCODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamEncoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamEncoderInitStatus FLAC__stream_encoder_init_file(FLAC__StreamEncoder *encoder, const char *filename, FLAC__StreamEncoderProgressCallback progress_callback, void *client_data);

/** Initialize the encoder instance to encode Ogg FLAC files.
 *
 *  This flavor of initialization sets up the encoder to encode to a plain
 *  Ogg FLAC file.  If POSIX fopen() semantics are not sufficient (for example,
 *  with Unicode filenames on Windows), you must use
 *  FLAC__stream_encoder_init_ogg_FILE(), or FLAC__stream_encoder_init_ogg_stream()
 *  and provide callbacks for the I/O.
 *
 *  This function should be called after FLAC__stream_encoder_new() and
 *  FLAC__stream_encoder_set_*() but before FLAC__stream_encoder_process()
 *  or FLAC__stream_encoder_process_interleaved().
 *  initialization succeeded.
 *
 * \param  encoder            An uninitialized encoder instance.
 * \param  filename           The name of the file to encode to.  The file will
 *                            be opened with fopen().  Use \c NULL to encode to
 *                            \c stdout.  Note however that a proper SEEKTABLE
 *                            cannot be created when encoding to \c stdout since
 *                            it is not seekable.
 * \param  progress_callback  See FLAC__StreamEncoderProgressCallback.  This
 *                            pointer may be \c NULL if the callback is not
 *                            desired.
 * \param  client_data        This value will be supplied to callbacks in their
 *                            \a client_data argument.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__StreamEncoderInitStatus
 *    \c FLAC__STREAM_ENCODER_INIT_STATUS_OK if initialization was successful;
 *    see FLAC__StreamEncoderInitStatus for the meanings of other return values.
 */
FLAC_API FLAC__StreamEncoderInitStatus FLAC__stream_encoder_init_ogg_file(FLAC__StreamEncoder *encoder, const char *filename, FLAC__StreamEncoderProgressCallback progress_callback, void *client_data);

/** Finish the encoding process.
 *  Flushes the encoding buffer, releases resources, resets the encoder
 *  settings to their defaults, and returns the encoder state to
 *  FLAC__STREAM_ENCODER_UNINITIALIZED.  Note that this can generate
 *  one or more write callbacks before returning, and will generate
 *  a metadata callback.
 *
 *  Note that in the course of processing the last frame, errors can
 *  occur, so the caller should be sure to check the return value to
 *  ensure the file was encoded properly.
 *
 *  In the event of a prematurely-terminated encode, it is not strictly
 *  necessary to call this immediately before FLAC__stream_encoder_delete()
 *  but it is good practice to match every FLAC__stream_encoder_init_*()
 *  with a FLAC__stream_encoder_finish().
 *
 * \param  encoder  An uninitialized encoder instance.
 * \assert
 *    \code encoder != NULL \endcode
 * \retval FLAC__bool
 *    \c false if an error occurred processing the last frame; or if verify
 *    mode is set (see FLAC__stream_encoder_set_verify()), there was a
 *    verify mismatch; else \c true.  If \c false, caller should check the
 *    state with FLAC__stream_encoder_get_state() for more information
 *    about the error.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_finish(FLAC__StreamEncoder *encoder);

/** Submit data for encoding.
 *  This version allows you to supply the input data via an array of
 *  pointers, each pointer pointing to an array of \a samples samples
 *  representing one channel.  The samples need not be block-aligned,
 *  but each channel should have the same number of samples.  Each sample
 *  should be a signed integer, right-justified to the resolution set by
 *  FLAC__stream_encoder_set_bits_per_sample().  For example, if the
 *  resolution is 16 bits per sample, the samples should all be in the
 *  range [-32768,32767].
 *
 *  For applications where channel order is important, channels must
 *  follow the order as described in the
 *  <A HREF="../format.html#frame_header">frame header</A>.
 *
 * \param  encoder  An initialized encoder instance in the OK state.
 * \param  buffer   An array of pointers to each channel's signal.
 * \param  samples  The number of samples in one channel.
 * \assert
 *    \code encoder != NULL \endcode
 *    \code FLAC__stream_encoder_get_state(encoder) == FLAC__STREAM_ENCODER_OK \endcode
 * \retval FLAC__bool
 *    \c true if successful, else \c false; in this case, check the
 *    encoder state with FLAC__stream_encoder_get_state() to see what
 *    went wrong.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_process(FLAC__StreamEncoder *encoder, const FLAC__int32 * const buffer[], unsigned samples);

/** Submit data for encoding.
 *  This version allows you to supply the input data where the channels
 *  are interleaved into a single array (i.e. channel0_sample0,
 *  channel1_sample0, ... , channelN_sample0, channel0_sample1, ...).
 *  The samples need not be block-aligned but they must be
 *  sample-aligned, i.e. the first value should be channel0_sample0
 *  and the last value channelN_sampleM.  Each sample should be a signed
 *  integer, right-justified to the resolution set by
 *  FLAC__stream_encoder_set_bits_per_sample().  For example, if the
 *  resolution is 16 bits per sample, the samples should all be in the
 *  range [-32768,32767].
 *
 *  For applications where channel order is important, channels must
 *  follow the order as described in the
 *  <A HREF="../format.html#frame_header">frame header</A>.
 *
 * \param  encoder  An initialized encoder instance in the OK state.
 * \param  buffer   An array of channel-interleaved data (see above).
 * \param  samples  The number of samples in one channel, the same as for
 *                  FLAC__stream_encoder_process().  For example, if
 *                  encoding two channels, \c 1000 \a samples corresponds
 *                  to a \a buffer of 2000 values.
 * \assert
 *    \code encoder != NULL \endcode
 *    \code FLAC__stream_encoder_get_state(encoder) == FLAC__STREAM_ENCODER_OK \endcode
 * \retval FLAC__bool
 *    \c true if successful, else \c false; in this case, check the
 *    encoder state with FLAC__stream_encoder_get_state() to see what
 *    went wrong.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_process_interleaved(FLAC__StreamEncoder *encoder, const FLAC__int32 buffer[], unsigned samples);


#if defined _MSC_VER || defined __BORLANDC__ || defined __MINGW32__
using FLAC__off_t = __int64; /* use this instead of off_t to fix the 2 GB limit */
#else
using FLAC__off_t = off_t;
#endif

#if defined __INTEL_COMPILER || (defined _MSC_VER && defined _WIN64)
/* MSVS generates VERY slow 32-bit code with __restrict */
#define flac_restrict __restrict
#elif defined __GNUC__
#define flac_restrict __restrict__
#else
#define flac_restrict
#endif

#define FLAC__U64L(x) x##ULL

#if defined _MSC_VER
#  if _MSC_VER >= 1600
/* Visual Studio 2010 has decent C99 support */
#    define PRIu64 "llu"
#    define PRId64 "lld"
#    define PRIx64 "llx"
#  else
#    ifndef UINT32_MAX
#      define UINT32_MAX _UI32_MAX
#    endif
     typedef unsigned __int64 uint64_t;
     typedef unsigned __int32 uint32_t;
     typedef unsigned __int16 uint16_t;
     typedef unsigned __int8 uint8_t;
     typedef __int64 int64_t;
     typedef __int32 int32_t;
     typedef __int16 int16_t;
     typedef __int8  int8_t;
#    define PRIu64 "I64u"
#    define PRId64 "I64d"
#    define PRIx64 "I64x"
#  endif
#endif /* defined _MSC_VER */

/* FLAC needs to compile and work correctly on systems with a normal ISO C99
 * snprintf as well as Microsoft Visual Studio which has an non-standards
 * conformant snprint_s function.
 *
 * This function wraps the MS version to behave more like the the ISO version.
 */
#ifdef __cplusplus
extern "C" {
#endif
int flac_snprintf(char *str, size_t size, const char *fmt, ...);
int flac_vsnprintf(char *str, size_t size, const char *fmt, va_list va);
#ifdef __cplusplus
}
#endif


/* avoid malloc()ing 0 bytes, see:
 * https://www.securecoding.cert.org/confluence/display/seccode/MEM04-A.+Do+not+make+assumptions+about+the+result+of+allocating+0+bytes?focusedCommentId=5407003
*/
static inline void *safe_malloc_(size_t size)
{
    /* malloc(0) is undefined; FLAC src convention is to always allocate */
    if(!size)
        size++;
    return malloc(size);
}

static inline void *safe_calloc_(size_t nmemb, size_t size)
{
    if(!nmemb || !size)
        return malloc(1); /* malloc(0) is undefined; FLAC src convention is to always allocate */
    return calloc(nmemb, size);
}

/*@@@@ there's probably a better way to prevent overflows when allocating untrusted sums but this works for now */

static inline void *safe_malloc_add_2op_(size_t size1, size_t size2)
{
    size2 += size1;
    if(size2 < size1)
        return 0;
    return safe_malloc_(size2);
}

static inline void *safe_malloc_add_3op_(size_t size1, size_t size2, size_t size3)
{
    size2 += size1;
    if(size2 < size1)
        return 0;
    size3 += size2;
    if(size3 < size2)
        return 0;
    return safe_malloc_(size3);
}

static inline void *safe_malloc_add_4op_(size_t size1, size_t size2, size_t size3, size_t size4)
{
    size2 += size1;
    if(size2 < size1)
        return 0;
    size3 += size2;
    if(size3 < size2)
        return 0;
    size4 += size3;
    if(size4 < size3)
        return 0;
    return safe_malloc_(size4);
}

void *safe_malloc_mul_2op_(size_t size1, size_t size2) ;

static inline void *safe_malloc_mul_3op_(size_t size1, size_t size2, size_t size3)
{
    if(!size1 || !size2 || !size3)
        return malloc(1); /* malloc(0) is undefined; FLAC src convention is to always allocate */
    if(size1 > SIZE_MAX / size2)
        return 0;
    size1 *= size2;
    if(size1 > SIZE_MAX / size3)
        return 0;
    return malloc(size1*size3);
}

/* size1*size2 + size3 */
static inline void *safe_malloc_mul2add_(size_t size1, size_t size2, size_t size3)
{
    if(!size1 || !size2)
        return safe_malloc_(size3);
    if(size1 > SIZE_MAX / size2)
        return 0;
    return safe_malloc_add_2op_(size1*size2, size3);
}

/* size1 * (size2 + size3) */
static inline void *safe_malloc_muladd2_(size_t size1, size_t size2, size_t size3)
{
    if(!size1 || (!size2 && !size3))
        return malloc(1); /* malloc(0) is undefined; FLAC src convention is to always allocate */
    size2 += size3;
    if(size2 < size3)
        return 0;
    if(size1 > SIZE_MAX / size2)
        return 0;
    return malloc(size1*size2);
}

static inline void *safe_realloc_add_2op_(void *ptr, size_t size1, size_t size2)
{
    size2 += size1;
    if(size2 < size1)
        return 0;
    return realloc(ptr, size2);
}

static inline void *safe_realloc_add_3op_(void *ptr, size_t size1, size_t size2, size_t size3)
{
    size2 += size1;
    if(size2 < size1)
        return 0;
    size3 += size2;
    if(size3 < size2)
        return 0;
    return realloc(ptr, size3);
}

static inline void *safe_realloc_add_4op_(void *ptr, size_t size1, size_t size2, size_t size3, size_t size4)
{
    size2 += size1;
    if(size2 < size1)
        return 0;
    size3 += size2;
    if(size3 < size2)
        return 0;
    size4 += size3;
    if(size4 < size3)
        return 0;
    return realloc(ptr, size4);
}

static inline void *safe_realloc_mul_2op_(void *ptr, size_t size1, size_t size2)
{
    if(!size1 || !size2)
        return realloc(ptr, 0); /* preserve POSIX realloc(ptr, 0) semantics */
    if(size1 > SIZE_MAX / size2)
        return 0;
    return realloc(ptr, size1*size2);
}

/* size1 * (size2 + size3) */
static inline void *safe_realloc_muladd2_(void *ptr, size_t size1, size_t size2, size_t size3)
{
    if(!size1 || (!size2 && !size3))
        return realloc(ptr, 0); /* preserve POSIX realloc(ptr, 0) semantics */
    size2 += size3;
    if(size2 < size3)
        return 0;
    return safe_realloc_mul_2op_(ptr, size1, size2);
}

typedef enum {
    FLAC__CPUINFO_TYPE_IA32,
    FLAC__CPUINFO_TYPE_X86_64,
    FLAC__CPUINFO_TYPE_UNKNOWN
} FLAC__CPUInfo_Type;

#if defined FLAC__CPU_IA32
typedef struct {
    FLAC__bool cmov;
    FLAC__bool mmx;
    FLAC__bool sse;
    FLAC__bool sse2;

    FLAC__bool sse3;
    FLAC__bool ssse3;
    FLAC__bool sse41;
    FLAC__bool sse42;
    FLAC__bool avx;
    FLAC__bool avx2;
    FLAC__bool fma;
} FLAC__CPUInfo_IA32;
#elif defined FLAC__CPU_X86_64
typedef struct {
    FLAC__bool sse3;
    FLAC__bool ssse3;
    FLAC__bool sse41;
    FLAC__bool sse42;
    FLAC__bool avx;
    FLAC__bool avx2;
    FLAC__bool fma;
} FLAC__CPUInfo_x86;
#endif

typedef struct {
    FLAC__bool use_asm;
    FLAC__CPUInfo_Type type;
#if defined FLAC__CPU_IA32
    FLAC__CPUInfo_IA32 ia32;
#elif defined FLAC__CPU_X86_64
    FLAC__CPUInfo_x86 x86;
#endif
} FLAC__CPUInfo;

void FLAC__cpu_info(FLAC__CPUInfo *info);

#ifndef FLAC__NO_ASM
# if defined FLAC__CPU_IA32 && defined FLAC__HAS_NASM
FLAC__uint32 FLAC__cpu_have_cpuid_asm_ia32(void);
void         FLAC__cpu_info_asm_ia32(FLAC__uint32 *flags_edx, FLAC__uint32 *flags_ecx);
# endif
# if (defined FLAC__CPU_IA32 || defined FLAC__CPU_X86_64) && defined FLAC__HAS_X86INTRIN
FLAC__uint32 FLAC__cpu_have_cpuid_x86(void);
void         FLAC__cpu_info_x86(FLAC__uint32 level, FLAC__uint32 *eax, FLAC__uint32 *ebx, FLAC__uint32 *ecx, FLAC__uint32 *edx);
FLAC__uint32 FLAC__cpu_xgetbv_x86(void);
# endif
#endif

/*
 * opaque structure definition
 */
struct FLAC__BitReader;
typedef struct FLAC__BitReader FLAC__BitReader;

typedef FLAC__bool (*FLAC__BitReaderReadCallback)(FLAC__byte buffer[], size_t *bytes, void *client_data);

void FLAC__bitreader_free(FLAC__BitReader *br); /* does not 'free(br)' */

/*
 * CRC functions
 */
void FLAC__bitreader_reset_read_crc16(FLAC__BitReader *br, FLAC__uint16 seed);
FLAC__uint16 FLAC__bitreader_get_read_crc16(FLAC__BitReader *br);

/*
 * info functions
 */
static FLAC__bool FLAC__bitreader_is_consumed_byte_aligned(const FLAC__BitReader *br);
static unsigned FLAC__bitreader_bits_left_for_byte_alignment(const FLAC__BitReader *br);
static unsigned FLAC__bitreader_get_input_bits_unconsumed(const FLAC__BitReader *br);

/*
 * read functions
 */

FLAC__bool FLAC__bitreader_read_raw_int32(FLAC__BitReader *br, FLAC__int32 *val, unsigned bits);
FLAC__bool FLAC__bitreader_read_raw_uint64(FLAC__BitReader *br, FLAC__uint64 *val, unsigned bits);
FLAC__bool FLAC__bitreader_read_uint32_little_endian(FLAC__BitReader *br, FLAC__uint32 *val); /*only for bits=32*/
FLAC__bool FLAC__bitreader_skip_bits_no_crc(FLAC__BitReader *br, unsigned bits); /* WATCHOUT: does not CRC the skipped data! */ /*@@@@ add to unit tests */
FLAC__bool FLAC__bitreader_skip_byte_block_aligned_no_crc(FLAC__BitReader *br, unsigned nvals); /* WATCHOUT: does not CRC the read data! */
FLAC__bool FLAC__bitreader_read_byte_block_aligned_no_crc(FLAC__BitReader *br, FLAC__byte *val, unsigned nvals); /* WATCHOUT: does not CRC the read data! */
FLAC__bool FLAC__bitreader_read_unary_unsigned(FLAC__BitReader *br, unsigned *val);
FLAC__bool FLAC__bitreader_read_rice_signed(FLAC__BitReader *br, int *val, unsigned parameter);
FLAC__bool FLAC__bitreader_read_rice_signed_block(FLAC__BitReader *br, int vals[], unsigned nvals, unsigned parameter);
FLAC__bool FLAC__bitreader_read_utf8_uint32(FLAC__BitReader *br, FLAC__uint32 *val, FLAC__byte *raw, unsigned *rawlen);
FLAC__bool FLAC__bitreader_read_utf8_uint64(FLAC__BitReader *br, FLAC__uint64 *val, FLAC__byte *raw, unsigned *rawlen);

/* 8 bit CRC generator, MSB shifted first
** polynomial = x^8 + x^2 + x^1 + x^0
** init = 0
*/
/* CRC-8, poly = x^8 + x^2 + x^1 + x^0, init = 0 */

static constexpr inline FLAC__byte FLAC__crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15,
    0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65,
    0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5,
    0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85,
    0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2,
    0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2,
    0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32,
    0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42,
    0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C,
    0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC,
    0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C,
    0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C,
    0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B,
    0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B,
    0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB,
    0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB,
    0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

#define FLAC__CRC8_UPDATE(data, crc) (crc) = FLAC__crc8_table[(crc) ^ (data)];
void FLAC__crc8_update(const FLAC__byte data, FLAC__uint8 *crc);
void FLAC__crc8_update_block(const FLAC__byte *data, unsigned len, FLAC__uint8 *crc);
FLAC__uint8 FLAC__crc8(const FLAC__byte *data, unsigned len);

/* 16 bit CRC generator, MSB shifted first
** polynomial = x^16 + x^15 + x^2 + x^0
** init = 0
*/
static inline constexpr unsigned const FLAC__crc16_table[256] = {
    0x0000,  0x8005,  0x800f,  0x000a,  0x801b,  0x001e,  0x0014,  0x8011,
    0x8033,  0x0036,  0x003c,  0x8039,  0x0028,  0x802d,  0x8027,  0x0022,
    0x8063,  0x0066,  0x006c,  0x8069,  0x0078,  0x807d,  0x8077,  0x0072,
    0x0050,  0x8055,  0x805f,  0x005a,  0x804b,  0x004e,  0x0044,  0x8041,
    0x80c3,  0x00c6,  0x00cc,  0x80c9,  0x00d8,  0x80dd,  0x80d7,  0x00d2,
    0x00f0,  0x80f5,  0x80ff,  0x00fa,  0x80eb,  0x00ee,  0x00e4,  0x80e1,
    0x00a0,  0x80a5,  0x80af,  0x00aa,  0x80bb,  0x00be,  0x00b4,  0x80b1,
    0x8093,  0x0096,  0x009c,  0x8099,  0x0088,  0x808d,  0x8087,  0x0082,
    0x8183,  0x0186,  0x018c,  0x8189,  0x0198,  0x819d,  0x8197,  0x0192,
    0x01b0,  0x81b5,  0x81bf,  0x01ba,  0x81ab,  0x01ae,  0x01a4,  0x81a1,
    0x01e0,  0x81e5,  0x81ef,  0x01ea,  0x81fb,  0x01fe,  0x01f4,  0x81f1,
    0x81d3,  0x01d6,  0x01dc,  0x81d9,  0x01c8,  0x81cd,  0x81c7,  0x01c2,
    0x0140,  0x8145,  0x814f,  0x014a,  0x815b,  0x015e,  0x0154,  0x8151,
    0x8173,  0x0176,  0x017c,  0x8179,  0x0168,  0x816d,  0x8167,  0x0162,
    0x8123,  0x0126,  0x012c,  0x8129,  0x0138,  0x813d,  0x8137,  0x0132,
    0x0110,  0x8115,  0x811f,  0x011a,  0x810b,  0x010e,  0x0104,  0x8101,
    0x8303,  0x0306,  0x030c,  0x8309,  0x0318,  0x831d,  0x8317,  0x0312,
    0x0330,  0x8335,  0x833f,  0x033a,  0x832b,  0x032e,  0x0324,  0x8321,
    0x0360,  0x8365,  0x836f,  0x036a,  0x837b,  0x037e,  0x0374,  0x8371,
    0x8353,  0x0356,  0x035c,  0x8359,  0x0348,  0x834d,  0x8347,  0x0342,
    0x03c0,  0x83c5,  0x83cf,  0x03ca,  0x83db,  0x03de,  0x03d4,  0x83d1,
    0x83f3,  0x03f6,  0x03fc,  0x83f9,  0x03e8,  0x83ed,  0x83e7,  0x03e2,
    0x83a3,  0x03a6,  0x03ac,  0x83a9,  0x03b8,  0x83bd,  0x83b7,  0x03b2,
    0x0390,  0x8395,  0x839f,  0x039a,  0x838b,  0x038e,  0x0384,  0x8381,
    0x0280,  0x8285,  0x828f,  0x028a,  0x829b,  0x029e,  0x0294,  0x8291,
    0x82b3,  0x02b6,  0x02bc,  0x82b9,  0x02a8,  0x82ad,  0x82a7,  0x02a2,
    0x82e3,  0x02e6,  0x02ec,  0x82e9,  0x02f8,  0x82fd,  0x82f7,  0x02f2,
    0x02d0,  0x82d5,  0x82df,  0x02da,  0x82cb,  0x02ce,  0x02c4,  0x82c1,
    0x8243,  0x0246,  0x024c,  0x8249,  0x0258,  0x825d,  0x8257,  0x0252,
    0x0270,  0x8275,  0x827f,  0x027a,  0x826b,  0x026e,  0x0264,  0x8261,
    0x0220,  0x8225,  0x822f,  0x022a,  0x823b,  0x023e,  0x0234,  0x8231,
    0x8213,  0x0216,  0x021c,  0x8219,  0x0208,  0x820d,  0x8207,  0x0202
};


#define FLAC__CRC16_UPDATE(data, crc) ((((crc)<<8) & 0xffff) ^ FLAC__crc16_table[((crc)>>8) ^ (data)])

unsigned FLAC__crc16(const FLAC__byte *data, unsigned len);


/* Will never be emitted for MSVC, GCC, Intel compilers */
static inline unsigned int FLAC__clz_soft_uint32(unsigned int word)
{
    static constexpr unsigned char byte_to_unary_table[] = {
    8, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };

    return (word) > 0xffffff ? byte_to_unary_table[(word) >> 24] :
    (word) > 0xffff ? byte_to_unary_table[(word) >> 16] + 8 :
    (word) > 0xff ? byte_to_unary_table[(word) >> 8] + 16 :
    byte_to_unary_table[(word)] + 24;
}

static inline unsigned int FLAC__clz_uint32(FLAC__uint32 v)
{
/* Never used with input 0 */
    CHOC_ASSERT(v > 0);
#if defined(__INTEL_COMPILER)
    return _bit_scan_reverse(v) ^ 31U;
#elif defined(__GNUC__) && (__GNUC__ >= 4 || (__GNUC__ == 3 && __GNUC_MINOR__ >= 4))
/* This will translate either to (bsr ^ 31U), clz , ctlz, cntlz, lzcnt depending on
 * -march= setting or to a software routine in exotic machines. */
    return __builtin_clz(v);
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
    {
        unsigned long idx;
        _BitScanReverse(&idx, v);
        return idx ^ 31U;
    }
#else
    return FLAC__clz_soft_uint32(v);
#endif
}

/* This one works with input 0 */
static inline unsigned int FLAC__clz2_uint32(FLAC__uint32 v)
{
    if (!v)
        return 32;
    return FLAC__clz_uint32(v);
}

/* An example of what FLAC__bitmath_ilog2() computes:
 *
 * ilog2( 0) = assertion failure
 * ilog2( 1) = 0
 * ilog2( 2) = 1
 * ilog2( 3) = 1
 * ilog2( 4) = 2
 * ilog2( 5) = 2
 * ilog2( 6) = 2
 * ilog2( 7) = 2
 * ilog2( 8) = 3
 * ilog2( 9) = 3
 * ilog2(10) = 3
 * ilog2(11) = 3
 * ilog2(12) = 3
 * ilog2(13) = 3
 * ilog2(14) = 3
 * ilog2(15) = 3
 * ilog2(16) = 4
 * ilog2(17) = 4
 * ilog2(18) = 4
 */

static inline unsigned FLAC__bitmath_ilog2(FLAC__uint32 v)
{
    CHOC_ASSERT(v > 0);
#if defined(__INTEL_COMPILER)
    return _bit_scan_reverse(v);
#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
    {
        unsigned long idx;
        _BitScanReverse(&idx, v);
        return idx;
    }
#else
    return sizeof(FLAC__uint32) * CHAR_BIT  - 1 - FLAC__clz_uint32(v);
#endif
}


typedef double FLAC__double;
typedef float FLAC__float;
/*
 * WATCHOUT: changing FLAC__real will change the signatures of many
 * functions that have assembly language equivalents and break them.
 */
typedef float FLAC__real;


#if HAVE_BSWAP32            /* GCC and Clang */

/* GCC prior to 4.8 didn't provide bswap16 on x86_64 */
#if ! HAVE_BSWAP16
static inline unsigned short __builtin_bswap16(unsigned short a)
{
    return (a<<8)|(a>>8);
}
#endif

#define    FLAC_ENDSWAP_16(x)        (__builtin_bswap16 (x))
#define    FLAC_ENDSWAP_32(x)        (__builtin_bswap32 (x))

#elif defined _MSC_VER        /* Windows. Apparently in <stdlib.h>. */

#define    FLAC_ENDSWAP_16(x)        (_byteswap_ushort (x))
#define    FLAC_ENDSWAP_32(x)        (_byteswap_ulong (x))

#elif defined HAVE_BYTESWAP_H        /* Linux */

#define    FLAC_ENDSWAP_16(x)        (bswap_16 (x))
#define    FLAC_ENDSWAP_32(x)        (bswap_32 (x))

#else

#define    FLAC_ENDSWAP_16(x)        ((((x) >> 8) & 0xFF) | (((x) & 0xFF) << 8))
#define    FLAC_ENDSWAP_32(x)        ((((x) >> 24) & 0xFF) | (((x) >> 8) & 0xFF00) | (((x) & 0xFF00) << 8) | (((x) & 0xFF) << 24))

#endif


/* Host to little-endian byte swapping. */
#if CPU_IS_BIG_ENDIAN

#define FLAC_H2LE_16(x)        FLAC_ENDSWAP_16 (x)
#define FLAC_H2LE_32(x)        FLAC_ENDSWAP_32 (x)

#else

#define FLAC_H2LE_16(x)        (x)
#define FLAC_H2LE_32(x)        (x)

#endif


static constexpr uint32_t FLAC__MAX_APODIZATION_FUNCTIONS = 32;

typedef enum {
    FLAC__APODIZATION_BARTLETT,
    FLAC__APODIZATION_BARTLETT_HANN,
    FLAC__APODIZATION_BLACKMAN,
    FLAC__APODIZATION_BLACKMAN_HARRIS_4TERM_92DB_SIDELOBE,
    FLAC__APODIZATION_CONNES,
    FLAC__APODIZATION_FLATTOP,
    FLAC__APODIZATION_GAUSS,
    FLAC__APODIZATION_HAMMING,
    FLAC__APODIZATION_HANN,
    FLAC__APODIZATION_KAISER_BESSEL,
    FLAC__APODIZATION_NUTTALL,
    FLAC__APODIZATION_RECTANGLE,
    FLAC__APODIZATION_TRIANGLE,
    FLAC__APODIZATION_TUKEY,
    FLAC__APODIZATION_PARTIAL_TUKEY,
    FLAC__APODIZATION_PUNCHOUT_TUKEY,
    FLAC__APODIZATION_WELCH
} FLAC__ApodizationFunction;

typedef struct {
    FLAC__ApodizationFunction type;
    union {
        struct {
            FLAC__real stddev;
        } gauss;
        struct {
            FLAC__real p;
        } tukey;
        struct {
            FLAC__real p;
            FLAC__real start;
            FLAC__real end;
        } multiple_tukey;
    } parameters;
} FLAC__ApodizationSpecification;


typedef struct FLAC__StreamEncoderProtected {
    FLAC__StreamEncoderState state;
    FLAC__bool verify;
    FLAC__bool streamable_subset;
    FLAC__bool do_md5;
    FLAC__bool do_mid_side_stereo;
    FLAC__bool loose_mid_side_stereo;
    unsigned channels;
    unsigned bits_per_sample;
    unsigned sample_rate;
    unsigned blocksize;
    unsigned num_apodizations;
    FLAC__ApodizationSpecification apodizations[FLAC__MAX_APODIZATION_FUNCTIONS];
    unsigned max_lpc_order;
    unsigned qlp_coeff_precision;
    FLAC__bool do_qlp_coeff_prec_search;
    FLAC__bool do_exhaustive_model_search;
    FLAC__bool do_escape_coding;
    unsigned min_residual_partition_order;
    unsigned max_residual_partition_order;
    unsigned rice_parameter_search_dist;
    FLAC__uint64 total_samples_estimate;
    FLAC__StreamMetadata **metadata;
    unsigned num_metadata_blocks;
    FLAC__uint64 streaminfo_offset, seektable_offset, audio_offset;
#if FLAC__HAS_OGG
    FLAC__OggEncoderAspect ogg_encoder_aspect;
#endif
} FLAC__StreamEncoderProtected;


/*
 * This is used to avoid overflow with unusual signals in 32-bit
 * accumulator in the *precompute_partition_info_sums_* functions.
 */
static constexpr uint32_t FLAC__MAX_EXTRA_RESIDUAL_BPS = 4;

#if (defined FLAC__CPU_IA32 || defined FLAC__CPU_X86_64) && defined FLAC__HAS_X86INTRIN

#ifdef FLAC__SSE2_SUPPORTED
extern void FLAC__precompute_partition_info_sums_intrin_sse2(const FLAC__int32 residual[], FLAC__uint64 abs_residual_partition_sums[],
            unsigned residual_samples, unsigned predictor_order, unsigned min_partition_order, unsigned max_partition_order, unsigned bps);
#endif

#ifdef FLAC__SSSE3_SUPPORTED
extern void FLAC__precompute_partition_info_sums_intrin_ssse3(const FLAC__int32 residual[], FLAC__uint64 abs_residual_partition_sums[],
            unsigned residual_samples, unsigned predictor_order, unsigned min_partition_order, unsigned max_partition_order, unsigned bps);
#endif

#ifdef FLAC__AVX2_SUPPORTED
extern void FLAC__precompute_partition_info_sums_intrin_avx2(const FLAC__int32 residual[], FLAC__uint64 abs_residual_partition_sums[],
            unsigned residual_samples, unsigned predictor_order, unsigned min_partition_order, unsigned max_partition_order, unsigned bps);
#endif

#endif


/*
 * opaque structure definition
 */
struct FLAC__BitWriter;
typedef struct FLAC__BitWriter FLAC__BitWriter;

/*
 * construction, deletion, initialization, etc functions
 */
FLAC__BitWriter *FLAC__bitwriter_new(void);
void FLAC__bitwriter_delete(FLAC__BitWriter *bw);
FLAC__bool FLAC__bitwriter_init(FLAC__BitWriter *bw);
void FLAC__bitwriter_free(FLAC__BitWriter *bw); /* does not 'free(buffer)' */
void FLAC__bitwriter_clear(FLAC__BitWriter *bw);
void FLAC__bitwriter_dump(const FLAC__BitWriter *bw, FILE *out);

/*
 * CRC functions
 *
 * non-const *bw because they have to cal FLAC__bitwriter_get_buffer()
 */
FLAC__bool FLAC__bitwriter_get_write_crc16(FLAC__BitWriter *bw, FLAC__uint16 *crc);
FLAC__bool FLAC__bitwriter_get_write_crc8(FLAC__BitWriter *bw, FLAC__byte *crc);

/*
 * info functions
 */
FLAC__bool FLAC__bitwriter_is_byte_aligned(const FLAC__BitWriter *bw);
unsigned FLAC__bitwriter_get_input_bits_unconsumed(const FLAC__BitWriter *bw); /* can be called anytime, returns total # of bits unconsumed */

/*
 * direct buffer access
 *
 * there may be no calls on the bitwriter between get and release.
 * the bitwriter continues to own the returned buffer.
 * before get, bitwriter MUST be byte aligned: check with FLAC__bitwriter_is_byte_aligned()
 */
FLAC__bool FLAC__bitwriter_get_buffer(FLAC__BitWriter *bw, const FLAC__byte **buffer, size_t *bytes);
void FLAC__bitwriter_release_buffer(FLAC__BitWriter *bw);

/*
 * write functions
 */
FLAC__bool FLAC__bitwriter_write_zeroes(FLAC__BitWriter *bw, unsigned bits);
FLAC__bool FLAC__bitwriter_write_raw_uint32(FLAC__BitWriter *bw, FLAC__uint32 val, unsigned bits);
FLAC__bool FLAC__bitwriter_write_raw_int32(FLAC__BitWriter *bw, FLAC__int32 val, unsigned bits);
FLAC__bool FLAC__bitwriter_write_raw_uint64(FLAC__BitWriter *bw, FLAC__uint64 val, unsigned bits);
FLAC__bool FLAC__bitwriter_write_raw_uint32_little_endian(FLAC__BitWriter *bw, FLAC__uint32 val); /*only for bits=32*/
FLAC__bool FLAC__bitwriter_write_byte_block(FLAC__BitWriter *bw, const FLAC__byte vals[], unsigned nvals);
FLAC__bool FLAC__bitwriter_write_unary_unsigned(FLAC__BitWriter *bw, unsigned val);
unsigned FLAC__bitwriter_rice_bits(FLAC__int32 val, unsigned parameter);
FLAC__bool FLAC__bitwriter_write_rice_signed(FLAC__BitWriter *bw, FLAC__int32 val, unsigned parameter);
FLAC__bool FLAC__bitwriter_write_rice_signed_block(FLAC__BitWriter *bw, const FLAC__int32 *vals, unsigned nvals, unsigned parameter);
FLAC__bool FLAC__bitwriter_write_utf8_uint32(FLAC__BitWriter *bw, FLAC__uint32 val);
FLAC__bool FLAC__bitwriter_write_utf8_uint64(FLAC__BitWriter *bw, FLAC__uint64 val);
FLAC__bool FLAC__bitwriter_zero_pad_to_byte_boundary(FLAC__BitWriter *bw);


FLAC__bool FLAC__add_metadata_block(const FLAC__StreamMetadata *metadata, FLAC__BitWriter *bw);
FLAC__bool FLAC__frame_add_header(const FLAC__FrameHeader *header, FLAC__BitWriter *bw);
FLAC__bool FLAC__subframe_add_constant(const FLAC__Subframe_Constant *subframe, unsigned subframe_bps, unsigned wasted_bits, FLAC__BitWriter *bw);
FLAC__bool FLAC__subframe_add_fixed(const FLAC__Subframe_Fixed *subframe, unsigned residual_samples, unsigned subframe_bps, unsigned wasted_bits, FLAC__BitWriter *bw);
FLAC__bool FLAC__subframe_add_lpc(const FLAC__Subframe_LPC *subframe, unsigned residual_samples, unsigned subframe_bps, unsigned wasted_bits, FLAC__BitWriter *bw);
FLAC__bool FLAC__subframe_add_verbatim(const FLAC__Subframe_Verbatim *subframe, unsigned samples, unsigned subframe_bps, unsigned wasted_bits, FLAC__BitWriter *bw);


/*
 *    FLAC__window_*()
 *    --------------------------------------------------------------------
 *    Calculates window coefficients according to different apodization
 *    functions.
 *
 *    OUT window[0,L-1]
 *    IN L (number of points in window)
 */
void FLAC__window_bartlett(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_bartlett_hann(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_blackman(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_blackman_harris_4term_92db_sidelobe(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_connes(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_flattop(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_gauss(FLAC__real *window, const FLAC__int32 L, const FLAC__real stddev); /* 0.0 < stddev <= 0.5 */
void FLAC__window_hamming(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_hann(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_kaiser_bessel(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_nuttall(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_rectangle(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_triangle(FLAC__real *window, const FLAC__int32 L);
void FLAC__window_tukey(FLAC__real *window, const FLAC__int32 L, const FLAC__real p);
void FLAC__window_partial_tukey(FLAC__real *window, const FLAC__int32 L, const FLAC__real p, const FLAC__real start, const FLAC__real end);
void FLAC__window_punchout_tukey(FLAC__real *window, const FLAC__int32 L, const FLAC__real p, const FLAC__real start, const FLAC__real end);
void FLAC__window_welch(FLAC__real *window, const FLAC__int32 L);


typedef struct FLAC__StreamDecoderProtected {
    FLAC__StreamDecoderState state;
    FLAC__StreamDecoderInitStatus initstate;
    unsigned channels;
    FLAC__ChannelAssignment channel_assignment;
    unsigned bits_per_sample;
    unsigned sample_rate; /* in Hz */
    unsigned blocksize; /* in samples (per channel) */
    FLAC__bool md5_checking; /* if true, generate MD5 signature of decoded data and compare against signature in the STREAMINFO metadata block */
#if FLAC__HAS_OGG
    FLAC__OggDecoderAspect ogg_decoder_aspect;
#endif
} FLAC__StreamDecoderProtected;

/*
 * return the number of input bytes consumed
 */
unsigned FLAC__stream_decoder_get_input_bytes_unconsumed(const FLAC__StreamDecoder *decoder);


typedef union {
    FLAC__byte *p8;
    FLAC__int16 *p16;
    FLAC__int32 *p32;
} FLAC__multibyte;

typedef struct {
    FLAC__uint32 in[16];
    FLAC__uint32 buf[4];
    FLAC__uint32 bytes[2];
    FLAC__multibyte internal_buf;
    size_t capacity;
} FLAC__MD5Context;


/*
 *    FLAC__lpc_window_data()
 *    --------------------------------------------------------------------
 *    Applies the given window to the data.
 *  OPT: asm implementation
 *
 *    IN in[0,data_len-1]
 *    IN window[0,data_len-1]
 *    OUT out[0,lag-1]
 *    IN data_len
 */
void FLAC__lpc_window_data(const FLAC__int32 in[], const FLAC__real window[], FLAC__real out[], unsigned data_len);

/*
 *    FLAC__lpc_compute_autocorrelation()
 *    --------------------------------------------------------------------
 *    Compute the autocorrelation for lags between 0 and lag-1.
 *    Assumes data[] outside of [0,data_len-1] == 0.
 *    Asserts that lag > 0.
 *
 *    IN data[0,data_len-1]
 *    IN data_len
 *    IN 0 < lag <= data_len
 *    OUT autoc[0,lag-1]
 */
void FLAC__lpc_compute_autocorrelation(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[]);
#ifndef FLAC__NO_ASM
#  ifdef FLAC__CPU_IA32
#    ifdef FLAC__HAS_NASM
void FLAC__lpc_compute_autocorrelation_asm_ia32(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[]);
void FLAC__lpc_compute_autocorrelation_asm_ia32_sse_lag_4(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[]);
void FLAC__lpc_compute_autocorrelation_asm_ia32_sse_lag_8(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[]);
void FLAC__lpc_compute_autocorrelation_asm_ia32_sse_lag_12(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[]);
void FLAC__lpc_compute_autocorrelation_asm_ia32_sse_lag_16(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[]);
#    endif
#  endif
#  if (defined FLAC__CPU_IA32 || defined FLAC__CPU_X86_64) && defined FLAC__HAS_X86INTRIN
#    ifdef FLAC__SSE_SUPPORTED
void FLAC__lpc_compute_autocorrelation_intrin_sse_lag_4(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[]);
void FLAC__lpc_compute_autocorrelation_intrin_sse_lag_8(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[]);
void FLAC__lpc_compute_autocorrelation_intrin_sse_lag_12(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[]);
void FLAC__lpc_compute_autocorrelation_intrin_sse_lag_16(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[]);
#    endif
#  endif
#endif

/*
 *    FLAC__lpc_compute_lp_coefficients()
 *    --------------------------------------------------------------------
 *    Computes LP coefficients for orders 1..max_order.
 *    Do not call if autoc[0] == 0.0.  This means the signal is zero
 *    and there is no point in calculating a predictor.
 *
 *    IN autoc[0,max_order]                      autocorrelation values
 *    IN 0 < max_order <= FLAC__MAX_LPC_ORDER    max LP order to compute
 *    OUT lp_coeff[0,max_order-1][0,max_order-1] LP coefficients for each order
 *    *** IMPORTANT:
 *    *** lp_coeff[0,max_order-1][max_order,FLAC__MAX_LPC_ORDER-1] are untouched
 *    OUT error[0,max_order-1]                   error for each order (more
 *                                               specifically, the variance of
 *                                               the error signal times # of
 *                                               samples in the signal)
 *
 *    Example: if max_order is 9, the LP coefficients for order 9 will be
 *             in lp_coeff[8][0,8], the LP coefficients for order 8 will be
 *             in lp_coeff[7][0,7], etc.
 */
void FLAC__lpc_compute_lp_coefficients(const FLAC__real autoc[], unsigned *max_order, FLAC__real lp_coeff[][FLAC__MAX_LPC_ORDER], FLAC__double error[]);

/*
 *    FLAC__lpc_quantize_coefficients()
 *    --------------------------------------------------------------------
 *    Quantizes the LP coefficients.  NOTE: precision + bits_per_sample
 *    must be less than 32 (sizeof(FLAC__int32)*8).
 *
 *    IN lp_coeff[0,order-1]    LP coefficients
 *    IN order                  LP order
 *    IN FLAC__MIN_QLP_COEFF_PRECISION < precision
 *                              desired precision (in bits, including sign
 *                              bit) of largest coefficient
 *    OUT qlp_coeff[0,order-1]  quantized coefficients
 *    OUT shift                 # of bits to shift right to get approximated
 *                              LP coefficients.  NOTE: could be negative.
 *    RETURN 0 => quantization OK
 *           1 => coefficients require too much shifting for *shift to
 *              fit in the LPC subframe header.  'shift' is unset.
 *         2 => coefficients are all zero, which is bad.  'shift' is
 *              unset.
 */
int FLAC__lpc_quantize_coefficients(const FLAC__real lp_coeff[], unsigned order, unsigned precision, FLAC__int32 qlp_coeff[], int *shift);

/*
 *    FLAC__lpc_compute_residual_from_qlp_coefficients()
 *    --------------------------------------------------------------------
 *    Compute the residual signal obtained from sutracting the predicted
 *    signal from the original.
 *
 *    IN data[-order,data_len-1] original signal (NOTE THE INDICES!)
 *    IN data_len                length of original signal
 *    IN qlp_coeff[0,order-1]    quantized LP coefficients
 *    IN order > 0               LP order
 *    IN lp_quantization         quantization of LP coefficients in bits
 *    OUT residual[0,data_len-1] residual signal
 */
void FLAC__lpc_compute_residual_from_qlp_coefficients(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
void FLAC__lpc_compute_residual_from_qlp_coefficients_wide(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
#ifndef FLAC__NO_ASM
#  ifdef FLAC__CPU_IA32
#    ifdef FLAC__HAS_NASM
void FLAC__lpc_compute_residual_from_qlp_coefficients_asm_ia32(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
void FLAC__lpc_compute_residual_from_qlp_coefficients_asm_ia32_mmx(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
void FLAC__lpc_compute_residual_from_qlp_coefficients_wide_asm_ia32(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
#    endif
#  endif
#  if (defined FLAC__CPU_IA32 || defined FLAC__CPU_X86_64) && defined FLAC__HAS_X86INTRIN
#    ifdef FLAC__SSE2_SUPPORTED
void FLAC__lpc_compute_residual_from_qlp_coefficients_16_intrin_sse2(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
void FLAC__lpc_compute_residual_from_qlp_coefficients_intrin_sse2(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
#    endif
#    ifdef FLAC__SSE4_1_SUPPORTED
void FLAC__lpc_compute_residual_from_qlp_coefficients_intrin_sse41(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
void FLAC__lpc_compute_residual_from_qlp_coefficients_wide_intrin_sse41(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
#    endif
#    ifdef FLAC__AVX2_SUPPORTED
void FLAC__lpc_compute_residual_from_qlp_coefficients_16_intrin_avx2(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
void FLAC__lpc_compute_residual_from_qlp_coefficients_intrin_avx2(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
void FLAC__lpc_compute_residual_from_qlp_coefficients_wide_intrin_avx2(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
#    endif
#  endif
#endif

/*
 *    FLAC__lpc_restore_signal()
 *    --------------------------------------------------------------------
 *    Restore the original signal by summing the residual and the
 *    predictor.
 *
 *    IN residual[0,data_len-1]  residual signal
 *    IN data_len                length of original signal
 *    IN qlp_coeff[0,order-1]    quantized LP coefficients
 *    IN order > 0               LP order
 *    IN lp_quantization         quantization of LP coefficients in bits
 *    *** IMPORTANT: the caller must pass in the historical samples:
 *    IN  data[-order,-1]        previously-reconstructed historical samples
 *    OUT data[0,data_len-1]     original signal
 */
void FLAC__lpc_restore_signal(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
void FLAC__lpc_restore_signal_wide(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
#ifndef FLAC__NO_ASM
#  ifdef FLAC__CPU_IA32
#    ifdef FLAC__HAS_NASM
void FLAC__lpc_restore_signal_asm_ia32(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
void FLAC__lpc_restore_signal_asm_ia32_mmx(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
void FLAC__lpc_restore_signal_wide_asm_ia32(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
#    endif /* FLAC__HAS_NASM */
#  endif /* FLAC__CPU_IA32 */
#  if (defined FLAC__CPU_IA32 || defined FLAC__CPU_X86_64) && defined FLAC__HAS_X86INTRIN
#    ifdef FLAC__SSE2_SUPPORTED
void FLAC__lpc_restore_signal_16_intrin_sse2(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
#    endif
#    ifdef FLAC__SSE4_1_SUPPORTED
void FLAC__lpc_restore_signal_wide_intrin_sse41(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
#    endif
#  endif
#endif /* FLAC__NO_ASM */

/*
 *    FLAC__lpc_compute_expected_bits_per_residual_sample()
 *    --------------------------------------------------------------------
 *    Compute the expected number of bits per residual signal sample
 *    based on the LP error (which is related to the residual variance).
 *
 *    IN lpc_error >= 0.0   error returned from calculating LP coefficients
 *    IN total_samples > 0  # of samples in residual signal
 *    RETURN                expected bits per sample
 */
FLAC__double FLAC__lpc_compute_expected_bits_per_residual_sample(FLAC__double lpc_error, unsigned total_samples);
FLAC__double FLAC__lpc_compute_expected_bits_per_residual_sample_with_error_scale(FLAC__double lpc_error, FLAC__double error_scale);

unsigned FLAC__format_get_max_rice_partition_order(unsigned blocksize, unsigned predictor_order);
unsigned FLAC__format_get_max_rice_partition_order_from_blocksize(unsigned blocksize);
unsigned FLAC__format_get_max_rice_partition_order_from_blocksize_limited_max_and_predictor_order(unsigned limit, unsigned blocksize, unsigned predictor_order);
void FLAC__format_entropy_coding_method_partitioned_rice_contents_init(FLAC__EntropyCodingMethod_PartitionedRiceContents *object);
void FLAC__format_entropy_coding_method_partitioned_rice_contents_clear(FLAC__EntropyCodingMethod_PartitionedRiceContents *object);
FLAC__bool FLAC__format_entropy_coding_method_partitioned_rice_contents_ensure_size(FLAC__EntropyCodingMethod_PartitionedRiceContents *object, unsigned max_partition_order);


/* An example of what FLAC__bitmath_silog2() computes:
 *
 * silog2(-10) = 5
 * silog2(- 9) = 5
 * silog2(- 8) = 4
 * silog2(- 7) = 4
 * silog2(- 6) = 4
 * silog2(- 5) = 4
 * silog2(- 4) = 3
 * silog2(- 3) = 3
 * silog2(- 2) = 2
 * silog2(- 1) = 2
 * silog2(  0) = 0
 * silog2(  1) = 2
 * silog2(  2) = 3
 * silog2(  3) = 3
 * silog2(  4) = 4
 * silog2(  5) = 4
 * silog2(  6) = 4
 * silog2(  7) = 4
 * silog2(  8) = 5
 * silog2(  9) = 5
 * silog2( 10) = 5
 */
static inline unsigned FLAC__bitmath_silog2(int v)
{
    while(1) {
        if(v == 0) {
            return 0;
        }
        else if(v > 0) {
            unsigned l = 0;
            while(v) {
                l++;
                v >>= 1;
            }
            return l+1;
        }
        else if(v == -1) {
            return 2;
        }
        else {
            v++;
            v = -v;
        }
    }
}

static inline unsigned FLAC__bitmath_silog2_wide(FLAC__int64 v)
{
    while(1) {
        if(v == 0) {
            return 0;
        }
        else if(v > 0) {
            unsigned l = 0;
            while(v) {
                l++;
                v >>= 1;
            }
            return l+1;
        }
        else if(v == -1) {
            return 2;
        }
        else {
            v++;
            v = -v;
        }
    }
}


/* Things should be fastest when this matches the machine word size */
/* WATCHOUT: if you change this you must also change the following #defines down to FLAC__clz_uint32 below to match */
/* WATCHOUT: there are a few places where the code will not work unless uint32_t is >= 32 bits wide */
/*           also, some sections currently only have fast versions for 4 or 8 bytes per word */
#define FLAC__BYTES_PER_WORD 4        /* sizeof uint32_t */
#define FLAC__BITS_PER_WORD (8 * FLAC__BYTES_PER_WORD)
#define FLAC__WORD_ALL_ONES ((FLAC__uint32)0xffffffff)
/* SWAP_BE_WORD_TO_HOST swaps bytes in a uint32_t (which is always big-endian) if necessary to match host byte order */
#if WORDS_BIGENDIAN
#define SWAP_BE_WORD_TO_HOST(x) (x)
#else
#define SWAP_BE_WORD_TO_HOST(x) FLAC_ENDSWAP_32(x)
#endif

/*
 * This should be at least twice as large as the largest number of words
 * required to represent any 'number' (in any encoding) you are going to
 * read.  With FLAC this is on the order of maybe a few hundred bits.
 * If the buffer is smaller than that, the decoder won't be able to read
 * in a whole number that is in a variable length encoding (e.g. Rice).
 * But to be practical it should be at least 1K bytes.
 *
 * Increase this number to decrease the number of read callbacks, at the
 * expense of using more memory.  Or decrease for the reverse effect,
 * keeping in mind the limit from the first paragraph.  The optimal size
 * also depends on the CPU cache size and other factors; some twiddling
 * may be necessary to squeeze out the best performance.
 */
static const unsigned FLAC__BITREADER_DEFAULT_CAPACITY = 65536u / FLAC__BITS_PER_WORD; /* in words */

struct FLAC__BitReader {
    /* any partially-consumed word at the head will stay right-justified as bits are consumed from the left */
    /* any incomplete word at the tail will be left-justified, and bytes from the read callback are added on the right */
    uint32_t *buffer;
    unsigned capacity; /* in words */
    unsigned words; /* # of completed words in buffer */
    unsigned bytes; /* # of bytes in incomplete word at buffer[words] */
    unsigned consumed_words; /* #words ... */
    unsigned consumed_bits; /* ... + (#bits of head word) already consumed from the front of buffer */
    unsigned read_crc16; /* the running frame CRC */
    unsigned crc16_align; /* the number of bits in the current consumed word that should not be CRC'd */
    FLAC__BitReaderReadCallback read_callback;
    void *client_data;
};

static inline void crc16_update_word_(FLAC__BitReader *br, uint32_t word)
{
    unsigned crc = br->read_crc16;
#if FLAC__BYTES_PER_WORD == 4
    switch(br->crc16_align) {
        case  0: crc = FLAC__CRC16_UPDATE((unsigned)(word >> 24), crc);
        case  8: crc = FLAC__CRC16_UPDATE((unsigned)((word >> 16) & 0xff), crc);
        case 16: crc = FLAC__CRC16_UPDATE((unsigned)((word >> 8) & 0xff), crc);
        case 24: br->read_crc16 = FLAC__CRC16_UPDATE((unsigned)(word & 0xff), crc);
    }
#elif FLAC__BYTES_PER_WORD == 8
    switch(br->crc16_align) {
        case  0: crc = FLAC__CRC16_UPDATE((unsigned)(word >> 56), crc);
        case  8: crc = FLAC__CRC16_UPDATE((unsigned)((word >> 48) & 0xff), crc);
        case 16: crc = FLAC__CRC16_UPDATE((unsigned)((word >> 40) & 0xff), crc);
        case 24: crc = FLAC__CRC16_UPDATE((unsigned)((word >> 32) & 0xff), crc);
        case 32: crc = FLAC__CRC16_UPDATE((unsigned)((word >> 24) & 0xff), crc);
        case 40: crc = FLAC__CRC16_UPDATE((unsigned)((word >> 16) & 0xff), crc);
        case 48: crc = FLAC__CRC16_UPDATE((unsigned)((word >> 8) & 0xff), crc);
        case 56: br->read_crc16 = FLAC__CRC16_UPDATE((unsigned)(word & 0xff), crc);
    }
#else
    for( ; br->crc16_align < FLAC__BITS_PER_WORD; br->crc16_align += 8)
        crc = FLAC__CRC16_UPDATE((unsigned)((word >> (FLAC__BITS_PER_WORD-8-br->crc16_align)) & 0xff), crc);
    br->read_crc16 = crc;
#endif
    br->crc16_align = 0;
}

static inline FLAC__bool bitreader_read_from_client_(FLAC__BitReader *br)
{
    unsigned start, end;
    size_t bytes;
    FLAC__byte *target;

    /* first shift the unconsumed buffer data toward the front as much as possible */
    if(br->consumed_words > 0) {
        start = br->consumed_words;
        end = br->words + (br->bytes? 1:0);
        memmove(br->buffer, br->buffer+start, FLAC__BYTES_PER_WORD * (end - start));

        br->words -= start;
        br->consumed_words = 0;
    }

    /*
     * set the target for reading, taking into account word alignment and endianness
     */
    bytes = (br->capacity - br->words) * FLAC__BYTES_PER_WORD - br->bytes;
    if(bytes == 0)
        return false; /* no space left, buffer is too small; see note for FLAC__BITREADER_DEFAULT_CAPACITY  */
    target = ((FLAC__byte*)(br->buffer+br->words)) + br->bytes;

    /* before reading, if the existing reader looks like this (say uint32_t is 32 bits wide)
     *   bitstream :  11 22 33 44 55            br->words=1 br->bytes=1 (partial tail word is left-justified)
     *   buffer[BE]:  11 22 33 44 55 ?? ?? ??   (shown layed out as bytes sequentially in memory)
     *   buffer[LE]:  44 33 22 11 ?? ?? ?? 55   (?? being don't-care)
     *                               ^^-------target, bytes=3
     * on LE machines, have to byteswap the odd tail word so nothing is
     * overwritten:
     */
#if WORDS_BIGENDIAN
#else
    if(br->bytes)
        br->buffer[br->words] = SWAP_BE_WORD_TO_HOST(br->buffer[br->words]);
#endif

    /* now it looks like:
     *   bitstream :  11 22 33 44 55            br->words=1 br->bytes=1
     *   buffer[BE]:  11 22 33 44 55 ?? ?? ??
     *   buffer[LE]:  44 33 22 11 55 ?? ?? ??
     *                               ^^-------target, bytes=3
     */

    /* read in the data; note that the callback may return a smaller number of bytes */
    if(!br->read_callback(target, &bytes, br->client_data))
        return false;

    /* after reading bytes 66 77 88 99 AA BB CC DD EE FF from the client:
     *   bitstream :  11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF
     *   buffer[BE]:  11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF ??
     *   buffer[LE]:  44 33 22 11 55 66 77 88 99 AA BB CC DD EE FF ??
     * now have to byteswap on LE machines:
     */
#if WORDS_BIGENDIAN
#else
    end = (br->words*FLAC__BYTES_PER_WORD + br->bytes + bytes + (FLAC__BYTES_PER_WORD-1)) / FLAC__BYTES_PER_WORD;
    for(start = br->words; start < end; start++)
        br->buffer[start] = SWAP_BE_WORD_TO_HOST(br->buffer[start]);
#endif

    /* now it looks like:
     *   bitstream :  11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF
     *   buffer[BE]:  11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF ??
     *   buffer[LE]:  44 33 22 11 88 77 66 55 CC BB AA 99 ?? FF EE DD
     * finally we'll update the reader values:
     */
    end = br->words*FLAC__BYTES_PER_WORD + br->bytes + bytes;
    br->words = end / FLAC__BYTES_PER_WORD;
    br->bytes = end % FLAC__BYTES_PER_WORD;

    return true;
}

/***********************************************************************
 *
 * Class constructor/destructor
 *
 ***********************************************************************/

inline FLAC__BitReader *FLAC__bitreader_new(void)
{
    FLAC__BitReader *br = (FLAC__BitReader*) calloc(1, sizeof(FLAC__BitReader));

    /* calloc() implies:
        memset(br, 0, sizeof(FLAC__BitReader));
        br->buffer = 0;
        br->capacity = 0;
        br->words = br->bytes = 0;
        br->consumed_words = br->consumed_bits = 0;
        br->read_callback = 0;
        br->client_data = 0;
    */
    return br;
}

inline void FLAC__bitreader_delete(FLAC__BitReader *br)
{
    CHOC_ASSERT(0 != br);

    FLAC__bitreader_free(br);
    free(br);
}

/***********************************************************************
 *
 * Public class methods
 *
 ***********************************************************************/

inline FLAC__bool FLAC__bitreader_init(FLAC__BitReader *br, FLAC__BitReaderReadCallback rcb, void *cd)
{
    CHOC_ASSERT(0 != br);

    br->words = br->bytes = 0;
    br->consumed_words = br->consumed_bits = 0;
    br->capacity = FLAC__BITREADER_DEFAULT_CAPACITY;
    br->buffer = (uint32_t*) malloc(sizeof(uint32_t) * br->capacity);
    if(br->buffer == 0)
        return false;
    br->read_callback = rcb;
    br->client_data = cd;

    return true;
}

inline void FLAC__bitreader_free(FLAC__BitReader *br)
{
    CHOC_ASSERT(0 != br);

    if(0 != br->buffer)
        free(br->buffer);
    br->buffer = 0;
    br->capacity = 0;
    br->words = br->bytes = 0;
    br->consumed_words = br->consumed_bits = 0;
    br->read_callback = 0;
    br->client_data = 0;
}

inline FLAC__bool FLAC__bitreader_clear(FLAC__BitReader *br)
{
    br->words = br->bytes = 0;
    br->consumed_words = br->consumed_bits = 0;
    return true;
}

inline void FLAC__bitreader_dump(const FLAC__BitReader *br, FILE *out)
{
    unsigned i, j;
    if(br == 0) {
        fprintf(out, "bitreader is NULL\n");
    }
    else {
        fprintf(out, "bitreader: capacity=%u words=%u bytes=%u consumed: words=%u, bits=%u\n", br->capacity, br->words, br->bytes, br->consumed_words, br->consumed_bits);

        for(i = 0; i < br->words; i++) {
            fprintf(out, "%08X: ", i);
            for(j = 0; j < FLAC__BITS_PER_WORD; j++)
                if(i < br->consumed_words || (i == br->consumed_words && j < br->consumed_bits))
                    fprintf(out, ".");
                else
                    fprintf(out, "%01u", br->buffer[i] & (1 << (FLAC__BITS_PER_WORD-j-1)) ? 1:0);
            fprintf(out, "\n");
        }
        if(br->bytes > 0) {
            fprintf(out, "%08X: ", i);
            for(j = 0; j < br->bytes*8; j++)
                if(i < br->consumed_words || (i == br->consumed_words && j < br->consumed_bits))
                    fprintf(out, ".");
                else
                    fprintf(out, "%01u", br->buffer[i] & (1 << (br->bytes*8-j-1)) ? 1:0);
            fprintf(out, "\n");
        }
    }
}

inline void FLAC__bitreader_reset_read_crc16(FLAC__BitReader *br, FLAC__uint16 seed)
{
    CHOC_ASSERT(0 != br);
    CHOC_ASSERT(0 != br->buffer);
    CHOC_ASSERT((br->consumed_bits & 7) == 0);

    br->read_crc16 = (unsigned)seed;
    br->crc16_align = br->consumed_bits;
}

inline FLAC__uint16 FLAC__bitreader_get_read_crc16(FLAC__BitReader *br)
{
    CHOC_ASSERT(0 != br);
    CHOC_ASSERT(0 != br->buffer);
    CHOC_ASSERT((br->consumed_bits & 7) == 0);
    CHOC_ASSERT(br->crc16_align <= br->consumed_bits);

    /* CRC any tail bytes in a partially-consumed word */
    if(br->consumed_bits) {
        const uint32_t tail = br->buffer[br->consumed_words];
        for( ; br->crc16_align < br->consumed_bits; br->crc16_align += 8)
            br->read_crc16 = FLAC__CRC16_UPDATE((unsigned)((tail >> (FLAC__BITS_PER_WORD-8-br->crc16_align)) & 0xff), br->read_crc16);
    }
    return br->read_crc16;
}

inline FLAC__bool FLAC__bitreader_is_consumed_byte_aligned(const FLAC__BitReader *br)
{
    return ((br->consumed_bits & 7) == 0);
}

inline unsigned FLAC__bitreader_bits_left_for_byte_alignment(const FLAC__BitReader *br)
{
    return 8 - (br->consumed_bits & 7);
}

inline unsigned FLAC__bitreader_get_input_bits_unconsumed(const FLAC__BitReader *br)
{
    return (br->words-br->consumed_words)*FLAC__BITS_PER_WORD + br->bytes*8 - br->consumed_bits;
}

static inline FLAC__bool FLAC__bitreader_read_raw_uint32(FLAC__BitReader *br, FLAC__uint32 *val, unsigned bits)
{
    CHOC_ASSERT(0 != br);
    CHOC_ASSERT(0 != br->buffer);

    CHOC_ASSERT(bits <= 32);
    CHOC_ASSERT((br->capacity*FLAC__BITS_PER_WORD) * 2 >= bits);
    CHOC_ASSERT(br->consumed_words <= br->words);

    /* WATCHOUT: code does not work with <32bit words; we can make things much faster with this assertion */
    CHOC_ASSERT(FLAC__BITS_PER_WORD >= 32);

    if(bits == 0) { /* OPT: investigate if this can ever happen, maybe change to assertion */
        *val = 0;
        return true;
    }

    while((br->words-br->consumed_words)*FLAC__BITS_PER_WORD + br->bytes*8 - br->consumed_bits < bits) {
        if(!bitreader_read_from_client_(br))
            return false;
    }
    if(br->consumed_words < br->words) { /* if we've not consumed up to a partial tail word... */
        /* OPT: taking out the consumed_bits==0 "else" case below might make things faster if less code allows the compiler to inline this function */
        if(br->consumed_bits) {
            /* this also works when consumed_bits==0, it's just a little slower than necessary for that case */
            const unsigned n = FLAC__BITS_PER_WORD - br->consumed_bits;
            const uint32_t word = br->buffer[br->consumed_words];
            if(bits < n) {
                *val = (word & (FLAC__WORD_ALL_ONES >> br->consumed_bits)) >> (n-bits);
                br->consumed_bits += bits;
                return true;
            }
            *val = word & (FLAC__WORD_ALL_ONES >> br->consumed_bits);
            bits -= n;
            crc16_update_word_(br, word);
            br->consumed_words++;
            br->consumed_bits = 0;
            if(bits) { /* if there are still bits left to read, there have to be less than 32 so they will all be in the next word */
                *val <<= bits;
                *val |= (br->buffer[br->consumed_words] >> (FLAC__BITS_PER_WORD-bits));
                br->consumed_bits = bits;
            }
            return true;
        }
        else {
            const uint32_t word = br->buffer[br->consumed_words];
            if(bits < FLAC__BITS_PER_WORD) {
                *val = word >> (FLAC__BITS_PER_WORD-bits);
                br->consumed_bits = bits;
                return true;
            }
            /* at this point 'bits' must be == FLAC__BITS_PER_WORD; because of previous assertions, it can't be larger */
            *val = word;
            crc16_update_word_(br, word);
            br->consumed_words++;
            return true;
        }
    }
    else {
        /* in this case we're starting our read at a partial tail word;
         * the reader has guaranteed that we have at least 'bits' bits
         * available to read, which makes this case simpler.
         */
        /* OPT: taking out the consumed_bits==0 "else" case below might make things faster if less code allows the compiler to inline this function */
        if(br->consumed_bits) {
            /* this also works when consumed_bits==0, it's just a little slower than necessary for that case */
            CHOC_ASSERT(br->consumed_bits + bits <= br->bytes*8);
            *val = (br->buffer[br->consumed_words] & (FLAC__WORD_ALL_ONES >> br->consumed_bits)) >> (FLAC__BITS_PER_WORD-br->consumed_bits-bits);
            br->consumed_bits += bits;
            return true;
        }
        else {
            *val = br->buffer[br->consumed_words] >> (FLAC__BITS_PER_WORD-bits);
            br->consumed_bits += bits;
            return true;
        }
    }
}

inline FLAC__bool FLAC__bitreader_read_raw_int32(FLAC__BitReader *br, FLAC__int32 *val, unsigned bits)
{
    /* OPT: inline raw uint32 code here, or make into a macro if possible in the .h file */
    if(!FLAC__bitreader_read_raw_uint32(br, (FLAC__uint32*)val, bits))
        return false;
    /* sign-extend: */
    *val <<= (32-bits);
    *val >>= (32-bits);
    return true;
}

inline FLAC__bool FLAC__bitreader_read_raw_uint64(FLAC__BitReader *br, FLAC__uint64 *val, unsigned bits)
{
    FLAC__uint32 hi, lo;

    if(bits > 32) {
        if(!FLAC__bitreader_read_raw_uint32(br, &hi, bits-32))
            return false;
        if(!FLAC__bitreader_read_raw_uint32(br, &lo, 32))
            return false;
        *val = hi;
        *val <<= 32;
        *val |= lo;
    }
    else {
        if(!FLAC__bitreader_read_raw_uint32(br, &lo, bits))
            return false;
        *val = lo;
    }
    return true;
}

inline FLAC__bool FLAC__bitreader_read_uint32_little_endian(FLAC__BitReader *br, FLAC__uint32 *val)
{
    FLAC__uint32 x8, x32 = 0;

    /* this doesn't need to be that fast as currently it is only used for vorbis comments */

    if(!FLAC__bitreader_read_raw_uint32(br, &x32, 8))
        return false;

    if(!FLAC__bitreader_read_raw_uint32(br, &x8, 8))
        return false;
    x32 |= (x8 << 8);

    if(!FLAC__bitreader_read_raw_uint32(br, &x8, 8))
        return false;
    x32 |= (x8 << 16);

    if(!FLAC__bitreader_read_raw_uint32(br, &x8, 8))
        return false;
    x32 |= (x8 << 24);

    *val = x32;
    return true;
}

inline FLAC__bool FLAC__bitreader_skip_bits_no_crc(FLAC__BitReader *br, unsigned bits)
{
    /*
     * OPT: a faster implementation is possible but probably not that useful
     * since this is only called a couple of times in the metadata readers.
     */
    CHOC_ASSERT(0 != br);
    CHOC_ASSERT(0 != br->buffer);

    if(bits > 0) {
        const unsigned n = br->consumed_bits & 7;
        unsigned m;
        FLAC__uint32 x;

        if(n != 0) {
            m = std::min(8-n, bits);
            if(!FLAC__bitreader_read_raw_uint32(br, &x, m))
                return false;
            bits -= m;
        }
        m = bits / 8;
        if(m > 0) {
            if(!FLAC__bitreader_skip_byte_block_aligned_no_crc(br, m))
                return false;
            bits %= 8;
        }
        if(bits > 0) {
            if(!FLAC__bitreader_read_raw_uint32(br, &x, bits))
                return false;
        }
    }

    return true;
}

inline FLAC__bool FLAC__bitreader_skip_byte_block_aligned_no_crc(FLAC__BitReader *br, unsigned nvals)
{
    FLAC__uint32 x;

    CHOC_ASSERT(0 != br);
    CHOC_ASSERT(0 != br->buffer);
    CHOC_ASSERT(FLAC__bitreader_is_consumed_byte_aligned(br));

    /* step 1: skip over partial head word to get word aligned */
    while(nvals && br->consumed_bits) { /* i.e. run until we read 'nvals' bytes or we hit the end of the head word */
        if(!FLAC__bitreader_read_raw_uint32(br, &x, 8))
            return false;
        nvals--;
    }
    if(0 == nvals)
        return true;
    /* step 2: skip whole words in chunks */
    while(nvals >= FLAC__BYTES_PER_WORD) {
        if(br->consumed_words < br->words) {
            br->consumed_words++;
            nvals -= FLAC__BYTES_PER_WORD;
        }
        else if(!bitreader_read_from_client_(br))
            return false;
    }
    /* step 3: skip any remainder from partial tail bytes */
    while(nvals) {
        if(!FLAC__bitreader_read_raw_uint32(br, &x, 8))
            return false;
        nvals--;
    }

    return true;
}

inline FLAC__bool FLAC__bitreader_read_byte_block_aligned_no_crc(FLAC__BitReader *br, FLAC__byte *val, unsigned nvals)
{
    FLAC__uint32 x;

    CHOC_ASSERT(0 != br);
    CHOC_ASSERT(0 != br->buffer);
    CHOC_ASSERT(FLAC__bitreader_is_consumed_byte_aligned(br));

    /* step 1: read from partial head word to get word aligned */
    while(nvals && br->consumed_bits) { /* i.e. run until we read 'nvals' bytes or we hit the end of the head word */
        if(!FLAC__bitreader_read_raw_uint32(br, &x, 8))
            return false;
        *val++ = (FLAC__byte)x;
        nvals--;
    }
    if(0 == nvals)
        return true;
    /* step 2: read whole words in chunks */
    while(nvals >= FLAC__BYTES_PER_WORD) {
        if(br->consumed_words < br->words) {
            const uint32_t word = br->buffer[br->consumed_words++];
#if FLAC__BYTES_PER_WORD == 4
            val[0] = (FLAC__byte)(word >> 24);
            val[1] = (FLAC__byte)(word >> 16);
            val[2] = (FLAC__byte)(word >> 8);
            val[3] = (FLAC__byte)word;
#elif FLAC__BYTES_PER_WORD == 8
            val[0] = (FLAC__byte)(word >> 56);
            val[1] = (FLAC__byte)(word >> 48);
            val[2] = (FLAC__byte)(word >> 40);
            val[3] = (FLAC__byte)(word >> 32);
            val[4] = (FLAC__byte)(word >> 24);
            val[5] = (FLAC__byte)(word >> 16);
            val[6] = (FLAC__byte)(word >> 8);
            val[7] = (FLAC__byte)word;
#else
            for(x = 0; x < FLAC__BYTES_PER_WORD; x++)
                val[x] = (FLAC__byte)(word >> (8*(FLAC__BYTES_PER_WORD-x-1)));
#endif
            val += FLAC__BYTES_PER_WORD;
            nvals -= FLAC__BYTES_PER_WORD;
        }
        else if(!bitreader_read_from_client_(br))
            return false;
    }
    /* step 3: read any remainder from partial tail bytes */
    while(nvals) {
        if(!FLAC__bitreader_read_raw_uint32(br, &x, 8))
            return false;
        *val++ = (FLAC__byte)x;
        nvals--;
    }

    return true;
}

inline FLAC__bool FLAC__bitreader_read_unary_unsigned(FLAC__BitReader *br, unsigned *val)
{
    unsigned i;

    CHOC_ASSERT(0 != br);
    CHOC_ASSERT(0 != br->buffer);

    *val = 0;
    while(1) {
        while(br->consumed_words < br->words) { /* if we've not consumed up to a partial tail word... */
            uint32_t b = br->buffer[br->consumed_words] << br->consumed_bits;
            if(b) {
                i = FLAC__clz_uint32(b);
                *val += i;
                i++;
                br->consumed_bits += i;
                if(br->consumed_bits >= FLAC__BITS_PER_WORD) { /* faster way of testing if(br->consumed_bits == FLAC__BITS_PER_WORD) */
                    crc16_update_word_(br, br->buffer[br->consumed_words]);
                    br->consumed_words++;
                    br->consumed_bits = 0;
                }
                return true;
            }
            else {
                *val += FLAC__BITS_PER_WORD - br->consumed_bits;
                crc16_update_word_(br, br->buffer[br->consumed_words]);
                br->consumed_words++;
                br->consumed_bits = 0;
                /* didn't find stop bit yet, have to keep going... */
            }
        }
        /* at this point we've eaten up all the whole words; have to try
         * reading through any tail bytes before calling the read callback.
         * this is a repeat of the above logic adjusted for the fact we
         * don't have a whole word.  note though if the client is feeding
         * us data a byte at a time (unlikely), br->consumed_bits may not
         * be zero.
         */
        if(br->bytes*8 > br->consumed_bits) {
            const unsigned end = br->bytes * 8;
            uint32_t b = (br->buffer[br->consumed_words] & (FLAC__WORD_ALL_ONES << (FLAC__BITS_PER_WORD-end))) << br->consumed_bits;
            if(b) {
                i = FLAC__clz_uint32(b);
                *val += i;
                i++;
                br->consumed_bits += i;
                CHOC_ASSERT(br->consumed_bits < FLAC__BITS_PER_WORD);
                return true;
            }
            else {
                *val += end - br->consumed_bits;
                br->consumed_bits = end;
                CHOC_ASSERT(br->consumed_bits < FLAC__BITS_PER_WORD);
                /* didn't find stop bit yet, have to keep going... */
            }
        }
        if(!bitreader_read_from_client_(br))
            return false;
    }
}

inline FLAC__bool FLAC__bitreader_read_rice_signed(FLAC__BitReader *br, int *val, unsigned parameter)
{
    FLAC__uint32 lsbs = 0, msbs = 0;
    unsigned uval;

    CHOC_ASSERT(0 != br);
    CHOC_ASSERT(0 != br->buffer);
    CHOC_ASSERT(parameter <= 31);

    /* read the unary MSBs and end bit */
    if(!FLAC__bitreader_read_unary_unsigned(br, &msbs))
        return false;

    /* read the binary LSBs */
    if(!FLAC__bitreader_read_raw_uint32(br, &lsbs, parameter))
        return false;

    /* compose the value */
    uval = (msbs << parameter) | lsbs;
    if(uval & 1)
        *val = -((int)(uval >> 1)) - 1;
    else
        *val = (int)(uval >> 1);

    return true;
}

/* this is by far the most heavily used reader call.  it ain't pretty but it's fast */
inline FLAC__bool FLAC__bitreader_read_rice_signed_block(FLAC__BitReader *br, int vals[], unsigned nvals, unsigned parameter)
{
    /* try and get br->consumed_words and br->consumed_bits into register;
     * must remember to flush them back to *br before calling other
     * bitreader functions that use them, and before returning */
    unsigned cwords, words, lsbs, msbs, x, y;
    unsigned ucbits; /* keep track of the number of unconsumed bits in word */
    uint32_t b;
    int *val, *end;

    CHOC_ASSERT(0 != br);
    CHOC_ASSERT(0 != br->buffer);
    /* WATCHOUT: code does not work with <32bit words; we can make things much faster with this assertion */
    CHOC_ASSERT(FLAC__BITS_PER_WORD >= 32);
    CHOC_ASSERT(parameter < 32);
    /* the above two asserts also guarantee that the binary part never straddles more than 2 words, so we don't have to loop to read it */

    val = vals;
    end = vals + nvals;

    if(parameter == 0) {
        while(val < end) {
            /* read the unary MSBs and end bit */
            if(!FLAC__bitreader_read_unary_unsigned(br, &msbs))
                return false;

            *val++ = (int)(msbs >> 1) ^ -(int)(msbs & 1);
        }

        return true;
    }

    CHOC_ASSERT(parameter > 0);

    cwords = br->consumed_words;
    words = br->words;

    /* if we've not consumed up to a partial tail word... */
    if(cwords >= words) {
        x = 0;
        goto process_tail;
    }

    ucbits = FLAC__BITS_PER_WORD - br->consumed_bits;
    b = br->buffer[cwords] << br->consumed_bits;  /* keep unconsumed bits aligned to left */

    while(val < end) {
        /* read the unary MSBs and end bit */
        x = y = FLAC__clz2_uint32(b);
        if(x == FLAC__BITS_PER_WORD) {
            x = ucbits;
            do {
                /* didn't find stop bit yet, have to keep going... */
                crc16_update_word_(br, br->buffer[cwords++]);
                if (cwords >= words)
                    goto incomplete_msbs;
                b = br->buffer[cwords];
                y = FLAC__clz2_uint32(b);
                x += y;
            } while(y == FLAC__BITS_PER_WORD);
        }
        b <<= y;
        b <<= 1; /* account for stop bit */
        ucbits = (ucbits - x - 1) % FLAC__BITS_PER_WORD;
        msbs = x;

        /* read the binary LSBs */
        x = b >> (FLAC__BITS_PER_WORD - parameter);
        if(parameter <= ucbits) {
            ucbits -= parameter;
            b <<= parameter;
        } else {
            /* there are still bits left to read, they will all be in the next word */
            crc16_update_word_(br, br->buffer[cwords++]);
            if (cwords >= words)
                goto incomplete_lsbs;
            b = br->buffer[cwords];
            ucbits += FLAC__BITS_PER_WORD - parameter;
            x |= b >> ucbits;
            b <<= FLAC__BITS_PER_WORD - ucbits;
        }
        lsbs = x;

        /* compose the value */
        x = (msbs << parameter) | lsbs;
        *val++ = (int)(x >> 1) ^ -(int)(x & 1);

        continue;

        /* at this point we've eaten up all the whole words */
process_tail:
        do {
            if(0) {
incomplete_msbs:
                br->consumed_bits = 0;
                br->consumed_words = cwords;
            }

            /* read the unary MSBs and end bit */
            if(!FLAC__bitreader_read_unary_unsigned(br, &msbs))
                return false;
            msbs += x;
            x = ucbits = 0;

            if(0) {
incomplete_lsbs:
                br->consumed_bits = 0;
                br->consumed_words = cwords;
            }

            /* read the binary LSBs */
            if(!FLAC__bitreader_read_raw_uint32(br, &lsbs, parameter - ucbits))
                return false;
            lsbs = x | lsbs;

            /* compose the value */
            x = (msbs << parameter) | lsbs;
            *val++ = (int)(x >> 1) ^ -(int)(x & 1);
            x = 0;

            cwords = br->consumed_words;
            words = br->words;
            ucbits = FLAC__BITS_PER_WORD - br->consumed_bits;
            b = br->buffer[cwords] << br->consumed_bits;
        } while(cwords >= words && val < end);
    }

    if(ucbits == 0 && cwords < words) {
        /* don't leave the head word with no unconsumed bits */
        crc16_update_word_(br, br->buffer[cwords++]);
        ucbits = FLAC__BITS_PER_WORD;
    }

    br->consumed_bits = FLAC__BITS_PER_WORD - ucbits;
    br->consumed_words = cwords;

    return true;
}

/* on return, if *val == 0xffffffff then the utf-8 sequence was invalid, but the return value will be true */
inline FLAC__bool FLAC__bitreader_read_utf8_uint32(FLAC__BitReader *br, FLAC__uint32 *val, FLAC__byte *raw, unsigned *rawlen)
{
    FLAC__uint32 v = 0;
    FLAC__uint32 x;
    unsigned i;

    if(!FLAC__bitreader_read_raw_uint32(br, &x, 8))
        return false;
    if(raw)
        raw[(*rawlen)++] = (FLAC__byte)x;
    if(!(x & 0x80)) { /* 0xxxxxxx */
        v = x;
        i = 0;
    }
    else if(x & 0xC0 && !(x & 0x20)) { /* 110xxxxx */
        v = x & 0x1F;
        i = 1;
    }
    else if(x & 0xE0 && !(x & 0x10)) { /* 1110xxxx */
        v = x & 0x0F;
        i = 2;
    }
    else if(x & 0xF0 && !(x & 0x08)) { /* 11110xxx */
        v = x & 0x07;
        i = 3;
    }
    else if(x & 0xF8 && !(x & 0x04)) { /* 111110xx */
        v = x & 0x03;
        i = 4;
    }
    else if(x & 0xFC && !(x & 0x02)) { /* 1111110x */
        v = x & 0x01;
        i = 5;
    }
    else {
        *val = 0xffffffff;
        return true;
    }
    for( ; i; i--) {
        if(!FLAC__bitreader_read_raw_uint32(br, &x, 8))
            return false;
        if(raw)
            raw[(*rawlen)++] = (FLAC__byte)x;
        if(!(x & 0x80) || (x & 0x40)) { /* 10xxxxxx */
            *val = 0xffffffff;
            return true;
        }
        v <<= 6;
        v |= (x & 0x3F);
    }
    *val = v;
    return true;
}

/* on return, if *val == 0xffffffffffffffff then the utf-8 sequence was invalid, but the return value will be true */
inline FLAC__bool FLAC__bitreader_read_utf8_uint64(FLAC__BitReader *br, FLAC__uint64 *val, FLAC__byte *raw, unsigned *rawlen)
{
    FLAC__uint64 v = 0;
    FLAC__uint32 x;
    unsigned i;

    if(!FLAC__bitreader_read_raw_uint32(br, &x, 8))
        return false;
    if(raw)
        raw[(*rawlen)++] = (FLAC__byte)x;
    if(!(x & 0x80)) { /* 0xxxxxxx */
        v = x;
        i = 0;
    }
    else if(x & 0xC0 && !(x & 0x20)) { /* 110xxxxx */
        v = x & 0x1F;
        i = 1;
    }
    else if(x & 0xE0 && !(x & 0x10)) { /* 1110xxxx */
        v = x & 0x0F;
        i = 2;
    }
    else if(x & 0xF0 && !(x & 0x08)) { /* 11110xxx */
        v = x & 0x07;
        i = 3;
    }
    else if(x & 0xF8 && !(x & 0x04)) { /* 111110xx */
        v = x & 0x03;
        i = 4;
    }
    else if(x & 0xFC && !(x & 0x02)) { /* 1111110x */
        v = x & 0x01;
        i = 5;
    }
    else if(x & 0xFE && !(x & 0x01)) { /* 11111110 */
        v = 0;
        i = 6;
    }
    else {
        *val = FLAC__U64L(0xffffffffffffffff);
        return true;
    }
    for( ; i; i--) {
        if(!FLAC__bitreader_read_raw_uint32(br, &x, 8))
            return false;
        if(raw)
            raw[(*rawlen)++] = (FLAC__byte)x;
        if(!(x & 0x80) || (x & 0x40)) { /* 10xxxxxx */
            *val = FLAC__U64L(0xffffffffffffffff);
            return true;
        }
        v <<= 6;
        v |= (x & 0x3F);
    }
    *val = v;
    return true;
}

/* These functions are declared inline in this file but are also callable as
 * externs from elsewhere.
 * According to the C99 spec, section 6.7.4, simply providing a function
 * prototype in a header file without 'inline' and making the function inline
 * in this file should be sufficient.
 * Unfortunately, the Microsoft VS compiler doesn't pick them up externally. To
 * fix that we add extern declarations here.
 */
extern FLAC__bool FLAC__bitreader_is_consumed_byte_aligned(const FLAC__BitReader *br);
extern unsigned FLAC__bitreader_bits_left_for_byte_alignment(const FLAC__BitReader *br);
extern unsigned FLAC__bitreader_get_input_bits_unconsumed(const FLAC__BitReader *br);
extern FLAC__bool FLAC__bitreader_read_uint32_little_endian(FLAC__BitReader *br, FLAC__uint32 *val);


/* Things should be fastest when this matches the machine word size */
/* WATCHOUT: if you change this you must also change the following #defines down to SWAP_BE_WORD_TO_HOST below to match */
/* WATCHOUT: there are a few places where the code will not work unless uint32_t is >= 32 bits wide */
#define FLAC__BYTES_PER_WORD 4
#define FLAC__BITS_PER_WORD (8 * FLAC__BYTES_PER_WORD)
#define FLAC__WORD_ALL_ONES ((FLAC__uint32)0xffffffff)
/* SWAP_BE_WORD_TO_HOST swaps bytes in a uint32_t (which is always big-endian) if necessary to match host byte order */
#if WORDS_BIGENDIAN
#define SWAP_BE_WORD_TO_HOST(x) (x)
#else
#define SWAP_BE_WORD_TO_HOST(x) FLAC_ENDSWAP_32(x)
#endif

/*
 * The default capacity here doesn't matter too much.  The buffer always grows
 * to hold whatever is written to it.  Usually the encoder will stop adding at
 * a frame or metadata block, then write that out and clear the buffer for the
 * next one.
 */
static const unsigned FLAC__BITWRITER_DEFAULT_CAPACITY = 32768u / sizeof(uint32_t); /* size in words */
/* When growing, increment 4K at a time */
static const unsigned FLAC__BITWRITER_DEFAULT_INCREMENT = 4096u / sizeof(uint32_t); /* size in words */

#define FLAC__WORDS_TO_BITS(words) ((words) * FLAC__BITS_PER_WORD)
#define FLAC__TOTAL_BITS(bw) (FLAC__WORDS_TO_BITS((bw)->words) + (bw)->bits)

struct FLAC__BitWriter {
    uint32_t *buffer;
    uint32_t accum; /* accumulator; bits are right-justified; when full, accum is appended to buffer */
    unsigned capacity; /* capacity of buffer in words */
    unsigned words; /* # of complete words in buffer */
    unsigned bits; /* # of used bits in accum */
};

/* * WATCHOUT: The current implementation only grows the buffer. */
static inline FLAC__bool bitwriter_grow_(FLAC__BitWriter *bw, unsigned bits_to_add)
{
    unsigned new_capacity;
    uint32_t *new_buffer;

    CHOC_ASSERT(0 != bw);
    CHOC_ASSERT(0 != bw->buffer);

    /* calculate total words needed to store 'bits_to_add' additional bits */
    new_capacity = bw->words + ((bw->bits + bits_to_add + FLAC__BITS_PER_WORD - 1) / FLAC__BITS_PER_WORD);

    /* it's possible (due to pessimism in the growth estimation that
     * leads to this call) that we don't actually need to grow
     */
    if(bw->capacity >= new_capacity)
        return true;

    /* round up capacity increase to the nearest FLAC__BITWRITER_DEFAULT_INCREMENT */
    if((new_capacity - bw->capacity) % FLAC__BITWRITER_DEFAULT_INCREMENT)
        new_capacity += FLAC__BITWRITER_DEFAULT_INCREMENT - ((new_capacity - bw->capacity) % FLAC__BITWRITER_DEFAULT_INCREMENT);
    /* make sure we got everything right */
    CHOC_ASSERT(0 == (new_capacity - bw->capacity) % FLAC__BITWRITER_DEFAULT_INCREMENT);
    CHOC_ASSERT(new_capacity > bw->capacity);
    CHOC_ASSERT(new_capacity >= bw->words + ((bw->bits + bits_to_add + FLAC__BITS_PER_WORD - 1) / FLAC__BITS_PER_WORD));

    new_buffer = (uint32_t*) safe_realloc_mul_2op_(bw->buffer, sizeof(uint32_t), /*times*/new_capacity);
    if(new_buffer == 0)
        return false;
    bw->buffer = new_buffer;
    bw->capacity = new_capacity;
    return true;
}


/***********************************************************************
 *
 * Class constructor/destructor
 *
 ***********************************************************************/

inline FLAC__BitWriter *FLAC__bitwriter_new(void)
{
    FLAC__BitWriter *bw = (FLAC__BitWriter*) calloc(1, sizeof(FLAC__BitWriter));
    /* note that calloc() sets all members to 0 for us */
    return bw;
}

inline void FLAC__bitwriter_delete(FLAC__BitWriter *bw)
{
    CHOC_ASSERT(0 != bw);

    FLAC__bitwriter_free(bw);
    free(bw);
}

/***********************************************************************
 *
 * Public class methods
 *
 ***********************************************************************/

inline FLAC__bool FLAC__bitwriter_init(FLAC__BitWriter *bw)
{
    CHOC_ASSERT(0 != bw);

    bw->words = bw->bits = 0;
    bw->capacity = FLAC__BITWRITER_DEFAULT_CAPACITY;
    bw->buffer = (uint32_t*) malloc(sizeof(uint32_t) * bw->capacity);
    if(bw->buffer == 0)
        return false;

    return true;
}

inline void FLAC__bitwriter_free(FLAC__BitWriter *bw)
{
    CHOC_ASSERT(0 != bw);

    if(0 != bw->buffer)
        free(bw->buffer);
    bw->buffer = 0;
    bw->capacity = 0;
    bw->words = bw->bits = 0;
}

inline void FLAC__bitwriter_clear(FLAC__BitWriter *bw)
{
    bw->words = bw->bits = 0;
}

inline void FLAC__bitwriter_dump(const FLAC__BitWriter *bw, FILE *out)
{
    unsigned i, j;
    if(bw == 0) {
        fprintf(out, "bitwriter is NULL\n");
    }
    else {
        fprintf(out, "bitwriter: capacity=%u words=%u bits=%u total_bits=%u\n", bw->capacity, bw->words, bw->bits, FLAC__TOTAL_BITS(bw));

        for(i = 0; i < bw->words; i++) {
            fprintf(out, "%08X: ", i);
            for(j = 0; j < FLAC__BITS_PER_WORD; j++)
                fprintf(out, "%01u", bw->buffer[i] & (1 << (FLAC__BITS_PER_WORD-j-1)) ? 1:0);
            fprintf(out, "\n");
        }
        if(bw->bits > 0) {
            fprintf(out, "%08X: ", i);
            for(j = 0; j < bw->bits; j++)
                fprintf(out, "%01u", bw->accum & (1 << (bw->bits-j-1)) ? 1:0);
            fprintf(out, "\n");
        }
    }
}

inline FLAC__bool FLAC__bitwriter_get_write_crc16(FLAC__BitWriter *bw, FLAC__uint16 *crc)
{
    const FLAC__byte *buffer;
    size_t bytes;

    CHOC_ASSERT((bw->bits & 7) == 0); /* assert that we're byte-aligned */

    if(!FLAC__bitwriter_get_buffer(bw, &buffer, &bytes))
        return false;

    *crc = (FLAC__uint16)FLAC__crc16(buffer, bytes);
    FLAC__bitwriter_release_buffer(bw);
    return true;
}

inline FLAC__bool FLAC__bitwriter_get_write_crc8(FLAC__BitWriter *bw, FLAC__byte *crc)
{
    const FLAC__byte *buffer;
    size_t bytes;

    CHOC_ASSERT((bw->bits & 7) == 0); /* assert that we're byte-aligned */

    if(!FLAC__bitwriter_get_buffer(bw, &buffer, &bytes))
        return false;

    *crc = FLAC__crc8(buffer, bytes);
    FLAC__bitwriter_release_buffer(bw);
    return true;
}

inline FLAC__bool FLAC__bitwriter_is_byte_aligned(const FLAC__BitWriter *bw)
{
    return ((bw->bits & 7) == 0);
}

inline unsigned FLAC__bitwriter_get_input_bits_unconsumed(const FLAC__BitWriter *bw)
{
    return FLAC__TOTAL_BITS(bw);
}

inline FLAC__bool FLAC__bitwriter_get_buffer(FLAC__BitWriter *bw, const FLAC__byte **buffer, size_t *bytes)
{
    CHOC_ASSERT((bw->bits & 7) == 0);
    /* double protection */
    if(bw->bits & 7)
        return false;
    /* if we have bits in the accumulator we have to flush those to the buffer first */
    if(bw->bits) {
        CHOC_ASSERT(bw->words <= bw->capacity);
        if(bw->words == bw->capacity && !bitwriter_grow_(bw, FLAC__BITS_PER_WORD))
            return false;
        /* append bits as complete word to buffer, but don't change bw->accum or bw->bits */
        bw->buffer[bw->words] = SWAP_BE_WORD_TO_HOST(bw->accum << (FLAC__BITS_PER_WORD-bw->bits));
    }
    /* now we can just return what we have */
    *buffer = (FLAC__byte*)bw->buffer;
    *bytes = (FLAC__BYTES_PER_WORD * bw->words) + (bw->bits >> 3);
    return true;
}

inline void FLAC__bitwriter_release_buffer(FLAC__BitWriter *bw)
{
    /* nothing to do.  in the future, strict checking of a 'writer-is-in-
     * get-mode' flag could be added everywhere and then cleared here
     */
    (void)bw;
}

inline FLAC__bool FLAC__bitwriter_write_zeroes(FLAC__BitWriter *bw, unsigned bits)
{
    unsigned n;

    CHOC_ASSERT(0 != bw);
    CHOC_ASSERT(0 != bw->buffer);

    if(bits == 0)
        return true;
    /* slightly pessimistic size check but faster than "<= bw->words + (bw->bits+bits+FLAC__BITS_PER_WORD-1)/FLAC__BITS_PER_WORD" */
    if(bw->capacity <= bw->words + bits && !bitwriter_grow_(bw, bits))
        return false;
    /* first part gets to word alignment */
    if(bw->bits) {
        n = std::min(FLAC__BITS_PER_WORD - bw->bits, bits);
        bw->accum <<= n;
        bits -= n;
        bw->bits += n;
        if(bw->bits == FLAC__BITS_PER_WORD) {
            bw->buffer[bw->words++] = SWAP_BE_WORD_TO_HOST(bw->accum);
            bw->bits = 0;
        }
        else
            return true;
    }
    /* do whole words */
    while(bits >= FLAC__BITS_PER_WORD) {
        bw->buffer[bw->words++] = 0;
        bits -= FLAC__BITS_PER_WORD;
    }
    /* do any leftovers */
    if(bits > 0) {
        bw->accum = 0;
        bw->bits = bits;
    }
    return true;
}

inline FLAC__bool FLAC__bitwriter_write_raw_uint32(FLAC__BitWriter *bw, FLAC__uint32 val, unsigned bits)
{
    unsigned left;

    /* WATCHOUT: code does not work with <32bit words; we can make things much faster with this assertion */
    CHOC_ASSERT(FLAC__BITS_PER_WORD >= 32);

    CHOC_ASSERT(0 != bw);
    CHOC_ASSERT(0 != bw->buffer);

    CHOC_ASSERT(bits <= 32);
    if(bits == 0)
        return true;

    /* slightly pessimistic size check but faster than "<= bw->words + (bw->bits+bits+FLAC__BITS_PER_WORD-1)/FLAC__BITS_PER_WORD" */
    if(bw->capacity <= bw->words + bits && !bitwriter_grow_(bw, bits))
        return false;

    left = FLAC__BITS_PER_WORD - bw->bits;
    if(bits < left) {
        bw->accum <<= bits;
        bw->accum |= val;
        bw->bits += bits;
    }
    else if(bw->bits) { /* WATCHOUT: if bw->bits == 0, left==FLAC__BITS_PER_WORD and bw->accum<<=left is a NOP instead of setting to 0 */
        bw->accum <<= left;
        bw->accum |= val >> (bw->bits = bits - left);
        bw->buffer[bw->words++] = SWAP_BE_WORD_TO_HOST(bw->accum);
        bw->accum = val;
    }
    else {
        bw->accum = val;
        bw->bits = 0;
        bw->buffer[bw->words++] = SWAP_BE_WORD_TO_HOST(val);
    }

    return true;
}

inline FLAC__bool FLAC__bitwriter_write_raw_int32(FLAC__BitWriter *bw, FLAC__int32 val, unsigned bits)
{
    /* zero-out unused bits */
    if(bits < 32)
        val &= (~(0xffffffff << bits));

    return FLAC__bitwriter_write_raw_uint32(bw, (FLAC__uint32)val, bits);
}

inline FLAC__bool FLAC__bitwriter_write_raw_uint64(FLAC__BitWriter *bw, FLAC__uint64 val, unsigned bits)
{
    /* this could be a little faster but it's not used for much */
    if(bits > 32) {
        return
            FLAC__bitwriter_write_raw_uint32(bw, (FLAC__uint32)(val>>32), bits-32) &&
            FLAC__bitwriter_write_raw_uint32(bw, (FLAC__uint32)val, 32);
    }
    else
        return FLAC__bitwriter_write_raw_uint32(bw, (FLAC__uint32)val, bits);
}

inline FLAC__bool FLAC__bitwriter_write_raw_uint32_little_endian(FLAC__BitWriter *bw, FLAC__uint32 val)
{
    /* this doesn't need to be that fast as currently it is only used for vorbis comments */

    if(!FLAC__bitwriter_write_raw_uint32(bw, val & 0xff, 8))
        return false;
    if(!FLAC__bitwriter_write_raw_uint32(bw, (val>>8) & 0xff, 8))
        return false;
    if(!FLAC__bitwriter_write_raw_uint32(bw, (val>>16) & 0xff, 8))
        return false;
    if(!FLAC__bitwriter_write_raw_uint32(bw, val>>24, 8))
        return false;

    return true;
}

inline FLAC__bool FLAC__bitwriter_write_byte_block(FLAC__BitWriter *bw, const FLAC__byte vals[], unsigned nvals)
{
    unsigned i;

    /* this could be faster but currently we don't need it to be since it's only used for writing metadata */
    for(i = 0; i < nvals; i++) {
        if(!FLAC__bitwriter_write_raw_uint32(bw, (FLAC__uint32)(vals[i]), 8))
            return false;
    }

    return true;
}

inline FLAC__bool FLAC__bitwriter_write_unary_unsigned(FLAC__BitWriter *bw, unsigned val)
{
    if(val < 32)
        return FLAC__bitwriter_write_raw_uint32(bw, 1, ++val);
    else
        return
            FLAC__bitwriter_write_zeroes(bw, val) &&
            FLAC__bitwriter_write_raw_uint32(bw, 1, 1);
}

inline unsigned FLAC__bitwriter_rice_bits(FLAC__int32 val, unsigned parameter)
{
    FLAC__uint32 uval;

    CHOC_ASSERT(parameter < sizeof(unsigned)*8);

    /* fold signed to unsigned; actual formula is: negative(v)? -2v-1 : 2v */
    uval = (val<<1) ^ (val>>31);

    return 1 + parameter + (uval >> parameter);
}

inline FLAC__bool FLAC__bitwriter_write_rice_signed(FLAC__BitWriter *bw, FLAC__int32 val, unsigned parameter)
{
    unsigned total_bits, interesting_bits, msbs;
    FLAC__uint32 uval, pattern;

    CHOC_ASSERT(0 != bw);
    CHOC_ASSERT(0 != bw->buffer);
    CHOC_ASSERT(parameter < 8*sizeof(uval));

    /* fold signed to unsigned; actual formula is: negative(v)? -2v-1 : 2v */
    uval = (val<<1) ^ (val>>31);

    msbs = uval >> parameter;
    interesting_bits = 1 + parameter;
    total_bits = interesting_bits + msbs;
    pattern = 1 << parameter; /* the unary end bit */
    pattern |= (uval & ((1<<parameter)-1)); /* the binary LSBs */

    if(total_bits <= 32)
        return FLAC__bitwriter_write_raw_uint32(bw, pattern, total_bits);
    else
        return
            FLAC__bitwriter_write_zeroes(bw, msbs) && /* write the unary MSBs */
            FLAC__bitwriter_write_raw_uint32(bw, pattern, interesting_bits); /* write the unary end bit and binary LSBs */
}

inline FLAC__bool FLAC__bitwriter_write_rice_signed_block(FLAC__BitWriter *bw, const FLAC__int32 *vals, unsigned nvals, unsigned parameter)
{
    const FLAC__uint32 mask1 = FLAC__WORD_ALL_ONES << parameter; /* we val|=mask1 to set the stop bit above it... */
    const FLAC__uint32 mask2 = FLAC__WORD_ALL_ONES >> (31-parameter); /* ...then mask off the bits above the stop bit with val&=mask2*/
    FLAC__uint32 uval;
    unsigned left;
    const unsigned lsbits = 1 + parameter;
    unsigned msbits;

    CHOC_ASSERT(0 != bw);
    CHOC_ASSERT(0 != bw->buffer);
    CHOC_ASSERT(parameter < 8*sizeof(uint32_t)-1);
    /* WATCHOUT: code does not work with <32bit words; we can make things much faster with this assertion */
    CHOC_ASSERT(FLAC__BITS_PER_WORD >= 32);

    while(nvals) {
        /* fold signed to unsigned; actual formula is: negative(v)? -2v-1 : 2v */
        uval = signedLeftShift (*vals, 1) ^ (*vals>>31);

        msbits = uval >> parameter;

        if(bw->bits && bw->bits + msbits + lsbits < FLAC__BITS_PER_WORD) { /* i.e. if the whole thing fits in the current uint32_t */
            /* ^^^ if bw->bits is 0 then we may have filled the buffer and have no free uint32_t to work in */
            bw->bits = bw->bits + msbits + lsbits;
            uval |= mask1; /* set stop bit */
            uval &= mask2; /* mask off unused top bits */
            bw->accum <<= msbits + lsbits;
            bw->accum |= uval;
        }
        else {
            /* slightly pessimistic size check but faster than "<= bw->words + (bw->bits+msbits+lsbits+FLAC__BITS_PER_WORD-1)/FLAC__BITS_PER_WORD" */
            /* OPT: pessimism may cause flurry of false calls to grow_ which eat up all savings before it */
            if(bw->capacity <= bw->words + bw->bits + msbits + 1/*lsbits always fit in 1 uint32_t*/ && !bitwriter_grow_(bw, msbits+lsbits))
                return false;

            if(msbits) {
                /* first part gets to word alignment */
                if(bw->bits) {
                    left = FLAC__BITS_PER_WORD - bw->bits;
                    if(msbits < left) {
                        bw->accum <<= msbits;
                        bw->bits += msbits;
                        goto break1;
                    }
                    else {
                        bw->accum <<= left;
                        msbits -= left;
                        bw->buffer[bw->words++] = SWAP_BE_WORD_TO_HOST(bw->accum);
                        bw->bits = 0;
                    }
                }
                /* do whole words */
                while(msbits >= FLAC__BITS_PER_WORD) {
                    bw->buffer[bw->words++] = 0;
                    msbits -= FLAC__BITS_PER_WORD;
                }
                /* do any leftovers */
                if(msbits > 0) {
                    bw->accum = 0;
                    bw->bits = msbits;
                }
            }
break1:
            uval |= mask1; /* set stop bit */
            uval &= mask2; /* mask off unused top bits */

            left = FLAC__BITS_PER_WORD - bw->bits;
            if(lsbits < left) {
                bw->accum <<= lsbits;
                bw->accum |= uval;
                bw->bits += lsbits;
            }
            else {
                /* if bw->bits == 0, left==FLAC__BITS_PER_WORD which will always
                 * be > lsbits (because of previous assertions) so it would have
                 * triggered the (lsbits<left) case above.
                 */
                CHOC_ASSERT(bw->bits);
                CHOC_ASSERT(left < FLAC__BITS_PER_WORD);
                bw->accum <<= left;
                bw->accum |= uval >> (bw->bits = lsbits - left);
                bw->buffer[bw->words++] = SWAP_BE_WORD_TO_HOST(bw->accum);
                bw->accum = uval;
            }
        }
        vals++;
        nvals--;
    }
    return true;
}

inline FLAC__bool FLAC__bitwriter_write_utf8_uint32(FLAC__BitWriter *bw, FLAC__uint32 val)
{
    FLAC__bool ok = 1;

    CHOC_ASSERT(0 != bw);
    CHOC_ASSERT(0 != bw->buffer);

    CHOC_ASSERT(!(val & 0x80000000)); /* this version only handles 31 bits */

    if(val < 0x80) {
        return FLAC__bitwriter_write_raw_uint32(bw, val, 8);
    }
    else if(val < 0x800) {
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0xC0 | (val>>6), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (val&0x3F), 8);
    }
    else if(val < 0x10000) {
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0xE0 | (val>>12), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | ((val>>6)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (val&0x3F), 8);
    }
    else if(val < 0x200000) {
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0xF0 | (val>>18), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | ((val>>12)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | ((val>>6)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (val&0x3F), 8);
    }
    else if(val < 0x4000000) {
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0xF8 | (val>>24), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | ((val>>18)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | ((val>>12)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | ((val>>6)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (val&0x3F), 8);
    }
    else {
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0xFC | (val>>30), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | ((val>>24)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | ((val>>18)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | ((val>>12)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | ((val>>6)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (val&0x3F), 8);
    }

    return ok;
}

inline FLAC__bool FLAC__bitwriter_write_utf8_uint64(FLAC__BitWriter *bw, FLAC__uint64 val)
{
    FLAC__bool ok = 1;

    CHOC_ASSERT(0 != bw);
    CHOC_ASSERT(0 != bw->buffer);

    CHOC_ASSERT(!(val & FLAC__U64L(0xFFFFFFF000000000))); /* this version only handles 36 bits */

    if(val < 0x80) {
        return FLAC__bitwriter_write_raw_uint32(bw, (FLAC__uint32)val, 8);
    }
    else if(val < 0x800) {
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0xC0 | (FLAC__uint32)(val>>6), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)(val&0x3F), 8);
    }
    else if(val < 0x10000) {
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0xE0 | (FLAC__uint32)(val>>12), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>6)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)(val&0x3F), 8);
    }
    else if(val < 0x200000) {
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0xF0 | (FLAC__uint32)(val>>18), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>12)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>6)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)(val&0x3F), 8);
    }
    else if(val < 0x4000000) {
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0xF8 | (FLAC__uint32)(val>>24), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>18)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>12)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>6)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)(val&0x3F), 8);
    }
    else if(val < 0x80000000) {
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0xFC | (FLAC__uint32)(val>>30), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>24)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>18)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>12)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>6)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)(val&0x3F), 8);
    }
    else {
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0xFE, 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>30)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>24)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>18)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>12)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)((val>>6)&0x3F), 8);
        ok &= FLAC__bitwriter_write_raw_uint32(bw, 0x80 | (FLAC__uint32)(val&0x3F), 8);
    }

    return ok;
}

inline FLAC__bool FLAC__bitwriter_zero_pad_to_byte_boundary(FLAC__BitWriter *bw)
{
    /* 0-pad to byte boundary */
    if(bw->bits & 7u)
        return FLAC__bitwriter_write_zeroes(bw, 8 - (bw->bits & 7u));
    else
        return true;
}

/* These functions are declared inline in this file but are also callable as
 * externs from elsewhere.
 * According to the C99 spec, section 6.7.4, simply providing a function
 * prototype in a header file without 'inline' and making the function inline
 * in this file should be sufficient.
 * Unfortunately, the Microsoft VS compiler doesn't pick them up externally. To
 * fix that we add extern declarations here.
 */
extern FLAC__bool FLAC__bitwriter_write_zeroes(FLAC__BitWriter *bw, unsigned bits);
extern FLAC__bool FLAC__bitwriter_write_raw_int32(FLAC__BitWriter *bw, FLAC__int32 val, unsigned bits);
extern FLAC__bool FLAC__bitwriter_write_raw_uint64(FLAC__BitWriter *bw, FLAC__uint64 val, unsigned bits);
extern FLAC__bool FLAC__bitwriter_write_raw_uint32_little_endian(FLAC__BitWriter *bw, FLAC__uint32 val);
extern FLAC__bool FLAC__bitwriter_write_byte_block(FLAC__BitWriter *bw, const FLAC__byte vals[], unsigned nvals);



#if defined FLAC__CPU_IA32

static void disable_sse(FLAC__CPUInfo *info)
{
    info->ia32.sse   = false;
    info->ia32.sse2  = false;
    info->ia32.sse3  = false;
    info->ia32.ssse3 = false;
    info->ia32.sse41 = false;
    info->ia32.sse42 = false;
}

static void disable_avx(FLAC__CPUInfo *info)
{
    info->ia32.avx     = false;
    info->ia32.avx2    = false;
    info->ia32.fma     = false;
}

#elif defined FLAC__CPU_X86_64

static void disable_avx(FLAC__CPUInfo *info)
{
    info->x86.avx     = false;
    info->x86.avx2    = false;
    info->x86.fma     = false;
}
#endif

#if defined(__APPLE__)
/* how to get sysctlbyname()? */
#endif

#ifdef FLAC__CPU_IA32
/* these are flags in EDX of CPUID AX=00000001 */
static const unsigned FLAC__CPUINFO_IA32_CPUID_CMOV = 0x00008000;
static const unsigned FLAC__CPUINFO_IA32_CPUID_MMX = 0x00800000;
static const unsigned FLAC__CPUINFO_IA32_CPUID_FXSR = 0x01000000;
static const unsigned FLAC__CPUINFO_IA32_CPUID_SSE = 0x02000000;
static const unsigned FLAC__CPUINFO_IA32_CPUID_SSE2 = 0x04000000;
#endif

/* these are flags in ECX of CPUID AX=00000001 */
static const unsigned FLAC__CPUINFO_IA32_CPUID_SSE3 = 0x00000001;
static const unsigned FLAC__CPUINFO_IA32_CPUID_SSSE3 = 0x00000200;
static const unsigned FLAC__CPUINFO_IA32_CPUID_SSE41 = 0x00080000;
static const unsigned FLAC__CPUINFO_IA32_CPUID_SSE42 = 0x00100000;

#if defined FLAC__AVX_SUPPORTED
/* these are flags in ECX of CPUID AX=00000001 */
static const unsigned FLAC__CPUINFO_IA32_CPUID_OSXSAVE = 0x08000000;
static const unsigned FLAC__CPUINFO_IA32_CPUID_AVX = 0x10000000;
static const unsigned FLAC__CPUINFO_IA32_CPUID_FMA = 0x00001000;
/* these are flags in EBX of CPUID AX=00000007 */
static const unsigned FLAC__CPUINFO_IA32_CPUID_AVX2 = 0x00000020;
#endif

/*
 * Extra stuff needed for detection of OS support for SSE on IA-32
 */
#if defined(FLAC__CPU_IA32) && !defined FLAC__NO_ASM && (defined FLAC__HAS_NASM || defined FLAC__HAS_X86INTRIN) && !defined FLAC__NO_SSE_OS && !defined FLAC__SSE_OS
# if defined(__linux__)
/*
 * If the OS doesn't support SSE, we will get here with a SIGILL.  We
 * modify the return address to jump over the offending SSE instruction
 * and also the operation following it that indicates the instruction
 * executed successfully.  In this way we use no global variables and
 * stay thread-safe.
 *
 * 3 + 3 + 6:
 *   3 bytes for "xorps xmm0,xmm0"
 *   3 bytes for estimate of how long the follwing "inc var" instruction is
 *   6 bytes extra in case our estimate is wrong
 * 12 bytes puts us in the NOP "landing zone"
 */
// #   include <sys/ucontext.h>
    static inline void sigill_handler_sse_os(int signal, siginfo_t *si, void *uc)
    {
        (void)signal, (void)si;
        ((ucontext_t*)uc)->uc_mcontext.gregs[14/*REG_EIP*/] += 3 + 3 + 6;
    }
# elif defined(_MSC_VER)
// #  include <windows.h>
# endif
#endif


inline void FLAC__cpu_info(FLAC__CPUInfo *info)
{
/*
 * IA32-specific
 */
#ifdef FLAC__CPU_IA32
    FLAC__bool ia32_fxsr = false;
    FLAC__bool ia32_osxsave = false;
    (void) ia32_fxsr; (void) ia32_osxsave; /* to avoid warnings about unused variables */
    memset(info, 0, sizeof(*info));
    info->type = FLAC__CPUINFO_TYPE_IA32;
#if !defined FLAC__NO_ASM && (defined FLAC__HAS_NASM || defined FLAC__HAS_X86INTRIN)
    info->use_asm = true; /* we assume a minimum of 80386 with FLAC__CPU_IA32 */
#ifdef FLAC__HAS_X86INTRIN
    if(!FLAC__cpu_have_cpuid_x86())
        return;
#else
    if(!FLAC__cpu_have_cpuid_asm_ia32())
        return;
#endif
    {
        /* http://www.sandpile.org/x86/cpuid.htm */
#ifdef FLAC__HAS_X86INTRIN
        FLAC__uint32 flags_eax, flags_ebx, flags_ecx, flags_edx;
        FLAC__cpu_info_x86(1, &flags_eax, &flags_ebx, &flags_ecx, &flags_edx);
#else
        FLAC__uint32 flags_ecx, flags_edx;
        FLAC__cpu_info_asm_ia32(&flags_edx, &flags_ecx);
#endif
        info->ia32.cmov  = (flags_edx & FLAC__CPUINFO_IA32_CPUID_CMOV )? true : false;
        info->ia32.mmx   = (flags_edx & FLAC__CPUINFO_IA32_CPUID_MMX  )? true : false;
              ia32_fxsr  = (flags_edx & FLAC__CPUINFO_IA32_CPUID_FXSR )? true : false;
        info->ia32.sse   = (flags_edx & FLAC__CPUINFO_IA32_CPUID_SSE  )? true : false;
        info->ia32.sse2  = (flags_edx & FLAC__CPUINFO_IA32_CPUID_SSE2 )? true : false;
        info->ia32.sse3  = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_SSE3 )? true : false;
        info->ia32.ssse3 = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_SSSE3)? true : false;
        info->ia32.sse41 = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_SSE41)? true : false;
        info->ia32.sse42 = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_SSE42)? true : false;
#if defined FLAC__HAS_X86INTRIN && defined FLAC__AVX_SUPPORTED
            ia32_osxsave = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_OSXSAVE)? true : false;
        info->ia32.avx   = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_AVX    )? true : false;
        info->ia32.fma   = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_FMA    )? true : false;
        FLAC__cpu_info_x86(7, &flags_eax, &flags_ebx, &flags_ecx, &flags_edx);
        info->ia32.avx2  = (flags_ebx & FLAC__CPUINFO_IA32_CPUID_AVX2   )? true : false;
#endif
    }

    /*
     * now have to check for OS support of SSE instructions
     */
    if(info->ia32.sse) {
#if defined FLAC__NO_SSE_OS
        /* assume user knows better than us; turn it off */
        disable_sse(info);
#elif defined FLAC__SSE_OS
        /* assume user knows better than us; leave as detected above */
#elif defined(__FreeBSD__) || defined(__FreeBSD_kernel__) || defined(__DragonFly__) || defined(__APPLE__)
        int sse = 0;
        size_t len;
        /* at least one of these must work: */
        len = sizeof(sse); sse = sse || (sysctlbyname("hw.instruction_sse", &sse, &len, NULL, 0) == 0 && sse);
        len = sizeof(sse); sse = sse || (sysctlbyname("hw.optional.sse"   , &sse, &len, NULL, 0) == 0 && sse); /* __APPLE__ ? */
        if(!sse)
            disable_sse(info);
#elif defined(__NetBSD__) || defined (__OpenBSD__)
# if __NetBSD_Version__ >= 105250000 || (defined __OpenBSD__)
        int val = 0, mib[2] = { CTL_MACHDEP, CPU_SSE };
        size_t len = sizeof(val);
        if(sysctl(mib, 2, &val, &len, NULL, 0) < 0 || !val)
            disable_sse(info);
        else { /* double-check SSE2 */
            mib[1] = CPU_SSE2;
            len = sizeof(val);
            if(sysctl(mib, 2, &val, &len, NULL, 0) < 0 || !val) {
                disable_sse(info);
                info->ia32.sse = true;
            }
        }
# else
        disable_sse(info);
# endif
#elif defined(__linux__)
        int sse = 0;
        struct sigaction sigill_save;
        struct sigaction sigill_sse;
        sigill_sse.sa_sigaction = sigill_handler_sse_os;
        sigemptyset (&sigill_sse.sa_mask);
        sigill_sse.sa_flags = SA_SIGINFO | SA_RESETHAND; /* SA_RESETHAND just in case our SIGILL return jump breaks, so we don't get stuck in a loop */
        if(0 == sigaction(SIGILL, &sigill_sse, &sigill_save))
        {
            /* http://www.ibiblio.org/gferg/ldp/GCC-Inline-Assembly-HOWTO.html */
            /* see sigill_handler_sse_os() for an explanation of the following: */
            asm volatile (
                "xorps %%xmm0,%%xmm0\n\t" /* will cause SIGILL if unsupported by OS */
                "incl %0\n\t"             /* SIGILL handler will jump over this */
                /* landing zone */
                "nop\n\t" /* SIGILL jump lands here if "inc" is 9 bytes */
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t"
                "nop\n\t" /* SIGILL jump lands here if "inc" is 3 bytes (expected) */
                "nop\n\t"
                "nop"     /* SIGILL jump lands here if "inc" is 1 byte */
                : "=r"(sse)
                : "0"(sse)
            );

            sigaction(SIGILL, &sigill_save, NULL);
        }

        if(!sse)
            disable_sse(info);
#elif defined(_MSC_VER)
        __try {
            __asm {
                xorps xmm0,xmm0
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER) {
            if (_exception_code() == 0xC000001D /*STATUS_ILLEGAL_INSTRUCTION*/)
                disable_sse(info);
        }
#elif defined(__GNUC__) /* MinGW goes here */
        int sse = 0;
        /* Based on the idea described in Agner Fog's manual "Optimizing subroutines in assembly language" */
        /* In theory, not guaranteed to detect lack of OS SSE support on some future Intel CPUs, but in practice works (see the aforementioned manual) */
        if (ia32_fxsr) {
            struct {
                FLAC__uint32 buff[128];
            } __attribute__((aligned(16))) fxsr;
            FLAC__uint32 old_val, new_val;

            asm volatile ("fxsave %0"  : "=m" (fxsr) : "m" (fxsr));
            old_val = fxsr.buff[50];
            fxsr.buff[50] ^= 0x0013c0de;                             /* change value in the buffer */
            asm volatile ("fxrstor %0" : "=m" (fxsr) : "m" (fxsr));  /* try to change SSE register */
            fxsr.buff[50] = old_val;                                 /* restore old value in the buffer */
            asm volatile ("fxsave %0 " : "=m" (fxsr) : "m" (fxsr));  /* old value will be overwritten if SSE register was changed */
            new_val = fxsr.buff[50];                                 /* == old_val if FXRSTOR didn't change SSE register and (old_val ^ 0x0013c0de) otherwise */
            fxsr.buff[50] = old_val;                                 /* again restore old value in the buffer */
            asm volatile ("fxrstor %0" : "=m" (fxsr) : "m" (fxsr));  /* restore old values of registers */

            if ((old_val^new_val) == 0x0013c0de)
                sse = 1;
        }
        if(!sse)
            disable_sse(info);
#else
        /* no way to test, disable to be safe */
        disable_sse(info);
#endif
    }
    else /* info->ia32.sse == false */
        disable_sse(info);

    /*
     * now have to check for OS support of AVX instructions
     */
    if(info->ia32.avx && ia32_osxsave) {
        FLAC__uint32 ecr = FLAC__cpu_xgetbv_x86();
        if ((ecr & 0x6) != 0x6)
            disable_avx(info);
    }
    else /* no OS AVX support*/
        disable_avx(info);
#else
    info->use_asm = false;
#endif

/*
 * x86-64-specific
 */
#elif defined FLAC__CPU_X86_64
    FLAC__bool x86_osxsave = false;
    (void) x86_osxsave; /* to avoid warnings about unused variables */
    memset(info, 0, sizeof(*info));
    info->type = FLAC__CPUINFO_TYPE_X86_64;
#if !defined FLAC__NO_ASM && defined FLAC__HAS_X86INTRIN
    info->use_asm = true;
    {
        /* http://www.sandpile.org/x86/cpuid.htm */
        FLAC__uint32 flags_eax, flags_ebx, flags_ecx, flags_edx;
        FLAC__cpu_info_x86(1, &flags_eax, &flags_ebx, &flags_ecx, &flags_edx);
        info->x86.sse3  = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_SSE3 )? true : false;
        info->x86.ssse3 = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_SSSE3)? true : false;
        info->x86.sse41 = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_SSE41)? true : false;
        info->x86.sse42 = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_SSE42)? true : false;
#if defined FLAC__AVX_SUPPORTED
            x86_osxsave = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_OSXSAVE)? true : false;
        info->x86.avx   = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_AVX    )? true : false;
        info->x86.fma   = (flags_ecx & FLAC__CPUINFO_IA32_CPUID_FMA    )? true : false;
        FLAC__cpu_info_x86(7, &flags_eax, &flags_ebx, &flags_ecx, &flags_edx);
        info->x86.avx2  = (flags_ebx & FLAC__CPUINFO_IA32_CPUID_AVX2   )? true : false;
#endif
    }

    /*
     * now have to check for OS support of AVX instructions
     */
    if(info->x86.avx && x86_osxsave) {
        FLAC__uint32 ecr = FLAC__cpu_xgetbv_x86();
        if ((ecr & 0x6) != 0x6)
            disable_avx(info);
    }
    else /* no OS AVX support*/
        disable_avx(info);
#else
    info->use_asm = false;
#endif

/*
 * unknown CPU
 */
#else
    info->type = FLAC__CPUINFO_TYPE_UNKNOWN;
    info->use_asm = false;
#endif
}

#if (defined FLAC__CPU_IA32 || defined FLAC__CPU_X86_64) && defined FLAC__HAS_X86INTRIN

inline FLAC__uint32 FLAC__cpu_have_cpuid_x86(void)
{
#ifdef FLAC__CPU_X86_64
    return 1;
#else
# if defined _MSC_VER || defined __INTEL_COMPILER /* Do they support CPUs w/o CPUID support (or OSes that work on those CPUs)? */
    FLAC__uint32 flags1, flags2;
    __asm {
        pushfd
        pushfd
        pop        eax
        mov        flags1, eax
        xor        eax, 0x200000
        push    eax
        popfd
        pushfd
        pop        eax
        mov        flags2, eax
        popfd
    }
    if (((flags1^flags2) & 0x200000) != 0)
        return 1;
    else
        return 0;
# elif defined __GNUC__ && defined HAVE_CPUID_H
    if (__get_cpuid_max(0, 0) != 0)
        return 1;
    else
        return 0;
# else
    return 0;
# endif
#endif
}

inline void FLAC__cpu_info_x86(FLAC__uint32 level, FLAC__uint32 *eax, FLAC__uint32 *ebx, FLAC__uint32 *ecx, FLAC__uint32 *edx)
{
    (void) level;

#if defined _MSC_VER || defined __INTEL_COMPILER
    int cpuinfo[4];
    int ext = level & 0x80000000;
    __cpuid(cpuinfo, ext);
    if((unsigned)cpuinfo[0] < level) {
        *eax = *ebx = *ecx = *edx = 0;
        return;
    }
#if defined FLAC__AVX_SUPPORTED
    __cpuidex(cpuinfo, level, 0); /* for AVX2 detection */
#else
    __cpuid(cpuinfo, level); /* some old compilers don't support __cpuidex */
#endif
    *eax = cpuinfo[0]; *ebx = cpuinfo[1]; *ecx = cpuinfo[2]; *edx = cpuinfo[3];
#elif defined __GNUC__ && defined HAVE_CPUID_H
    FLAC__uint32 ext = level & 0x80000000;
    __cpuid(ext, *eax, *ebx, *ecx, *edx);
    if (*eax < level) {
        *eax = *ebx = *ecx = *edx = 0;
        return;
    }
    __cpuid_count(level, 0, *eax, *ebx, *ecx, *edx);
#else
    *eax = *ebx = *ecx = *edx = 0;
#endif
}

inline FLAC__uint32 FLAC__cpu_xgetbv_x86(void)
{
#if (defined _MSC_VER || defined __INTEL_COMPILER) && defined FLAC__AVX_SUPPORTED
    return (FLAC__uint32)_xgetbv(0);
#elif defined __GNUC__
    FLAC__uint32 lo, hi;
    asm volatile (".byte 0x0f, 0x01, 0xd0" : "=a"(lo), "=d"(hi) : "c" (0));
    return lo;
#else
    return 0;
#endif
}

#endif /* (FLAC__CPU_IA32 || FLAC__CPU_X86_64) && FLAC__HAS_X86INTRIN */


inline void FLAC__crc8_update(const FLAC__byte data, FLAC__uint8 *crc)
{
    *crc = FLAC__crc8_table[*crc ^ data];
}

inline void FLAC__crc8_update_block(const FLAC__byte *data, unsigned len, FLAC__uint8 *crc)
{
    while(len--)
        *crc = FLAC__crc8_table[*crc ^ *data++];
}

inline FLAC__uint8 FLAC__crc8(const FLAC__byte *data, unsigned len)
{
    FLAC__uint8 crc = 0;

    while(len--)
        crc = FLAC__crc8_table[crc ^ *data++];

    return crc;
}

inline unsigned FLAC__crc16(const FLAC__byte *data, unsigned len)
{
    unsigned crc = 0;

    while(len--)
        crc = ((crc<<8) ^ FLAC__crc16_table[(crc>>8) ^ *data++]) & 0xffff;

    return crc;
}


static inline unsigned local_abs (int x) { return (unsigned) (x < 0 ? -x : x); }

#ifndef FLAC__INTEGER_ONLY_LIBRARY
inline unsigned FLAC__fixed_compute_best_predictor(const FLAC__int32 data[], unsigned data_len, FLAC__float residual_bits_per_sample[FLAC__MAX_FIXED_ORDER+1])
#else
inline unsigned FLAC__fixed_compute_best_predictor(const FLAC__int32 data[], unsigned data_len, FLAC__fixedpoint residual_bits_per_sample[FLAC__MAX_FIXED_ORDER+1])
#endif
{
    FLAC__int32 last_error_0 = data[-1];
    FLAC__int32 last_error_1 = data[-1] - data[-2];
    FLAC__int32 last_error_2 = last_error_1 - (data[-2] - data[-3]);
    FLAC__int32 last_error_3 = last_error_2 - (data[-2] - 2*data[-3] + data[-4]);
    FLAC__int32 error, save;
    FLAC__uint32 total_error_0 = 0, total_error_1 = 0, total_error_2 = 0, total_error_3 = 0, total_error_4 = 0;
    unsigned i, order;

    for(i = 0; i < data_len; i++) {
        error  = data[i]     ; total_error_0 += local_abs(error);                      save = error;
        error -= last_error_0; total_error_1 += local_abs(error); last_error_0 = save; save = error;
        error -= last_error_1; total_error_2 += local_abs(error); last_error_1 = save; save = error;
        error -= last_error_2; total_error_3 += local_abs(error); last_error_2 = save; save = error;
        error -= last_error_3; total_error_4 += local_abs(error); last_error_3 = save;
    }

    if(total_error_0 < std::min(std::min(std::min(total_error_1, total_error_2), total_error_3), total_error_4))
        order = 0;
    else if(total_error_1 < std::min(std::min(total_error_2, total_error_3), total_error_4))
        order = 1;
    else if(total_error_2 < std::min(total_error_3, total_error_4))
        order = 2;
    else if(total_error_3 < total_error_4)
        order = 3;
    else
        order = 4;

    /* Estimate the expected number of bits per residual signal sample. */
    /* 'total_error*' is linearly related to the variance of the residual */
    /* signal, so we use it directly to compute E(|x|) */
    CHOC_ASSERT(data_len > 0 || total_error_0 == 0);
    CHOC_ASSERT(data_len > 0 || total_error_1 == 0);
    CHOC_ASSERT(data_len > 0 || total_error_2 == 0);
    CHOC_ASSERT(data_len > 0 || total_error_3 == 0);
    CHOC_ASSERT(data_len > 0 || total_error_4 == 0);
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    residual_bits_per_sample[0] = (FLAC__float)((total_error_0 > 0) ? log(M_LN2 * (FLAC__double)total_error_0 / (FLAC__double)data_len) / M_LN2 : 0.0);
    residual_bits_per_sample[1] = (FLAC__float)((total_error_1 > 0) ? log(M_LN2 * (FLAC__double)total_error_1 / (FLAC__double)data_len) / M_LN2 : 0.0);
    residual_bits_per_sample[2] = (FLAC__float)((total_error_2 > 0) ? log(M_LN2 * (FLAC__double)total_error_2 / (FLAC__double)data_len) / M_LN2 : 0.0);
    residual_bits_per_sample[3] = (FLAC__float)((total_error_3 > 0) ? log(M_LN2 * (FLAC__double)total_error_3 / (FLAC__double)data_len) / M_LN2 : 0.0);
    residual_bits_per_sample[4] = (FLAC__float)((total_error_4 > 0) ? log(M_LN2 * (FLAC__double)total_error_4 / (FLAC__double)data_len) / M_LN2 : 0.0);
#else
    residual_bits_per_sample[0] = (total_error_0 > 0) ? local__compute_rbps_integerized(total_error_0, data_len) : 0;
    residual_bits_per_sample[1] = (total_error_1 > 0) ? local__compute_rbps_integerized(total_error_1, data_len) : 0;
    residual_bits_per_sample[2] = (total_error_2 > 0) ? local__compute_rbps_integerized(total_error_2, data_len) : 0;
    residual_bits_per_sample[3] = (total_error_3 > 0) ? local__compute_rbps_integerized(total_error_3, data_len) : 0;
    residual_bits_per_sample[4] = (total_error_4 > 0) ? local__compute_rbps_integerized(total_error_4, data_len) : 0;
#endif

    return order;
}

#ifndef FLAC__INTEGER_ONLY_LIBRARY
inline unsigned FLAC__fixed_compute_best_predictor_wide(const FLAC__int32 data[], unsigned data_len, FLAC__float residual_bits_per_sample[FLAC__MAX_FIXED_ORDER+1])
#else
inline unsigned FLAC__fixed_compute_best_predictor_wide(const FLAC__int32 data[], unsigned data_len, FLAC__fixedpoint residual_bits_per_sample[FLAC__MAX_FIXED_ORDER+1])
#endif
{
    FLAC__int32 last_error_0 = data[-1];
    FLAC__int32 last_error_1 = data[-1] - data[-2];
    FLAC__int32 last_error_2 = last_error_1 - (data[-2] - data[-3]);
    FLAC__int32 last_error_3 = last_error_2 - (data[-2] - 2*data[-3] + data[-4]);
    FLAC__int32 error, save;
    /* total_error_* are 64-bits to avoid overflow when encoding
     * erratic signals when the bits-per-sample and blocksize are
     * large.
     */
    FLAC__uint64 total_error_0 = 0, total_error_1 = 0, total_error_2 = 0, total_error_3 = 0, total_error_4 = 0;
    unsigned i, order;

    for(i = 0; i < data_len; i++) {
        error  = data[i]     ; total_error_0 += local_abs(error);                      save = error;
        error -= last_error_0; total_error_1 += local_abs(error); last_error_0 = save; save = error;
        error -= last_error_1; total_error_2 += local_abs(error); last_error_1 = save; save = error;
        error -= last_error_2; total_error_3 += local_abs(error); last_error_2 = save; save = error;
        error -= last_error_3; total_error_4 += local_abs(error); last_error_3 = save;
    }

    if(total_error_0 < std::min(std::min(std::min(total_error_1, total_error_2), total_error_3), total_error_4))
        order = 0;
    else if(total_error_1 < std::min(std::min(total_error_2, total_error_3), total_error_4))
        order = 1;
    else if(total_error_2 < std::min(total_error_3, total_error_4))
        order = 2;
    else if(total_error_3 < total_error_4)
        order = 3;
    else
        order = 4;

    /* Estimate the expected number of bits per residual signal sample. */
    /* 'total_error*' is linearly related to the variance of the residual */
    /* signal, so we use it directly to compute E(|x|) */
    CHOC_ASSERT(data_len > 0 || total_error_0 == 0);
    CHOC_ASSERT(data_len > 0 || total_error_1 == 0);
    CHOC_ASSERT(data_len > 0 || total_error_2 == 0);
    CHOC_ASSERT(data_len > 0 || total_error_3 == 0);
    CHOC_ASSERT(data_len > 0 || total_error_4 == 0);
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    residual_bits_per_sample[0] = (FLAC__float)((total_error_0 > 0) ? log(M_LN2 * (FLAC__double)total_error_0 / (FLAC__double)data_len) / M_LN2 : 0.0);
    residual_bits_per_sample[1] = (FLAC__float)((total_error_1 > 0) ? log(M_LN2 * (FLAC__double)total_error_1 / (FLAC__double)data_len) / M_LN2 : 0.0);
    residual_bits_per_sample[2] = (FLAC__float)((total_error_2 > 0) ? log(M_LN2 * (FLAC__double)total_error_2 / (FLAC__double)data_len) / M_LN2 : 0.0);
    residual_bits_per_sample[3] = (FLAC__float)((total_error_3 > 0) ? log(M_LN2 * (FLAC__double)total_error_3 / (FLAC__double)data_len) / M_LN2 : 0.0);
    residual_bits_per_sample[4] = (FLAC__float)((total_error_4 > 0) ? log(M_LN2 * (FLAC__double)total_error_4 / (FLAC__double)data_len) / M_LN2 : 0.0);
#else
    residual_bits_per_sample[0] = (total_error_0 > 0) ? local__compute_rbps_wide_integerized(total_error_0, data_len) : 0;
    residual_bits_per_sample[1] = (total_error_1 > 0) ? local__compute_rbps_wide_integerized(total_error_1, data_len) : 0;
    residual_bits_per_sample[2] = (total_error_2 > 0) ? local__compute_rbps_wide_integerized(total_error_2, data_len) : 0;
    residual_bits_per_sample[3] = (total_error_3 > 0) ? local__compute_rbps_wide_integerized(total_error_3, data_len) : 0;
    residual_bits_per_sample[4] = (total_error_4 > 0) ? local__compute_rbps_wide_integerized(total_error_4, data_len) : 0;
#endif

    return order;
}

inline void FLAC__fixed_compute_residual(const FLAC__int32 data[], unsigned data_len, unsigned order, FLAC__int32 residual[])
{
    const int idata_len = (int)data_len;
    int i;

    switch(order) {
        case 0:
            CHOC_ASSERT(sizeof(residual[0]) == sizeof(data[0]));
            memcpy(residual, data, sizeof(residual[0])*data_len);
            break;
        case 1:
            for(i = 0; i < idata_len; i++)
                residual[i] = data[i] - data[i-1];
            break;
        case 2:
            for(i = 0; i < idata_len; i++)
                residual[i] = data[i] - (data[i-1] << 1) + data[i-2];
            break;
        case 3:
            for(i = 0; i < idata_len; i++)
                residual[i] = data[i] - (signedLeftShift ((data[i-1]-data[i-2]), 1) + (data[i-1]-data[i-2])) - data[i-3];
            break;
        case 4:
            for(i = 0; i < idata_len; i++)
                residual[i] = data[i] - signedLeftShift ((data[i-1]+data[i-3]), 2) + (signedLeftShift (data[i-2], 2) + signedLeftShift (data[i-2], 1)) + data[i-4];
            break;
        default:
            CHOC_ASSERT(0);
    }
}

inline void FLAC__fixed_restore_signal(const FLAC__int32 residual[], unsigned data_len, unsigned order, FLAC__int32 data[])
{
    int i, idata_len = (int)data_len;

    switch(order) {
        case 0:
            CHOC_ASSERT(sizeof(residual[0]) == sizeof(data[0]));
            memcpy(data, residual, sizeof(residual[0])*data_len);
            break;
        case 1:
            for(i = 0; i < idata_len; i++)
                data[i] = residual[i] + data[i-1];
            break;
        case 2:
            for(i = 0; i < idata_len; i++)
                data[i] = residual[i] + (data[i-1]<<1) - data[i-2];
            break;
        case 3:
            for(i = 0; i < idata_len; i++)
                data[i] = residual[i] + (((data[i-1]-data[i-2])<<1) + (data[i-1]-data[i-2])) + data[i-3];
            break;
        case 4:
            for(i = 0; i < idata_len; i++)
                data[i] = residual[i] + signedLeftShift ((data[i-1]+data[i-3]), 2) - (signedLeftShift (data[i-2], 2) + signedLeftShift (data[i-2], 1)) - data[i-4];
            break;
        default:
            CHOC_ASSERT(0);
    }
}


/* VERSION should come from configure */
FLAC_API const char *FLAC__VERSION_STRING = FLAC_VERSION;

FLAC_API const char *FLAC__VENDOR_STRING = "reference libFLAC " FLAC_VERSION " 20141125";

FLAC_API const FLAC__byte FLAC__STREAM_SYNC_STRING[4] = { 'f','L','a','C' };
FLAC_API const unsigned FLAC__STREAM_SYNC = 0x664C6143;
FLAC_API const unsigned FLAC__STREAM_SYNC_LEN = 32; /* bits */

FLAC_API const unsigned FLAC__STREAM_METADATA_STREAMINFO_MIN_BLOCK_SIZE_LEN = 16; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN = 16; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN = 24; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_STREAMINFO_MAX_FRAME_SIZE_LEN = 24; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_STREAMINFO_SAMPLE_RATE_LEN = 20; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN = 3; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN = 5; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_STREAMINFO_TOTAL_SAMPLES_LEN = 36; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_STREAMINFO_MD5SUM_LEN = 128; /* bits */

FLAC_API const unsigned FLAC__STREAM_METADATA_APPLICATION_ID_LEN = 32; /* bits */

FLAC_API const unsigned FLAC__STREAM_METADATA_SEEKPOINT_SAMPLE_NUMBER_LEN = 64; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_SEEKPOINT_STREAM_OFFSET_LEN = 64; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_SEEKPOINT_FRAME_SAMPLES_LEN = 16; /* bits */

FLAC_API const FLAC__uint64 FLAC__STREAM_METADATA_SEEKPOINT_PLACEHOLDER = FLAC__U64L(0xffffffffffffffff);

FLAC_API const unsigned FLAC__STREAM_METADATA_VORBIS_COMMENT_ENTRY_LENGTH_LEN = 32; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_VORBIS_COMMENT_NUM_COMMENTS_LEN = 32; /* bits */

FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_INDEX_OFFSET_LEN = 64; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_INDEX_NUMBER_LEN = 8; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_INDEX_RESERVED_LEN = 3*8; /* bits */

FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_TRACK_OFFSET_LEN = 64; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_TRACK_NUMBER_LEN = 8; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_TRACK_ISRC_LEN = 12*8; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_TRACK_TYPE_LEN = 1; /* bit */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_TRACK_PRE_EMPHASIS_LEN = 1; /* bit */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_TRACK_RESERVED_LEN = 6+13*8; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_TRACK_NUM_INDICES_LEN = 8; /* bits */

FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_MEDIA_CATALOG_NUMBER_LEN = 128*8; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_LEAD_IN_LEN = 64; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_IS_CD_LEN = 1; /* bit */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_RESERVED_LEN = 7+258*8; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_CUESHEET_NUM_TRACKS_LEN = 8; /* bits */

FLAC_API const unsigned FLAC__STREAM_METADATA_PICTURE_TYPE_LEN = 32; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_PICTURE_MIME_TYPE_LENGTH_LEN = 32; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_PICTURE_DESCRIPTION_LENGTH_LEN = 32; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_PICTURE_WIDTH_LEN = 32; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_PICTURE_HEIGHT_LEN = 32; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_PICTURE_DEPTH_LEN = 32; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_PICTURE_COLORS_LEN = 32; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_PICTURE_DATA_LENGTH_LEN = 32; /* bits */

FLAC_API const unsigned FLAC__STREAM_METADATA_IS_LAST_LEN = 1; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_TYPE_LEN = 7; /* bits */
FLAC_API const unsigned FLAC__STREAM_METADATA_LENGTH_LEN = 24; /* bits */

FLAC_API const unsigned FLAC__FRAME_HEADER_SYNC = 0x3ffe;
FLAC_API const unsigned FLAC__FRAME_HEADER_SYNC_LEN = 14; /* bits */
FLAC_API const unsigned FLAC__FRAME_HEADER_RESERVED_LEN = 1; /* bits */
FLAC_API const unsigned FLAC__FRAME_HEADER_BLOCKING_STRATEGY_LEN = 1; /* bits */
FLAC_API const unsigned FLAC__FRAME_HEADER_BLOCK_SIZE_LEN = 4; /* bits */
FLAC_API const unsigned FLAC__FRAME_HEADER_SAMPLE_RATE_LEN = 4; /* bits */
FLAC_API const unsigned FLAC__FRAME_HEADER_CHANNEL_ASSIGNMENT_LEN = 4; /* bits */
FLAC_API const unsigned FLAC__FRAME_HEADER_BITS_PER_SAMPLE_LEN = 3; /* bits */
FLAC_API const unsigned FLAC__FRAME_HEADER_ZERO_PAD_LEN = 1; /* bits */
FLAC_API const unsigned FLAC__FRAME_HEADER_CRC_LEN = 8; /* bits */

FLAC_API const unsigned FLAC__FRAME_FOOTER_CRC_LEN = 16; /* bits */

FLAC_API const unsigned FLAC__ENTROPY_CODING_METHOD_TYPE_LEN = 2; /* bits */
FLAC_API const unsigned FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ORDER_LEN = 4; /* bits */
FLAC_API const unsigned FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN = 4; /* bits */
FLAC_API const unsigned FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_PARAMETER_LEN = 5; /* bits */
FLAC_API const unsigned FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_RAW_LEN = 5; /* bits */

FLAC_API const unsigned FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ESCAPE_PARAMETER = 15; /* == (1<<FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN)-1 */
FLAC_API const unsigned FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_ESCAPE_PARAMETER = 31; /* == (1<<FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_PARAMETER_LEN)-1 */

FLAC_API const char * const FLAC__EntropyCodingMethodTypeString[] = {
    "PARTITIONED_RICE",
    "PARTITIONED_RICE2"
};

FLAC_API const unsigned FLAC__SUBFRAME_LPC_QLP_COEFF_PRECISION_LEN = 4; /* bits */
FLAC_API const unsigned FLAC__SUBFRAME_LPC_QLP_SHIFT_LEN = 5; /* bits */

FLAC_API const unsigned FLAC__SUBFRAME_ZERO_PAD_LEN = 1; /* bits */
FLAC_API const unsigned FLAC__SUBFRAME_TYPE_LEN = 6; /* bits */
FLAC_API const unsigned FLAC__SUBFRAME_WASTED_BITS_FLAG_LEN = 1; /* bits */

FLAC_API const unsigned FLAC__SUBFRAME_TYPE_CONSTANT_BYTE_ALIGNED_MASK = 0x00;
FLAC_API const unsigned FLAC__SUBFRAME_TYPE_VERBATIM_BYTE_ALIGNED_MASK = 0x02;
FLAC_API const unsigned FLAC__SUBFRAME_TYPE_FIXED_BYTE_ALIGNED_MASK = 0x10;
FLAC_API const unsigned FLAC__SUBFRAME_TYPE_LPC_BYTE_ALIGNED_MASK = 0x40;

FLAC_API const char * const FLAC__SubframeTypeString[] = {
    "CONSTANT",
    "VERBATIM",
    "FIXED",
    "LPC"
};

FLAC_API const char * const FLAC__ChannelAssignmentString[] = {
    "INDEPENDENT",
    "LEFT_SIDE",
    "RIGHT_SIDE",
    "MID_SIDE"
};

FLAC_API const char * const FLAC__FrameNumberTypeString[] = {
    "FRAME_NUMBER_TYPE_FRAME_NUMBER",
    "FRAME_NUMBER_TYPE_SAMPLE_NUMBER"
};

FLAC_API const char * const FLAC__MetadataTypeString[] = {
    "STREAMINFO",
    "PADDING",
    "APPLICATION",
    "SEEKTABLE",
    "VORBIS_COMMENT",
    "CUESHEET",
    "PICTURE"
};

FLAC_API const char * const FLAC__StreamMetadata_Picture_TypeString[] = {
    "Other",
    "32x32 pixels 'file icon' (PNG only)",
    "Other file icon",
    "Cover (front)",
    "Cover (back)",
    "Leaflet page",
    "Media (e.g. label side of CD)",
    "Lead artist/lead performer/soloist",
    "Artist/performer",
    "Conductor",
    "Band/Orchestra",
    "Composer",
    "Lyricist/text writer",
    "Recording Location",
    "During recording",
    "During performance",
    "Movie/video screen capture",
    "A bright coloured fish",
    "Illustration",
    "Band/artist logotype",
    "Publisher/Studio logotype"
};

FLAC_API FLAC__bool FLAC__format_sample_rate_is_valid(unsigned sample_rate)
{
    if(sample_rate == 0 || sample_rate > FLAC__MAX_SAMPLE_RATE) {
        return false;
    }
    else
        return true;
}

FLAC_API FLAC__bool FLAC__format_blocksize_is_subset(unsigned blocksize, unsigned sample_rate)
{
    if(blocksize > 16384)
        return false;
    else if(sample_rate <= 48000 && blocksize > 4608)
        return false;
    else
        return true;
}

FLAC_API FLAC__bool FLAC__format_sample_rate_is_subset(unsigned sample_rate)
{
    if(
        !FLAC__format_sample_rate_is_valid(sample_rate) ||
        (
            sample_rate >= (1u << 16) &&
            !(sample_rate % 1000 == 0 || sample_rate % 10 == 0)
        )
    ) {
        return false;
    }
    else
        return true;
}

/* @@@@ add to unit tests; it is already indirectly tested by the metadata_object tests */
FLAC_API FLAC__bool FLAC__format_seektable_is_legal(const FLAC__StreamMetadata_SeekTable *seek_table)
{
    unsigned i;
    FLAC__uint64 prev_sample_number = 0;
    FLAC__bool got_prev = false;

    CHOC_ASSERT(0 != seek_table);

    for(i = 0; i < seek_table->num_points; i++) {
        if(got_prev) {
            if(
                seek_table->points[i].sample_number != FLAC__STREAM_METADATA_SEEKPOINT_PLACEHOLDER &&
                seek_table->points[i].sample_number <= prev_sample_number
            )
                return false;
        }
        prev_sample_number = seek_table->points[i].sample_number;
        got_prev = true;
    }

    return true;
}

/* used as the sort predicate for qsort() */
static int seekpoint_compare_(const FLAC__StreamMetadata_SeekPoint *l, const FLAC__StreamMetadata_SeekPoint *r)
{
    /* we don't just 'return l->sample_number - r->sample_number' since the result (FLAC__int64) might overflow an 'int' */
    if(l->sample_number == r->sample_number)
        return 0;
    else if(l->sample_number < r->sample_number)
        return -1;
    else
        return 1;
}

/* @@@@ add to unit tests; it is already indirectly tested by the metadata_object tests */
FLAC_API unsigned FLAC__format_seektable_sort(FLAC__StreamMetadata_SeekTable *seek_table)
{
    unsigned i, j;
    FLAC__bool first;

    CHOC_ASSERT(0 != seek_table);

    /* sort the seekpoints */
    qsort(seek_table->points, seek_table->num_points, sizeof(FLAC__StreamMetadata_SeekPoint), (int (*)(const void *, const void *))seekpoint_compare_);

    /* uniquify the seekpoints */
    first = true;
    for(i = j = 0; i < seek_table->num_points; i++) {
        if(seek_table->points[i].sample_number != FLAC__STREAM_METADATA_SEEKPOINT_PLACEHOLDER) {
            if(!first) {
                if(seek_table->points[i].sample_number == seek_table->points[j-1].sample_number)
                    continue;
            }
        }
        first = false;
        seek_table->points[j++] = seek_table->points[i];
    }

    for(i = j; i < seek_table->num_points; i++) {
        seek_table->points[i].sample_number = FLAC__STREAM_METADATA_SEEKPOINT_PLACEHOLDER;
        seek_table->points[i].stream_offset = 0;
        seek_table->points[i].frame_samples = 0;
    }

    return j;
}

/*
 * also disallows non-shortest-form encodings, c.f.
 *   http://www.unicode.org/versions/corrigendum1.html
 * and a more clear explanation at the end of this section:
 *   http://www.cl.cam.ac.uk/~mgk25/unicode.html#utf-8
 */
static unsigned utf8len_(const FLAC__byte *utf8)
{
    CHOC_ASSERT(0 != utf8);
    if ((utf8[0] & 0x80) == 0) {
        return 1;
    }
    else if ((utf8[0] & 0xE0) == 0xC0 && (utf8[1] & 0xC0) == 0x80) {
        if ((utf8[0] & 0xFE) == 0xC0) /* overlong sequence check */
            return 0;
        return 2;
    }
    else if ((utf8[0] & 0xF0) == 0xE0 && (utf8[1] & 0xC0) == 0x80 && (utf8[2] & 0xC0) == 0x80) {
        if (utf8[0] == 0xE0 && (utf8[1] & 0xE0) == 0x80) /* overlong sequence check */
            return 0;
        /* illegal surrogates check (U+D800...U+DFFF and U+FFFE...U+FFFF) */
        if (utf8[0] == 0xED && (utf8[1] & 0xE0) == 0xA0) /* D800-DFFF */
            return 0;
        if (utf8[0] == 0xEF && utf8[1] == 0xBF && (utf8[2] & 0xFE) == 0xBE) /* FFFE-FFFF */
            return 0;
        return 3;
    }
    else if ((utf8[0] & 0xF8) == 0xF0 && (utf8[1] & 0xC0) == 0x80 && (utf8[2] & 0xC0) == 0x80 && (utf8[3] & 0xC0) == 0x80) {
        if (utf8[0] == 0xF0 && (utf8[1] & 0xF0) == 0x80) /* overlong sequence check */
            return 0;
        return 4;
    }
    else if ((utf8[0] & 0xFC) == 0xF8 && (utf8[1] & 0xC0) == 0x80 && (utf8[2] & 0xC0) == 0x80 && (utf8[3] & 0xC0) == 0x80 && (utf8[4] & 0xC0) == 0x80) {
        if (utf8[0] == 0xF8 && (utf8[1] & 0xF8) == 0x80) /* overlong sequence check */
            return 0;
        return 5;
    }
    else if ((utf8[0] & 0xFE) == 0xFC && (utf8[1] & 0xC0) == 0x80 && (utf8[2] & 0xC0) == 0x80 && (utf8[3] & 0xC0) == 0x80 && (utf8[4] & 0xC0) == 0x80 && (utf8[5] & 0xC0) == 0x80) {
        if (utf8[0] == 0xFC && (utf8[1] & 0xFC) == 0x80) /* overlong sequence check */
            return 0;
        return 6;
    }
    else {
        return 0;
    }
}

FLAC_API FLAC__bool FLAC__format_vorbiscomment_entry_name_is_legal(const char *name)
{
    char c;
    for(c = *name; c; c = *(++name))
        if(c < 0x20 || c == 0x3d || c > 0x7d)
            return false;
    return true;
}

FLAC_API FLAC__bool FLAC__format_vorbiscomment_entry_value_is_legal(const FLAC__byte *value, unsigned length)
{
    if(length == (unsigned)(-1)) {
        while(*value) {
            unsigned n = utf8len_(value);
            if(n == 0)
                return false;
            value += n;
        }
    }
    else {
        const FLAC__byte *end = value + length;
        while(value < end) {
            unsigned n = utf8len_(value);
            if(n == 0)
                return false;
            value += n;
        }
        if(value != end)
            return false;
    }
    return true;
}

FLAC_API FLAC__bool FLAC__format_vorbiscomment_entry_is_legal(const FLAC__byte *entry, unsigned length)
{
    const FLAC__byte *s, *end;

    for(s = entry, end = s + length; s < end && *s != '='; s++) {
        if(*s < 0x20 || *s > 0x7D)
            return false;
    }
    if(s == end)
        return false;

    s++; /* skip '=' */

    while(s < end) {
        unsigned n = utf8len_(s);
        if(n == 0)
            return false;
        s += n;
    }
    if(s != end)
        return false;

    return true;
}

/* @@@@ add to unit tests; it is already indirectly tested by the metadata_object tests */
FLAC_API FLAC__bool FLAC__format_cuesheet_is_legal(const FLAC__StreamMetadata_CueSheet *cue_sheet, FLAC__bool check_cd_da_subset, const char **violation)
{
    unsigned i, j;

    if(check_cd_da_subset) {
        if(cue_sheet->lead_in < 2 * 44100) {
            if(violation) *violation = "CD-DA cue sheet must have a lead-in length of at least 2 seconds";
            return false;
        }
        if(cue_sheet->lead_in % 588 != 0) {
            if(violation) *violation = "CD-DA cue sheet lead-in length must be evenly divisible by 588 samples";
            return false;
        }
    }

    if(cue_sheet->num_tracks == 0) {
        if(violation) *violation = "cue sheet must have at least one track (the lead-out)";
        return false;
    }

    if(check_cd_da_subset && cue_sheet->tracks[cue_sheet->num_tracks-1].number != 170) {
        if(violation) *violation = "CD-DA cue sheet must have a lead-out track number 170 (0xAA)";
        return false;
    }

    for(i = 0; i < cue_sheet->num_tracks; i++) {
        if(cue_sheet->tracks[i].number == 0) {
            if(violation) *violation = "cue sheet may not have a track number 0";
            return false;
        }

        if(check_cd_da_subset) {
            if(!((cue_sheet->tracks[i].number >= 1 && cue_sheet->tracks[i].number <= 99) || cue_sheet->tracks[i].number == 170)) {
                if(violation) *violation = "CD-DA cue sheet track number must be 1-99 or 170";
                return false;
            }
        }

        if(check_cd_da_subset && cue_sheet->tracks[i].offset % 588 != 0) {
            if(violation) {
                if(i == cue_sheet->num_tracks-1) /* the lead-out track... */
                    *violation = "CD-DA cue sheet lead-out offset must be evenly divisible by 588 samples";
                else
                    *violation = "CD-DA cue sheet track offset must be evenly divisible by 588 samples";
            }
            return false;
        }

        if(i < cue_sheet->num_tracks - 1) {
            if(cue_sheet->tracks[i].num_indices == 0) {
                if(violation) *violation = "cue sheet track must have at least one index point";
                return false;
            }

            if(cue_sheet->tracks[i].indices[0].number > 1) {
                if(violation) *violation = "cue sheet track's first index number must be 0 or 1";
                return false;
            }
        }

        for(j = 0; j < cue_sheet->tracks[i].num_indices; j++) {
            if(check_cd_da_subset && cue_sheet->tracks[i].indices[j].offset % 588 != 0) {
                if(violation) *violation = "CD-DA cue sheet track index offset must be evenly divisible by 588 samples";
                return false;
            }

            if(j > 0) {
                if(cue_sheet->tracks[i].indices[j].number != cue_sheet->tracks[i].indices[j-1].number + 1) {
                    if(violation) *violation = "cue sheet track index numbers must increase by 1";
                    return false;
                }
            }
        }
    }

    return true;
}

/* @@@@ add to unit tests; it is already indirectly tested by the metadata_object tests */
FLAC_API FLAC__bool FLAC__format_picture_is_legal(const FLAC__StreamMetadata_Picture *picture, const char **violation)
{
    char *p;
    FLAC__byte *b;

    for(p = picture->mime_type; *p; p++) {
        if(*p < 0x20 || *p > 0x7e) {
            if(violation) *violation = "MIME type string must contain only printable ASCII characters (0x20-0x7e)";
            return false;
        }
    }

    for(b = picture->description; *b; ) {
        unsigned n = utf8len_(b);
        if(n == 0) {
            if(violation) *violation = "description string must be valid UTF-8";
            return false;
        }
        b += n;
    }

    return true;
}

/*
 * These routines are private to libFLAC
 */
inline unsigned FLAC__format_get_max_rice_partition_order(unsigned blocksize, unsigned predictor_order)
{
    return
        FLAC__format_get_max_rice_partition_order_from_blocksize_limited_max_and_predictor_order(
            FLAC__format_get_max_rice_partition_order_from_blocksize(blocksize),
            blocksize,
            predictor_order
        );
}

inline unsigned FLAC__format_get_max_rice_partition_order_from_blocksize(unsigned blocksize)
{
    unsigned max_rice_partition_order = 0;
    while(!(blocksize & 1)) {
        max_rice_partition_order++;
        blocksize >>= 1;
    }
    return std::min(FLAC__MAX_RICE_PARTITION_ORDER, max_rice_partition_order);
}

inline unsigned FLAC__format_get_max_rice_partition_order_from_blocksize_limited_max_and_predictor_order(unsigned limit, unsigned blocksize, unsigned predictor_order)
{
    unsigned max_rice_partition_order = limit;

    while(max_rice_partition_order > 0 && (blocksize >> max_rice_partition_order) <= predictor_order)
        max_rice_partition_order--;

    CHOC_ASSERT(
        (max_rice_partition_order == 0 && blocksize >= predictor_order) ||
        (max_rice_partition_order > 0 && blocksize >> max_rice_partition_order > predictor_order)
    );

    return max_rice_partition_order;
}

inline void FLAC__format_entropy_coding_method_partitioned_rice_contents_init(FLAC__EntropyCodingMethod_PartitionedRiceContents *object)
{
    CHOC_ASSERT(0 != object);

    object->parameters = 0;
    object->raw_bits = 0;
    object->capacity_by_order = 0;
}

inline void FLAC__format_entropy_coding_method_partitioned_rice_contents_clear(FLAC__EntropyCodingMethod_PartitionedRiceContents *object)
{
    CHOC_ASSERT(0 != object);

    if(0 != object->parameters)
        free(object->parameters);
    if(0 != object->raw_bits)
        free(object->raw_bits);
    FLAC__format_entropy_coding_method_partitioned_rice_contents_init(object);
}

inline FLAC__bool FLAC__format_entropy_coding_method_partitioned_rice_contents_ensure_size(FLAC__EntropyCodingMethod_PartitionedRiceContents *object, unsigned max_partition_order)
{
    CHOC_ASSERT(0 != object);

    CHOC_ASSERT(object->capacity_by_order > 0 || (0 == object->parameters && 0 == object->raw_bits));

    if(object->capacity_by_order < max_partition_order) {
        if(0 == (object->parameters = (unsigned int*) realloc(object->parameters, sizeof(unsigned)*(1 << max_partition_order))))
            return false;
        if(0 == (object->raw_bits = (unsigned int*) realloc(object->raw_bits, sizeof(unsigned)*(1 << max_partition_order))))
            return false;
        memset(object->raw_bits, 0, sizeof(unsigned)*(1 << max_partition_order));
        object->capacity_by_order = max_partition_order;
    }

    return true;
}


/* OPT: #undef'ing this may improve the speed on some architectures */
#define FLAC__LPC_UNROLLED_FILTER_LOOPS

#ifndef FLAC__INTEGER_ONLY_LIBRARY

#if defined(_MSC_VER)
static inline long int lround(double x) {
    return (long)(x + _copysign (0.5, x));
}
/* If this fails, we are in the presence of a mid 90's compiler, move along... */
#endif

inline void FLAC__lpc_window_data(const FLAC__int32 in[], const FLAC__real window[], FLAC__real out[], unsigned data_len)
{
    unsigned i;
    for(i = 0; i < data_len; i++)
        out[i] = in[i] * window[i];
}

inline void FLAC__lpc_compute_autocorrelation(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[])
{
    /*
     * this version tends to run faster because of better data locality
     * ('data_len' is usually much larger than 'lag')
     */
    FLAC__real d;
    unsigned sample, coeff;
    const unsigned limit = data_len - lag;

    CHOC_ASSERT(lag > 0);
    CHOC_ASSERT(lag <= data_len);

    for(coeff = 0; coeff < lag; coeff++)
        autoc[coeff] = 0.0;
    for(sample = 0; sample <= limit; sample++) {
        d = data[sample];
        for(coeff = 0; coeff < lag; coeff++)
            autoc[coeff] += d * data[sample+coeff];
    }
    for(; sample < data_len; sample++) {
        d = data[sample];
        for(coeff = 0; coeff < data_len - sample; coeff++)
            autoc[coeff] += d * data[sample+coeff];
    }
}

inline void FLAC__lpc_compute_lp_coefficients(const FLAC__real autoc[], unsigned *max_order, FLAC__real lp_coeff[][FLAC__MAX_LPC_ORDER], FLAC__double error[])
{
    unsigned i, j;
    FLAC__double r, err, lpc[FLAC__MAX_LPC_ORDER];

    CHOC_ASSERT(0 != max_order);
    CHOC_ASSERT(0 < *max_order);
    CHOC_ASSERT(*max_order <= FLAC__MAX_LPC_ORDER);
    CHOC_ASSERT(autoc[0] != 0.0);

    err = autoc[0];

    for(i = 0; i < *max_order; i++) {
        /* Sum up this iteration's reflection coefficient. */
        r = -autoc[i+1];
        for(j = 0; j < i; j++)
            r -= lpc[j] * autoc[i-j];
        r /= err;

        /* Update LPC coefficients and total error. */
        lpc[i]=r;
        for(j = 0; j < (i>>1); j++) {
            FLAC__double tmp = lpc[j];
            lpc[j] += r * lpc[i-1-j];
            lpc[i-1-j] += r * tmp;
        }
        if(i & 1)
            lpc[j] += lpc[j] * r;

        err *= (1.0 - r * r);

        /* save this order */
        for(j = 0; j <= i; j++)
            lp_coeff[i][j] = (FLAC__real)(-lpc[j]); /* negate FIR filter coeff to get predictor coeff */
        error[i] = err;

        /* see SF bug https://sourceforge.net/p/flac/bugs/234/ */
        if(err == 0.0) {
            *max_order = i+1;
            return;
        }
    }
}

inline int FLAC__lpc_quantize_coefficients(const FLAC__real lp_coeff[], unsigned order, unsigned precision, FLAC__int32 qlp_coeff[], int *shift)
{
    unsigned i;
    FLAC__double cmax;
    FLAC__int32 qmax, qmin;

    CHOC_ASSERT(precision > 0);
    CHOC_ASSERT(precision >= FLAC__MIN_QLP_COEFF_PRECISION);

    /* drop one bit for the sign; from here on out we consider only |lp_coeff[i]| */
    precision--;
    qmax = 1 << precision;
    qmin = -qmax;
    qmax--;

    /* calc cmax = max( |lp_coeff[i]| ) */
    cmax = 0.0;
    for(i = 0; i < order; i++) {
        const FLAC__double d = fabs(lp_coeff[i]);
        if(d > cmax)
            cmax = d;
    }

    if(cmax <= 0.0) {
        /* => coefficients are all 0, which means our constant-detect didn't work */
        return 2;
    }
    else {
        const int max_shiftlimit = (1 << (FLAC__SUBFRAME_LPC_QLP_SHIFT_LEN-1)) - 1;
        const int min_shiftlimit = -max_shiftlimit - 1;
        int log2cmax;

        (void)frexp(cmax, &log2cmax);
        log2cmax--;
        *shift = (int)precision - log2cmax - 1;

        if(*shift > max_shiftlimit)
            *shift = max_shiftlimit;
        else if(*shift < min_shiftlimit)
            return 1;
    }

    if(*shift >= 0) {
        FLAC__double error = 0.0;
        FLAC__int32 q;
        for(i = 0; i < order; i++) {
            error += lp_coeff[i] * (1 << *shift);
            q = lround(error);

#ifdef FLAC__OVERFLOW_DETECT
            if(q > qmax+1) /* we expect q==qmax+1 occasionally due to rounding */
                fprintf(stderr,"FLAC__lpc_quantize_coefficients: quantizer overflow: q>qmax %d>%d shift=%d cmax=%f precision=%u lpc[%u]=%f\n",q,qmax,*shift,cmax,precision+1,i,lp_coeff[i]);
            else if(q < qmin)
                fprintf(stderr,"FLAC__lpc_quantize_coefficients: quantizer overflow: q<qmin %d<%d shift=%d cmax=%f precision=%u lpc[%u]=%f\n",q,qmin,*shift,cmax,precision+1,i,lp_coeff[i]);
#endif
            if(q > qmax)
                q = qmax;
            else if(q < qmin)
                q = qmin;
            error -= q;
            qlp_coeff[i] = q;
        }
    }
    /* negative shift is very rare but due to design flaw, negative shift is
     * a NOP in the decoder, so it must be handled specially by scaling down
     * coeffs
     */
    else {
        const int nshift = -(*shift);
        FLAC__double error = 0.0;
        FLAC__int32 q;

        for(i = 0; i < order; i++) {
            error += lp_coeff[i] / (1 << nshift);
            q = lround(error);
#ifdef FLAC__OVERFLOW_DETECT
            if(q > qmax+1) /* we expect q==qmax+1 occasionally due to rounding */
                fprintf(stderr,"FLAC__lpc_quantize_coefficients: quantizer overflow: q>qmax %d>%d shift=%d cmax=%f precision=%u lpc[%u]=%f\n",q,qmax,*shift,cmax,precision+1,i,lp_coeff[i]);
            else if(q < qmin)
                fprintf(stderr,"FLAC__lpc_quantize_coefficients: quantizer overflow: q<qmin %d<%d shift=%d cmax=%f precision=%u lpc[%u]=%f\n",q,qmin,*shift,cmax,precision+1,i,lp_coeff[i]);
#endif
            if(q > qmax)
                q = qmax;
            else if(q < qmin)
                q = qmin;
            error -= q;
            qlp_coeff[i] = q;
        }
        *shift = 0;
    }

    return 0;
}

#if defined(_MSC_VER)
// silence MSVC warnings about __restrict modifier
#pragma warning ( disable : 4028 )
#endif

inline void FLAC__lpc_compute_residual_from_qlp_coefficients(const FLAC__int32 * flac_restrict data, unsigned data_len, const FLAC__int32 * flac_restrict qlp_coeff, unsigned order, int lp_quantization, FLAC__int32 * flac_restrict residual)
#if defined(FLAC__OVERFLOW_DETECT) || !defined(FLAC__LPC_UNROLLED_FILTER_LOOPS)
{
    FLAC__int64 sumo;
    unsigned i, j;
    FLAC__int32 sum;
    const FLAC__int32 *history;

#ifdef FLAC__OVERFLOW_DETECT_VERBOSE
    fprintf(stderr,"FLAC__lpc_compute_residual_from_qlp_coefficients: data_len=%d, order=%u, lpq=%d",data_len,order,lp_quantization);
    for(i=0;i<order;i++)
        fprintf(stderr,", q[%u]=%d",i,qlp_coeff[i]);
    fprintf(stderr,"\n");
#endif
    CHOC_ASSERT(order > 0);

    for(i = 0; i < data_len; i++) {
        sumo = 0;
        sum = 0;
        history = data;
        for(j = 0; j < order; j++) {
            sum += qlp_coeff[j] * (*(--history));
            sumo += (FLAC__int64)qlp_coeff[j] * (FLAC__int64)(*history);
                fprintf(stderr,"FLAC__lpc_compute_residual_from_qlp_coefficients: OVERFLOW, i=%u, j=%u, c=%d, d=%d, sumo=%" PRId64 "\n",i,j,qlp_coeff[j],*history,sumo);
        }
        *(residual++) = *(data++) - (sum >> lp_quantization);
    }

    /* Here's a slower but clearer version:
    for(i = 0; i < data_len; i++) {
        sum = 0;
        for(j = 0; j < order; j++)
            sum += qlp_coeff[j] * data[i-j-1];
        residual[i] = data[i] - (sum >> lp_quantization);
    }
    */
}
#else /* fully unrolled version for normal use */
{
    int i;
    FLAC__int32 sum;

    CHOC_ASSERT(order > 0);
    CHOC_ASSERT(order <= 32);

    /*
     * We do unique versions up to 12th order since that's the subset limit.
     * Also they are roughly ordered to match frequency of occurrence to
     * minimize branching.
     */
    if(order <= 12) {
        if(order > 8) {
            if(order > 10) {
                if(order == 12) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[11] * data[i-12];
                        sum += qlp_coeff[10] * data[i-11];
                        sum += qlp_coeff[9] * data[i-10];
                        sum += qlp_coeff[8] * data[i-9];
                        sum += qlp_coeff[7] * data[i-8];
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        residual[i] = data[i] - (sum >> lp_quantization);
                    }
                }
                else { /* order == 11 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[10] * data[i-11];
                        sum += qlp_coeff[9] * data[i-10];
                        sum += qlp_coeff[8] * data[i-9];
                        sum += qlp_coeff[7] * data[i-8];
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        residual[i] = data[i] - (sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 10) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[9] * data[i-10];
                        sum += qlp_coeff[8] * data[i-9];
                        sum += qlp_coeff[7] * data[i-8];
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        residual[i] = data[i] - (sum >> lp_quantization);
                    }
                }
                else { /* order == 9 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[8] * data[i-9];
                        sum += qlp_coeff[7] * data[i-8];
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        residual[i] = data[i] - (sum >> lp_quantization);
                    }
                }
            }
        }
        else if(order > 4) {
            if(order > 6) {
                if(order == 8) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[7] * data[i-8];
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        residual[i] = data[i] - (sum >> lp_quantization);
                    }
                }
                else { /* order == 7 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        residual[i] = data[i] - (sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 6) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        residual[i] = data[i] - (sum >> lp_quantization);
                    }
                }
                else { /* order == 5 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        residual[i] = data[i] - (sum >> lp_quantization);
                    }
                }
            }
        }
        else {
            if(order > 2) {
                if(order == 4) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        residual[i] = data[i] - (sum >> lp_quantization);
                    }
                }
                else { /* order == 3 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        residual[i] = data[i] - (sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 2) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        residual[i] = data[i] - (sum >> lp_quantization);
                    }
                }
                else { /* order == 1 */
                    for(i = 0; i < (int)data_len; i++)
                        residual[i] = data[i] - ((qlp_coeff[0] * data[i-1]) >> lp_quantization);
                }
            }
        }
    }
    else { /* order > 12 */
        for(i = 0; i < (int)data_len; i++) {
            sum = 0;
            switch(order) {
                case 32: sum += qlp_coeff[31] * data[i-32];
                case 31: sum += qlp_coeff[30] * data[i-31];
                case 30: sum += qlp_coeff[29] * data[i-30];
                case 29: sum += qlp_coeff[28] * data[i-29];
                case 28: sum += qlp_coeff[27] * data[i-28];
                case 27: sum += qlp_coeff[26] * data[i-27];
                case 26: sum += qlp_coeff[25] * data[i-26];
                case 25: sum += qlp_coeff[24] * data[i-25];
                case 24: sum += qlp_coeff[23] * data[i-24];
                case 23: sum += qlp_coeff[22] * data[i-23];
                case 22: sum += qlp_coeff[21] * data[i-22];
                case 21: sum += qlp_coeff[20] * data[i-21];
                case 20: sum += qlp_coeff[19] * data[i-20];
                case 19: sum += qlp_coeff[18] * data[i-19];
                case 18: sum += qlp_coeff[17] * data[i-18];
                case 17: sum += qlp_coeff[16] * data[i-17];
                case 16: sum += qlp_coeff[15] * data[i-16];
                case 15: sum += qlp_coeff[14] * data[i-15];
                case 14: sum += qlp_coeff[13] * data[i-14];
                case 13: sum += qlp_coeff[12] * data[i-13];
                         sum += qlp_coeff[11] * data[i-12];
                         sum += qlp_coeff[10] * data[i-11];
                         sum += qlp_coeff[ 9] * data[i-10];
                         sum += qlp_coeff[ 8] * data[i- 9];
                         sum += qlp_coeff[ 7] * data[i- 8];
                         sum += qlp_coeff[ 6] * data[i- 7];
                         sum += qlp_coeff[ 5] * data[i- 6];
                         sum += qlp_coeff[ 4] * data[i- 5];
                         sum += qlp_coeff[ 3] * data[i- 4];
                         sum += qlp_coeff[ 2] * data[i- 3];
                         sum += qlp_coeff[ 1] * data[i- 2];
                         sum += qlp_coeff[ 0] * data[i- 1];
            }
            residual[i] = data[i] - (sum >> lp_quantization);
        }
    }
}
#endif

inline void FLAC__lpc_compute_residual_from_qlp_coefficients_wide(const FLAC__int32 * flac_restrict data, unsigned data_len, const FLAC__int32 * flac_restrict qlp_coeff, unsigned order, int lp_quantization, FLAC__int32 * flac_restrict residual)
#if defined(FLAC__OVERFLOW_DETECT) || !defined(FLAC__LPC_UNROLLED_FILTER_LOOPS)
{
    unsigned i, j;
    FLAC__int64 sum;
    const FLAC__int32 *history;

#ifdef FLAC__OVERFLOW_DETECT_VERBOSE
    fprintf(stderr,"FLAC__lpc_compute_residual_from_qlp_coefficients_wide: data_len=%d, order=%u, lpq=%d",data_len,order,lp_quantization);
    for(i=0;i<order;i++)
        fprintf(stderr,", q[%u]=%d",i,qlp_coeff[i]);
    fprintf(stderr,"\n");
#endif
    CHOC_ASSERT(order > 0);

    for(i = 0; i < data_len; i++) {
        sum = 0;
        history = data;
        for(j = 0; j < order; j++)
            sum += (FLAC__int64)qlp_coeff[j] * (FLAC__int64)(*(--history));
        if(FLAC__bitmath_silog2_wide(sum >> lp_quantization) > 32) {
            fprintf(stderr,"FLAC__lpc_compute_residual_from_qlp_coefficients_wide: OVERFLOW, i=%u, sum=%" PRId64 "\n", i, (sum >> lp_quantization));
            break;
        }
        if(FLAC__bitmath_silog2_wide((FLAC__int64)(*data) - (sum >> lp_quantization)) > 32) {
            fprintf(stderr,"FLAC__lpc_compute_residual_from_qlp_coefficients_wide: OVERFLOW, i=%u, data=%d, sum=%" PRId64 ", residual=%" PRId64 "\n", i, *data, (long long)(sum >> lp_quantization), ((FLAC__int64)(*data) - (sum >> lp_quantization)));
            break;
        }
        *(residual++) = *(data++) - (FLAC__int32)(sum >> lp_quantization);
    }
}
#else /* fully unrolled version for normal use */
{
    int i;
    FLAC__int64 sum;

    CHOC_ASSERT(order > 0);
    CHOC_ASSERT(order <= 32);

    /*
     * We do unique versions up to 12th order since that's the subset limit.
     * Also they are roughly ordered to match frequency of occurrence to
     * minimize branching.
     */
    if(order <= 12) {
        if(order > 8) {
            if(order > 10) {
                if(order == 12) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[11] * (FLAC__int64)data[i-12];
                        sum += qlp_coeff[10] * (FLAC__int64)data[i-11];
                        sum += qlp_coeff[9] * (FLAC__int64)data[i-10];
                        sum += qlp_coeff[8] * (FLAC__int64)data[i-9];
                        sum += qlp_coeff[7] * (FLAC__int64)data[i-8];
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 11 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[10] * (FLAC__int64)data[i-11];
                        sum += qlp_coeff[9] * (FLAC__int64)data[i-10];
                        sum += qlp_coeff[8] * (FLAC__int64)data[i-9];
                        sum += qlp_coeff[7] * (FLAC__int64)data[i-8];
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 10) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[9] * (FLAC__int64)data[i-10];
                        sum += qlp_coeff[8] * (FLAC__int64)data[i-9];
                        sum += qlp_coeff[7] * (FLAC__int64)data[i-8];
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 9 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[8] * (FLAC__int64)data[i-9];
                        sum += qlp_coeff[7] * (FLAC__int64)data[i-8];
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
                    }
                }
            }
        }
        else if(order > 4) {
            if(order > 6) {
                if(order == 8) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[7] * (FLAC__int64)data[i-8];
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 7 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 6) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 5 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
                    }
                }
            }
        }
        else {
            if(order > 2) {
                if(order == 4) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 3 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 2) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 1 */
                    for(i = 0; i < (int)data_len; i++)
                        residual[i] = data[i] - (FLAC__int32)((qlp_coeff[0] * (FLAC__int64)data[i-1]) >> lp_quantization);
                }
            }
        }
    }
    else { /* order > 12 */
        for(i = 0; i < (int)data_len; i++) {
            sum = 0;
            switch(order) {
                case 32: sum += qlp_coeff[31] * (FLAC__int64)data[i-32];
                case 31: sum += qlp_coeff[30] * (FLAC__int64)data[i-31];
                case 30: sum += qlp_coeff[29] * (FLAC__int64)data[i-30];
                case 29: sum += qlp_coeff[28] * (FLAC__int64)data[i-29];
                case 28: sum += qlp_coeff[27] * (FLAC__int64)data[i-28];
                case 27: sum += qlp_coeff[26] * (FLAC__int64)data[i-27];
                case 26: sum += qlp_coeff[25] * (FLAC__int64)data[i-26];
                case 25: sum += qlp_coeff[24] * (FLAC__int64)data[i-25];
                case 24: sum += qlp_coeff[23] * (FLAC__int64)data[i-24];
                case 23: sum += qlp_coeff[22] * (FLAC__int64)data[i-23];
                case 22: sum += qlp_coeff[21] * (FLAC__int64)data[i-22];
                case 21: sum += qlp_coeff[20] * (FLAC__int64)data[i-21];
                case 20: sum += qlp_coeff[19] * (FLAC__int64)data[i-20];
                case 19: sum += qlp_coeff[18] * (FLAC__int64)data[i-19];
                case 18: sum += qlp_coeff[17] * (FLAC__int64)data[i-18];
                case 17: sum += qlp_coeff[16] * (FLAC__int64)data[i-17];
                case 16: sum += qlp_coeff[15] * (FLAC__int64)data[i-16];
                case 15: sum += qlp_coeff[14] * (FLAC__int64)data[i-15];
                case 14: sum += qlp_coeff[13] * (FLAC__int64)data[i-14];
                case 13: sum += qlp_coeff[12] * (FLAC__int64)data[i-13];
                         sum += qlp_coeff[11] * (FLAC__int64)data[i-12];
                         sum += qlp_coeff[10] * (FLAC__int64)data[i-11];
                         sum += qlp_coeff[ 9] * (FLAC__int64)data[i-10];
                         sum += qlp_coeff[ 8] * (FLAC__int64)data[i- 9];
                         sum += qlp_coeff[ 7] * (FLAC__int64)data[i- 8];
                         sum += qlp_coeff[ 6] * (FLAC__int64)data[i- 7];
                         sum += qlp_coeff[ 5] * (FLAC__int64)data[i- 6];
                         sum += qlp_coeff[ 4] * (FLAC__int64)data[i- 5];
                         sum += qlp_coeff[ 3] * (FLAC__int64)data[i- 4];
                         sum += qlp_coeff[ 2] * (FLAC__int64)data[i- 3];
                         sum += qlp_coeff[ 1] * (FLAC__int64)data[i- 2];
                         sum += qlp_coeff[ 0] * (FLAC__int64)data[i- 1];
            }
            residual[i] = data[i] - (FLAC__int32)(sum >> lp_quantization);
        }
    }
}
#endif

#endif /* !defined FLAC__INTEGER_ONLY_LIBRARY */

inline void FLAC__lpc_restore_signal(const FLAC__int32 * flac_restrict residual, unsigned data_len, const FLAC__int32 * flac_restrict qlp_coeff, unsigned order, int lp_quantization, FLAC__int32 * flac_restrict data)
#if defined(FLAC__OVERFLOW_DETECT) || !defined(FLAC__LPC_UNROLLED_FILTER_LOOPS)
{
    FLAC__int64 sumo;
    unsigned i, j;
    FLAC__int32 sum;
    const FLAC__int32 *r = residual, *history;

#ifdef FLAC__OVERFLOW_DETECT_VERBOSE
    fprintf(stderr,"FLAC__lpc_restore_signal: data_len=%d, order=%u, lpq=%d",data_len,order,lp_quantization);
    for(i=0;i<order;i++)
        fprintf(stderr,", q[%u]=%d",i,qlp_coeff[i]);
    fprintf(stderr,"\n");
#endif
    CHOC_ASSERT(order > 0);

    for(i = 0; i < data_len; i++) {
        sumo = 0;
        sum = 0;
        history = data;
        for(j = 0; j < order; j++) {
            sum += qlp_coeff[j] * (*(--history));
            sumo += (FLAC__int64)qlp_coeff[j] * (FLAC__int64)(*history);
            if(sumo > 2147483647ll || sumo < -2147483648ll)
                fprintf(stderr,"FLAC__lpc_restore_signal: OVERFLOW, i=%u, j=%u, c=%d, d=%d, sumo=%" PRId64 "\n",i,j,qlp_coeff[j],*history,sumo);
        }
        *(data++) = *(r++) + (sum >> lp_quantization);
    }

    /* Here's a slower but clearer version:
    for(i = 0; i < data_len; i++) {
        sum = 0;
        for(j = 0; j < order; j++)
            sum += qlp_coeff[j] * data[i-j-1];
        data[i] = residual[i] + (sum >> lp_quantization);
    }
    */
}
#else /* fully unrolled version for normal use */
{
    int i;
    FLAC__int32 sum;

    CHOC_ASSERT(order > 0);
    CHOC_ASSERT(order <= 32);

    /*
     * We do unique versions up to 12th order since that's the subset limit.
     * Also they are roughly ordered to match frequency of occurrence to
     * minimize branching.
     */
    if(order <= 12) {
        if(order > 8) {
            if(order > 10) {
                if(order == 12) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[11] * data[i-12];
                        sum += qlp_coeff[10] * data[i-11];
                        sum += qlp_coeff[9] * data[i-10];
                        sum += qlp_coeff[8] * data[i-9];
                        sum += qlp_coeff[7] * data[i-8];
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        data[i] = residual[i] + (sum >> lp_quantization);
                    }
                }
                else { /* order == 11 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[10] * data[i-11];
                        sum += qlp_coeff[9] * data[i-10];
                        sum += qlp_coeff[8] * data[i-9];
                        sum += qlp_coeff[7] * data[i-8];
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        data[i] = residual[i] + (sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 10) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[9] * data[i-10];
                        sum += qlp_coeff[8] * data[i-9];
                        sum += qlp_coeff[7] * data[i-8];
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        data[i] = residual[i] + (sum >> lp_quantization);
                    }
                }
                else { /* order == 9 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[8] * data[i-9];
                        sum += qlp_coeff[7] * data[i-8];
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        data[i] = residual[i] + (sum >> lp_quantization);
                    }
                }
            }
        }
        else if(order > 4) {
            if(order > 6) {
                if(order == 8) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[7] * data[i-8];
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        data[i] = residual[i] + (sum >> lp_quantization);
                    }
                }
                else { /* order == 7 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[6] * data[i-7];
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        data[i] = residual[i] + (sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 6) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[5] * data[i-6];
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        data[i] = residual[i] + (sum >> lp_quantization);
                    }
                }
                else { /* order == 5 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[4] * data[i-5];
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        data[i] = residual[i] + (sum >> lp_quantization);
                    }
                }
            }
        }
        else {
            if(order > 2) {
                if(order == 4) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[3] * data[i-4];
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        data[i] = residual[i] + (sum >> lp_quantization);
                    }
                }
                else { /* order == 3 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[2] * data[i-3];
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        data[i] = residual[i] + (sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 2) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[1] * data[i-2];
                        sum += qlp_coeff[0] * data[i-1];
                        data[i] = residual[i] + (sum >> lp_quantization);
                    }
                }
                else { /* order == 1 */
                    for(i = 0; i < (int)data_len; i++)
                        data[i] = residual[i] + ((qlp_coeff[0] * data[i-1]) >> lp_quantization);
                }
            }
        }
    }
    else { /* order > 12 */
        for(i = 0; i < (int)data_len; i++) {
            sum = 0;
            switch(order) {
                case 32: sum += qlp_coeff[31] * data[i-32];
                case 31: sum += qlp_coeff[30] * data[i-31];
                case 30: sum += qlp_coeff[29] * data[i-30];
                case 29: sum += qlp_coeff[28] * data[i-29];
                case 28: sum += qlp_coeff[27] * data[i-28];
                case 27: sum += qlp_coeff[26] * data[i-27];
                case 26: sum += qlp_coeff[25] * data[i-26];
                case 25: sum += qlp_coeff[24] * data[i-25];
                case 24: sum += qlp_coeff[23] * data[i-24];
                case 23: sum += qlp_coeff[22] * data[i-23];
                case 22: sum += qlp_coeff[21] * data[i-22];
                case 21: sum += qlp_coeff[20] * data[i-21];
                case 20: sum += qlp_coeff[19] * data[i-20];
                case 19: sum += qlp_coeff[18] * data[i-19];
                case 18: sum += qlp_coeff[17] * data[i-18];
                case 17: sum += qlp_coeff[16] * data[i-17];
                case 16: sum += qlp_coeff[15] * data[i-16];
                case 15: sum += qlp_coeff[14] * data[i-15];
                case 14: sum += qlp_coeff[13] * data[i-14];
                case 13: sum += qlp_coeff[12] * data[i-13];
                         sum += qlp_coeff[11] * data[i-12];
                         sum += qlp_coeff[10] * data[i-11];
                         sum += qlp_coeff[ 9] * data[i-10];
                         sum += qlp_coeff[ 8] * data[i- 9];
                         sum += qlp_coeff[ 7] * data[i- 8];
                         sum += qlp_coeff[ 6] * data[i- 7];
                         sum += qlp_coeff[ 5] * data[i- 6];
                         sum += qlp_coeff[ 4] * data[i- 5];
                         sum += qlp_coeff[ 3] * data[i- 4];
                         sum += qlp_coeff[ 2] * data[i- 3];
                         sum += qlp_coeff[ 1] * data[i- 2];
                         sum += qlp_coeff[ 0] * data[i- 1];
            }
            data[i] = residual[i] + (sum >> lp_quantization);
        }
    }
}
#endif

inline void FLAC__lpc_restore_signal_wide(const FLAC__int32 * flac_restrict residual, unsigned data_len, const FLAC__int32 * flac_restrict qlp_coeff, unsigned order, int lp_quantization, FLAC__int32 * flac_restrict data)
#if defined(FLAC__OVERFLOW_DETECT) || !defined(FLAC__LPC_UNROLLED_FILTER_LOOPS)
{
    unsigned i, j;
    FLAC__int64 sum;
    const FLAC__int32 *r = residual, *history;

#ifdef FLAC__OVERFLOW_DETECT_VERBOSE
    fprintf(stderr,"FLAC__lpc_restore_signal_wide: data_len=%d, order=%u, lpq=%d",data_len,order,lp_quantization);
    for(i=0;i<order;i++)
        fprintf(stderr,", q[%u]=%d",i,qlp_coeff[i]);
    fprintf(stderr,"\n");
#endif
    CHOC_ASSERT(order > 0);

    for(i = 0; i < data_len; i++) {
        sum = 0;
        history = data;
        for(j = 0; j < order; j++)
            sum += (FLAC__int64)qlp_coeff[j] * (FLAC__int64)(*(--history));
        if(FLAC__bitmath_silog2_wide(sum >> lp_quantization) > 32) {
            fprintf(stderr,"FLAC__lpc_restore_signal_wide: OVERFLOW, i=%u, sum=%" PRId64 "\n", i, (sum >> lp_quantization));
            break;
        }
        if(FLAC__bitmath_silog2_wide((FLAC__int64)(*r) + (sum >> lp_quantization)) > 32) {
            fprintf(stderr,"FLAC__lpc_restore_signal_wide: OVERFLOW, i=%u, residual=%d, sum=%" PRId64 ", data=%" PRId64 "\n", i, *r, (sum >> lp_quantization), ((FLAC__int64)(*r) + (sum >> lp_quantization)));
            break;
        }
        *(data++) = *(r++) + (FLAC__int32)(sum >> lp_quantization);
    }
}
#else /* fully unrolled version for normal use */
{
    int i;
    FLAC__int64 sum;

    CHOC_ASSERT(order > 0);
    CHOC_ASSERT(order <= 32);

    /*
     * We do unique versions up to 12th order since that's the subset limit.
     * Also they are roughly ordered to match frequency of occurrence to
     * minimize branching.
     */
    if(order <= 12) {
        if(order > 8) {
            if(order > 10) {
                if(order == 12) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[11] * (FLAC__int64)data[i-12];
                        sum += qlp_coeff[10] * (FLAC__int64)data[i-11];
                        sum += qlp_coeff[9] * (FLAC__int64)data[i-10];
                        sum += qlp_coeff[8] * (FLAC__int64)data[i-9];
                        sum += qlp_coeff[7] * (FLAC__int64)data[i-8];
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 11 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[10] * (FLAC__int64)data[i-11];
                        sum += qlp_coeff[9] * (FLAC__int64)data[i-10];
                        sum += qlp_coeff[8] * (FLAC__int64)data[i-9];
                        sum += qlp_coeff[7] * (FLAC__int64)data[i-8];
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 10) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[9] * (FLAC__int64)data[i-10];
                        sum += qlp_coeff[8] * (FLAC__int64)data[i-9];
                        sum += qlp_coeff[7] * (FLAC__int64)data[i-8];
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 9 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[8] * (FLAC__int64)data[i-9];
                        sum += qlp_coeff[7] * (FLAC__int64)data[i-8];
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
                    }
                }
            }
        }
        else if(order > 4) {
            if(order > 6) {
                if(order == 8) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[7] * (FLAC__int64)data[i-8];
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 7 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[6] * (FLAC__int64)data[i-7];
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 6) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[5] * (FLAC__int64)data[i-6];
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 5 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[4] * (FLAC__int64)data[i-5];
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
                    }
                }
            }
        }
        else {
            if(order > 2) {
                if(order == 4) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[3] * (FLAC__int64)data[i-4];
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 3 */
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[2] * (FLAC__int64)data[i-3];
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
                    }
                }
            }
            else {
                if(order == 2) {
                    for(i = 0; i < (int)data_len; i++) {
                        sum = 0;
                        sum += qlp_coeff[1] * (FLAC__int64)data[i-2];
                        sum += qlp_coeff[0] * (FLAC__int64)data[i-1];
                        data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
                    }
                }
                else { /* order == 1 */
                    for(i = 0; i < (int)data_len; i++)
                        data[i] = residual[i] + (FLAC__int32)((qlp_coeff[0] * (FLAC__int64)data[i-1]) >> lp_quantization);
                }
            }
        }
    }
    else { /* order > 12 */
        for(i = 0; i < (int)data_len; i++) {
            sum = 0;
            switch(order) {
                case 32: sum += qlp_coeff[31] * (FLAC__int64)data[i-32];
                case 31: sum += qlp_coeff[30] * (FLAC__int64)data[i-31];
                case 30: sum += qlp_coeff[29] * (FLAC__int64)data[i-30];
                case 29: sum += qlp_coeff[28] * (FLAC__int64)data[i-29];
                case 28: sum += qlp_coeff[27] * (FLAC__int64)data[i-28];
                case 27: sum += qlp_coeff[26] * (FLAC__int64)data[i-27];
                case 26: sum += qlp_coeff[25] * (FLAC__int64)data[i-26];
                case 25: sum += qlp_coeff[24] * (FLAC__int64)data[i-25];
                case 24: sum += qlp_coeff[23] * (FLAC__int64)data[i-24];
                case 23: sum += qlp_coeff[22] * (FLAC__int64)data[i-23];
                case 22: sum += qlp_coeff[21] * (FLAC__int64)data[i-22];
                case 21: sum += qlp_coeff[20] * (FLAC__int64)data[i-21];
                case 20: sum += qlp_coeff[19] * (FLAC__int64)data[i-20];
                case 19: sum += qlp_coeff[18] * (FLAC__int64)data[i-19];
                case 18: sum += qlp_coeff[17] * (FLAC__int64)data[i-18];
                case 17: sum += qlp_coeff[16] * (FLAC__int64)data[i-17];
                case 16: sum += qlp_coeff[15] * (FLAC__int64)data[i-16];
                case 15: sum += qlp_coeff[14] * (FLAC__int64)data[i-15];
                case 14: sum += qlp_coeff[13] * (FLAC__int64)data[i-14];
                case 13: sum += qlp_coeff[12] * (FLAC__int64)data[i-13];
                         sum += qlp_coeff[11] * (FLAC__int64)data[i-12];
                         sum += qlp_coeff[10] * (FLAC__int64)data[i-11];
                         sum += qlp_coeff[ 9] * (FLAC__int64)data[i-10];
                         sum += qlp_coeff[ 8] * (FLAC__int64)data[i- 9];
                         sum += qlp_coeff[ 7] * (FLAC__int64)data[i- 8];
                         sum += qlp_coeff[ 6] * (FLAC__int64)data[i- 7];
                         sum += qlp_coeff[ 5] * (FLAC__int64)data[i- 6];
                         sum += qlp_coeff[ 4] * (FLAC__int64)data[i- 5];
                         sum += qlp_coeff[ 3] * (FLAC__int64)data[i- 4];
                         sum += qlp_coeff[ 2] * (FLAC__int64)data[i- 3];
                         sum += qlp_coeff[ 1] * (FLAC__int64)data[i- 2];
                         sum += qlp_coeff[ 0] * (FLAC__int64)data[i- 1];
            }
            data[i] = residual[i] + (FLAC__int32)(sum >> lp_quantization);
        }
    }
}
#endif

#if defined(_MSC_VER)
#pragma warning ( default : 4028 )
#endif

#ifndef FLAC__INTEGER_ONLY_LIBRARY

inline FLAC__double FLAC__lpc_compute_expected_bits_per_residual_sample(FLAC__double lpc_error, unsigned total_samples)
{
    FLAC__double error_scale;

    CHOC_ASSERT(total_samples > 0);

    error_scale = 0.5 * M_LN2 * M_LN2 / (FLAC__double)total_samples;

    return FLAC__lpc_compute_expected_bits_per_residual_sample_with_error_scale(lpc_error, error_scale);
}

inline FLAC__double FLAC__lpc_compute_expected_bits_per_residual_sample_with_error_scale(FLAC__double lpc_error, FLAC__double error_scale)
{
    if(lpc_error > 0.0) {
        FLAC__double bps = (FLAC__double)0.5 * log(error_scale * lpc_error) / M_LN2;
        if(bps >= 0.0)
            return bps;
        else
            return 0.0;
    }
    else if(lpc_error < 0.0) { /* error should not be negative but can happen due to inadequate floating-point resolution */
        return 1e32;
    }
    else {
        return 0.0;
    }
}

static inline unsigned FLAC__lpc_compute_best_order(const FLAC__double lpc_error[], unsigned max_order, unsigned total_samples, unsigned overhead_bits_per_order)
{
    unsigned order, indx, best_index; /* 'index' the index into lpc_error; index==order-1 since lpc_error[0] is for order==1, lpc_error[1] is for order==2, etc */
    FLAC__double bits, best_bits, error_scale;

    CHOC_ASSERT(max_order > 0);
    CHOC_ASSERT(total_samples > 0);

    error_scale = 0.5 * M_LN2 * M_LN2 / (FLAC__double)total_samples;

    best_index = 0;
    best_bits = (unsigned)(-1);

    for(indx = 0, order = 1; indx < max_order; indx++, order++) {
        bits = FLAC__lpc_compute_expected_bits_per_residual_sample_with_error_scale(lpc_error[indx], error_scale) * (FLAC__double)(total_samples - order) + (FLAC__double)(order * overhead_bits_per_order);
        if(bits < best_bits) {
            best_index = indx;
            best_bits = bits;
        }
    }

    return best_index+1; /* +1 since indx of lpc_error[] is order-1 */
}

#endif /* !defined FLAC__INTEGER_ONLY_LIBRARY */


/*
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 *
 * Changed so as no longer to depend on Colin Plumb's `usual.h' header
 * definitions; now uses stuff from dpkg's config.h.
 *  - Ian Jackson <ijackson@nyx.cs.du.edu>.
 * Still in the public domain.
 *
 * Josh Coalson: made some changes to integrate with libFLAC.
 * Still in the public domain.
 */

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
#define FLAC_F1(x, y, z) (z ^ (x & (y ^ z)))
#define FLAC_F2(x, y, z) FLAC_F1(z, x, y)
#define FLAC_F3(x, y, z) (x ^ y ^ z)
#define FLAC_F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
#define FLAC_MD5STEP(f,w,x,y,z,in,s) \
     (w += f(x,y,z) + in, w = (w<<s | w>>(32-s)) + x)

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void FLAC__MD5Transform(FLAC__uint32 buf[4], FLAC__uint32 const in[16])
{
    FLAC__uint32 a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    FLAC_MD5STEP(FLAC_F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    FLAC_MD5STEP(FLAC_F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    FLAC_MD5STEP(FLAC_F1, c, d, a, b, in[2] + 0x242070db, 17);
    FLAC_MD5STEP(FLAC_F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    FLAC_MD5STEP(FLAC_F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    FLAC_MD5STEP(FLAC_F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    FLAC_MD5STEP(FLAC_F1, c, d, a, b, in[6] + 0xa8304613, 17);
    FLAC_MD5STEP(FLAC_F1, b, c, d, a, in[7] + 0xfd469501, 22);
    FLAC_MD5STEP(FLAC_F1, a, b, c, d, in[8] + 0x698098d8, 7);
    FLAC_MD5STEP(FLAC_F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    FLAC_MD5STEP(FLAC_F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    FLAC_MD5STEP(FLAC_F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    FLAC_MD5STEP(FLAC_F1, a, b, c, d, in[12] + 0x6b901122, 7);
    FLAC_MD5STEP(FLAC_F1, d, a, b, c, in[13] + 0xfd987193, 12);
    FLAC_MD5STEP(FLAC_F1, c, d, a, b, in[14] + 0xa679438e, 17);
    FLAC_MD5STEP(FLAC_F1, b, c, d, a, in[15] + 0x49b40821, 22);

    FLAC_MD5STEP(FLAC_F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    FLAC_MD5STEP(FLAC_F2, d, a, b, c, in[6] + 0xc040b340, 9);
    FLAC_MD5STEP(FLAC_F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    FLAC_MD5STEP(FLAC_F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    FLAC_MD5STEP(FLAC_F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    FLAC_MD5STEP(FLAC_F2, d, a, b, c, in[10] + 0x02441453, 9);
    FLAC_MD5STEP(FLAC_F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    FLAC_MD5STEP(FLAC_F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    FLAC_MD5STEP(FLAC_F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    FLAC_MD5STEP(FLAC_F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    FLAC_MD5STEP(FLAC_F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    FLAC_MD5STEP(FLAC_F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    FLAC_MD5STEP(FLAC_F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    FLAC_MD5STEP(FLAC_F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    FLAC_MD5STEP(FLAC_F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    FLAC_MD5STEP(FLAC_F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    FLAC_MD5STEP(FLAC_F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    FLAC_MD5STEP(FLAC_F3, d, a, b, c, in[8] + 0x8771f681, 11);
    FLAC_MD5STEP(FLAC_F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    FLAC_MD5STEP(FLAC_F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    FLAC_MD5STEP(FLAC_F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    FLAC_MD5STEP(FLAC_F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    FLAC_MD5STEP(FLAC_F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    FLAC_MD5STEP(FLAC_F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    FLAC_MD5STEP(FLAC_F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    FLAC_MD5STEP(FLAC_F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    FLAC_MD5STEP(FLAC_F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    FLAC_MD5STEP(FLAC_F3, b, c, d, a, in[6] + 0x04881d05, 23);
    FLAC_MD5STEP(FLAC_F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    FLAC_MD5STEP(FLAC_F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    FLAC_MD5STEP(FLAC_F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    FLAC_MD5STEP(FLAC_F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    FLAC_MD5STEP(FLAC_F4, a, b, c, d, in[0] + 0xf4292244, 6);
    FLAC_MD5STEP(FLAC_F4, d, a, b, c, in[7] + 0x432aff97, 10);
    FLAC_MD5STEP(FLAC_F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    FLAC_MD5STEP(FLAC_F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    FLAC_MD5STEP(FLAC_F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    FLAC_MD5STEP(FLAC_F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    FLAC_MD5STEP(FLAC_F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    FLAC_MD5STEP(FLAC_F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    FLAC_MD5STEP(FLAC_F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    FLAC_MD5STEP(FLAC_F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    FLAC_MD5STEP(FLAC_F4, c, d, a, b, in[6] + 0xa3014314, 15);
    FLAC_MD5STEP(FLAC_F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    FLAC_MD5STEP(FLAC_F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    FLAC_MD5STEP(FLAC_F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    FLAC_MD5STEP(FLAC_F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    FLAC_MD5STEP(FLAC_F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}

#if WORDS_BIGENDIAN
//@@@@@@ OPT: use bswap/intrinsics
static void byteSwap(FLAC__uint32 *buf, unsigned words)
{
    FLAC__uint32 x;
    do {
        x = *buf;
        x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff);
        *buf++ = (x >> 16) | (x << 16);
    } while (--words);
}
static void byteSwapX16(FLAC__uint32 *buf)
{
    FLAC__uint32 x;

    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf++ = (x >> 16) | (x << 16);
    x = *buf; x = ((x << 8) & 0xff00ff00) | ((x >> 8) & 0x00ff00ff); *buf   = (x >> 16) | (x << 16);
}
#else
static void byteSwap(FLAC__uint32*, unsigned) {}
static void byteSwapX16(FLAC__uint32*) {}
#endif

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void FLAC__MD5Update(FLAC__MD5Context *ctx, FLAC__byte const *buf, unsigned len)
{
    FLAC__uint32 t;

    /* Update byte count */

    t = ctx->bytes[0];
    if ((ctx->bytes[0] = t + len) < t)
        ctx->bytes[1]++;    /* Carry from low to high */

    t = 64 - (t & 0x3f);    /* Space available in ctx->in (at least 1) */
    if (t > len) {
        memcpy((FLAC__byte *)ctx->in + 64 - t, buf, len);
        return;
    }
    /* First chunk is an odd size */
    memcpy((FLAC__byte *)ctx->in + 64 - t, buf, t);
    byteSwapX16(ctx->in);
    FLAC__MD5Transform(ctx->buf, ctx->in);
    buf += t;
    len -= t;

    /* Process data in 64-byte chunks */
    while (len >= 64) {
        memcpy(ctx->in, buf, 64);
        byteSwapX16(ctx->in);
        FLAC__MD5Transform(ctx->buf, ctx->in);
        buf += 64;
        len -= 64;
    }

    /* Handle any remaining bytes of data. */
    memcpy(ctx->in, buf, len);
}

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
inline void FLAC__MD5Init(FLAC__MD5Context *ctx)
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;

    ctx->bytes[0] = 0;
    ctx->bytes[1] = 0;

    ctx->internal_buf.p8= 0;
    ctx->capacity = 0;
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
inline void FLAC__MD5Final(FLAC__byte digest[16], FLAC__MD5Context *ctx)
{
    int count = ctx->bytes[0] & 0x3f;    /* Number of bytes in ctx->in */
    FLAC__byte *p = (FLAC__byte *)ctx->in + count;

    /* Set the first char of padding to 0x80.  There is always room. */
    *p++ = 0x80;

    /* Bytes of padding needed to make 56 bytes (-8..55) */
    count = 56 - 1 - count;

    if (count < 0) {    /* Padding forces an extra block */
        memset(p, 0, count + 8);
        byteSwapX16(ctx->in);
        FLAC__MD5Transform(ctx->buf, ctx->in);
        p = (FLAC__byte *)ctx->in;
        count = 56;
    }
    memset(p, 0, count);
    byteSwap(ctx->in, 14);

    /* Append length in bits and transform */
    ctx->in[14] = ctx->bytes[0] << 3;
    ctx->in[15] = ctx->bytes[1] << 3 | ctx->bytes[0] >> 29;
    FLAC__MD5Transform(ctx->buf, ctx->in);

    byteSwap(ctx->buf, 4);
    memcpy(digest, ctx->buf, 16);
    if (0 != ctx->internal_buf.p8) {
        free(ctx->internal_buf.p8);
        ctx->internal_buf.p8= 0;
        ctx->capacity = 0;
    }
    memset(ctx, 0, sizeof(*ctx));    /* In case it's sensitive */
}

/*
 * Convert the incoming audio signal to a byte stream
 */
static inline void format_input_(FLAC__multibyte *mbuf, const FLAC__int32 * const signal[], unsigned channels, unsigned samples, unsigned bytes_per_sample)
{
    FLAC__byte *buf_ = mbuf->p8;
    FLAC__int16 *buf16 = mbuf->p16;
    FLAC__int32 *buf32 = mbuf->p32;
    FLAC__int32 a_word;
    unsigned channel, sample;

    /* Storage in the output buffer, buf, is little endian. */

#define BYTES_CHANNEL_SELECTOR(bytes, channels)   (bytes * 100 + channels)

    /* First do the most commonly used combinations. */
    switch (BYTES_CHANNEL_SELECTOR (bytes_per_sample, channels)) {
        /* One byte per sample. */
        case (BYTES_CHANNEL_SELECTOR (1, 1)):
            for (sample = 0; sample < samples; sample++)
                *buf_++ = signal[0][sample];
            return;

        case (BYTES_CHANNEL_SELECTOR (1, 2)):
            for (sample = 0; sample < samples; sample++) {
                *buf_++ = signal[0][sample];
                *buf_++ = signal[1][sample];
            }
            return;

        case (BYTES_CHANNEL_SELECTOR (1, 4)):
            for (sample = 0; sample < samples; sample++) {
                *buf_++ = signal[0][sample];
                *buf_++ = signal[1][sample];
                *buf_++ = signal[2][sample];
                *buf_++ = signal[3][sample];
            }
            return;

        case (BYTES_CHANNEL_SELECTOR (1, 6)):
            for (sample = 0; sample < samples; sample++) {
                *buf_++ = signal[0][sample];
                *buf_++ = signal[1][sample];
                *buf_++ = signal[2][sample];
                *buf_++ = signal[3][sample];
                *buf_++ = signal[4][sample];
                *buf_++ = signal[5][sample];
            }
            return;

        case (BYTES_CHANNEL_SELECTOR (1, 8)):
            for (sample = 0; sample < samples; sample++) {
                *buf_++ = signal[0][sample];
                *buf_++ = signal[1][sample];
                *buf_++ = signal[2][sample];
                *buf_++ = signal[3][sample];
                *buf_++ = signal[4][sample];
                *buf_++ = signal[5][sample];
                *buf_++ = signal[6][sample];
                *buf_++ = signal[7][sample];
            }
            return;

        /* Two bytes per sample. */
        case (BYTES_CHANNEL_SELECTOR (2, 1)):
            for (sample = 0; sample < samples; sample++)
                *buf16++ = FLAC_H2LE_16(signal[0][sample]);
            return;

        case (BYTES_CHANNEL_SELECTOR (2, 2)):
            for (sample = 0; sample < samples; sample++) {
                *buf16++ = FLAC_H2LE_16(signal[0][sample]);
                *buf16++ = FLAC_H2LE_16(signal[1][sample]);
            }
            return;

        case (BYTES_CHANNEL_SELECTOR (2, 4)):
            for (sample = 0; sample < samples; sample++) {
                *buf16++ = FLAC_H2LE_16(signal[0][sample]);
                *buf16++ = FLAC_H2LE_16(signal[1][sample]);
                *buf16++ = FLAC_H2LE_16(signal[2][sample]);
                *buf16++ = FLAC_H2LE_16(signal[3][sample]);
            }
            return;

        case (BYTES_CHANNEL_SELECTOR (2, 6)):
            for (sample = 0; sample < samples; sample++) {
                *buf16++ = FLAC_H2LE_16(signal[0][sample]);
                *buf16++ = FLAC_H2LE_16(signal[1][sample]);
                *buf16++ = FLAC_H2LE_16(signal[2][sample]);
                *buf16++ = FLAC_H2LE_16(signal[3][sample]);
                *buf16++ = FLAC_H2LE_16(signal[4][sample]);
                *buf16++ = FLAC_H2LE_16(signal[5][sample]);
            }
            return;

        case (BYTES_CHANNEL_SELECTOR (2, 8)):
            for (sample = 0; sample < samples; sample++) {
                *buf16++ = FLAC_H2LE_16(signal[0][sample]);
                *buf16++ = FLAC_H2LE_16(signal[1][sample]);
                *buf16++ = FLAC_H2LE_16(signal[2][sample]);
                *buf16++ = FLAC_H2LE_16(signal[3][sample]);
                *buf16++ = FLAC_H2LE_16(signal[4][sample]);
                *buf16++ = FLAC_H2LE_16(signal[5][sample]);
                *buf16++ = FLAC_H2LE_16(signal[6][sample]);
                *buf16++ = FLAC_H2LE_16(signal[7][sample]);
            }
            return;

        /* Three bytes per sample. */
        case (BYTES_CHANNEL_SELECTOR (3, 1)):
            for (sample = 0; sample < samples; sample++) {
                a_word = signal[0][sample];
                *buf_++ = (FLAC__byte)a_word; a_word >>= 8;
                *buf_++ = (FLAC__byte)a_word; a_word >>= 8;
                *buf_++ = (FLAC__byte)a_word;
            }
            return;

        case (BYTES_CHANNEL_SELECTOR (3, 2)):
            for (sample = 0; sample < samples; sample++) {
                a_word = signal[0][sample];
                *buf_++ = (FLAC__byte)a_word; a_word >>= 8;
                *buf_++ = (FLAC__byte)a_word; a_word >>= 8;
                *buf_++ = (FLAC__byte)a_word;
                a_word = signal[1][sample];
                *buf_++ = (FLAC__byte)a_word; a_word >>= 8;
                *buf_++ = (FLAC__byte)a_word; a_word >>= 8;
                *buf_++ = (FLAC__byte)a_word;
            }
            return;

        /* Four bytes per sample. */
        case (BYTES_CHANNEL_SELECTOR (4, 1)):
            for (sample = 0; sample < samples; sample++)
                *buf32++ = FLAC_H2LE_32(signal[0][sample]);
            return;

        case (BYTES_CHANNEL_SELECTOR (4, 2)):
            for (sample = 0; sample < samples; sample++) {
                *buf32++ = FLAC_H2LE_32(signal[0][sample]);
                *buf32++ = FLAC_H2LE_32(signal[1][sample]);
            }
            return;

        case (BYTES_CHANNEL_SELECTOR (4, 4)):
            for (sample = 0; sample < samples; sample++) {
                *buf32++ = FLAC_H2LE_32(signal[0][sample]);
                *buf32++ = FLAC_H2LE_32(signal[1][sample]);
                *buf32++ = FLAC_H2LE_32(signal[2][sample]);
                *buf32++ = FLAC_H2LE_32(signal[3][sample]);
            }
            return;

        case (BYTES_CHANNEL_SELECTOR (4, 6)):
            for (sample = 0; sample < samples; sample++) {
                *buf32++ = FLAC_H2LE_32(signal[0][sample]);
                *buf32++ = FLAC_H2LE_32(signal[1][sample]);
                *buf32++ = FLAC_H2LE_32(signal[2][sample]);
                *buf32++ = FLAC_H2LE_32(signal[3][sample]);
                *buf32++ = FLAC_H2LE_32(signal[4][sample]);
                *buf32++ = FLAC_H2LE_32(signal[5][sample]);
            }
            return;

        case (BYTES_CHANNEL_SELECTOR (4, 8)):
            for (sample = 0; sample < samples; sample++) {
                *buf32++ = FLAC_H2LE_32(signal[0][sample]);
                *buf32++ = FLAC_H2LE_32(signal[1][sample]);
                *buf32++ = FLAC_H2LE_32(signal[2][sample]);
                *buf32++ = FLAC_H2LE_32(signal[3][sample]);
                *buf32++ = FLAC_H2LE_32(signal[4][sample]);
                *buf32++ = FLAC_H2LE_32(signal[5][sample]);
                *buf32++ = FLAC_H2LE_32(signal[6][sample]);
                *buf32++ = FLAC_H2LE_32(signal[7][sample]);
            }
            return;

        default:
            break;
    }

    /* General version. */
    switch (bytes_per_sample) {
        case 1:
            for (sample = 0; sample < samples; sample++)
                for (channel = 0; channel < channels; channel++)
                    *buf_++ = signal[channel][sample];
            return;

        case 2:
            for (sample = 0; sample < samples; sample++)
                for (channel = 0; channel < channels; channel++)
                    *buf16++ = FLAC_H2LE_16(signal[channel][sample]);
            return;

        case 3:
            for (sample = 0; sample < samples; sample++)
                for (channel = 0; channel < channels; channel++) {
                    a_word = signal[channel][sample];
                    *buf_++ = (FLAC__byte)a_word; a_word >>= 8;
                    *buf_++ = (FLAC__byte)a_word; a_word >>= 8;
                    *buf_++ = (FLAC__byte)a_word;
                }
            return;

        case 4:
            for (sample = 0; sample < samples; sample++)
                for (channel = 0; channel < channels; channel++)
                    *buf32++ = FLAC_H2LE_32(signal[channel][sample]);
            return;

        default:
            break;
    }
}

/*
 * Convert the incoming audio signal to a byte stream and FLAC__MD5Update it.
 */
static inline FLAC__bool FLAC__MD5Accumulate(FLAC__MD5Context *ctx, const FLAC__int32 * const signal[], unsigned channels, unsigned samples, unsigned bytes_per_sample)
{
    const size_t bytes_needed = (size_t)channels * (size_t)samples * (size_t)bytes_per_sample;

    /* overflow check */
    if ((size_t)channels > SIZE_MAX / (size_t)bytes_per_sample)
        return false;
    if ((size_t)channels * (size_t)bytes_per_sample > SIZE_MAX / (size_t)samples)
        return false;

    if (ctx->capacity < bytes_needed) {
        FLAC__byte *tmp = (FLAC__byte*) realloc(ctx->internal_buf.p8, bytes_needed);
        if (0 == tmp) {
            free(ctx->internal_buf.p8);
            if (0 == (ctx->internal_buf.p8= (FLAC__byte*) safe_malloc_(bytes_needed)))
                return false;
        }
        else
            ctx->internal_buf.p8= tmp;
        ctx->capacity = bytes_needed;
    }

    format_input_(&ctx->internal_buf, signal, channels, samples, bytes_per_sample);

    FLAC__MD5Update(ctx, ctx->internal_buf.p8, bytes_needed);

    return true;
}

static inline void *FLAC__memory_alloc_aligned(size_t bytes, void **aligned_address)
{
    void *x;

    CHOC_ASSERT(0 != aligned_address);

#ifdef FLAC__ALIGN_MALLOC_DATA
    /* align on 32-byte (256-bit) boundary */
    x = safe_malloc_add_2op_(bytes, /*+*/31L);
    *aligned_address = (void*)(((uintptr_t)x + 31L) & -32L);
#else
    x = safe_malloc_(bytes);
    *aligned_address = x;
#endif
    return x;
}

static inline FLAC__bool FLAC__memory_alloc_aligned_int32_array(size_t elements, FLAC__int32 **unaligned_pointer, FLAC__int32 **aligned_pointer)
{
    FLAC__int32 *pu; /* unaligned pointer */
    union { /* union needed to comply with C99 pointer aliasing rules */
        FLAC__int32 *pa; /* aligned pointer */
        void        *pv; /* aligned pointer alias */
    } u;

    CHOC_ASSERT(elements > 0);
    CHOC_ASSERT(0 != unaligned_pointer);
    CHOC_ASSERT(0 != aligned_pointer);
    CHOC_ASSERT(unaligned_pointer != aligned_pointer);

    if(elements > SIZE_MAX / sizeof(*pu)) /* overflow check */
        return false;

    pu = (FLAC__int32*) FLAC__memory_alloc_aligned(sizeof(*pu) * elements, &u.pv);
    if(0 == pu) {
        return false;
    }
    else {
        if(*unaligned_pointer != 0)
            free(*unaligned_pointer);
        *unaligned_pointer = pu;
        *aligned_pointer = u.pa;
        return true;
    }
}

static inline FLAC__bool FLAC__memory_alloc_aligned_uint32_array(size_t elements, FLAC__uint32 **unaligned_pointer, FLAC__uint32 **aligned_pointer)
{
    FLAC__uint32 *pu; /* unaligned pointer */
    union { /* union needed to comply with C99 pointer aliasing rules */
        FLAC__uint32 *pa; /* aligned pointer */
        void         *pv; /* aligned pointer alias */
    } u;

    CHOC_ASSERT(elements > 0);
    CHOC_ASSERT(0 != unaligned_pointer);
    CHOC_ASSERT(0 != aligned_pointer);
    CHOC_ASSERT(unaligned_pointer != aligned_pointer);

    if(elements > SIZE_MAX / sizeof(*pu)) /* overflow check */
        return false;

    pu = (FLAC__uint32*) FLAC__memory_alloc_aligned(sizeof(*pu) * elements, &u.pv);
    if(0 == pu) {
        return false;
    }
    else {
        if(*unaligned_pointer != 0)
            free(*unaligned_pointer);
        *unaligned_pointer = pu;
        *aligned_pointer = u.pa;
        return true;
    }
}

static inline FLAC__bool FLAC__memory_alloc_aligned_uint64_array(size_t elements, FLAC__uint64 **unaligned_pointer, FLAC__uint64 **aligned_pointer)
{
    FLAC__uint64 *pu; /* unaligned pointer */
    union { /* union needed to comply with C99 pointer aliasing rules */
        FLAC__uint64 *pa; /* aligned pointer */
        void         *pv; /* aligned pointer alias */
    } u;

    CHOC_ASSERT(elements > 0);
    CHOC_ASSERT(0 != unaligned_pointer);
    CHOC_ASSERT(0 != aligned_pointer);
    CHOC_ASSERT(unaligned_pointer != aligned_pointer);

    if(elements > SIZE_MAX / sizeof(*pu)) /* overflow check */
        return false;

    pu = (FLAC__uint64*) FLAC__memory_alloc_aligned(sizeof(*pu) * elements, &u.pv);
    if(0 == pu) {
        return false;
    }
    else {
        if(*unaligned_pointer != 0)
            free(*unaligned_pointer);
        *unaligned_pointer = pu;
        *aligned_pointer = u.pa;
        return true;
    }
}

static inline FLAC__bool FLAC__memory_alloc_aligned_unsigned_array(size_t elements, unsigned **unaligned_pointer, unsigned **aligned_pointer)
{
    unsigned *pu; /* unaligned pointer */
    union { /* union needed to comply with C99 pointer aliasing rules */
        unsigned *pa; /* aligned pointer */
        void     *pv; /* aligned pointer alias */
    } u;

    CHOC_ASSERT(elements > 0);
    CHOC_ASSERT(0 != unaligned_pointer);
    CHOC_ASSERT(0 != aligned_pointer);
    CHOC_ASSERT(unaligned_pointer != aligned_pointer);

    if(elements > SIZE_MAX / sizeof(*pu)) /* overflow check */
        return false;

    pu = (unsigned int*) FLAC__memory_alloc_aligned(sizeof(*pu) * elements, &u.pv);
    if(0 == pu) {
        return false;
    }
    else {
        if(*unaligned_pointer != 0)
            free(*unaligned_pointer);
        *unaligned_pointer = pu;
        *aligned_pointer = u.pa;
        return true;
    }
}

#ifndef FLAC__INTEGER_ONLY_LIBRARY

static inline FLAC__bool FLAC__memory_alloc_aligned_real_array(size_t elements, FLAC__real **unaligned_pointer, FLAC__real **aligned_pointer)
{
    FLAC__real *pu; /* unaligned pointer */
    union { /* union needed to comply with C99 pointer aliasing rules */
        FLAC__real *pa; /* aligned pointer */
        void       *pv; /* aligned pointer alias */
    } u;

    CHOC_ASSERT(elements > 0);
    CHOC_ASSERT(0 != unaligned_pointer);
    CHOC_ASSERT(0 != aligned_pointer);
    CHOC_ASSERT(unaligned_pointer != aligned_pointer);

    if(elements > SIZE_MAX / sizeof(*pu)) /* overflow check */
        return false;

    pu = (FLAC__real*) FLAC__memory_alloc_aligned(sizeof(*pu) * elements, &u.pv);
    if(0 == pu) {
        return false;
    }
    else {
        if(*unaligned_pointer != 0)
            free(*unaligned_pointer);
        *unaligned_pointer = pu;
        *aligned_pointer = u.pa;
        return true;
    }
}

#endif

static inline void *safe_malloc_mul_2op_p(size_t size1, size_t size2)
{
    if(!size1 || !size2)
        return malloc(1); /* malloc(0) is undefined; FLAC src convention is to always allocate */
    if(size1 > SIZE_MAX / size2)
        return 0;
    return malloc(size1*size2);
}

/***********************************************************************
 *
 * Private static data
 *
 ***********************************************************************/

static const FLAC__byte ID3V2_TAG_[3] = { 'I', 'D', '3' };

/***********************************************************************
 *
 * Private class method prototypes
 *
 ***********************************************************************/

static void set_defaults_(FLAC__StreamDecoder *decoder);
//static FILE *get_binary_stdin_(void);
static FLAC__bool allocate_output_(FLAC__StreamDecoder *decoder, unsigned size, unsigned channels);
static FLAC__bool has_id_filtered_(FLAC__StreamDecoder *decoder, FLAC__byte *id);
static FLAC__bool find_metadata_(FLAC__StreamDecoder *decoder);
static FLAC__bool read_metadata_(FLAC__StreamDecoder *decoder);
static FLAC__bool read_metadata_streaminfo_(FLAC__StreamDecoder *decoder, FLAC__bool is_last, unsigned length);
static FLAC__bool read_metadata_seektable_(FLAC__StreamDecoder *decoder, FLAC__bool is_last, unsigned length);
static FLAC__bool read_metadata_vorbiscomment_(FLAC__StreamDecoder *decoder, FLAC__StreamMetadata_VorbisComment *obj, unsigned length);
static FLAC__bool read_metadata_cuesheet_(FLAC__StreamDecoder *decoder, FLAC__StreamMetadata_CueSheet *obj);
static FLAC__bool read_metadata_picture_(FLAC__StreamDecoder *decoder, FLAC__StreamMetadata_Picture *obj);
static FLAC__bool skip_id3v2_tag_(FLAC__StreamDecoder *decoder);
static FLAC__bool frame_sync_(FLAC__StreamDecoder *decoder);
static FLAC__bool read_frame_(FLAC__StreamDecoder *decoder, FLAC__bool *got_a_frame, FLAC__bool do_full_decode);
static FLAC__bool read_frame_header_(FLAC__StreamDecoder *decoder);
static FLAC__bool read_subframe_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, FLAC__bool do_full_decode);
static FLAC__bool read_subframe_constant_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, FLAC__bool do_full_decode);
static FLAC__bool read_subframe_fixed_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, const unsigned order, FLAC__bool do_full_decode);
static FLAC__bool read_subframe_lpc_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, const unsigned order, FLAC__bool do_full_decode);
static FLAC__bool read_subframe_verbatim_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, FLAC__bool do_full_decode);
static FLAC__bool read_residual_partitioned_rice_(FLAC__StreamDecoder *decoder, unsigned predictor_order, unsigned partition_order, FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents, FLAC__int32 *residual, FLAC__bool is_extended);
static FLAC__bool read_zero_padding_(FLAC__StreamDecoder *decoder);
static FLAC__bool read_callback_(FLAC__byte buffer[], size_t *bytes, void *client_data);
#if FLAC__HAS_OGG
static FLAC__StreamDecoderReadStatus read_callback_ogg_aspect_(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes);
static FLAC__OggDecoderAspectReadStatus read_callback_proxy_(const void *void_decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
#endif
static FLAC__StreamDecoderWriteStatus write_audio_frame_to_client_(FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[]);
static void send_error_to_client_(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status);
static FLAC__bool seek_to_absolute_sample_(FLAC__StreamDecoder *decoder, FLAC__uint64 stream_length, FLAC__uint64 target_sample);
#if FLAC__HAS_OGG
static FLAC__bool seek_to_absolute_sample_ogg_(FLAC__StreamDecoder *decoder, FLAC__uint64 stream_length, FLAC__uint64 target_sample);
#endif
//static FLAC__StreamDecoderReadStatus file_read_callback_(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
//static FLAC__StreamDecoderSeekStatus file_seek_callback_(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data);
//static FLAC__StreamDecoderTellStatus file_tell_callback_(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data);
//static FLAC__StreamDecoderLengthStatus file_length_callback_(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data);
//static FLAC__bool file_eof_callback_(const FLAC__StreamDecoder *decoder, void *client_data);

/***********************************************************************
 *
 * Private class data
 *
 ***********************************************************************/

typedef struct FLAC__StreamDecoderPrivate {
#if FLAC__HAS_OGG
    FLAC__bool is_ogg;
#endif
    FLAC__StreamDecoderReadCallback read_callback;
    FLAC__StreamDecoderSeekCallback seek_callback;
    FLAC__StreamDecoderTellCallback tell_callback;
    FLAC__StreamDecoderLengthCallback length_callback;
    FLAC__StreamDecoderEofCallback eof_callback;
    FLAC__StreamDecoderWriteCallback write_callback;
    FLAC__StreamDecoderMetadataCallback metadata_callback;
    FLAC__StreamDecoderErrorCallback error_callback;
    /* generic 32-bit datapath: */
    void (*local_lpc_restore_signal)(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
    /* generic 64-bit datapath: */
    void (*local_lpc_restore_signal_64bit)(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
    /* for use when the signal is <= 16 bits-per-sample, or <= 15 bits-per-sample on a side channel (which requires 1 extra bit): */
    void (*local_lpc_restore_signal_16bit)(const FLAC__int32 residual[], unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 data[]);
    void *client_data;
    FILE *file; /* only used if FLAC__stream_decoder_init_file()/FLAC__stream_decoder_init_file() called, else NULL */
    FLAC__BitReader *input;
    FLAC__int32 *output[FLAC__MAX_CHANNELS];
    FLAC__int32 *residual[FLAC__MAX_CHANNELS]; /* WATCHOUT: these are the aligned pointers; the real pointers that should be free()'d are residual_unaligned[] below */
    FLAC__EntropyCodingMethod_PartitionedRiceContents partitioned_rice_contents[FLAC__MAX_CHANNELS];
    unsigned output_capacity, output_channels;
    FLAC__uint32 fixed_block_size, next_fixed_block_size;
    FLAC__uint64 samples_decoded;
    FLAC__bool has_stream_info, has_seek_table;
    FLAC__StreamMetadata stream_info;
    FLAC__StreamMetadata seek_table;
    FLAC__bool metadata_filter[128]; /* MAGIC number 128 == total number of metadata block types == 1 << 7 */
    FLAC__byte *metadata_filter_ids;
    size_t metadata_filter_ids_count, metadata_filter_ids_capacity; /* units for both are IDs, not bytes */
    FLAC__Frame frame;
    FLAC__bool cached; /* true if there is a byte in lookahead */
    FLAC__CPUInfo cpuinfo;
    FLAC__byte header_warmup[2]; /* contains the sync code and reserved bits */
    FLAC__byte lookahead; /* temp storage when we need to look ahead one byte in the stream */
    /* unaligned (original) pointers to allocated data */
    FLAC__int32 *residual_unaligned[FLAC__MAX_CHANNELS];
    FLAC__bool do_md5_checking; /* initially gets protected_->md5_checking but is turned off after a seek or if the metadata has a zero MD5 */
    FLAC__bool internal_reset_hack; /* used only during init() so we can call reset to set up the decoder without rewinding the input */
    FLAC__bool is_seeking;
    FLAC__MD5Context md5context;
    FLAC__byte computed_md5sum[16]; /* this is the sum we computed from the decoded data */
    /* (the rest of these are only used for seeking) */
    FLAC__Frame last_frame; /* holds the info of the last frame we seeked to */
    FLAC__uint64 first_frame_offset; /* hint to the seek routine of where in the stream the first audio frame starts */
    FLAC__uint64 target_sample;
    unsigned unparseable_frame_count; /* used to tell whether we're decoding a future version of FLAC or just got a bad sync */
#if FLAC__HAS_OGG
    FLAC__bool got_a_frame; /* hack needed in Ogg FLAC seek routine to check when process_single() actually writes a frame */
#endif
} FLAC__StreamDecoderPrivate;

/***********************************************************************
 *
 * Public static class data
 *
 ***********************************************************************/

FLAC_API const char * const FLAC__StreamDecoderStateString[] = {
    "FLAC__STREAM_DECODER_SEARCH_FOR_METADATA",
    "FLAC__STREAM_DECODER_READ_METADATA",
    "FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC",
    "FLAC__STREAM_DECODER_READ_FRAME",
    "FLAC__STREAM_DECODER_END_OF_STREAM",
    "FLAC__STREAM_DECODER_OGG_ERROR",
    "FLAC__STREAM_DECODER_SEEK_ERROR",
    "FLAC__STREAM_DECODER_ABORTED",
    "FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR",
    "FLAC__STREAM_DECODER_UNINITIALIZED"
};

FLAC_API const char * const FLAC__StreamDecoderInitStatusString[] = {
    "FLAC__STREAM_DECODER_INIT_STATUS_OK",
    "FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER",
    "FLAC__STREAM_DECODER_INIT_STATUS_INVALID_CALLBACKS",
    "FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR",
    "FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE",
    "FLAC__STREAM_DECODER_INIT_STATUS_ALREADY_INITIALIZED"
};

FLAC_API const char * const FLAC__StreamDecoderReadStatusString[] = {
    "FLAC__STREAM_DECODER_READ_STATUS_CONTINUE",
    "FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM",
    "FLAC__STREAM_DECODER_READ_STATUS_ABORT"
};

FLAC_API const char * const FLAC__StreamDecoderSeekStatusString[] = {
    "FLAC__STREAM_DECODER_SEEK_STATUS_OK",
    "FLAC__STREAM_DECODER_SEEK_STATUS_ERROR",
    "FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED"
};

FLAC_API const char * const FLAC__StreamDecoderTellStatusString[] = {
    "FLAC__STREAM_DECODER_TELL_STATUS_OK",
    "FLAC__STREAM_DECODER_TELL_STATUS_ERROR",
    "FLAC__STREAM_DECODER_TELL_STATUS_UNSUPPORTED"
};

FLAC_API const char * const FLAC__StreamDecoderLengthStatusString[] = {
    "FLAC__STREAM_DECODER_LENGTH_STATUS_OK",
    "FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR",
    "FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED"
};

FLAC_API const char * const FLAC__StreamDecoderWriteStatusString[] = {
    "FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE",
    "FLAC__STREAM_DECODER_WRITE_STATUS_ABORT"
};

FLAC_API const char * const FLAC__StreamDecoderErrorStatusString[] = {
    "FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC",
    "FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER",
    "FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH",
    "FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM"
};

/***********************************************************************
 *
 * Class constructor/destructor
 *
 ***********************************************************************/
FLAC_API FLAC__StreamDecoder *FLAC__stream_decoder_new(void)
{
    FLAC__StreamDecoder *decoder;
    unsigned i;

    CHOC_ASSERT(sizeof(int) >= 4); /* we want to die right away if this is not true */

    decoder = (FLAC__StreamDecoder*) calloc(1, sizeof(FLAC__StreamDecoder));
    if(decoder == 0) {
        return 0;
    }

    decoder->protected_ = (FLAC__StreamDecoderProtected*) calloc(1, sizeof(FLAC__StreamDecoderProtected));
    if(decoder->protected_ == 0) {
        free(decoder);
        return 0;
    }

    decoder->private_ = (FLAC__StreamDecoderPrivate*) calloc(1, sizeof(FLAC__StreamDecoderPrivate));
    if(decoder->private_ == 0) {
        free(decoder->protected_);
        free(decoder);
        return 0;
    }

    decoder->private_->input = FLAC__bitreader_new();
    if(decoder->private_->input == 0) {
        free(decoder->private_);
        free(decoder->protected_);
        free(decoder);
        return 0;
    }

    decoder->private_->metadata_filter_ids_capacity = 16;
    if(0 == (decoder->private_->metadata_filter_ids = (FLAC__byte*) malloc((FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8) * decoder->private_->metadata_filter_ids_capacity))) {
        FLAC__bitreader_delete(decoder->private_->input);
        free(decoder->private_);
        free(decoder->protected_);
        free(decoder);
        return 0;
    }

    for(i = 0; i < FLAC__MAX_CHANNELS; i++) {
        decoder->private_->output[i] = 0;
        decoder->private_->residual_unaligned[i] = decoder->private_->residual[i] = 0;
    }

    decoder->private_->output_capacity = 0;
    decoder->private_->output_channels = 0;
    decoder->private_->has_seek_table = false;

    for(i = 0; i < FLAC__MAX_CHANNELS; i++)
        FLAC__format_entropy_coding_method_partitioned_rice_contents_init(&decoder->private_->partitioned_rice_contents[i]);

    decoder->private_->file = 0;

    set_defaults_(decoder);

    decoder->protected_->state = FLAC__STREAM_DECODER_UNINITIALIZED;

    return decoder;
}

FLAC_API void FLAC__stream_decoder_delete(FLAC__StreamDecoder *decoder)
{
    unsigned i;

    if (decoder == NULL)
        return ;

    CHOC_ASSERT(0 != decoder->protected_);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != decoder->private_->input);

    (void)FLAC__stream_decoder_finish(decoder);

    if(0 != decoder->private_->metadata_filter_ids)
        free(decoder->private_->metadata_filter_ids);

    FLAC__bitreader_delete(decoder->private_->input);

    for(i = 0; i < FLAC__MAX_CHANNELS; i++)
        FLAC__format_entropy_coding_method_partitioned_rice_contents_clear(&decoder->private_->partitioned_rice_contents[i]);

    free(decoder->private_);
    free(decoder->protected_);
    free(decoder);
}

/***********************************************************************
 *
 * Public class methods
 *
 ***********************************************************************/

static FLAC__StreamDecoderInitStatus init_stream_internal_(
    FLAC__StreamDecoder *decoder,
    FLAC__StreamDecoderReadCallback read_callback,
    FLAC__StreamDecoderSeekCallback seek_callback,
    FLAC__StreamDecoderTellCallback tell_callback,
    FLAC__StreamDecoderLengthCallback length_callback,
    FLAC__StreamDecoderEofCallback eof_callback,
    FLAC__StreamDecoderWriteCallback write_callback,
    FLAC__StreamDecoderMetadataCallback metadata_callback,
    FLAC__StreamDecoderErrorCallback error_callback,
    void *client_data,
    FLAC__bool is_ogg
)
{
    CHOC_ASSERT(0 != decoder);

    if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
        return FLAC__STREAM_DECODER_INIT_STATUS_ALREADY_INITIALIZED;

#if !FLAC__HAS_OGG
    if(is_ogg)
        return FLAC__STREAM_DECODER_INIT_STATUS_UNSUPPORTED_CONTAINER;
#endif

    if(
        0 == read_callback ||
        0 == write_callback ||
        0 == error_callback ||
        (seek_callback && (0 == tell_callback || 0 == length_callback || 0 == eof_callback))
    )
        return FLAC__STREAM_DECODER_INIT_STATUS_INVALID_CALLBACKS;

#if FLAC__HAS_OGG
    decoder->private_->is_ogg = is_ogg;
    if(is_ogg && !FLAC__ogg_decoder_aspect_init(&decoder->protected_->ogg_decoder_aspect))
        return decoder->protected_->initstate = FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE;
#endif

    /*
     * get the CPU info and set the function pointers
     */
    FLAC__cpu_info(&decoder->private_->cpuinfo);
    /* first default to the non-asm routines */
    decoder->private_->local_lpc_restore_signal = FLAC__lpc_restore_signal;
    decoder->private_->local_lpc_restore_signal_64bit = FLAC__lpc_restore_signal_wide;
    decoder->private_->local_lpc_restore_signal_16bit = FLAC__lpc_restore_signal;
    /* now override with asm where appropriate */
#ifndef FLAC__NO_ASM
    if(decoder->private_->cpuinfo.use_asm) {
#ifdef FLAC__CPU_IA32
        CHOC_ASSERT(decoder->private_->cpuinfo.type == FLAC__CPUINFO_TYPE_IA32);
#ifdef FLAC__HAS_NASM
        decoder->private_->local_lpc_restore_signal_64bit = FLAC__lpc_restore_signal_wide_asm_ia32; /* OPT_IA32: was really necessary for GCC < 4.9 */
        if(decoder->private_->cpuinfo.ia32.mmx) {
            decoder->private_->local_lpc_restore_signal = FLAC__lpc_restore_signal_asm_ia32;
            decoder->private_->local_lpc_restore_signal_16bit = FLAC__lpc_restore_signal_asm_ia32_mmx;
        }
        else {
            decoder->private_->local_lpc_restore_signal = FLAC__lpc_restore_signal_asm_ia32;
            decoder->private_->local_lpc_restore_signal_16bit = FLAC__lpc_restore_signal_asm_ia32;
        }
#endif
#ifdef FLAC__HAS_X86INTRIN
# if defined FLAC__SSE2_SUPPORTED && !defined FLAC__HAS_NASM /* OPT_SSE: not better than MMX asm */
        if(decoder->private_->cpuinfo.ia32.sse2) {
            decoder->private_->local_lpc_restore_signal_16bit = FLAC__lpc_restore_signal_16_intrin_sse2;
        }
# endif
# if defined FLAC__SSE4_1_SUPPORTED
        if(decoder->private_->cpuinfo.ia32.sse41) {
            decoder->private_->local_lpc_restore_signal_64bit = FLAC__lpc_restore_signal_wide_intrin_sse41;
        }
# endif
#endif
#elif defined FLAC__CPU_X86_64
        CHOC_ASSERT(decoder->private_->cpuinfo.type == FLAC__CPUINFO_TYPE_X86_64);
        /* No useful SSE optimizations yet */
#endif
    }
#endif

    /* from here on, errors are fatal */

    if(!FLAC__bitreader_init(decoder->private_->input, read_callback_, decoder)) {
        decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
        return FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR;
    }

    decoder->private_->read_callback = read_callback;
    decoder->private_->seek_callback = seek_callback;
    decoder->private_->tell_callback = tell_callback;
    decoder->private_->length_callback = length_callback;
    decoder->private_->eof_callback = eof_callback;
    decoder->private_->write_callback = write_callback;
    decoder->private_->metadata_callback = metadata_callback;
    decoder->private_->error_callback = error_callback;
    decoder->private_->client_data = client_data;
    decoder->private_->fixed_block_size = decoder->private_->next_fixed_block_size = 0;
    decoder->private_->samples_decoded = 0;
    decoder->private_->has_stream_info = false;
    decoder->private_->cached = false;

    decoder->private_->do_md5_checking = decoder->protected_->md5_checking;
    decoder->private_->is_seeking = false;

    decoder->private_->internal_reset_hack = true; /* so the following reset does not try to rewind the input */
    if(!FLAC__stream_decoder_reset(decoder)) {
        /* above call sets the state for us */
        return FLAC__STREAM_DECODER_INIT_STATUS_MEMORY_ALLOCATION_ERROR;
    }

    return FLAC__STREAM_DECODER_INIT_STATUS_OK;
}

FLAC_API FLAC__StreamDecoderInitStatus FLAC__stream_decoder_init_stream(
    FLAC__StreamDecoder *decoder,
    FLAC__StreamDecoderReadCallback read_callback,
    FLAC__StreamDecoderSeekCallback seek_callback,
    FLAC__StreamDecoderTellCallback tell_callback,
    FLAC__StreamDecoderLengthCallback length_callback,
    FLAC__StreamDecoderEofCallback eof_callback,
    FLAC__StreamDecoderWriteCallback write_callback,
    FLAC__StreamDecoderMetadataCallback metadata_callback,
    FLAC__StreamDecoderErrorCallback error_callback,
    void *client_data
)
{
    return init_stream_internal_(
        decoder,
        read_callback,
        seek_callback,
        tell_callback,
        length_callback,
        eof_callback,
        write_callback,
        metadata_callback,
        error_callback,
        client_data,
        /*is_ogg=*/false
    );
}

FLAC_API FLAC__StreamDecoderInitStatus FLAC__stream_decoder_init_ogg_stream(
    FLAC__StreamDecoder *decoder,
    FLAC__StreamDecoderReadCallback read_callback,
    FLAC__StreamDecoderSeekCallback seek_callback,
    FLAC__StreamDecoderTellCallback tell_callback,
    FLAC__StreamDecoderLengthCallback length_callback,
    FLAC__StreamDecoderEofCallback eof_callback,
    FLAC__StreamDecoderWriteCallback write_callback,
    FLAC__StreamDecoderMetadataCallback metadata_callback,
    FLAC__StreamDecoderErrorCallback error_callback,
    void *client_data
)
{
    return init_stream_internal_(
        decoder,
        read_callback,
        seek_callback,
        tell_callback,
        length_callback,
        eof_callback,
        write_callback,
        metadata_callback,
        error_callback,
        client_data,
        /*is_ogg=*/true
    );
}

FLAC_API FLAC__bool FLAC__stream_decoder_finish(FLAC__StreamDecoder *decoder)
{
    FLAC__bool md5_failed = false;
    unsigned i;

    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != decoder->protected_);

    if(decoder->protected_->state == FLAC__STREAM_DECODER_UNINITIALIZED)
        return true;

    /* see the comment in FLAC__stream_decoder_reset() as to why we
     * always call FLAC__MD5Final()
     */
    FLAC__MD5Final(decoder->private_->computed_md5sum, &decoder->private_->md5context);

    if(decoder->private_->has_seek_table && 0 != decoder->private_->seek_table.data.seek_table.points) {
        free(decoder->private_->seek_table.data.seek_table.points);
        decoder->private_->seek_table.data.seek_table.points = 0;
        decoder->private_->has_seek_table = false;
    }
    FLAC__bitreader_free(decoder->private_->input);
    for(i = 0; i < FLAC__MAX_CHANNELS; i++) {
        /* WATCHOUT:
         * FLAC__lpc_restore_signal_asm_ia32_mmx() requires that the
         * output arrays have a buffer of up to 3 zeroes in front
         * (at negative indices) for alignment purposes; we use 4
         * to keep the data well-aligned.
         */
        if(0 != decoder->private_->output[i]) {
            free(decoder->private_->output[i]-4);
            decoder->private_->output[i] = 0;
        }
        if(0 != decoder->private_->residual_unaligned[i]) {
            free(decoder->private_->residual_unaligned[i]);
            decoder->private_->residual_unaligned[i] = decoder->private_->residual[i] = 0;
        }
    }
    decoder->private_->output_capacity = 0;
    decoder->private_->output_channels = 0;

#if FLAC__HAS_OGG
    if(decoder->private_->is_ogg)
        FLAC__ogg_decoder_aspect_finish(&decoder->protected_->ogg_decoder_aspect);
#endif

    if(0 != decoder->private_->file) {
        if(decoder->private_->file != stdin)
            fclose(decoder->private_->file);
        decoder->private_->file = 0;
    }

    if(decoder->private_->do_md5_checking) {
        if(memcmp(decoder->private_->stream_info.data.stream_info.md5sum, decoder->private_->computed_md5sum, 16))
            md5_failed = true;
    }
    decoder->private_->is_seeking = false;

    set_defaults_(decoder);

    decoder->protected_->state = FLAC__STREAM_DECODER_UNINITIALIZED;

    return !md5_failed;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_ogg_serial_number(FLAC__StreamDecoder *decoder, long value)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != decoder->protected_);
    if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
        return false;
#if FLAC__HAS_OGG
    /* can't check decoder->private_->is_ogg since that's not set until init time */
    FLAC__ogg_decoder_aspect_set_serial_number(&decoder->protected_->ogg_decoder_aspect, value);
    return true;
#else
    (void)value;
    return false;
#endif
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_md5_checking(FLAC__StreamDecoder *decoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);
    if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
        return false;
    decoder->protected_->md5_checking = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_respond(FLAC__StreamDecoder *decoder, FLAC__MetadataType type)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != decoder->protected_);
    CHOC_ASSERT((unsigned)type <= FLAC__MAX_METADATA_TYPE_CODE);
    /* double protection */
    if((unsigned)type > FLAC__MAX_METADATA_TYPE_CODE)
        return false;
    if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
        return false;
    decoder->private_->metadata_filter[type] = true;
    if(type == FLAC__METADATA_TYPE_APPLICATION)
        decoder->private_->metadata_filter_ids_count = 0;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_respond_application(FLAC__StreamDecoder *decoder, const FLAC__byte id[4])
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != decoder->protected_);
    CHOC_ASSERT(0 != id);
    if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
        return false;

    if(decoder->private_->metadata_filter[FLAC__METADATA_TYPE_APPLICATION])
        return true;

    CHOC_ASSERT(0 != decoder->private_->metadata_filter_ids);

    if(decoder->private_->metadata_filter_ids_count == decoder->private_->metadata_filter_ids_capacity) {
        if(0 == (decoder->private_->metadata_filter_ids = (FLAC__byte*) safe_realloc_mul_2op_(decoder->private_->metadata_filter_ids, decoder->private_->metadata_filter_ids_capacity, /*times*/2))) {
            decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
            return false;
        }
        decoder->private_->metadata_filter_ids_capacity *= 2;
    }

    memcpy(decoder->private_->metadata_filter_ids + decoder->private_->metadata_filter_ids_count * (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8), id, (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8));
    decoder->private_->metadata_filter_ids_count++;

    return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_respond_all(FLAC__StreamDecoder *decoder)
{
    unsigned i;
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != decoder->protected_);
    if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
        return false;
    for(i = 0; i < sizeof(decoder->private_->metadata_filter) / sizeof(decoder->private_->metadata_filter[0]); i++)
        decoder->private_->metadata_filter[i] = true;
    decoder->private_->metadata_filter_ids_count = 0;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_ignore(FLAC__StreamDecoder *decoder, FLAC__MetadataType type)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != decoder->protected_);
    CHOC_ASSERT((unsigned)type <= FLAC__MAX_METADATA_TYPE_CODE);
    /* double protection */
    if((unsigned)type > FLAC__MAX_METADATA_TYPE_CODE)
        return false;
    if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
        return false;
    decoder->private_->metadata_filter[type] = false;
    if(type == FLAC__METADATA_TYPE_APPLICATION)
        decoder->private_->metadata_filter_ids_count = 0;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_ignore_application(FLAC__StreamDecoder *decoder, const FLAC__byte id[4])
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != decoder->protected_);
    CHOC_ASSERT(0 != id);
    if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
        return false;

    if(!decoder->private_->metadata_filter[FLAC__METADATA_TYPE_APPLICATION])
        return true;

    CHOC_ASSERT(0 != decoder->private_->metadata_filter_ids);

    if(decoder->private_->metadata_filter_ids_count == decoder->private_->metadata_filter_ids_capacity) {
        if(0 == (decoder->private_->metadata_filter_ids = (FLAC__byte*) safe_realloc_mul_2op_(decoder->private_->metadata_filter_ids, decoder->private_->metadata_filter_ids_capacity, /*times*/2))) {
            decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
            return false;
        }
        decoder->private_->metadata_filter_ids_capacity *= 2;
    }

    memcpy(decoder->private_->metadata_filter_ids + decoder->private_->metadata_filter_ids_count * (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8), id, (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8));
    decoder->private_->metadata_filter_ids_count++;

    return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_set_metadata_ignore_all(FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != decoder->protected_);
    if(decoder->protected_->state != FLAC__STREAM_DECODER_UNINITIALIZED)
        return false;
    memset(decoder->private_->metadata_filter, 0, sizeof(decoder->private_->metadata_filter));
    decoder->private_->metadata_filter_ids_count = 0;
    return true;
}

FLAC_API FLAC__StreamDecoderState FLAC__stream_decoder_get_state(const FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);
    return decoder->protected_->state;
}

FLAC_API const char *FLAC__stream_decoder_get_resolved_state_string(const FLAC__StreamDecoder *decoder)
{
    return FLAC__StreamDecoderStateString[decoder->protected_->state];
}

FLAC_API FLAC__bool FLAC__stream_decoder_get_md5_checking(const FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);
    return decoder->protected_->md5_checking;
}

FLAC_API FLAC__uint64 FLAC__stream_decoder_get_total_samples(const FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);
    return decoder->private_->has_stream_info? decoder->private_->stream_info.data.stream_info.total_samples : 0;
}

FLAC_API unsigned FLAC__stream_decoder_get_channels(const FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);
    return decoder->protected_->channels;
}

FLAC_API FLAC__ChannelAssignment FLAC__stream_decoder_get_channel_assignment(const FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);
    return decoder->protected_->channel_assignment;
}

FLAC_API unsigned FLAC__stream_decoder_get_bits_per_sample(const FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);
    return decoder->protected_->bits_per_sample;
}

FLAC_API unsigned FLAC__stream_decoder_get_sample_rate(const FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);
    return decoder->protected_->sample_rate;
}

FLAC_API unsigned FLAC__stream_decoder_get_blocksize(const FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);
    return decoder->protected_->blocksize;
}

FLAC_API FLAC__bool FLAC__stream_decoder_get_decode_position(const FLAC__StreamDecoder *decoder, FLAC__uint64 *position)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != position);

#if FLAC__HAS_OGG
    if(decoder->private_->is_ogg)
        return false;
#endif
    if(0 == decoder->private_->tell_callback)
        return false;
    if(decoder->private_->tell_callback(decoder, position, decoder->private_->client_data) != FLAC__STREAM_DECODER_TELL_STATUS_OK)
        return false;
    /* should never happen since all FLAC frames and metadata blocks are byte aligned, but check just in case */
    if(!FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input))
        return false;
    CHOC_ASSERT(*position >= FLAC__stream_decoder_get_input_bytes_unconsumed(decoder));
    *position -= FLAC__stream_decoder_get_input_bytes_unconsumed(decoder);
    return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_flush(FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != decoder->protected_);

    decoder->private_->samples_decoded = 0;
    decoder->private_->do_md5_checking = false;

#if FLAC__HAS_OGG
    if(decoder->private_->is_ogg)
        FLAC__ogg_decoder_aspect_flush(&decoder->protected_->ogg_decoder_aspect);
#endif

    if(!FLAC__bitreader_clear(decoder->private_->input)) {
        decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
        return false;
    }
    decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;

    return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_reset(FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);
    CHOC_ASSERT(0 != decoder->protected_);

    if(!FLAC__stream_decoder_flush(decoder)) {
        /* above call sets the state for us */
        return false;
    }

#if FLAC__HAS_OGG
    /*@@@ could go in !internal_reset_hack block below */
    if(decoder->private_->is_ogg)
        FLAC__ogg_decoder_aspect_reset(&decoder->protected_->ogg_decoder_aspect);
#endif

    /* Rewind if necessary.  If FLAC__stream_decoder_init() is calling us,
     * (internal_reset_hack) don't try to rewind since we are already at
     * the beginning of the stream and don't want to fail if the input is
     * not seekable.
     */
    if(!decoder->private_->internal_reset_hack) {
        if(decoder->private_->file == stdin)
            return false; /* can't rewind stdin, reset fails */
        if(decoder->private_->seek_callback && decoder->private_->seek_callback(decoder, 0, decoder->private_->client_data) == FLAC__STREAM_DECODER_SEEK_STATUS_ERROR)
            return false; /* seekable and seek fails, reset fails */
    }
    else
        decoder->private_->internal_reset_hack = false;

    decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_METADATA;

    decoder->private_->has_stream_info = false;
    if(decoder->private_->has_seek_table && 0 != decoder->private_->seek_table.data.seek_table.points) {
        free(decoder->private_->seek_table.data.seek_table.points);
        decoder->private_->seek_table.data.seek_table.points = 0;
        decoder->private_->has_seek_table = false;
    }
    decoder->private_->do_md5_checking = decoder->protected_->md5_checking;
    /*
     * This goes in reset() and not flush() because according to the spec, a
     * fixed-blocksize stream must stay that way through the whole stream.
     */
    decoder->private_->fixed_block_size = decoder->private_->next_fixed_block_size = 0;

    /* We initialize the FLAC__MD5Context even though we may never use it.  This
     * is because md5 checking may be turned on to start and then turned off if
     * a seek occurs.  So we init the context here and finalize it in
     * FLAC__stream_decoder_finish() to make sure things are always cleaned up
     * properly.
     */
    FLAC__MD5Init(&decoder->private_->md5context);

    decoder->private_->first_frame_offset = 0;
    decoder->private_->unparseable_frame_count = 0;

    return true;
}

FLAC_API FLAC__bool FLAC__stream_decoder_process_single(FLAC__StreamDecoder *decoder)
{
    FLAC__bool got_a_frame;
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);

    while(1) {
        switch(decoder->protected_->state) {
            case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
                if(!find_metadata_(decoder))
                    return false; /* above function sets the status for us */
                break;
            case FLAC__STREAM_DECODER_READ_METADATA:
                if(!read_metadata_(decoder))
                    return false; /* above function sets the status for us */
                else
                    return true;
            case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
                if(!frame_sync_(decoder))
                    return true; /* above function sets the status for us */
                break;
            case FLAC__STREAM_DECODER_READ_FRAME:
                if(!read_frame_(decoder, &got_a_frame, /*do_full_decode=*/true))
                    return false; /* above function sets the status for us */
                if(got_a_frame)
                    return true; /* above function sets the status for us */
                break;
            case FLAC__STREAM_DECODER_END_OF_STREAM:
            case FLAC__STREAM_DECODER_ABORTED:
                return true;
            default:
                CHOC_ASSERT(0);
                return false;
        }
    }
}

FLAC_API FLAC__bool FLAC__stream_decoder_process_until_end_of_metadata(FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);

    while(1) {
        switch(decoder->protected_->state) {
            case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
                if(!find_metadata_(decoder))
                    return false; /* above function sets the status for us */
                break;
            case FLAC__STREAM_DECODER_READ_METADATA:
                if(!read_metadata_(decoder))
                    return false; /* above function sets the status for us */
                break;
            case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
            case FLAC__STREAM_DECODER_READ_FRAME:
            case FLAC__STREAM_DECODER_END_OF_STREAM:
            case FLAC__STREAM_DECODER_ABORTED:
                return true;
            default:
                CHOC_ASSERT(0);
                return false;
        }
    }
}

FLAC_API FLAC__bool FLAC__stream_decoder_process_until_end_of_stream(FLAC__StreamDecoder *decoder)
{
    FLAC__bool dummy;
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);

    while(1) {
        switch(decoder->protected_->state) {
            case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
                if(!find_metadata_(decoder))
                    return false; /* above function sets the status for us */
                break;
            case FLAC__STREAM_DECODER_READ_METADATA:
                if(!read_metadata_(decoder))
                    return false; /* above function sets the status for us */
                break;
            case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
                if(!frame_sync_(decoder))
                    return true; /* above function sets the status for us */
                break;
            case FLAC__STREAM_DECODER_READ_FRAME:
                if(!read_frame_(decoder, &dummy, /*do_full_decode=*/true))
                    return false; /* above function sets the status for us */
                break;
            case FLAC__STREAM_DECODER_END_OF_STREAM:
            case FLAC__STREAM_DECODER_ABORTED:
                return true;
            default:
                CHOC_ASSERT(0);
                return false;
        }
    }
}

FLAC_API FLAC__bool FLAC__stream_decoder_skip_single_frame(FLAC__StreamDecoder *decoder)
{
    FLAC__bool got_a_frame;
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->protected_);

    while(1) {
        switch(decoder->protected_->state) {
            case FLAC__STREAM_DECODER_SEARCH_FOR_METADATA:
            case FLAC__STREAM_DECODER_READ_METADATA:
                return false; /* above function sets the status for us */
            case FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC:
                if(!frame_sync_(decoder))
                    return true; /* above function sets the status for us */
                break;
            case FLAC__STREAM_DECODER_READ_FRAME:
                if(!read_frame_(decoder, &got_a_frame, /*do_full_decode=*/false))
                    return false; /* above function sets the status for us */
                if(got_a_frame)
                    return true; /* above function sets the status for us */
                break;
            case FLAC__STREAM_DECODER_END_OF_STREAM:
            case FLAC__STREAM_DECODER_ABORTED:
                return true;
            default:
                CHOC_ASSERT(0);
                return false;
        }
    }
}

FLAC_API FLAC__bool FLAC__stream_decoder_seek_absolute(FLAC__StreamDecoder *decoder, FLAC__uint64 sample)
{
    FLAC__uint64 length;

    CHOC_ASSERT(0 != decoder);

    if(
        decoder->protected_->state != FLAC__STREAM_DECODER_SEARCH_FOR_METADATA &&
        decoder->protected_->state != FLAC__STREAM_DECODER_READ_METADATA &&
        decoder->protected_->state != FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC &&
        decoder->protected_->state != FLAC__STREAM_DECODER_READ_FRAME &&
        decoder->protected_->state != FLAC__STREAM_DECODER_END_OF_STREAM
    )
        return false;

    if(0 == decoder->private_->seek_callback)
        return false;

    CHOC_ASSERT(decoder->private_->seek_callback);
    CHOC_ASSERT(decoder->private_->tell_callback);
    CHOC_ASSERT(decoder->private_->length_callback);
    CHOC_ASSERT(decoder->private_->eof_callback);

    if(FLAC__stream_decoder_get_total_samples(decoder) > 0 && sample >= FLAC__stream_decoder_get_total_samples(decoder))
        return false;

    decoder->private_->is_seeking = true;

    /* turn off md5 checking if a seek is attempted */
    decoder->private_->do_md5_checking = false;

    /* get the file length (currently our algorithm needs to know the length so it's also an error to get FLAC__STREAM_DECODER_LENGTH_STATUS_UNSUPPORTED) */
    if(decoder->private_->length_callback(decoder, &length, decoder->private_->client_data) != FLAC__STREAM_DECODER_LENGTH_STATUS_OK) {
        decoder->private_->is_seeking = false;
        return false;
    }

    /* if we haven't finished processing the metadata yet, do that so we have the STREAMINFO, SEEK_TABLE, and first_frame_offset */
    if(
        decoder->protected_->state == FLAC__STREAM_DECODER_SEARCH_FOR_METADATA ||
        decoder->protected_->state == FLAC__STREAM_DECODER_READ_METADATA
    ) {
        if(!FLAC__stream_decoder_process_until_end_of_metadata(decoder)) {
            /* above call sets the state for us */
            decoder->private_->is_seeking = false;
            return false;
        }
        /* check this again in case we didn't know total_samples the first time */
        if(FLAC__stream_decoder_get_total_samples(decoder) > 0 && sample >= FLAC__stream_decoder_get_total_samples(decoder)) {
            decoder->private_->is_seeking = false;
            return false;
        }
    }

    {
        const FLAC__bool ok =
#if FLAC__HAS_OGG
            decoder->private_->is_ogg?
            seek_to_absolute_sample_ogg_(decoder, length, sample) :
#endif
            seek_to_absolute_sample_(decoder, length, sample)
        ;
        decoder->private_->is_seeking = false;
        return ok;
    }
}

/***********************************************************************
 *
 * Protected class methods
 *
 ***********************************************************************/

inline unsigned FLAC__stream_decoder_get_input_bytes_unconsumed(const FLAC__StreamDecoder *decoder)
{
    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input));
    CHOC_ASSERT(!(FLAC__bitreader_get_input_bits_unconsumed(decoder->private_->input) & 7));
    return FLAC__bitreader_get_input_bits_unconsumed(decoder->private_->input) / 8;
}

/***********************************************************************
 *
 * Private class methods
 *
 ***********************************************************************/

inline void set_defaults_(FLAC__StreamDecoder *decoder)
{
#if FLAC__HAS_OGG
    decoder->private_->is_ogg = false;
#endif
    decoder->private_->read_callback = 0;
    decoder->private_->seek_callback = 0;
    decoder->private_->tell_callback = 0;
    decoder->private_->length_callback = 0;
    decoder->private_->eof_callback = 0;
    decoder->private_->write_callback = 0;
    decoder->private_->metadata_callback = 0;
    decoder->private_->error_callback = 0;
    decoder->private_->client_data = 0;

    memset(decoder->private_->metadata_filter, 0, sizeof(decoder->private_->metadata_filter));
    decoder->private_->metadata_filter[FLAC__METADATA_TYPE_STREAMINFO] = true;
    decoder->private_->metadata_filter_ids_count = 0;

    decoder->protected_->md5_checking = false;

#if FLAC__HAS_OGG
    FLAC__ogg_decoder_aspect_set_defaults(&decoder->protected_->ogg_decoder_aspect);
#endif
}

inline FLAC__bool allocate_output_(FLAC__StreamDecoder *decoder, unsigned size, unsigned channels)
{
    unsigned i;
    FLAC__int32 *tmp;

    if(size <= decoder->private_->output_capacity && channels <= decoder->private_->output_channels)
        return true;

    /* simply using realloc() is not practical because the number of channels may change mid-stream */

    for(i = 0; i < FLAC__MAX_CHANNELS; i++) {
        if(0 != decoder->private_->output[i]) {
            free(decoder->private_->output[i]-4);
            decoder->private_->output[i] = 0;
        }
        if(0 != decoder->private_->residual_unaligned[i]) {
            free(decoder->private_->residual_unaligned[i]);
            decoder->private_->residual_unaligned[i] = decoder->private_->residual[i] = 0;
        }
    }

    for(i = 0; i < channels; i++) {
        /* WATCHOUT:
         * FLAC__lpc_restore_signal_asm_ia32_mmx() requires that the
         * output arrays have a buffer of up to 3 zeroes in front
         * (at negative indices) for alignment purposes; we use 4
         * to keep the data well-aligned.
         */
        tmp = (FLAC__int32*) safe_malloc_muladd2_(sizeof(FLAC__int32), /*times (*/size, /*+*/4/*)*/);
        if(tmp == 0) {
            decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
            return false;
        }
        memset(tmp, 0, sizeof(FLAC__int32)*4);
        decoder->private_->output[i] = tmp + 4;

        if(!FLAC__memory_alloc_aligned_int32_array(size, &decoder->private_->residual_unaligned[i], &decoder->private_->residual[i])) {
            decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
            return false;
        }
    }

    decoder->private_->output_capacity = size;
    decoder->private_->output_channels = channels;

    return true;
}

FLAC__bool has_id_filtered_(FLAC__StreamDecoder *decoder, FLAC__byte *id)
{
    size_t i;

    CHOC_ASSERT(0 != decoder);
    CHOC_ASSERT(0 != decoder->private_);

    for(i = 0; i < decoder->private_->metadata_filter_ids_count; i++)
        if(0 == memcmp(decoder->private_->metadata_filter_ids + i * (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8), id, (FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8)))
            return true;

    return false;
}

FLAC__bool find_metadata_(FLAC__StreamDecoder *decoder)
{
    FLAC__uint32 x;
    unsigned i, id_;
    FLAC__bool first = true;

    CHOC_ASSERT(FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input));

    for(i = id_ = 0; i < 4; ) {
        if(decoder->private_->cached) {
            x = (FLAC__uint32)decoder->private_->lookahead;
            decoder->private_->cached = false;
        }
        else {
            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, 8))
                return false; /* read_callback_ sets the state for us */
        }
        if(x == FLAC__STREAM_SYNC_STRING[i]) {
            first = true;
            i++;
            id_ = 0;
            continue;
        }

        if(id_ >= 3)
            return false;

        if(x == ID3V2_TAG_[id_]) {
            id_++;
            i = 0;
            if(id_ == 3) {
                if(!skip_id3v2_tag_(decoder))
                    return false; /* skip_id3v2_tag_ sets the state for us */
            }
            continue;
        }
        id_ = 0;
        if(x == 0xff) { /* MAGIC NUMBER for the first 8 frame sync bits */
            decoder->private_->header_warmup[0] = (FLAC__byte)x;
            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, 8))
                return false; /* read_callback_ sets the state for us */

            /* we have to check if we just read two 0xff's in a row; the second may actually be the beginning of the sync code */
            /* else we have to check if the second byte is the end of a sync code */
            if(x == 0xff) { /* MAGIC NUMBER for the first 8 frame sync bits */
                decoder->private_->lookahead = (FLAC__byte)x;
                decoder->private_->cached = true;
            }
            else if(x >> 1 == 0x7c) { /* MAGIC NUMBER for the last 6 sync bits and reserved 7th bit */
                decoder->private_->header_warmup[1] = (FLAC__byte)x;
                decoder->protected_->state = FLAC__STREAM_DECODER_READ_FRAME;
                return true;
            }
        }
        i = 0;
        if(first) {
            send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC);
            first = false;
        }
    }

    decoder->protected_->state = FLAC__STREAM_DECODER_READ_METADATA;
    return true;
}

FLAC__bool read_metadata_(FLAC__StreamDecoder *decoder)
{
    FLAC__bool is_last;
    FLAC__uint32 i, x, type, length;

    CHOC_ASSERT(FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input));

    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_IS_LAST_LEN))
        return false; /* read_callback_ sets the state for us */
    is_last = x? true : false;

    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &type, FLAC__STREAM_METADATA_TYPE_LEN))
        return false; /* read_callback_ sets the state for us */

    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &length, FLAC__STREAM_METADATA_LENGTH_LEN))
        return false; /* read_callback_ sets the state for us */

    if(type == FLAC__METADATA_TYPE_STREAMINFO) {
        if(!read_metadata_streaminfo_(decoder, is_last, length))
            return false;

        decoder->private_->has_stream_info = true;
        if(0 == memcmp(decoder->private_->stream_info.data.stream_info.md5sum, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16))
            decoder->private_->do_md5_checking = false;
        if(!decoder->private_->is_seeking && decoder->private_->metadata_filter[FLAC__METADATA_TYPE_STREAMINFO] && decoder->private_->metadata_callback)
            decoder->private_->metadata_callback(decoder, &decoder->private_->stream_info, decoder->private_->client_data);
    }
    else if(type == FLAC__METADATA_TYPE_SEEKTABLE) {
        if(!read_metadata_seektable_(decoder, is_last, length))
            return false;

        decoder->private_->has_seek_table = true;
        if(!decoder->private_->is_seeking && decoder->private_->metadata_filter[FLAC__METADATA_TYPE_SEEKTABLE] && decoder->private_->metadata_callback)
            decoder->private_->metadata_callback(decoder, &decoder->private_->seek_table, decoder->private_->client_data);
    }
    else {
        FLAC__bool skip_it = !decoder->private_->metadata_filter[type];
        unsigned real_length = length;
        FLAC__StreamMetadata block;

        memset(&block, 0, sizeof(block));
        block.is_last = is_last;
        block.type = (FLAC__MetadataType)type;
        block.length = length;

        if(type == FLAC__METADATA_TYPE_APPLICATION) {
            if(!FLAC__bitreader_read_byte_block_aligned_no_crc(decoder->private_->input, block.data.application.id, FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8))
                return false; /* read_callback_ sets the state for us */

            if(real_length < FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8) { /* underflow check */
                decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;/*@@@@@@ maybe wrong error? need to resync?*/
                return false;
            }

            real_length -= FLAC__STREAM_METADATA_APPLICATION_ID_LEN/8;

            if(decoder->private_->metadata_filter_ids_count > 0 && has_id_filtered_(decoder, block.data.application.id))
                skip_it = !skip_it;
        }

        if(skip_it) {
            if(!FLAC__bitreader_skip_byte_block_aligned_no_crc(decoder->private_->input, real_length))
                return false; /* read_callback_ sets the state for us */
        }
        else {
            FLAC__bool ok = true;
            switch(type) {
                case FLAC__METADATA_TYPE_PADDING:
                    /* skip the padding bytes */
                    if(!FLAC__bitreader_skip_byte_block_aligned_no_crc(decoder->private_->input, real_length))
                        ok = false; /* read_callback_ sets the state for us */
                    break;
                case FLAC__METADATA_TYPE_APPLICATION:
                    /* remember, we read the ID already */
                    if(real_length > 0) {
                        if(0 == (block.data.application.data = (FLAC__byte*) malloc(real_length))) {
                            decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
                            ok = false;
                        }
                        else if(!FLAC__bitreader_read_byte_block_aligned_no_crc(decoder->private_->input, block.data.application.data, real_length))
                            ok = false; /* read_callback_ sets the state for us */
                    }
                    else
                        block.data.application.data = 0;
                    break;
                case FLAC__METADATA_TYPE_VORBIS_COMMENT:
                    if(!read_metadata_vorbiscomment_(decoder, &block.data.vorbis_comment, real_length))
                        ok = false;
                    break;
                case FLAC__METADATA_TYPE_CUESHEET:
                    if(!read_metadata_cuesheet_(decoder, &block.data.cue_sheet))
                        ok = false;
                    break;
                case FLAC__METADATA_TYPE_PICTURE:
                    if(!read_metadata_picture_(decoder, &block.data.picture))
                        ok = false;
                    break;
                case FLAC__METADATA_TYPE_STREAMINFO:
                case FLAC__METADATA_TYPE_SEEKTABLE:
                    CHOC_ASSERT(0);
                    break;
                default:
                    if(real_length > 0) {
                        if(0 == (block.data.unknown.data = (FLAC__byte*) malloc(real_length))) {
                            decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
                            ok = false;
                        }
                        else if(!FLAC__bitreader_read_byte_block_aligned_no_crc(decoder->private_->input, block.data.unknown.data, real_length))
                            ok = false; /* read_callback_ sets the state for us */
                    }
                    else
                        block.data.unknown.data = 0;
                    break;
            }
            if(ok && !decoder->private_->is_seeking && decoder->private_->metadata_callback)
                decoder->private_->metadata_callback(decoder, &block, decoder->private_->client_data);

            /* now we have to free any malloc()ed data in the block */
            switch(type) {
                case FLAC__METADATA_TYPE_PADDING:
                    break;
                case FLAC__METADATA_TYPE_APPLICATION:
                    if(0 != block.data.application.data)
                        free(block.data.application.data);
                    break;
                case FLAC__METADATA_TYPE_VORBIS_COMMENT:
                    if(0 != block.data.vorbis_comment.vendor_string.entry)
                        free(block.data.vorbis_comment.vendor_string.entry);
                    if(block.data.vorbis_comment.num_comments > 0)
                        for(i = 0; i < block.data.vorbis_comment.num_comments; i++)
                            if(0 != block.data.vorbis_comment.comments[i].entry)
                                free(block.data.vorbis_comment.comments[i].entry);
                    if(0 != block.data.vorbis_comment.comments)
                        free(block.data.vorbis_comment.comments);
                    break;
                case FLAC__METADATA_TYPE_CUESHEET:
                    if(block.data.cue_sheet.num_tracks > 0)
                        for(i = 0; i < block.data.cue_sheet.num_tracks; i++)
                            if(0 != block.data.cue_sheet.tracks[i].indices)
                                free(block.data.cue_sheet.tracks[i].indices);
                    if(0 != block.data.cue_sheet.tracks)
                        free(block.data.cue_sheet.tracks);
                    break;
                case FLAC__METADATA_TYPE_PICTURE:
                    if(0 != block.data.picture.mime_type)
                        free(block.data.picture.mime_type);
                    if(0 != block.data.picture.description)
                        free(block.data.picture.description);
                    if(0 != block.data.picture.data)
                        free(block.data.picture.data);
                    break;
                case FLAC__METADATA_TYPE_STREAMINFO:
                case FLAC__METADATA_TYPE_SEEKTABLE:
                    CHOC_ASSERT(0);
                default:
                    if(0 != block.data.unknown.data)
                        free(block.data.unknown.data);
                    break;
            }

            if(!ok) /* anything that unsets "ok" should also make sure decoder->protected_->state is updated */
                return false;
        }
    }

    if(is_last) {
        /* if this fails, it's OK, it's just a hint for the seek routine */
        if(!FLAC__stream_decoder_get_decode_position(decoder, &decoder->private_->first_frame_offset))
            decoder->private_->first_frame_offset = 0;
        decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
    }

    return true;
}

FLAC__bool read_metadata_streaminfo_(FLAC__StreamDecoder *decoder, FLAC__bool is_last, unsigned length)
{
    FLAC__uint32 x;
    unsigned bits, used_bits = 0;

    CHOC_ASSERT(FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input));

    decoder->private_->stream_info.type = FLAC__METADATA_TYPE_STREAMINFO;
    decoder->private_->stream_info.is_last = is_last;
    decoder->private_->stream_info.length = length;

    bits = FLAC__STREAM_METADATA_STREAMINFO_MIN_BLOCK_SIZE_LEN;
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, bits))
        return false; /* read_callback_ sets the state for us */
    decoder->private_->stream_info.data.stream_info.min_blocksize = x;
    used_bits += bits;

    bits = FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN;
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN))
        return false; /* read_callback_ sets the state for us */
    decoder->private_->stream_info.data.stream_info.max_blocksize = x;
    used_bits += bits;

    bits = FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN;
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN))
        return false; /* read_callback_ sets the state for us */
    decoder->private_->stream_info.data.stream_info.min_framesize = x;
    used_bits += bits;

    bits = FLAC__STREAM_METADATA_STREAMINFO_MAX_FRAME_SIZE_LEN;
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_MAX_FRAME_SIZE_LEN))
        return false; /* read_callback_ sets the state for us */
    decoder->private_->stream_info.data.stream_info.max_framesize = x;
    used_bits += bits;

    bits = FLAC__STREAM_METADATA_STREAMINFO_SAMPLE_RATE_LEN;
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_SAMPLE_RATE_LEN))
        return false; /* read_callback_ sets the state for us */
    decoder->private_->stream_info.data.stream_info.sample_rate = x;
    used_bits += bits;

    bits = FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN;
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN))
        return false; /* read_callback_ sets the state for us */
    decoder->private_->stream_info.data.stream_info.channels = x+1;
    used_bits += bits;

    bits = FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN;
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN))
        return false; /* read_callback_ sets the state for us */
    decoder->private_->stream_info.data.stream_info.bits_per_sample = x+1;
    used_bits += bits;

    bits = FLAC__STREAM_METADATA_STREAMINFO_TOTAL_SAMPLES_LEN;
    if(!FLAC__bitreader_read_raw_uint64(decoder->private_->input, &decoder->private_->stream_info.data.stream_info.total_samples, FLAC__STREAM_METADATA_STREAMINFO_TOTAL_SAMPLES_LEN))
        return false; /* read_callback_ sets the state for us */
    used_bits += bits;

    if(!FLAC__bitreader_read_byte_block_aligned_no_crc(decoder->private_->input, decoder->private_->stream_info.data.stream_info.md5sum, 16))
        return false; /* read_callback_ sets the state for us */
    used_bits += 16*8;

    /* skip the rest of the block */
    CHOC_ASSERT(used_bits % 8 == 0);
    length -= (used_bits / 8);
    if(!FLAC__bitreader_skip_byte_block_aligned_no_crc(decoder->private_->input, length))
        return false; /* read_callback_ sets the state for us */

    return true;
}

FLAC__bool read_metadata_seektable_(FLAC__StreamDecoder *decoder, FLAC__bool is_last, unsigned length)
{
    FLAC__uint32 i, x;
    FLAC__uint64 xx;

    CHOC_ASSERT(FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input));

    decoder->private_->seek_table.type = FLAC__METADATA_TYPE_SEEKTABLE;
    decoder->private_->seek_table.is_last = is_last;
    decoder->private_->seek_table.length = length;

    decoder->private_->seek_table.data.seek_table.num_points = length / FLAC__STREAM_METADATA_SEEKPOINT_LENGTH;

    /* use realloc since we may pass through here several times (e.g. after seeking) */
    if(0 == (decoder->private_->seek_table.data.seek_table.points = (FLAC__StreamMetadata_SeekPoint*) safe_realloc_mul_2op_(decoder->private_->seek_table.data.seek_table.points, decoder->private_->seek_table.data.seek_table.num_points, /*times*/sizeof(FLAC__StreamMetadata_SeekPoint)))) {
        decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
        return false;
    }
    for(i = 0; i < decoder->private_->seek_table.data.seek_table.num_points; i++) {
        if(!FLAC__bitreader_read_raw_uint64(decoder->private_->input, &xx, FLAC__STREAM_METADATA_SEEKPOINT_SAMPLE_NUMBER_LEN))
            return false; /* read_callback_ sets the state for us */
        decoder->private_->seek_table.data.seek_table.points[i].sample_number = xx;

        if(!FLAC__bitreader_read_raw_uint64(decoder->private_->input, &xx, FLAC__STREAM_METADATA_SEEKPOINT_STREAM_OFFSET_LEN))
            return false; /* read_callback_ sets the state for us */
        decoder->private_->seek_table.data.seek_table.points[i].stream_offset = xx;

        if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_SEEKPOINT_FRAME_SAMPLES_LEN))
            return false; /* read_callback_ sets the state for us */
        decoder->private_->seek_table.data.seek_table.points[i].frame_samples = x;
    }
    length -= (decoder->private_->seek_table.data.seek_table.num_points * FLAC__STREAM_METADATA_SEEKPOINT_LENGTH);
    /* if there is a partial point left, skip over it */
    if(length > 0) {
        /*@@@ do a send_error_to_client_() here?  there's an argument for either way */
        if(!FLAC__bitreader_skip_byte_block_aligned_no_crc(decoder->private_->input, length))
            return false; /* read_callback_ sets the state for us */
    }

    return true;
}

FLAC__bool read_metadata_vorbiscomment_(FLAC__StreamDecoder *decoder, FLAC__StreamMetadata_VorbisComment *obj, unsigned length)
{
    FLAC__uint32 i;

    CHOC_ASSERT(FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input));

    /* read vendor string */
    if (length >= 8) {
        length -= 8; /* vendor string length + num comments entries alone take 8 bytes */
        CHOC_ASSERT(FLAC__STREAM_METADATA_VORBIS_COMMENT_ENTRY_LENGTH_LEN == 32);
        if (!FLAC__bitreader_read_uint32_little_endian(decoder->private_->input, &obj->vendor_string.length))
            return false; /* read_callback_ sets the state for us */
        if (obj->vendor_string.length > 0) {
            if (length < obj->vendor_string.length) {
                obj->vendor_string.length = 0;
                obj->vendor_string.entry = 0;
                goto skip;
            }
            else
                length -= obj->vendor_string.length;
            if (0 == (obj->vendor_string.entry = (FLAC__byte*) safe_malloc_add_2op_(obj->vendor_string.length, /*+*/1))) {
                decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
                return false;
            }
            if (!FLAC__bitreader_read_byte_block_aligned_no_crc(decoder->private_->input, obj->vendor_string.entry, obj->vendor_string.length))
                return false; /* read_callback_ sets the state for us */
            obj->vendor_string.entry[obj->vendor_string.length] = '\0';
        }
        else
            obj->vendor_string.entry = 0;

        /* read num comments */
        CHOC_ASSERT(FLAC__STREAM_METADATA_VORBIS_COMMENT_NUM_COMMENTS_LEN == 32);
        if (!FLAC__bitreader_read_uint32_little_endian(decoder->private_->input, &obj->num_comments))
            return false; /* read_callback_ sets the state for us */

        /* read comments */
        if (obj->num_comments > 0) {
            if (0 == (obj->comments = (FLAC__StreamMetadata_VorbisComment_Entry*) safe_malloc_mul_2op_p(obj->num_comments, /*times*/sizeof(FLAC__StreamMetadata_VorbisComment_Entry)))) {
                decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
                return false;
            }
            for (i = 0; i < obj->num_comments; i++) {
                CHOC_ASSERT(FLAC__STREAM_METADATA_VORBIS_COMMENT_ENTRY_LENGTH_LEN == 32);
                if (length < 4) {
                    obj->num_comments = i;
                    goto skip;
                }
                else
                    length -= 4;
                if (!FLAC__bitreader_read_uint32_little_endian(decoder->private_->input, &obj->comments[i].length))
                    return false; /* read_callback_ sets the state for us */
                if (obj->comments[i].length > 0) {
                    if (length < obj->comments[i].length) {
                        obj->comments[i].length = 0;
                        obj->comments[i].entry = 0;
                        obj->num_comments = i;
                        goto skip;
                    }
                    else
                        length -= obj->comments[i].length;
                    if (0 == (obj->comments[i].entry = (FLAC__byte*) safe_malloc_add_2op_(obj->comments[i].length, /*+*/1))) {
                        decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
                        return false;
                    }
                    if (!FLAC__bitreader_read_byte_block_aligned_no_crc(decoder->private_->input, obj->comments[i].entry, obj->comments[i].length))
                        return false; /* read_callback_ sets the state for us */
                    obj->comments[i].entry[obj->comments[i].length] = '\0';
                }
                else
                    obj->comments[i].entry = 0;
            }
        }
        else
            obj->comments = 0;
    }

  skip:
    if (length > 0) {
        /* This will only happen on files with invalid data in comments */
        if(!FLAC__bitreader_skip_byte_block_aligned_no_crc(decoder->private_->input, length))
            return false; /* read_callback_ sets the state for us */
    }

    return true;
}

FLAC__bool read_metadata_cuesheet_(FLAC__StreamDecoder *decoder, FLAC__StreamMetadata_CueSheet *obj)
{
    FLAC__uint32 i, j, x;

    CHOC_ASSERT(FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input));

    memset(obj, 0, sizeof(FLAC__StreamMetadata_CueSheet));

    CHOC_ASSERT(FLAC__STREAM_METADATA_CUESHEET_MEDIA_CATALOG_NUMBER_LEN % 8 == 0);
    if(!FLAC__bitreader_read_byte_block_aligned_no_crc(decoder->private_->input, (FLAC__byte*)obj->media_catalog_number, FLAC__STREAM_METADATA_CUESHEET_MEDIA_CATALOG_NUMBER_LEN/8))
        return false; /* read_callback_ sets the state for us */

    if(!FLAC__bitreader_read_raw_uint64(decoder->private_->input, &obj->lead_in, FLAC__STREAM_METADATA_CUESHEET_LEAD_IN_LEN))
        return false; /* read_callback_ sets the state for us */

    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_IS_CD_LEN))
        return false; /* read_callback_ sets the state for us */
    obj->is_cd = x? true : false;

    if(!FLAC__bitreader_skip_bits_no_crc(decoder->private_->input, FLAC__STREAM_METADATA_CUESHEET_RESERVED_LEN))
        return false; /* read_callback_ sets the state for us */

    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_NUM_TRACKS_LEN))
        return false; /* read_callback_ sets the state for us */
    obj->num_tracks = x;

    if(obj->num_tracks > 0) {
        if(0 == (obj->tracks = (FLAC__StreamMetadata_CueSheet_Track*) safe_calloc_(obj->num_tracks, sizeof(FLAC__StreamMetadata_CueSheet_Track)))) {
            decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
            return false;
        }
        for(i = 0; i < obj->num_tracks; i++) {
            FLAC__StreamMetadata_CueSheet_Track *track = &obj->tracks[i];
            if(!FLAC__bitreader_read_raw_uint64(decoder->private_->input, &track->offset, FLAC__STREAM_METADATA_CUESHEET_TRACK_OFFSET_LEN))
                return false; /* read_callback_ sets the state for us */

            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_TRACK_NUMBER_LEN))
                return false; /* read_callback_ sets the state for us */
            track->number = (FLAC__byte)x;

            CHOC_ASSERT(FLAC__STREAM_METADATA_CUESHEET_TRACK_ISRC_LEN % 8 == 0);
            if(!FLAC__bitreader_read_byte_block_aligned_no_crc(decoder->private_->input, (FLAC__byte*)track->isrc, FLAC__STREAM_METADATA_CUESHEET_TRACK_ISRC_LEN/8))
                return false; /* read_callback_ sets the state for us */

            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_TRACK_TYPE_LEN))
                return false; /* read_callback_ sets the state for us */
            track->type = x;

            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_TRACK_PRE_EMPHASIS_LEN))
                return false; /* read_callback_ sets the state for us */
            track->pre_emphasis = x;

            if(!FLAC__bitreader_skip_bits_no_crc(decoder->private_->input, FLAC__STREAM_METADATA_CUESHEET_TRACK_RESERVED_LEN))
                return false; /* read_callback_ sets the state for us */

            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_TRACK_NUM_INDICES_LEN))
                return false; /* read_callback_ sets the state for us */
            track->num_indices = (FLAC__byte)x;

            if(track->num_indices > 0) {
                if(0 == (track->indices = (FLAC__StreamMetadata_CueSheet_Index*) safe_calloc_(track->num_indices, sizeof(FLAC__StreamMetadata_CueSheet_Index)))) {
                    decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
                    return false;
                }
                for(j = 0; j < track->num_indices; j++) {
                    FLAC__StreamMetadata_CueSheet_Index *indx = &track->indices[j];
                    if(!FLAC__bitreader_read_raw_uint64(decoder->private_->input, &indx->offset, FLAC__STREAM_METADATA_CUESHEET_INDEX_OFFSET_LEN))
                        return false; /* read_callback_ sets the state for us */

                    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_CUESHEET_INDEX_NUMBER_LEN))
                        return false; /* read_callback_ sets the state for us */
                    indx->number = (FLAC__byte)x;

                    if(!FLAC__bitreader_skip_bits_no_crc(decoder->private_->input, FLAC__STREAM_METADATA_CUESHEET_INDEX_RESERVED_LEN))
                        return false; /* read_callback_ sets the state for us */
                }
            }
        }
    }

    return true;
}

FLAC__bool read_metadata_picture_(FLAC__StreamDecoder *decoder, FLAC__StreamMetadata_Picture *obj)
{
    FLAC__uint32 x;

    CHOC_ASSERT(FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input));

    /* read type */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_PICTURE_TYPE_LEN))
        return false; /* read_callback_ sets the state for us */
    obj->type = (FLAC__StreamMetadata_Picture_Type) x;

    /* read MIME type */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_PICTURE_MIME_TYPE_LENGTH_LEN))
        return false; /* read_callback_ sets the state for us */
    if(0 == (obj->mime_type = (char*) safe_malloc_add_2op_(x, /*+*/1))) {
        decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
        return false;
    }
    if(x > 0) {
        if(!FLAC__bitreader_read_byte_block_aligned_no_crc(decoder->private_->input, (FLAC__byte*)obj->mime_type, x))
            return false; /* read_callback_ sets the state for us */
    }
    obj->mime_type[x] = '\0';

    /* read description */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__STREAM_METADATA_PICTURE_DESCRIPTION_LENGTH_LEN))
        return false; /* read_callback_ sets the state for us */
    if(0 == (obj->description = (FLAC__byte*) safe_malloc_add_2op_(x, /*+*/1))) {
        decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
        return false;
    }
    if(x > 0) {
        if(!FLAC__bitreader_read_byte_block_aligned_no_crc(decoder->private_->input, obj->description, x))
            return false; /* read_callback_ sets the state for us */
    }
    obj->description[x] = '\0';

    /* read width */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &obj->width, FLAC__STREAM_METADATA_PICTURE_WIDTH_LEN))
        return false; /* read_callback_ sets the state for us */

    /* read height */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &obj->height, FLAC__STREAM_METADATA_PICTURE_HEIGHT_LEN))
        return false; /* read_callback_ sets the state for us */

    /* read depth */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &obj->depth, FLAC__STREAM_METADATA_PICTURE_DEPTH_LEN))
        return false; /* read_callback_ sets the state for us */

    /* read colors */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &obj->colors, FLAC__STREAM_METADATA_PICTURE_COLORS_LEN))
        return false; /* read_callback_ sets the state for us */

    /* read data */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &(obj->data_length), FLAC__STREAM_METADATA_PICTURE_DATA_LENGTH_LEN))
        return false; /* read_callback_ sets the state for us */
    if(0 == (obj->data = (FLAC__byte*) safe_malloc_(obj->data_length))) {
        decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
        return false;
    }
    if(obj->data_length > 0) {
        if(!FLAC__bitreader_read_byte_block_aligned_no_crc(decoder->private_->input, obj->data, obj->data_length))
            return false; /* read_callback_ sets the state for us */
    }

    return true;
}

FLAC__bool skip_id3v2_tag_(FLAC__StreamDecoder *decoder)
{
    FLAC__uint32 x;
    unsigned i, skip;

    /* skip the version and flags bytes */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, 24))
        return false; /* read_callback_ sets the state for us */
    /* get the size (in bytes) to skip */
    skip = 0;
    for(i = 0; i < 4; i++) {
        if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, 8))
            return false; /* read_callback_ sets the state for us */
        skip <<= 7;
        skip |= (x & 0x7f);
    }
    /* skip the rest of the tag */
    if(!FLAC__bitreader_skip_byte_block_aligned_no_crc(decoder->private_->input, skip))
        return false; /* read_callback_ sets the state for us */
    return true;
}

FLAC__bool frame_sync_(FLAC__StreamDecoder *decoder)
{
    FLAC__uint32 x;
    FLAC__bool first = true;

    /* If we know the total number of samples in the stream, stop if we've read that many. */
    /* This will stop us, for example, from wasting time trying to sync on an ID3V1 tag. */
    if(FLAC__stream_decoder_get_total_samples(decoder) > 0) {
        if(decoder->private_->samples_decoded >= FLAC__stream_decoder_get_total_samples(decoder)) {
            decoder->protected_->state = FLAC__STREAM_DECODER_END_OF_STREAM;
            return true;
        }
    }

    /* make sure we're byte aligned */
    if(!FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input)) {
        if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__bitreader_bits_left_for_byte_alignment(decoder->private_->input)))
            return false; /* read_callback_ sets the state for us */
    }

    while(1) {
        if(decoder->private_->cached) {
            x = (FLAC__uint32)decoder->private_->lookahead;
            decoder->private_->cached = false;
        }
        else {
            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, 8))
                return false; /* read_callback_ sets the state for us */
        }
        if(x == 0xff) { /* MAGIC NUMBER for the first 8 frame sync bits */
            decoder->private_->header_warmup[0] = (FLAC__byte)x;
            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, 8))
                return false; /* read_callback_ sets the state for us */

            /* we have to check if we just read two 0xff's in a row; the second may actually be the beginning of the sync code */
            /* else we have to check if the second byte is the end of a sync code */
            if(x == 0xff) { /* MAGIC NUMBER for the first 8 frame sync bits */
                decoder->private_->lookahead = (FLAC__byte)x;
                decoder->private_->cached = true;
            }
            else if(x >> 1 == 0x7c) { /* MAGIC NUMBER for the last 6 sync bits and reserved 7th bit */
                decoder->private_->header_warmup[1] = (FLAC__byte)x;
                decoder->protected_->state = FLAC__STREAM_DECODER_READ_FRAME;
                return true;
            }
        }
        if(first) {
            send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC);
            first = false;
        }
    }

    return true;
}

FLAC__bool read_frame_(FLAC__StreamDecoder *decoder, FLAC__bool *got_a_frame, FLAC__bool do_full_decode)
{
    unsigned channel;
    unsigned i;
    FLAC__int32 mid, side;
    unsigned frame_crc; /* the one we calculate from the input stream */
    FLAC__uint32 x;

    *got_a_frame = false;

    /* init the CRC */
    frame_crc = 0;
    frame_crc = FLAC__CRC16_UPDATE(decoder->private_->header_warmup[0], frame_crc);
    frame_crc = FLAC__CRC16_UPDATE(decoder->private_->header_warmup[1], frame_crc);
    FLAC__bitreader_reset_read_crc16(decoder->private_->input, (FLAC__uint16)frame_crc);

    if(!read_frame_header_(decoder))
        return false;
    if(decoder->protected_->state == FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC) /* means we didn't sync on a valid header */
        return true;
    if(!allocate_output_(decoder, decoder->private_->frame.header.blocksize, decoder->private_->frame.header.channels))
        return false;
    for(channel = 0; channel < decoder->private_->frame.header.channels; channel++) {
        /*
         * first figure the correct bits-per-sample of the subframe
         */
        unsigned bps = decoder->private_->frame.header.bits_per_sample;
        switch(decoder->private_->frame.header.channel_assignment) {
            case FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT:
                /* no adjustment needed */
                break;
            case FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE:
                CHOC_ASSERT(decoder->private_->frame.header.channels == 2);
                if(channel == 1)
                    bps++;
                break;
            case FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE:
                CHOC_ASSERT(decoder->private_->frame.header.channels == 2);
                if(channel == 0)
                    bps++;
                break;
            case FLAC__CHANNEL_ASSIGNMENT_MID_SIDE:
                CHOC_ASSERT(decoder->private_->frame.header.channels == 2);
                if(channel == 1)
                    bps++;
                break;
            default:
                CHOC_ASSERT(0);
        }
        /*
         * now read it
         */
        if(!read_subframe_(decoder, channel, bps, do_full_decode))
            return false;
        if(decoder->protected_->state == FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC) /* means bad sync or got corruption */
            return true;
    }
    if(!read_zero_padding_(decoder))
        return false;
    if(decoder->protected_->state == FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC) /* means bad sync or got corruption (i.e. "zero bits" were not all zeroes) */
        return true;

    /*
     * Read the frame CRC-16 from the footer and check
     */
    frame_crc = FLAC__bitreader_get_read_crc16(decoder->private_->input);
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, FLAC__FRAME_FOOTER_CRC_LEN))
        return false; /* read_callback_ sets the state for us */
    if(frame_crc == x) {
        if(do_full_decode) {
            /* Undo any special channel coding */
            switch(decoder->private_->frame.header.channel_assignment) {
                case FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT:
                    /* do nothing */
                    break;
                case FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE:
                    CHOC_ASSERT(decoder->private_->frame.header.channels == 2);
                    for(i = 0; i < decoder->private_->frame.header.blocksize; i++)
                        decoder->private_->output[1][i] = decoder->private_->output[0][i] - decoder->private_->output[1][i];
                    break;
                case FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE:
                    CHOC_ASSERT(decoder->private_->frame.header.channels == 2);
                    for(i = 0; i < decoder->private_->frame.header.blocksize; i++)
                        decoder->private_->output[0][i] += decoder->private_->output[1][i];
                    break;
                case FLAC__CHANNEL_ASSIGNMENT_MID_SIDE:
                    CHOC_ASSERT(decoder->private_->frame.header.channels == 2);
                    for(i = 0; i < decoder->private_->frame.header.blocksize; i++) {
                        mid = decoder->private_->output[0][i];
                        side = decoder->private_->output[1][i];
                        mid <<= 1;
                        mid |= (side & 1); /* i.e. if 'side' is odd... */
                        decoder->private_->output[0][i] = (mid + side) >> 1;
                        decoder->private_->output[1][i] = (mid - side) >> 1;
                    }
                    break;
                default:
                    CHOC_ASSERT(0);
                    break;
            }
        }
    }
    else {
        /* Bad frame, emit error and zero the output signal */
        send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_FRAME_CRC_MISMATCH);
        if(do_full_decode) {
            for(channel = 0; channel < decoder->private_->frame.header.channels; channel++) {
                memset(decoder->private_->output[channel], 0, sizeof(FLAC__int32) * decoder->private_->frame.header.blocksize);
            }
        }
    }

    *got_a_frame = true;

    /* we wait to update fixed_block_size until here, when we're sure we've got a proper frame and hence a correct blocksize */
    if(decoder->private_->next_fixed_block_size)
        decoder->private_->fixed_block_size = decoder->private_->next_fixed_block_size;

    /* put the latest values into the public section of the decoder instance */
    decoder->protected_->channels = decoder->private_->frame.header.channels;
    decoder->protected_->channel_assignment = decoder->private_->frame.header.channel_assignment;
    decoder->protected_->bits_per_sample = decoder->private_->frame.header.bits_per_sample;
    decoder->protected_->sample_rate = decoder->private_->frame.header.sample_rate;
    decoder->protected_->blocksize = decoder->private_->frame.header.blocksize;

    CHOC_ASSERT(decoder->private_->frame.header.number_type == FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER);
    decoder->private_->samples_decoded = decoder->private_->frame.header.number.sample_number + decoder->private_->frame.header.blocksize;

    /* write it */
    if(do_full_decode) {
        if(write_audio_frame_to_client_(decoder, &decoder->private_->frame, (const FLAC__int32 * const *)decoder->private_->output) != FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE)
            return false;
    }

    decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
    return true;
}

FLAC__bool read_frame_header_(FLAC__StreamDecoder *decoder)
{
    FLAC__uint32 x;
    FLAC__uint64 xx;
    unsigned i, blocksize_hint = 0, sample_rate_hint = 0;
    FLAC__byte crc8, raw_header[16]; /* MAGIC NUMBER based on the maximum frame header size, including CRC */
    unsigned raw_header_len;
    FLAC__bool is_unparseable = false;

    CHOC_ASSERT(FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input));

    /* init the raw header with the saved bits from synchronization */
    raw_header[0] = decoder->private_->header_warmup[0];
    raw_header[1] = decoder->private_->header_warmup[1];
    raw_header_len = 2;

    /* check to make sure that reserved bit is 0 */
    if(raw_header[1] & 0x02) /* MAGIC NUMBER */
        is_unparseable = true;

    /*
     * Note that along the way as we read the header, we look for a sync
     * code inside.  If we find one it would indicate that our original
     * sync was bad since there cannot be a sync code in a valid header.
     *
     * Three kinds of things can go wrong when reading the frame header:
     *  1) We may have sync'ed incorrectly and not landed on a frame header.
     *     If we don't find a sync code, it can end up looking like we read
     *     a valid but unparseable header, until getting to the frame header
     *     CRC.  Even then we could get a false positive on the CRC.
     *  2) We may have sync'ed correctly but on an unparseable frame (from a
     *     future encoder).
     *  3) We may be on a damaged frame which appears valid but unparseable.
     *
     * For all these reasons, we try and read a complete frame header as
     * long as it seems valid, even if unparseable, up until the frame
     * header CRC.
     */

    /*
     * read in the raw header as bytes so we can CRC it, and parse it on the way
     */
    for(i = 0; i < 2; i++) {
        if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, 8))
            return false; /* read_callback_ sets the state for us */
        if(x == 0xff) { /* MAGIC NUMBER for the first 8 frame sync bits */
            /* if we get here it means our original sync was erroneous since the sync code cannot appear in the header */
            decoder->private_->lookahead = (FLAC__byte)x;
            decoder->private_->cached = true;
            send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER);
            decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
            return true;
        }
        raw_header[raw_header_len++] = (FLAC__byte)x;
    }

    switch(x = raw_header[2] >> 4) {
        case 0:
            is_unparseable = true;
            break;
        case 1:
            decoder->private_->frame.header.blocksize = 192;
            break;
        case 2:
        case 3:
        case 4:
        case 5:
            decoder->private_->frame.header.blocksize = 576 << (x-2);
            break;
        case 6:
        case 7:
            blocksize_hint = x;
            break;
        case 8:
        case 9:
        case 10:
        case 11:
        case 12:
        case 13:
        case 14:
        case 15:
            decoder->private_->frame.header.blocksize = 256 << (x-8);
            break;
        default:
            CHOC_ASSERT(0);
            break;
    }

    switch(x = raw_header[2] & 0x0f) {
        case 0:
            if(decoder->private_->has_stream_info)
                decoder->private_->frame.header.sample_rate = decoder->private_->stream_info.data.stream_info.sample_rate;
            else
                is_unparseable = true;
            break;
        case 1:  decoder->private_->frame.header.sample_rate = 88200; break;
        case 2:  decoder->private_->frame.header.sample_rate = 176400; break;
        case 3:  decoder->private_->frame.header.sample_rate = 192000; break;
        case 4:  decoder->private_->frame.header.sample_rate = 8000; break;
        case 5:  decoder->private_->frame.header.sample_rate = 16000; break;
        case 6:  decoder->private_->frame.header.sample_rate = 22050; break;
        case 7:  decoder->private_->frame.header.sample_rate = 24000; break;
        case 8:  decoder->private_->frame.header.sample_rate = 32000; break;
        case 9:  decoder->private_->frame.header.sample_rate = 44100; break;
        case 10: decoder->private_->frame.header.sample_rate = 48000; break;
        case 11: decoder->private_->frame.header.sample_rate = 96000; break;
        case 12:
        case 13:
        case 14:
            sample_rate_hint = x;
            break;
        case 15:
            send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER);
            decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
            return true;
        default:
            CHOC_ASSERT(0);
    }

    x = (unsigned)(raw_header[3] >> 4);
    if(x & 8) {
        decoder->private_->frame.header.channels = 2;
        switch(x & 7) {
            case 0:
                decoder->private_->frame.header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE;
                break;
            case 1:
                decoder->private_->frame.header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE;
                break;
            case 2:
                decoder->private_->frame.header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_MID_SIDE;
                break;
            default:
                is_unparseable = true;
                break;
        }
    }
    else {
        decoder->private_->frame.header.channels = (unsigned)x + 1;
        decoder->private_->frame.header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT;
    }

    switch(x = (unsigned)(raw_header[3] & 0x0e) >> 1) {
        case 0:
            if(decoder->private_->has_stream_info)
                decoder->private_->frame.header.bits_per_sample = decoder->private_->stream_info.data.stream_info.bits_per_sample;
            else
                is_unparseable = true;
            break;
        case 1:
            decoder->private_->frame.header.bits_per_sample = 8;
            break;
        case 2:
            decoder->private_->frame.header.bits_per_sample = 12;
            break;
        case 4:
            decoder->private_->frame.header.bits_per_sample = 16;
            break;
        case 5:
            decoder->private_->frame.header.bits_per_sample = 20;
            break;
        case 6:
            decoder->private_->frame.header.bits_per_sample = 24;
            break;
        case 3:
        case 7:
            is_unparseable = true;
            break;
        default:
            CHOC_ASSERT(0);
            break;
    }

    /* check to make sure that reserved bit is 0 */
    if(raw_header[3] & 0x01) /* MAGIC NUMBER */
        is_unparseable = true;

    /* read the frame's starting sample number (or frame number as the case may be) */
    if(
        raw_header[1] & 0x01 ||
        /*@@@ this clause is a concession to the old way of doing variable blocksize; the only known implementation is flake and can probably be removed without inconveniencing anyone */
        (decoder->private_->has_stream_info && decoder->private_->stream_info.data.stream_info.min_blocksize != decoder->private_->stream_info.data.stream_info.max_blocksize)
    ) { /* variable blocksize */
        if(!FLAC__bitreader_read_utf8_uint64(decoder->private_->input, &xx, raw_header, &raw_header_len))
            return false; /* read_callback_ sets the state for us */
        if(xx == FLAC__U64L(0xffffffffffffffff)) { /* i.e. non-UTF8 code... */
            decoder->private_->lookahead = raw_header[raw_header_len-1]; /* back up as much as we can */
            decoder->private_->cached = true;
            send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER);
            decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
            return true;
        }
        decoder->private_->frame.header.number_type = FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER;
        decoder->private_->frame.header.number.sample_number = xx;
    }
    else { /* fixed blocksize */
        if(!FLAC__bitreader_read_utf8_uint32(decoder->private_->input, &x, raw_header, &raw_header_len))
            return false; /* read_callback_ sets the state for us */
        if(x == 0xffffffff) { /* i.e. non-UTF8 code... */
            decoder->private_->lookahead = raw_header[raw_header_len-1]; /* back up as much as we can */
            decoder->private_->cached = true;
            send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER);
            decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
            return true;
        }
        decoder->private_->frame.header.number_type = FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER;
        decoder->private_->frame.header.number.frame_number = x;
    }

    if(blocksize_hint) {
        if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, 8))
            return false; /* read_callback_ sets the state for us */
        raw_header[raw_header_len++] = (FLAC__byte)x;
        if(blocksize_hint == 7) {
            FLAC__uint32 _x;
            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &_x, 8))
                return false; /* read_callback_ sets the state for us */
            raw_header[raw_header_len++] = (FLAC__byte)_x;
            x = (x << 8) | _x;
        }
        decoder->private_->frame.header.blocksize = x+1;
    }

    if(sample_rate_hint) {
        if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, 8))
            return false; /* read_callback_ sets the state for us */
        raw_header[raw_header_len++] = (FLAC__byte)x;
        if(sample_rate_hint != 12) {
            FLAC__uint32 _x;
            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &_x, 8))
                return false; /* read_callback_ sets the state for us */
            raw_header[raw_header_len++] = (FLAC__byte)_x;
            x = (x << 8) | _x;
        }
        if(sample_rate_hint == 12)
            decoder->private_->frame.header.sample_rate = x*1000;
        else if(sample_rate_hint == 13)
            decoder->private_->frame.header.sample_rate = x;
        else
            decoder->private_->frame.header.sample_rate = x*10;
    }

    /* read the CRC-8 byte */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, 8))
        return false; /* read_callback_ sets the state for us */
    crc8 = (FLAC__byte)x;

    if(FLAC__crc8(raw_header, raw_header_len) != crc8) {
        send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_BAD_HEADER);
        decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
        return true;
    }

    /* calculate the sample number from the frame number if needed */
    decoder->private_->next_fixed_block_size = 0;
    if(decoder->private_->frame.header.number_type == FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER) {
        x = decoder->private_->frame.header.number.frame_number;
        decoder->private_->frame.header.number_type = FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER;
        if(decoder->private_->fixed_block_size)
            decoder->private_->frame.header.number.sample_number = (FLAC__uint64)decoder->private_->fixed_block_size * (FLAC__uint64)x;
        else if(decoder->private_->has_stream_info) {
            if(decoder->private_->stream_info.data.stream_info.min_blocksize == decoder->private_->stream_info.data.stream_info.max_blocksize) {
                decoder->private_->frame.header.number.sample_number = (FLAC__uint64)decoder->private_->stream_info.data.stream_info.min_blocksize * (FLAC__uint64)x;
                decoder->private_->next_fixed_block_size = decoder->private_->stream_info.data.stream_info.max_blocksize;
            }
            else
                is_unparseable = true;
        }
        else if(x == 0) {
            decoder->private_->frame.header.number.sample_number = 0;
            decoder->private_->next_fixed_block_size = decoder->private_->frame.header.blocksize;
        }
        else {
            /* can only get here if the stream has invalid frame numbering and no STREAMINFO, so assume it's not the last (possibly short) frame */
            decoder->private_->frame.header.number.sample_number = (FLAC__uint64)decoder->private_->frame.header.blocksize * (FLAC__uint64)x;
        }
    }

    if(is_unparseable) {
        send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM);
        decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
        return true;
    }

    return true;
}

FLAC__bool read_subframe_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, FLAC__bool do_full_decode)
{
    FLAC__uint32 x;
    FLAC__bool wasted_bits;
    unsigned i;

    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &x, 8)) /* MAGIC NUMBER */
        return false; /* read_callback_ sets the state for us */

    wasted_bits = (x & 1);
    x &= 0xfe;

    if(wasted_bits) {
        unsigned u;
        if(!FLAC__bitreader_read_unary_unsigned(decoder->private_->input, &u))
            return false; /* read_callback_ sets the state for us */
        decoder->private_->frame.subframes[channel].wasted_bits = u+1;
        bps -= decoder->private_->frame.subframes[channel].wasted_bits;
    }
    else
        decoder->private_->frame.subframes[channel].wasted_bits = 0;

    /*
     * Lots of magic numbers here
     */
    if(x & 0x80) {
        send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC);
        decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
        return true;
    }
    else if(x == 0) {
        if(!read_subframe_constant_(decoder, channel, bps, do_full_decode))
            return false;
    }
    else if(x == 2) {
        if(!read_subframe_verbatim_(decoder, channel, bps, do_full_decode))
            return false;
    }
    else if(x < 16) {
        send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM);
        decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
        return true;
    }
    else if(x <= 24) {
        if(!read_subframe_fixed_(decoder, channel, bps, (x>>1)&7, do_full_decode))
            return false;
        if(decoder->protected_->state == FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC) /* means bad sync or got corruption */
            return true;
    }
    else if(x < 64) {
        send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM);
        decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
        return true;
    }
    else {
        if(!read_subframe_lpc_(decoder, channel, bps, ((x>>1)&31)+1, do_full_decode))
            return false;
        if(decoder->protected_->state == FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC) /* means bad sync or got corruption */
            return true;
    }

    if(wasted_bits && do_full_decode) {
        x = decoder->private_->frame.subframes[channel].wasted_bits;
        for(i = 0; i < decoder->private_->frame.header.blocksize; i++)
            decoder->private_->output[channel][i] = signedLeftShift (decoder->private_->output[channel][i], x);
    }

    return true;
}

FLAC__bool read_subframe_constant_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, FLAC__bool do_full_decode)
{
    FLAC__Subframe_Constant *subframe = &decoder->private_->frame.subframes[channel].data.constant;
    FLAC__int32 x;
    unsigned i;
    FLAC__int32 *output = decoder->private_->output[channel];

    decoder->private_->frame.subframes[channel].type = FLAC__SUBFRAME_TYPE_CONSTANT;

    if(!FLAC__bitreader_read_raw_int32(decoder->private_->input, &x, bps))
        return false; /* read_callback_ sets the state for us */

    subframe->value = x;

    /* decode the subframe */
    if(do_full_decode) {
        for(i = 0; i < decoder->private_->frame.header.blocksize; i++)
            output[i] = x;
    }

    return true;
}

FLAC__bool read_subframe_fixed_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, const unsigned order, FLAC__bool do_full_decode)
{
    FLAC__Subframe_Fixed *subframe = &decoder->private_->frame.subframes[channel].data.fixed;
    FLAC__int32 i32;
    FLAC__uint32 u32;
    unsigned u;

    decoder->private_->frame.subframes[channel].type = FLAC__SUBFRAME_TYPE_FIXED;

    subframe->residual = decoder->private_->residual[channel];
    subframe->order = order;

    /* read warm-up samples */
    for(u = 0; u < order; u++) {
        if(!FLAC__bitreader_read_raw_int32(decoder->private_->input, &i32, bps))
            return false; /* read_callback_ sets the state for us */
        subframe->warmup[u] = i32;
    }

    /* read entropy coding method info */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &u32, FLAC__ENTROPY_CODING_METHOD_TYPE_LEN))
        return false; /* read_callback_ sets the state for us */
    subframe->entropy_coding_method.type = (FLAC__EntropyCodingMethodType)u32;
    switch(subframe->entropy_coding_method.type) {
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE:
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2:
            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &u32, FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ORDER_LEN))
                return false; /* read_callback_ sets the state for us */
            subframe->entropy_coding_method.data.partitioned_rice.order = u32;
            subframe->entropy_coding_method.data.partitioned_rice.contents = &decoder->private_->partitioned_rice_contents[channel];
            break;
        default:
            send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM);
            decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
            return true;
    }

    /* read residual */
    switch(subframe->entropy_coding_method.type) {
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE:
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2:
            if(!read_residual_partitioned_rice_(decoder, order, subframe->entropy_coding_method.data.partitioned_rice.order, &decoder->private_->partitioned_rice_contents[channel], decoder->private_->residual[channel], /*is_extended=*/subframe->entropy_coding_method.type == FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2))
                return false;
            break;
        default:
            CHOC_ASSERT(0);
    }

    /* decode the subframe */
    if(do_full_decode) {
        memcpy(decoder->private_->output[channel], subframe->warmup, sizeof(FLAC__int32) * order);
        FLAC__fixed_restore_signal(decoder->private_->residual[channel], decoder->private_->frame.header.blocksize-order, order, decoder->private_->output[channel]+order);
    }

    return true;
}

FLAC__bool read_subframe_lpc_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, const unsigned order, FLAC__bool do_full_decode)
{
    FLAC__Subframe_LPC *subframe = &decoder->private_->frame.subframes[channel].data.lpc;
    FLAC__int32 i32;
    FLAC__uint32 u32;
    unsigned u;

    decoder->private_->frame.subframes[channel].type = FLAC__SUBFRAME_TYPE_LPC;

    subframe->residual = decoder->private_->residual[channel];
    subframe->order = order;

    /* read warm-up samples */
    for(u = 0; u < order; u++) {
        if(!FLAC__bitreader_read_raw_int32(decoder->private_->input, &i32, bps))
            return false; /* read_callback_ sets the state for us */
        subframe->warmup[u] = i32;
    }

    /* read qlp coeff precision */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &u32, FLAC__SUBFRAME_LPC_QLP_COEFF_PRECISION_LEN))
        return false; /* read_callback_ sets the state for us */
    if(u32 == (1u << FLAC__SUBFRAME_LPC_QLP_COEFF_PRECISION_LEN) - 1) {
        send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC);
        decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
        return true;
    }
    subframe->qlp_coeff_precision = u32+1;

    /* read qlp shift */
    if(!FLAC__bitreader_read_raw_int32(decoder->private_->input, &i32, FLAC__SUBFRAME_LPC_QLP_SHIFT_LEN))
        return false; /* read_callback_ sets the state for us */
    subframe->quantization_level = i32;

    /* read quantized lp coefficiencts */
    for(u = 0; u < order; u++) {
        if(!FLAC__bitreader_read_raw_int32(decoder->private_->input, &i32, subframe->qlp_coeff_precision))
            return false; /* read_callback_ sets the state for us */
        subframe->qlp_coeff[u] = i32;
    }

    /* read entropy coding method info */
    if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &u32, FLAC__ENTROPY_CODING_METHOD_TYPE_LEN))
        return false; /* read_callback_ sets the state for us */
    subframe->entropy_coding_method.type = (FLAC__EntropyCodingMethodType)u32;
    switch(subframe->entropy_coding_method.type) {
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE:
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2:
            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &u32, FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ORDER_LEN))
                return false; /* read_callback_ sets the state for us */
            subframe->entropy_coding_method.data.partitioned_rice.order = u32;
            subframe->entropy_coding_method.data.partitioned_rice.contents = &decoder->private_->partitioned_rice_contents[channel];
            break;
        default:
            send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM);
            decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
            return true;
    }

    /* read residual */
    switch(subframe->entropy_coding_method.type) {
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE:
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2:
            if(!read_residual_partitioned_rice_(decoder, order, subframe->entropy_coding_method.data.partitioned_rice.order, &decoder->private_->partitioned_rice_contents[channel], decoder->private_->residual[channel], /*is_extended=*/subframe->entropy_coding_method.type == FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2))
                return false;
            break;
        default:
            CHOC_ASSERT(0);
    }

    /* decode the subframe */
    if(do_full_decode) {
        memcpy(decoder->private_->output[channel], subframe->warmup, sizeof(FLAC__int32) * order);
        /*@@@@@@ technically not pessimistic enough, should be more like
        if( (FLAC__uint64)order * ((((FLAC__uint64)1)<<bps)-1) * ((1<<subframe->qlp_coeff_precision)-1) < (((FLAC__uint64)-1) << 32) )
        */
        if(bps + subframe->qlp_coeff_precision + FLAC__bitmath_ilog2(order) <= 32)
            if(bps <= 16 && subframe->qlp_coeff_precision <= 16)
                decoder->private_->local_lpc_restore_signal_16bit(decoder->private_->residual[channel], decoder->private_->frame.header.blocksize-order, subframe->qlp_coeff, order, subframe->quantization_level, decoder->private_->output[channel]+order);
            else
                decoder->private_->local_lpc_restore_signal(decoder->private_->residual[channel], decoder->private_->frame.header.blocksize-order, subframe->qlp_coeff, order, subframe->quantization_level, decoder->private_->output[channel]+order);
        else
            decoder->private_->local_lpc_restore_signal_64bit(decoder->private_->residual[channel], decoder->private_->frame.header.blocksize-order, subframe->qlp_coeff, order, subframe->quantization_level, decoder->private_->output[channel]+order);
    }

    return true;
}

FLAC__bool read_subframe_verbatim_(FLAC__StreamDecoder *decoder, unsigned channel, unsigned bps, FLAC__bool do_full_decode)
{
    FLAC__Subframe_Verbatim *subframe = &decoder->private_->frame.subframes[channel].data.verbatim;
    FLAC__int32 x, *residual = decoder->private_->residual[channel];
    unsigned i;

    decoder->private_->frame.subframes[channel].type = FLAC__SUBFRAME_TYPE_VERBATIM;

    subframe->data = residual;

    for(i = 0; i < decoder->private_->frame.header.blocksize; i++) {
        if(!FLAC__bitreader_read_raw_int32(decoder->private_->input, &x, bps))
            return false; /* read_callback_ sets the state for us */
        residual[i] = x;
    }

    /* decode the subframe */
    if(do_full_decode)
        memcpy(decoder->private_->output[channel], subframe->data, sizeof(FLAC__int32) * decoder->private_->frame.header.blocksize);

    return true;
}

FLAC__bool read_residual_partitioned_rice_(FLAC__StreamDecoder *decoder, unsigned predictor_order, unsigned partition_order, FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents, FLAC__int32 *residual, FLAC__bool is_extended)
{
    FLAC__uint32 rice_parameter;
    int i;
    unsigned partition, sample, u;
    const unsigned partitions = 1u << partition_order;
    const unsigned partition_samples = partition_order > 0? decoder->private_->frame.header.blocksize >> partition_order : decoder->private_->frame.header.blocksize - predictor_order;
    const unsigned plen = is_extended? FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_PARAMETER_LEN : FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN;
    const unsigned pesc = is_extended? FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_ESCAPE_PARAMETER : FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ESCAPE_PARAMETER;

    /* sanity checks */
    if(partition_order == 0) {
        if(decoder->private_->frame.header.blocksize < predictor_order) {
            send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC);
            decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
            /* We have received a potentially malicious bit stream. All we can do is error out to avoid a heap overflow. */
            return false;
        }
    }
    else {
        if(partition_samples < predictor_order) {
            send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC);
            decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
            /* We have received a potentially malicious bit stream. All we can do is error out to avoid a heap overflow. */
            return false;
        }
    }

    if(!FLAC__format_entropy_coding_method_partitioned_rice_contents_ensure_size(partitioned_rice_contents, std::max(6u, partition_order))) {
        decoder->protected_->state = FLAC__STREAM_DECODER_MEMORY_ALLOCATION_ERROR;
        return false;
    }

    sample = 0;
    for(partition = 0; partition < partitions; partition++) {
        if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &rice_parameter, plen))
            return false; /* read_callback_ sets the state for us */
        partitioned_rice_contents->parameters[partition] = rice_parameter;
        if(rice_parameter < pesc) {
            partitioned_rice_contents->raw_bits[partition] = 0;
            u = (partition_order == 0 || partition > 0)? partition_samples : partition_samples - predictor_order;
            if(!FLAC__bitreader_read_rice_signed_block(decoder->private_->input, residual + sample, u, rice_parameter))
                return false; /* read_callback_ sets the state for us */
            sample += u;
        }
        else {
            if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &rice_parameter, FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_RAW_LEN))
                return false; /* read_callback_ sets the state for us */
            partitioned_rice_contents->raw_bits[partition] = rice_parameter;
            for(u = (partition_order == 0 || partition > 0)? 0 : predictor_order; u < partition_samples; u++, sample++) {
                if(!FLAC__bitreader_read_raw_int32(decoder->private_->input, &i, rice_parameter))
                    return false; /* read_callback_ sets the state for us */
                residual[sample] = i;
            }
        }
    }

    return true;
}

FLAC__bool read_zero_padding_(FLAC__StreamDecoder *decoder)
{
    if(!FLAC__bitreader_is_consumed_byte_aligned(decoder->private_->input)) {
        FLAC__uint32 zero = 0;
        if(!FLAC__bitreader_read_raw_uint32(decoder->private_->input, &zero, FLAC__bitreader_bits_left_for_byte_alignment(decoder->private_->input)))
            return false; /* read_callback_ sets the state for us */
        if(zero != 0) {
            send_error_to_client_(decoder, FLAC__STREAM_DECODER_ERROR_STATUS_LOST_SYNC);
            decoder->protected_->state = FLAC__STREAM_DECODER_SEARCH_FOR_FRAME_SYNC;
        }
    }
    return true;
}

FLAC__bool read_callback_(FLAC__byte buffer[], size_t *bytes, void *client_data)
{
    FLAC__StreamDecoder *decoder = (FLAC__StreamDecoder *)client_data;

    if(
#if FLAC__HAS_OGG
        /* see [1] HACK NOTE below for why we don't call the eof_callback when decoding Ogg FLAC */
        !decoder->private_->is_ogg &&
#endif
        decoder->private_->eof_callback && decoder->private_->eof_callback(decoder, decoder->private_->client_data)
    ) {
        *bytes = 0;
        decoder->protected_->state = FLAC__STREAM_DECODER_END_OF_STREAM;
        return false;
    }
    else if(*bytes > 0) {
        /* While seeking, it is possible for our seek to land in the
         * middle of audio data that looks exactly like a frame header
         * from a future version of an encoder.  When that happens, our
         * error callback will get an
         * FLAC__STREAM_DECODER_UNPARSEABLE_STREAM and increment its
         * unparseable_frame_count.  But there is a remote possibility
         * that it is properly synced at such a "future-codec frame",
         * so to make sure, we wait to see many "unparseable" errors in
         * a row before bailing out.
         */
        if(decoder->private_->is_seeking && decoder->private_->unparseable_frame_count > 20) {
            decoder->protected_->state = FLAC__STREAM_DECODER_ABORTED;
            return false;
        }
        else {
            const FLAC__StreamDecoderReadStatus status =
#if FLAC__HAS_OGG
                decoder->private_->is_ogg?
                read_callback_ogg_aspect_(decoder, buffer, bytes) :
#endif
                decoder->private_->read_callback(decoder, buffer, bytes, decoder->private_->client_data)
            ;
            if(status == FLAC__STREAM_DECODER_READ_STATUS_ABORT) {
                decoder->protected_->state = FLAC__STREAM_DECODER_ABORTED;
                return false;
            }
            else if(*bytes == 0) {
                if(
                    status == FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM ||
                    (
#if FLAC__HAS_OGG
                        /* see [1] HACK NOTE below for why we don't call the eof_callback when decoding Ogg FLAC */
                        !decoder->private_->is_ogg &&
#endif
                        decoder->private_->eof_callback && decoder->private_->eof_callback(decoder, decoder->private_->client_data)
                    )
                ) {
                    decoder->protected_->state = FLAC__STREAM_DECODER_END_OF_STREAM;
                    return false;
                }
                else
                    return true;
            }
            else
                return true;
        }
    }
    else {
        /* abort to avoid a deadlock */
        decoder->protected_->state = FLAC__STREAM_DECODER_ABORTED;
        return false;
    }
    /* [1] @@@ HACK NOTE: The end-of-stream checking has to be hacked around
     * for Ogg FLAC.  This is because the ogg decoder aspect can lose sync
     * and at the same time hit the end of the stream (for example, seeking
     * to a point that is after the beginning of the last Ogg page).  There
     * is no way to report an Ogg sync loss through the callbacks (see note
     * in read_callback_ogg_aspect_()) so it returns CONTINUE with *bytes==0.
     * So to keep the decoder from stopping at this point we gate the call
     * to the eof_callback and let the Ogg decoder aspect set the
     * end-of-stream state when it is needed.
     */
}

#if FLAC__HAS_OGG
FLAC__StreamDecoderReadStatus read_callback_ogg_aspect_(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes)
{
    switch(FLAC__ogg_decoder_aspect_read_callback_wrapper(&decoder->protected_->ogg_decoder_aspect, buffer, bytes, read_callback_proxy_, decoder, decoder->private_->client_data)) {
        case FLAC__OGG_DECODER_ASPECT_READ_STATUS_OK:
            return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
        /* we don't really have a way to handle lost sync via read
         * callback so we'll let it pass and let the underlying
         * FLAC decoder catch the error
         */
        case FLAC__OGG_DECODER_ASPECT_READ_STATUS_LOST_SYNC:
            return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
        case FLAC__OGG_DECODER_ASPECT_READ_STATUS_END_OF_STREAM:
            return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
        case FLAC__OGG_DECODER_ASPECT_READ_STATUS_NOT_FLAC:
        case FLAC__OGG_DECODER_ASPECT_READ_STATUS_UNSUPPORTED_MAPPING_VERSION:
        case FLAC__OGG_DECODER_ASPECT_READ_STATUS_ABORT:
        case FLAC__OGG_DECODER_ASPECT_READ_STATUS_ERROR:
        case FLAC__OGG_DECODER_ASPECT_READ_STATUS_MEMORY_ALLOCATION_ERROR:
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        default:
            CHOC_ASSERT(0);
            /* double protection */
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    }
}

FLAC__OggDecoderAspectReadStatus read_callback_proxy_(const void *void_decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
    FLAC__StreamDecoder *decoder = (FLAC__StreamDecoder*)void_decoder;

    switch(decoder->private_->read_callback(decoder, buffer, bytes, client_data)) {
        case FLAC__STREAM_DECODER_READ_STATUS_CONTINUE:
            return FLAC__OGG_DECODER_ASPECT_READ_STATUS_OK;
        case FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM:
            return FLAC__OGG_DECODER_ASPECT_READ_STATUS_END_OF_STREAM;
        case FLAC__STREAM_DECODER_READ_STATUS_ABORT:
            return FLAC__OGG_DECODER_ASPECT_READ_STATUS_ABORT;
        default:
            /* double protection: */
            CHOC_ASSERT(0);
            return FLAC__OGG_DECODER_ASPECT_READ_STATUS_ABORT;
    }
}
#endif

FLAC__StreamDecoderWriteStatus write_audio_frame_to_client_(FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[])
{
    if(decoder->private_->is_seeking) {
        FLAC__uint64 this_frame_sample = frame->header.number.sample_number;
        FLAC__uint64 next_frame_sample = this_frame_sample + (FLAC__uint64)frame->header.blocksize;
        FLAC__uint64 target_sample = decoder->private_->target_sample;

        CHOC_ASSERT(frame->header.number_type == FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER);

#if FLAC__HAS_OGG
        decoder->private_->got_a_frame = true;
#endif
        decoder->private_->last_frame = *frame; /* save the frame */
        if(this_frame_sample <= target_sample && target_sample < next_frame_sample) { /* we hit our target frame */
            unsigned delta = (unsigned)(target_sample - this_frame_sample);
            /* kick out of seek mode */
            decoder->private_->is_seeking = false;
            /* shift out the samples before target_sample */
            if(delta > 0) {
                unsigned channel;
                const FLAC__int32 *newbuffer[FLAC__MAX_CHANNELS];
                for(channel = 0; channel < frame->header.channels; channel++)
                    newbuffer[channel] = buffer[channel] + delta;
                decoder->private_->last_frame.header.blocksize -= delta;
                decoder->private_->last_frame.header.number.sample_number += (FLAC__uint64)delta;
                /* write the relevant samples */
                return decoder->private_->write_callback(decoder, &decoder->private_->last_frame, newbuffer, decoder->private_->client_data);
            }
            else {
                /* write the relevant samples */
                return decoder->private_->write_callback(decoder, frame, buffer, decoder->private_->client_data);
            }
        }
        else {
            return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
        }
    }
    else {
        /*
         * If we never got STREAMINFO, turn off MD5 checking to save
         * cycles since we don't have a sum to compare to anyway
         */
        if(!decoder->private_->has_stream_info)
            decoder->private_->do_md5_checking = false;
        if(decoder->private_->do_md5_checking) {
            if(!FLAC__MD5Accumulate(&decoder->private_->md5context, buffer, frame->header.channels, frame->header.blocksize, (frame->header.bits_per_sample+7) / 8))
                return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }
        return decoder->private_->write_callback(decoder, frame, buffer, decoder->private_->client_data);
    }
}

void send_error_to_client_(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status)
{
    if(!decoder->private_->is_seeking)
        decoder->private_->error_callback(decoder, status, decoder->private_->client_data);
    else if(status == FLAC__STREAM_DECODER_ERROR_STATUS_UNPARSEABLE_STREAM)
        decoder->private_->unparseable_frame_count++;
}

FLAC__bool seek_to_absolute_sample_(FLAC__StreamDecoder *decoder, FLAC__uint64 stream_length, FLAC__uint64 target_sample)
{
    FLAC__uint64 first_frame_offset = decoder->private_->first_frame_offset, lower_bound, upper_bound, lower_bound_sample, upper_bound_sample, this_frame_sample;
    FLAC__int64 pos = -1;
    int i;
    unsigned approx_bytes_per_frame;
    FLAC__bool first_seek = true;
    const FLAC__uint64 total_samples = FLAC__stream_decoder_get_total_samples(decoder);
    const unsigned min_blocksize = decoder->private_->stream_info.data.stream_info.min_blocksize;
    const unsigned max_blocksize = decoder->private_->stream_info.data.stream_info.max_blocksize;
    const unsigned max_framesize = decoder->private_->stream_info.data.stream_info.max_framesize;
    const unsigned min_framesize = decoder->private_->stream_info.data.stream_info.min_framesize;
    /* take these from the current frame in case they've changed mid-stream */
    unsigned channels = FLAC__stream_decoder_get_channels(decoder);
    unsigned bps = FLAC__stream_decoder_get_bits_per_sample(decoder);
    const FLAC__StreamMetadata_SeekTable *seek_table = decoder->private_->has_seek_table? &decoder->private_->seek_table.data.seek_table : 0;

    /* use values from stream info if we didn't decode a frame */
    if(channels == 0)
        channels = decoder->private_->stream_info.data.stream_info.channels;
    if(bps == 0)
        bps = decoder->private_->stream_info.data.stream_info.bits_per_sample;

    /* we are just guessing here */
    if(max_framesize > 0)
        approx_bytes_per_frame = (max_framesize + min_framesize) / 2 + 1;
    /*
     * Check if it's a known fixed-blocksize stream.  Note that though
     * the spec doesn't allow zeroes in the STREAMINFO block, we may
     * never get a STREAMINFO block when decoding so the value of
     * min_blocksize might be zero.
     */
    else if(min_blocksize == max_blocksize && min_blocksize > 0) {
        /* note there are no () around 'bps/8' to keep precision up since it's an integer calulation */
        approx_bytes_per_frame = min_blocksize * channels * bps/8 + 64;
    }
    else
        approx_bytes_per_frame = 4096 * channels * bps/8 + 64;

    /*
     * First, we set an upper and lower bound on where in the
     * stream we will search.  For now we assume the worst case
     * scenario, which is our best guess at the beginning of
     * the first frame and end of the stream.
     */
    lower_bound = first_frame_offset;
    lower_bound_sample = 0;
    upper_bound = stream_length;
    upper_bound_sample = total_samples > 0 ? total_samples : target_sample /*estimate it*/;

    /*
     * Now we refine the bounds if we have a seektable with
     * suitable points.  Note that according to the spec they
     * must be ordered by ascending sample number.
     *
     * Note: to protect against invalid seek tables we will ignore points
     * that have frame_samples==0 or sample_number>=total_samples
     */
    if(seek_table) {
        FLAC__uint64 new_lower_bound = lower_bound;
        FLAC__uint64 new_upper_bound = upper_bound;
        FLAC__uint64 new_lower_bound_sample = lower_bound_sample;
        FLAC__uint64 new_upper_bound_sample = upper_bound_sample;

        /* find the closest seek point <= target_sample, if it exists */
        for(i = (int)seek_table->num_points - 1; i >= 0; i--) {
            if(
                seek_table->points[i].sample_number != FLAC__STREAM_METADATA_SEEKPOINT_PLACEHOLDER &&
                seek_table->points[i].frame_samples > 0 && /* defense against bad seekpoints */
                (total_samples <= 0 || seek_table->points[i].sample_number < total_samples) && /* defense against bad seekpoints */
                seek_table->points[i].sample_number <= target_sample
            )
                break;
        }
        if(i >= 0) { /* i.e. we found a suitable seek point... */
            new_lower_bound = first_frame_offset + seek_table->points[i].stream_offset;
            new_lower_bound_sample = seek_table->points[i].sample_number;
        }

        /* find the closest seek point > target_sample, if it exists */
        for(i = 0; i < (int)seek_table->num_points; i++) {
            if(
                seek_table->points[i].sample_number != FLAC__STREAM_METADATA_SEEKPOINT_PLACEHOLDER &&
                seek_table->points[i].frame_samples > 0 && /* defense against bad seekpoints */
                (total_samples <= 0 || seek_table->points[i].sample_number < total_samples) && /* defense against bad seekpoints */
                seek_table->points[i].sample_number > target_sample
            )
                break;
        }
        if(i < (int)seek_table->num_points) { /* i.e. we found a suitable seek point... */
            new_upper_bound = first_frame_offset + seek_table->points[i].stream_offset;
            new_upper_bound_sample = seek_table->points[i].sample_number;
        }
        /* final protection against unsorted seek tables; keep original values if bogus */
        if(new_upper_bound >= new_lower_bound) {
            lower_bound = new_lower_bound;
            upper_bound = new_upper_bound;
            lower_bound_sample = new_lower_bound_sample;
            upper_bound_sample = new_upper_bound_sample;
        }
    }

    CHOC_ASSERT(upper_bound_sample >= lower_bound_sample);
    /* there are 2 insidious ways that the following equality occurs, which
     * we need to fix:
     *  1) total_samples is 0 (unknown) and target_sample is 0
     *  2) total_samples is 0 (unknown) and target_sample happens to be
     *     exactly equal to the last seek point in the seek table; this
     *     means there is no seek point above it, and upper_bound_samples
     *     remains equal to the estimate (of target_samples) we made above
     * in either case it does not hurt to move upper_bound_sample up by 1
     */
    if(upper_bound_sample == lower_bound_sample)
        upper_bound_sample++;

    decoder->private_->target_sample = target_sample;
    while(1) {
        /* check if the bounds are still ok */
        if (lower_bound_sample >= upper_bound_sample || lower_bound > upper_bound) {
            decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
            return false;
        }
#ifndef FLAC__INTEGER_ONLY_LIBRARY
        pos = (FLAC__int64)lower_bound + (FLAC__int64)((FLAC__double)(target_sample - lower_bound_sample) / (FLAC__double)(upper_bound_sample - lower_bound_sample) * (FLAC__double)(upper_bound - lower_bound)) - approx_bytes_per_frame;
#else
        /* a little less accurate: */
        if(upper_bound - lower_bound < 0xffffffff)
            pos = (FLAC__int64)lower_bound + (FLAC__int64)(((target_sample - lower_bound_sample) * (upper_bound - lower_bound)) / (upper_bound_sample - lower_bound_sample)) - approx_bytes_per_frame;
        else /* @@@ WATCHOUT, ~2TB limit */
            pos = (FLAC__int64)lower_bound + (FLAC__int64)((((target_sample - lower_bound_sample)>>8) * ((upper_bound - lower_bound)>>8)) / ((upper_bound_sample - lower_bound_sample)>>16)) - approx_bytes_per_frame;
#endif
        if(pos >= (FLAC__int64)upper_bound)
            pos = (FLAC__int64)upper_bound - 1;
        if(pos < (FLAC__int64)lower_bound)
            pos = (FLAC__int64)lower_bound;
        if(decoder->private_->seek_callback(decoder, (FLAC__uint64)pos, decoder->private_->client_data) != FLAC__STREAM_DECODER_SEEK_STATUS_OK) {
            decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
            return false;
        }
        if(!FLAC__stream_decoder_flush(decoder)) {
            /* above call sets the state for us */
            return false;
        }
        /* Now we need to get a frame.  First we need to reset our
         * unparseable_frame_count; if we get too many unparseable
         * frames in a row, the read callback will return
         * FLAC__STREAM_DECODER_READ_STATUS_ABORT, causing
         * FLAC__stream_decoder_process_single() to return false.
         */
        decoder->private_->unparseable_frame_count = 0;
        if(!FLAC__stream_decoder_process_single(decoder)) {
            decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
            return false;
        }
        /* our write callback will change the state when it gets to the target frame */
        /* actually, we could have got_a_frame if our decoder is at FLAC__STREAM_DECODER_END_OF_STREAM so we need to check for that also */

        if(!decoder->private_->is_seeking)
            break;

        CHOC_ASSERT(decoder->private_->last_frame.header.number_type == FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER);
        this_frame_sample = decoder->private_->last_frame.header.number.sample_number;

        if (0 == decoder->private_->samples_decoded || (this_frame_sample + decoder->private_->last_frame.header.blocksize >= upper_bound_sample && !first_seek)) {
            if (pos == (FLAC__int64)lower_bound) {
                /* can't move back any more than the first frame, something is fatally wrong */
                decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
                return false;
            }
            /* our last move backwards wasn't big enough, try again */
            approx_bytes_per_frame = approx_bytes_per_frame? approx_bytes_per_frame * 2 : 16;
            continue;
        }
        /* allow one seek over upper bound, so we can get a correct upper_bound_sample for streams with unknown total_samples */
        first_seek = false;

        /* make sure we are not seeking in corrupted stream */
        if (this_frame_sample < lower_bound_sample) {
            decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
            return false;
        }

        /* we need to narrow the search */
        if(target_sample < this_frame_sample) {
            upper_bound_sample = this_frame_sample + decoder->private_->last_frame.header.blocksize;
/*@@@@@@ what will decode position be if at end of stream? */
            if(!FLAC__stream_decoder_get_decode_position(decoder, &upper_bound)) {
                decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
                return false;
            }
            approx_bytes_per_frame = (unsigned)(2 * (upper_bound - pos) / 3 + 16);
        }
        else { /* target_sample >= this_frame_sample + this frame's blocksize */
            lower_bound_sample = this_frame_sample + decoder->private_->last_frame.header.blocksize;
            if(!FLAC__stream_decoder_get_decode_position(decoder, &lower_bound)) {
                decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
                return false;
            }
            approx_bytes_per_frame = (unsigned)(2 * (lower_bound - pos) / 3 + 16);
        }
    }

    return true;
}

#if FLAC__HAS_OGG
FLAC__bool seek_to_absolute_sample_ogg_(FLAC__StreamDecoder *decoder, FLAC__uint64 stream_length, FLAC__uint64 target_sample)
{
    FLAC__uint64 left_pos = 0, right_pos = stream_length;
    FLAC__uint64 left_sample = 0, right_sample = FLAC__stream_decoder_get_total_samples(decoder);
    FLAC__uint64 this_frame_sample = (FLAC__uint64)0 - 1;
    FLAC__uint64 pos = 0; /* only initialized to avoid compiler warning */
    FLAC__bool did_a_seek;
    unsigned iteration = 0;

    /* In the first iterations, we will calculate the target byte position
     * by the distance from the target sample to left_sample and
     * right_sample (let's call it "proportional search").  After that, we
     * will switch to binary search.
     */
    unsigned BINARY_SEARCH_AFTER_ITERATION = 2;

    /* We will switch to a linear search once our current sample is less
     * than this number of samples ahead of the target sample
     */
    static const FLAC__uint64 LINEAR_SEARCH_WITHIN_SAMPLES = FLAC__MAX_BLOCK_SIZE * 2;

    /* If the total number of samples is unknown, use a large value, and
     * force binary search immediately.
     */
    if(right_sample == 0) {
        right_sample = (FLAC__uint64)(-1);
        BINARY_SEARCH_AFTER_ITERATION = 0;
    }

    decoder->private_->target_sample = target_sample;
    for( ; ; iteration++) {
        if (iteration == 0 || this_frame_sample > target_sample || target_sample - this_frame_sample > LINEAR_SEARCH_WITHIN_SAMPLES) {
            if (iteration >= BINARY_SEARCH_AFTER_ITERATION) {
                pos = (right_pos + left_pos) / 2;
            }
            else {
#ifndef FLAC__INTEGER_ONLY_LIBRARY
                pos = (FLAC__uint64)((FLAC__double)(target_sample - left_sample) / (FLAC__double)(right_sample - left_sample) * (FLAC__double)(right_pos - left_pos));
#else
                /* a little less accurate: */
                if ((target_sample-left_sample <= 0xffffffff) && (right_pos-left_pos <= 0xffffffff))
                    pos = (FLAC__int64)(((target_sample-left_sample) * (right_pos-left_pos)) / (right_sample-left_sample));
                else /* @@@ WATCHOUT, ~2TB limit */
                    pos = (FLAC__int64)((((target_sample-left_sample)>>8) * ((right_pos-left_pos)>>8)) / ((right_sample-left_sample)>>16));
#endif
                /* @@@ TODO: might want to limit pos to some distance
                 * before EOF, to make sure we land before the last frame,
                 * thereby getting a this_frame_sample and so having a better
                 * estimate.
                 */
            }

            /* physical seek */
            if(decoder->private_->seek_callback((FLAC__StreamDecoder*)decoder, (FLAC__uint64)pos, decoder->private_->client_data) != FLAC__STREAM_DECODER_SEEK_STATUS_OK) {
                decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
                return false;
            }
            if(!FLAC__stream_decoder_flush(decoder)) {
                /* above call sets the state for us */
                return false;
            }
            did_a_seek = true;
        }
        else
            did_a_seek = false;

        decoder->private_->got_a_frame = false;
        if(!FLAC__stream_decoder_process_single(decoder)) {
            decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
            return false;
        }
        if(!decoder->private_->got_a_frame) {
            if(did_a_seek) {
                /* this can happen if we seek to a point after the last frame; we drop
                 * to binary search right away in this case to avoid any wasted
                 * iterations of proportional search.
                 */
                right_pos = pos;
                BINARY_SEARCH_AFTER_ITERATION = 0;
            }
            else {
                /* this can probably only happen if total_samples is unknown and the
                 * target_sample is past the end of the stream
                 */
                decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
                return false;
            }
        }
        /* our write callback will change the state when it gets to the target frame */
        else if(!decoder->private_->is_seeking) {
            break;
        }
        else {
            this_frame_sample = decoder->private_->last_frame.header.number.sample_number;
            CHOC_ASSERT(decoder->private_->last_frame.header.number_type == FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER);

            if (did_a_seek) {
                if (this_frame_sample <= target_sample) {
                    /* The 'equal' case should not happen, since
                     * FLAC__stream_decoder_process_single()
                     * should recognize that it has hit the
                     * target sample and we would exit through
                     * the 'break' above.
                     */
                    CHOC_ASSERT(this_frame_sample != target_sample);

                    left_sample = this_frame_sample;
                    /* sanity check to avoid infinite loop */
                    if (left_pos == pos) {
                        decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
                        return false;
                    }
                    left_pos = pos;
                }
                else if(this_frame_sample > target_sample) {
                    right_sample = this_frame_sample;
                    /* sanity check to avoid infinite loop */
                    if (right_pos == pos) {
                        decoder->protected_->state = FLAC__STREAM_DECODER_SEEK_ERROR;
                        return false;
                    }
                    right_pos = pos;
                }
            }
        }
    }

    return true;
}
#endif


/* Exact Rice codeword length calculation is off by default.  The simple
 * (and fast) estimation (of how many bits a residual value will be
 * encoded with) in this encoder is very good, almost always yielding
 * compression within 0.1% of exact calculation.
 */
#undef EXACT_RICE_BITS_CALCULATION
/* Rice parameter searching is off by default.  The simple (and fast)
 * parameter estimation in this encoder is very good, almost always
 * yielding compression within 0.1% of the optimal parameters.
 */
#undef ENABLE_RICE_PARAMETER_SEARCH


typedef struct {
    FLAC__int32 *data[FLAC__MAX_CHANNELS];
    unsigned size; /* of each data[] in samples */
    unsigned tail;
} verify_input_fifo;

typedef struct {
    const FLAC__byte *data;
    unsigned capacity;
    unsigned bytes;
} verify_output;

typedef enum {
    ENCODER_IN_MAGIC = 0,
    ENCODER_IN_METADATA = 1,
    ENCODER_IN_AUDIO = 2
} EncoderStateHint;

static struct CompressionLevels {
    FLAC__bool do_mid_side_stereo;
    FLAC__bool loose_mid_side_stereo;
    unsigned max_lpc_order;
    unsigned qlp_coeff_precision;
    FLAC__bool do_qlp_coeff_prec_search;
    FLAC__bool do_escape_coding;
    FLAC__bool do_exhaustive_model_search;
    unsigned min_residual_partition_order;
    unsigned max_residual_partition_order;
    unsigned rice_parameter_search_dist;
    const char *apodization;
} compression_levels_[] = {
    { false, false,  0, 0, false, false, false, 0, 3, 0, "tukey(5e-1)" },
    { true , true ,  0, 0, false, false, false, 0, 3, 0, "tukey(5e-1)" },
    { true , false,  0, 0, false, false, false, 0, 3, 0, "tukey(5e-1)" },
    { false, false,  6, 0, false, false, false, 0, 4, 0, "tukey(5e-1)" },
    { true , true ,  8, 0, false, false, false, 0, 4, 0, "tukey(5e-1)" },
    { true , false,  8, 0, false, false, false, 0, 5, 0, "tukey(5e-1)" },
    { true , false,  8, 0, false, false, false, 0, 6, 0, "tukey(5e-1);partial_tukey(2)" },
    { true , false, 12, 0, false, false, false, 0, 6, 0, "tukey(5e-1);partial_tukey(2)" },
    { true , false, 12, 0, false, false, false, 0, 6, 0, "tukey(5e-1);partial_tukey(2);punchout_tukey(3)" }
    /* here we use locale-independent 5e-1 instead of 0.5 or 0,5 */
};


/***********************************************************************
 *
 * Private class method prototypes
 *
 ***********************************************************************/

static void set_defaults_(FLAC__StreamEncoder *encoder);
static void free_(FLAC__StreamEncoder *encoder);
static FLAC__bool resize_buffers_(FLAC__StreamEncoder *encoder, unsigned new_blocksize);
static FLAC__bool write_bitbuffer_(FLAC__StreamEncoder *encoder, unsigned samples, FLAC__bool is_last_block);
static FLAC__StreamEncoderWriteStatus write_frame_(FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, FLAC__bool is_last_block);
static void update_metadata_(const FLAC__StreamEncoder *encoder);
#if FLAC__HAS_OGG
static void update_ogg_metadata_(FLAC__StreamEncoder *encoder);
#endif
static FLAC__bool process_frame_(FLAC__StreamEncoder *encoder, FLAC__bool is_fractional_block, FLAC__bool is_last_block);
static FLAC__bool process_subframes_(FLAC__StreamEncoder *encoder, FLAC__bool is_fractional_block);

static FLAC__bool process_subframe_(
    FLAC__StreamEncoder *encoder,
    unsigned min_partition_order,
    unsigned max_partition_order,
    const FLAC__FrameHeader *frame_header,
    unsigned subframe_bps,
    const FLAC__int32 integer_signal[],
    FLAC__Subframe *subframe[2],
    FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents[2],
    FLAC__int32 *residual[2],
    unsigned *best_subframe,
    unsigned *best_bits
);

static FLAC__bool add_subframe_(
    FLAC__StreamEncoder *encoder,
    unsigned blocksize,
    unsigned subframe_bps,
    const FLAC__Subframe *subframe,
    FLAC__BitWriter *frame
);

static unsigned evaluate_constant_subframe_(
    FLAC__StreamEncoder *encoder,
    const FLAC__int32 signal,
    unsigned blocksize,
    unsigned subframe_bps,
    FLAC__Subframe *subframe
);

static unsigned evaluate_fixed_subframe_(
    FLAC__StreamEncoder *encoder,
    const FLAC__int32 signal[],
    FLAC__int32 residual[],
    FLAC__uint64 abs_residual_partition_sums[],
    unsigned raw_bits_per_partition[],
    unsigned blocksize,
    unsigned subframe_bps,
    unsigned order,
    unsigned rice_parameter,
    unsigned rice_parameter_limit,
    unsigned min_partition_order,
    unsigned max_partition_order,
    FLAC__bool do_escape_coding,
    unsigned rice_parameter_search_dist,
    FLAC__Subframe *subframe,
    FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents
);

#ifndef FLAC__INTEGER_ONLY_LIBRARY
static unsigned evaluate_lpc_subframe_(
    FLAC__StreamEncoder *encoder,
    const FLAC__int32 signal[],
    FLAC__int32 residual[],
    FLAC__uint64 abs_residual_partition_sums[],
    unsigned raw_bits_per_partition[],
    const FLAC__real lp_coeff[],
    unsigned blocksize,
    unsigned subframe_bps,
    unsigned order,
    unsigned qlp_coeff_precision,
    unsigned rice_parameter,
    unsigned rice_parameter_limit,
    unsigned min_partition_order,
    unsigned max_partition_order,
    FLAC__bool do_escape_coding,
    unsigned rice_parameter_search_dist,
    FLAC__Subframe *subframe,
    FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents
);
#endif

static unsigned evaluate_verbatim_subframe_(
    FLAC__StreamEncoder *encoder,
    const FLAC__int32 signal[],
    unsigned blocksize,
    unsigned subframe_bps,
    FLAC__Subframe *subframe
);

static unsigned find_best_partition_order_(
    struct FLAC__StreamEncoderPrivate *private_,
    const FLAC__int32 residual[],
    FLAC__uint64 abs_residual_partition_sums[],
    unsigned raw_bits_per_partition[],
    unsigned residual_samples,
    unsigned predictor_order,
    unsigned rice_parameter,
    unsigned rice_parameter_limit,
    unsigned min_partition_order,
    unsigned max_partition_order,
    unsigned bps,
    FLAC__bool do_escape_coding,
    unsigned rice_parameter_search_dist,
    FLAC__EntropyCodingMethod *best_ecm
);

static void precompute_partition_info_sums_(
    const FLAC__int32 residual[],
    FLAC__uint64 abs_residual_partition_sums[],
    unsigned residual_samples,
    unsigned predictor_order,
    unsigned min_partition_order,
    unsigned max_partition_order,
    unsigned bps
);

static void precompute_partition_info_escapes_(
    const FLAC__int32 residual[],
    unsigned raw_bits_per_partition[],
    unsigned residual_samples,
    unsigned predictor_order,
    unsigned min_partition_order,
    unsigned max_partition_order
);

static FLAC__bool set_partitioned_rice_(
#ifdef EXACT_RICE_BITS_CALCULATION
    const FLAC__int32 residual[],
#endif
    const FLAC__uint64 abs_residual_partition_sums[],
    const unsigned raw_bits_per_partition[],
    const unsigned residual_samples,
    const unsigned predictor_order,
    const unsigned suggested_rice_parameter,
    const unsigned rice_parameter_limit,
    const unsigned rice_parameter_search_dist,
    const unsigned partition_order,
    const FLAC__bool search_for_escapes,
    FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents,
    unsigned *bits
);

static unsigned get_wasted_bits_(FLAC__int32 signal[], unsigned samples);

/* verify-related routines: */
static void append_to_verify_fifo_(
    verify_input_fifo *fifo,
    const FLAC__int32 * const input[],
    unsigned input_offset,
    unsigned channels,
    unsigned wide_samples
);

static void append_to_verify_fifo_interleaved_(
    verify_input_fifo *fifo,
    const FLAC__int32 input[],
    unsigned input_offset,
    unsigned channels,
    unsigned wide_samples
);

static FLAC__StreamDecoderReadStatus verify_read_callback_(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data);
static FLAC__StreamDecoderWriteStatus verify_write_callback_(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data);
static void verify_metadata_callback_(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data);
static void verify_error_callback_(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data);

/***********************************************************************
 *
 * Private class data
 *
 ***********************************************************************/

typedef struct FLAC__StreamEncoderPrivate {
    unsigned input_capacity;                          /* current size (in samples) of the signal and residual buffers */
    FLAC__int32 *integer_signal[FLAC__MAX_CHANNELS];  /* the integer version of the input signal */
    FLAC__int32 *integer_signal_mid_side[2];          /* the integer version of the mid-side input signal (stereo only) */
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    FLAC__real *real_signal[FLAC__MAX_CHANNELS];      /* (@@@ currently unused) the floating-point version of the input signal */
    FLAC__real *real_signal_mid_side[2];              /* (@@@ currently unused) the floating-point version of the mid-side input signal (stereo only) */
    FLAC__real *window[FLAC__MAX_APODIZATION_FUNCTIONS]; /* the pre-computed floating-point window for each apodization function */
    FLAC__real *windowed_signal;                      /* the integer_signal[] * current window[] */
#endif
    unsigned subframe_bps[FLAC__MAX_CHANNELS];        /* the effective bits per sample of the input signal (stream bps - wasted bits) */
    unsigned subframe_bps_mid_side[2];                /* the effective bits per sample of the mid-side input signal (stream bps - wasted bits + 0/1) */
    FLAC__int32 *residual_workspace[FLAC__MAX_CHANNELS][2]; /* each channel has a candidate and best workspace where the subframe residual signals will be stored */
    FLAC__int32 *residual_workspace_mid_side[2][2];
    FLAC__Subframe subframe_workspace[FLAC__MAX_CHANNELS][2];
    FLAC__Subframe subframe_workspace_mid_side[2][2];
    FLAC__Subframe *subframe_workspace_ptr[FLAC__MAX_CHANNELS][2];
    FLAC__Subframe *subframe_workspace_ptr_mid_side[2][2];
    FLAC__EntropyCodingMethod_PartitionedRiceContents partitioned_rice_contents_workspace[FLAC__MAX_CHANNELS][2];
    FLAC__EntropyCodingMethod_PartitionedRiceContents partitioned_rice_contents_workspace_mid_side[FLAC__MAX_CHANNELS][2];
    FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents_workspace_ptr[FLAC__MAX_CHANNELS][2];
    FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents_workspace_ptr_mid_side[FLAC__MAX_CHANNELS][2];
    unsigned best_subframe[FLAC__MAX_CHANNELS];       /* index (0 or 1) into 2nd dimension of the above workspaces */
    unsigned best_subframe_mid_side[2];
    unsigned best_subframe_bits[FLAC__MAX_CHANNELS];  /* size in bits of the best subframe for each channel */
    unsigned best_subframe_bits_mid_side[2];
    FLAC__uint64 *abs_residual_partition_sums;        /* workspace where the sum of abs(candidate residual) for each partition is stored */
    unsigned *raw_bits_per_partition;                 /* workspace where the sum of silog2(candidate residual) for each partition is stored */
    FLAC__BitWriter *frame;                           /* the current frame being worked on */
    unsigned loose_mid_side_stereo_frames;            /* rounded number of frames the encoder will use before trying both independent and mid/side frames again */
    unsigned loose_mid_side_stereo_frame_count;       /* number of frames using the current channel assignment */
    FLAC__ChannelAssignment last_channel_assignment;
    FLAC__StreamMetadata streaminfo;                  /* scratchpad for STREAMINFO as it is built */
    FLAC__StreamMetadata_SeekTable *seek_table;       /* pointer into encoder->protected_->metadata_ where the seek table is */
    unsigned current_sample_number;
    unsigned current_frame_number;
    FLAC__MD5Context md5context;
    FLAC__CPUInfo cpuinfo;
    void (*local_precompute_partition_info_sums)(const FLAC__int32 residual[], FLAC__uint64 abs_residual_partition_sums[], unsigned residual_samples, unsigned predictor_order, unsigned min_partition_order, unsigned max_partition_order, unsigned bps);
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    unsigned (*local_fixed_compute_best_predictor)(const FLAC__int32 data[], unsigned data_len, FLAC__float residual_bits_per_sample[FLAC__MAX_FIXED_ORDER+1]);
    unsigned (*local_fixed_compute_best_predictor_wide)(const FLAC__int32 data[], unsigned data_len, FLAC__float residual_bits_per_sample[FLAC__MAX_FIXED_ORDER+1]);
#else
    unsigned (*local_fixed_compute_best_predictor)(const FLAC__int32 data[], unsigned data_len, FLAC__fixedpoint residual_bits_per_sample[FLAC__MAX_FIXED_ORDER+1]);
    unsigned (*local_fixed_compute_best_predictor_wide)(const FLAC__int32 data[], unsigned data_len, FLAC__fixedpoint residual_bits_per_sample[FLAC__MAX_FIXED_ORDER+1]);
#endif
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    void (*local_lpc_compute_autocorrelation)(const FLAC__real data[], unsigned data_len, unsigned lag, FLAC__real autoc[]);
    void (*local_lpc_compute_residual_from_qlp_coefficients)(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
    void (*local_lpc_compute_residual_from_qlp_coefficients_64bit)(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
    void (*local_lpc_compute_residual_from_qlp_coefficients_16bit)(const FLAC__int32 *data, unsigned data_len, const FLAC__int32 qlp_coeff[], unsigned order, int lp_quantization, FLAC__int32 residual[]);
#endif
    FLAC__bool use_wide_by_block;          /* use slow 64-bit versions of some functions because of the block size */
    FLAC__bool use_wide_by_partition;      /* use slow 64-bit versions of some functions because of the min partition order and blocksize */
    FLAC__bool use_wide_by_order;          /* use slow 64-bit versions of some functions because of the lpc order */
    FLAC__bool disable_constant_subframes;
    FLAC__bool disable_fixed_subframes;
    FLAC__bool disable_verbatim_subframes;
#if FLAC__HAS_OGG
    FLAC__bool is_ogg;
#endif
    FLAC__StreamEncoderReadCallback read_callback; /* currently only needed for Ogg FLAC */
    FLAC__StreamEncoderSeekCallback seek_callback;
    FLAC__StreamEncoderTellCallback tell_callback;
    FLAC__StreamEncoderWriteCallback write_callback;
    FLAC__StreamEncoderMetadataCallback metadata_callback;
    FLAC__StreamEncoderProgressCallback progress_callback;
    void *client_data;
    unsigned first_seekpoint_to_check;
    FILE *file;                            /* only used when encoding to a file */
    FLAC__uint64 bytes_written;
    FLAC__uint64 samples_written;
    unsigned frames_written;
    unsigned total_frames_estimate;
    /* unaligned (original) pointers to allocated data */
    FLAC__int32 *integer_signal_unaligned[FLAC__MAX_CHANNELS];
    FLAC__int32 *integer_signal_mid_side_unaligned[2];
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    FLAC__real *real_signal_unaligned[FLAC__MAX_CHANNELS]; /* (@@@ currently unused) */
    FLAC__real *real_signal_mid_side_unaligned[2]; /* (@@@ currently unused) */
    FLAC__real *window_unaligned[FLAC__MAX_APODIZATION_FUNCTIONS];
    FLAC__real *windowed_signal_unaligned;
#endif
    FLAC__int32 *residual_workspace_unaligned[FLAC__MAX_CHANNELS][2];
    FLAC__int32 *residual_workspace_mid_side_unaligned[2][2];
    FLAC__uint64 *abs_residual_partition_sums_unaligned;
    unsigned *raw_bits_per_partition_unaligned;
    /*
     * These fields have been moved here from private function local
     * declarations merely to save stack space during encoding.
     */
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    FLAC__real lp_coeff[FLAC__MAX_LPC_ORDER][FLAC__MAX_LPC_ORDER]; /* from process_subframe_() */
#endif
    FLAC__EntropyCodingMethod_PartitionedRiceContents partitioned_rice_contents_extra[2]; /* from find_best_partition_order_() */
    /*
     * The data for the verify section
     */
    struct {
        FLAC__StreamDecoder *decoder;
        EncoderStateHint state_hint;
        FLAC__bool needs_magic_hack;
        verify_input_fifo input_fifo;
        verify_output output;
        struct {
            FLAC__uint64 absolute_sample;
            unsigned frame_number;
            unsigned channel;
            unsigned sample;
            FLAC__int32 expected;
            FLAC__int32 got;
        } error_stats;
    } verify;
    FLAC__bool is_being_deleted; /* if true, call to ..._finish() from ..._delete() will not call the callbacks */
} FLAC__StreamEncoderPrivate;

/***********************************************************************
 *
 * Public static class data
 *
 ***********************************************************************/

FLAC_API const char * const FLAC__StreamEncoderStateString[] = {
    "FLAC__STREAM_ENCODER_OK",
    "FLAC__STREAM_ENCODER_UNINITIALIZED",
    "FLAC__STREAM_ENCODER_OGG_ERROR",
    "FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR",
    "FLAC__STREAM_ENCODER_VERIFY_MISMATCH_IN_AUDIO_DATA",
    "FLAC__STREAM_ENCODER_CLIENT_ERROR",
    "FLAC__STREAM_ENCODER_IO_ERROR",
    "FLAC__STREAM_ENCODER_FRAMING_ERROR",
    "FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR"
};

FLAC_API const char * const FLAC__StreamEncoderInitStatusString[] = {
    "FLAC__STREAM_ENCODER_INIT_STATUS_OK",
    "FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR",
    "FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER",
    "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS",
    "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS",
    "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE",
    "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE",
    "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE",
    "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER",
    "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION",
    "FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER",
    "FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE",
    "FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA",
    "FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED"
};

FLAC_API const char * const FLAC__StreamEncoderReadStatusString[] = {
    "FLAC__STREAM_ENCODER_READ_STATUS_CONTINUE",
    "FLAC__STREAM_ENCODER_READ_STATUS_END_OF_STREAM",
    "FLAC__STREAM_ENCODER_READ_STATUS_ABORT",
    "FLAC__STREAM_ENCODER_READ_STATUS_UNSUPPORTED"
};

FLAC_API const char * const FLAC__StreamEncoderWriteStatusString[] = {
    "FLAC__STREAM_ENCODER_WRITE_STATUS_OK",
    "FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR"
};

FLAC_API const char * const FLAC__StreamEncoderSeekStatusString[] = {
    "FLAC__STREAM_ENCODER_SEEK_STATUS_OK",
    "FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR",
    "FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED"
};

FLAC_API const char * const FLAC__StreamEncoderTellStatusString[] = {
    "FLAC__STREAM_ENCODER_TELL_STATUS_OK",
    "FLAC__STREAM_ENCODER_TELL_STATUS_ERROR",
    "FLAC__STREAM_ENCODER_TELL_STATUS_UNSUPPORTED"
};

/* Number of samples that will be overread to watch for end of stream.  By
 * 'overread', we mean that the FLAC__stream_encoder_process*() calls will
 * always try to read blocksize+1 samples before encoding a block, so that
 * even if the stream has a total sample count that is an integral multiple
 * of the blocksize, we will still notice when we are encoding the last
 * block.  This is needed, for example, to correctly set the end-of-stream
 * marker in Ogg FLAC.
 *
 * WATCHOUT: some parts of the code assert that OVERREAD_ == 1 and there's
 * not really any reason to change it.
 */
static const unsigned OVERREAD_ = 1;

/***********************************************************************
 *
 * Class constructor/destructor
 *
 */
FLAC_API FLAC__StreamEncoder *FLAC__stream_encoder_new(void)
{
    FLAC__StreamEncoder *encoder;
    unsigned i;

    CHOC_ASSERT(sizeof(int) >= 4); /* we want to die right away if this is not true */

    encoder = (FLAC__StreamEncoder*) calloc(1, sizeof(FLAC__StreamEncoder));
    if(encoder == 0) {
        return 0;
    }

    encoder->protected_ = (FLAC__StreamEncoderProtected*) calloc(1, sizeof(FLAC__StreamEncoderProtected));
    if(encoder->protected_ == 0) {
        free(encoder);
        return 0;
    }

    encoder->private_ = (FLAC__StreamEncoderPrivate*) calloc(1, sizeof(FLAC__StreamEncoderPrivate));
    if(encoder->private_ == 0) {
        free(encoder->protected_);
        free(encoder);
        return 0;
    }

    encoder->private_->frame = FLAC__bitwriter_new();
    if(encoder->private_->frame == 0) {
        free(encoder->private_);
        free(encoder->protected_);
        free(encoder);
        return 0;
    }

    encoder->private_->file = 0;

    set_defaults_(encoder);

    encoder->private_->is_being_deleted = false;

    for(i = 0; i < FLAC__MAX_CHANNELS; i++) {
        encoder->private_->subframe_workspace_ptr[i][0] = &encoder->private_->subframe_workspace[i][0];
        encoder->private_->subframe_workspace_ptr[i][1] = &encoder->private_->subframe_workspace[i][1];
    }
    for(i = 0; i < 2; i++) {
        encoder->private_->subframe_workspace_ptr_mid_side[i][0] = &encoder->private_->subframe_workspace_mid_side[i][0];
        encoder->private_->subframe_workspace_ptr_mid_side[i][1] = &encoder->private_->subframe_workspace_mid_side[i][1];
    }
    for(i = 0; i < FLAC__MAX_CHANNELS; i++) {
        encoder->private_->partitioned_rice_contents_workspace_ptr[i][0] = &encoder->private_->partitioned_rice_contents_workspace[i][0];
        encoder->private_->partitioned_rice_contents_workspace_ptr[i][1] = &encoder->private_->partitioned_rice_contents_workspace[i][1];
    }
    for(i = 0; i < 2; i++) {
        encoder->private_->partitioned_rice_contents_workspace_ptr_mid_side[i][0] = &encoder->private_->partitioned_rice_contents_workspace_mid_side[i][0];
        encoder->private_->partitioned_rice_contents_workspace_ptr_mid_side[i][1] = &encoder->private_->partitioned_rice_contents_workspace_mid_side[i][1];
    }

    for(i = 0; i < FLAC__MAX_CHANNELS; i++) {
        FLAC__format_entropy_coding_method_partitioned_rice_contents_init(&encoder->private_->partitioned_rice_contents_workspace[i][0]);
        FLAC__format_entropy_coding_method_partitioned_rice_contents_init(&encoder->private_->partitioned_rice_contents_workspace[i][1]);
    }
    for(i = 0; i < 2; i++) {
        FLAC__format_entropy_coding_method_partitioned_rice_contents_init(&encoder->private_->partitioned_rice_contents_workspace_mid_side[i][0]);
        FLAC__format_entropy_coding_method_partitioned_rice_contents_init(&encoder->private_->partitioned_rice_contents_workspace_mid_side[i][1]);
    }
    for(i = 0; i < 2; i++)
        FLAC__format_entropy_coding_method_partitioned_rice_contents_init(&encoder->private_->partitioned_rice_contents_extra[i]);

    encoder->protected_->state = FLAC__STREAM_ENCODER_UNINITIALIZED;

    return encoder;
}

FLAC_API void FLAC__stream_encoder_delete(FLAC__StreamEncoder *encoder)
{
    unsigned i;

    if (encoder == NULL)
        return ;

    CHOC_ASSERT(0 != encoder->protected_);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->private_->frame);

    encoder->private_->is_being_deleted = true;

    (void)FLAC__stream_encoder_finish(encoder);

    if(0 != encoder->private_->verify.decoder)
        FLAC__stream_decoder_delete(encoder->private_->verify.decoder);

    for(i = 0; i < FLAC__MAX_CHANNELS; i++) {
        FLAC__format_entropy_coding_method_partitioned_rice_contents_clear(&encoder->private_->partitioned_rice_contents_workspace[i][0]);
        FLAC__format_entropy_coding_method_partitioned_rice_contents_clear(&encoder->private_->partitioned_rice_contents_workspace[i][1]);
    }
    for(i = 0; i < 2; i++) {
        FLAC__format_entropy_coding_method_partitioned_rice_contents_clear(&encoder->private_->partitioned_rice_contents_workspace_mid_side[i][0]);
        FLAC__format_entropy_coding_method_partitioned_rice_contents_clear(&encoder->private_->partitioned_rice_contents_workspace_mid_side[i][1]);
    }
    for(i = 0; i < 2; i++)
        FLAC__format_entropy_coding_method_partitioned_rice_contents_clear(&encoder->private_->partitioned_rice_contents_extra[i]);

    FLAC__bitwriter_delete(encoder->private_->frame);
    free(encoder->private_);
    free(encoder->protected_);
    free(encoder);
}

/***********************************************************************
 *
 * Public class methods
 *
 ***********************************************************************/

static FLAC__StreamEncoderInitStatus init_stream_internal_(
    FLAC__StreamEncoder *encoder,
    FLAC__StreamEncoderReadCallback read_callback,
    FLAC__StreamEncoderWriteCallback write_callback,
    FLAC__StreamEncoderSeekCallback seek_callback,
    FLAC__StreamEncoderTellCallback tell_callback,
    FLAC__StreamEncoderMetadataCallback metadata_callback,
    void *client_data,
    FLAC__bool is_ogg
)
{
    unsigned i;
    FLAC__bool metadata_has_seektable, metadata_has_vorbis_comment, metadata_picture_has_type1, metadata_picture_has_type2;

    CHOC_ASSERT(0 != encoder);

    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return FLAC__STREAM_ENCODER_INIT_STATUS_ALREADY_INITIALIZED;

#if !FLAC__HAS_OGG
    if(is_ogg)
        return FLAC__STREAM_ENCODER_INIT_STATUS_UNSUPPORTED_CONTAINER;
#endif

    if(0 == write_callback || (seek_callback && 0 == tell_callback))
        return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_CALLBACKS;

    if(encoder->protected_->channels == 0 || encoder->protected_->channels > FLAC__MAX_CHANNELS)
        return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_NUMBER_OF_CHANNELS;

    if(encoder->protected_->channels != 2) {
        encoder->protected_->do_mid_side_stereo = false;
        encoder->protected_->loose_mid_side_stereo = false;
    }
    else if(!encoder->protected_->do_mid_side_stereo)
        encoder->protected_->loose_mid_side_stereo = false;

    if(encoder->protected_->bits_per_sample >= 32)
        encoder->protected_->do_mid_side_stereo = false; /* since we currenty do 32-bit math, the side channel would have 33 bps and overflow */

    if(encoder->protected_->bits_per_sample < FLAC__MIN_BITS_PER_SAMPLE || encoder->protected_->bits_per_sample > FLAC__REFERENCE_CODEC_MAX_BITS_PER_SAMPLE)
        return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BITS_PER_SAMPLE;

    if(!FLAC__format_sample_rate_is_valid(encoder->protected_->sample_rate))
        return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_SAMPLE_RATE;

    if(encoder->protected_->blocksize == 0) {
        if(encoder->protected_->max_lpc_order == 0)
            encoder->protected_->blocksize = 1152;
        else
            encoder->protected_->blocksize = 4096;
    }

    if(encoder->protected_->blocksize < FLAC__MIN_BLOCK_SIZE || encoder->protected_->blocksize > FLAC__MAX_BLOCK_SIZE)
        return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_BLOCK_SIZE;

    if(encoder->protected_->max_lpc_order > FLAC__MAX_LPC_ORDER)
        return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_MAX_LPC_ORDER;

    if(encoder->protected_->blocksize < encoder->protected_->max_lpc_order)
        return FLAC__STREAM_ENCODER_INIT_STATUS_BLOCK_SIZE_TOO_SMALL_FOR_LPC_ORDER;

    if(encoder->protected_->qlp_coeff_precision == 0) {
        if(encoder->protected_->bits_per_sample < 16) {
            /* @@@ need some data about how to set this here w.r.t. blocksize and sample rate */
            /* @@@ until then we'll make a guess */
            encoder->protected_->qlp_coeff_precision = std::max(FLAC__MIN_QLP_COEFF_PRECISION, 2 + encoder->protected_->bits_per_sample / 2);
        }
        else if(encoder->protected_->bits_per_sample == 16) {
            if(encoder->protected_->blocksize <= 192)
                encoder->protected_->qlp_coeff_precision = 7;
            else if(encoder->protected_->blocksize <= 384)
                encoder->protected_->qlp_coeff_precision = 8;
            else if(encoder->protected_->blocksize <= 576)
                encoder->protected_->qlp_coeff_precision = 9;
            else if(encoder->protected_->blocksize <= 1152)
                encoder->protected_->qlp_coeff_precision = 10;
            else if(encoder->protected_->blocksize <= 2304)
                encoder->protected_->qlp_coeff_precision = 11;
            else if(encoder->protected_->blocksize <= 4608)
                encoder->protected_->qlp_coeff_precision = 12;
            else
                encoder->protected_->qlp_coeff_precision = 13;
        }
        else {
            if(encoder->protected_->blocksize <= 384)
                encoder->protected_->qlp_coeff_precision = FLAC__MAX_QLP_COEFF_PRECISION-2;
            else if(encoder->protected_->blocksize <= 1152)
                encoder->protected_->qlp_coeff_precision = FLAC__MAX_QLP_COEFF_PRECISION-1;
            else
                encoder->protected_->qlp_coeff_precision = FLAC__MAX_QLP_COEFF_PRECISION;
        }
        CHOC_ASSERT(encoder->protected_->qlp_coeff_precision <= FLAC__MAX_QLP_COEFF_PRECISION);
    }
    else if(encoder->protected_->qlp_coeff_precision < FLAC__MIN_QLP_COEFF_PRECISION || encoder->protected_->qlp_coeff_precision > FLAC__MAX_QLP_COEFF_PRECISION)
        return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_QLP_COEFF_PRECISION;

    if(encoder->protected_->streamable_subset) {
        if(!FLAC__format_blocksize_is_subset(encoder->protected_->blocksize, encoder->protected_->sample_rate))
            return FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE;
        if(!FLAC__format_sample_rate_is_subset(encoder->protected_->sample_rate))
            return FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE;
        if(
            encoder->protected_->bits_per_sample != 8 &&
            encoder->protected_->bits_per_sample != 12 &&
            encoder->protected_->bits_per_sample != 16 &&
            encoder->protected_->bits_per_sample != 20 &&
            encoder->protected_->bits_per_sample != 24
        )
            return FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE;
        if(encoder->protected_->max_residual_partition_order > FLAC__SUBSET_MAX_RICE_PARTITION_ORDER)
            return FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE;
        if(
            encoder->protected_->sample_rate <= 48000 &&
            (
                encoder->protected_->blocksize > FLAC__SUBSET_MAX_BLOCK_SIZE_48000HZ ||
                encoder->protected_->max_lpc_order > FLAC__SUBSET_MAX_LPC_ORDER_48000HZ
            )
        ) {
            return FLAC__STREAM_ENCODER_INIT_STATUS_NOT_STREAMABLE;
        }
    }

    if(encoder->protected_->max_residual_partition_order >= (1u << FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ORDER_LEN))
        encoder->protected_->max_residual_partition_order = (1u << FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ORDER_LEN) - 1;
    if(encoder->protected_->min_residual_partition_order >= encoder->protected_->max_residual_partition_order)
        encoder->protected_->min_residual_partition_order = encoder->protected_->max_residual_partition_order;

#if FLAC__HAS_OGG
    /* reorder metadata if necessary to ensure that any VORBIS_COMMENT is the first, according to the mapping spec */
    if(is_ogg && 0 != encoder->protected_->metadata && encoder->protected_->num_metadata_blocks > 1) {
        unsigned i1;
        for(i1 = 1; i1 < encoder->protected_->num_metadata_blocks; i1++) {
            if(0 != encoder->protected_->metadata[i1] && encoder->protected_->metadata[i1]->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
                FLAC__StreamMetadata *vc = encoder->protected_->metadata[i1];
                for( ; i1 > 0; i1--)
                    encoder->protected_->metadata[i1] = encoder->protected_->metadata[i1-1];
                encoder->protected_->metadata[0] = vc;
                break;
            }
        }
    }
#endif
    /* keep track of any SEEKTABLE block */
    if(0 != encoder->protected_->metadata && encoder->protected_->num_metadata_blocks > 0) {
        unsigned i2;
        for(i2 = 0; i2 < encoder->protected_->num_metadata_blocks; i2++) {
            if(0 != encoder->protected_->metadata[i2] && encoder->protected_->metadata[i2]->type == FLAC__METADATA_TYPE_SEEKTABLE) {
                encoder->private_->seek_table = &encoder->protected_->metadata[i2]->data.seek_table;
                break; /* take only the first one */
            }
        }
    }

    /* validate metadata */
    if(0 == encoder->protected_->metadata && encoder->protected_->num_metadata_blocks > 0)
        return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA;
    metadata_has_seektable = false;
    metadata_has_vorbis_comment = false;
    metadata_picture_has_type1 = false;
    metadata_picture_has_type2 = false;
    for(i = 0; i < encoder->protected_->num_metadata_blocks; i++) {
        const FLAC__StreamMetadata *m = encoder->protected_->metadata[i];
        if(m->type == FLAC__METADATA_TYPE_STREAMINFO)
            return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA;
        else if(m->type == FLAC__METADATA_TYPE_SEEKTABLE) {
            if(metadata_has_seektable) /* only one is allowed */
                return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA;
            metadata_has_seektable = true;
            if(!FLAC__format_seektable_is_legal(&m->data.seek_table))
                return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA;
        }
        else if(m->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
            if(metadata_has_vorbis_comment) /* only one is allowed */
                return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA;
            metadata_has_vorbis_comment = true;
        }
        else if(m->type == FLAC__METADATA_TYPE_CUESHEET) {
            if(!FLAC__format_cuesheet_is_legal(&m->data.cue_sheet, m->data.cue_sheet.is_cd, /*violation=*/0))
                return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA;
        }
        else if(m->type == FLAC__METADATA_TYPE_PICTURE) {
            if(!FLAC__format_picture_is_legal(&m->data.picture, /*violation=*/0))
                return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA;
            if(m->data.picture.type == FLAC__STREAM_METADATA_PICTURE_TYPE_FILE_ICON_STANDARD) {
                if(metadata_picture_has_type1) /* there should only be 1 per stream */
                    return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA;
                metadata_picture_has_type1 = true;
                /* standard icon must be 32x32 pixel PNG */
                if(
                    m->data.picture.type == FLAC__STREAM_METADATA_PICTURE_TYPE_FILE_ICON_STANDARD &&
                    (
                        (strcmp(m->data.picture.mime_type, "image/png") && strcmp(m->data.picture.mime_type, "-->")) ||
                        m->data.picture.width != 32 ||
                        m->data.picture.height != 32
                    )
                )
                    return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA;
            }
            else if(m->data.picture.type == FLAC__STREAM_METADATA_PICTURE_TYPE_FILE_ICON) {
                if(metadata_picture_has_type2) /* there should only be 1 per stream */
                    return FLAC__STREAM_ENCODER_INIT_STATUS_INVALID_METADATA;
                metadata_picture_has_type2 = true;
            }
        }
    }

    encoder->private_->input_capacity = 0;
    for(i = 0; i < encoder->protected_->channels; i++) {
        encoder->private_->integer_signal_unaligned[i] = encoder->private_->integer_signal[i] = 0;
#ifndef FLAC__INTEGER_ONLY_LIBRARY
        encoder->private_->real_signal_unaligned[i] = encoder->private_->real_signal[i] = 0;
#endif
    }
    for(i = 0; i < 2; i++) {
        encoder->private_->integer_signal_mid_side_unaligned[i] = encoder->private_->integer_signal_mid_side[i] = 0;
#ifndef FLAC__INTEGER_ONLY_LIBRARY
        encoder->private_->real_signal_mid_side_unaligned[i] = encoder->private_->real_signal_mid_side[i] = 0;
#endif
    }
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    for(i = 0; i < encoder->protected_->num_apodizations; i++)
        encoder->private_->window_unaligned[i] = encoder->private_->window[i] = 0;
    encoder->private_->windowed_signal_unaligned = encoder->private_->windowed_signal = 0;
#endif
    for(i = 0; i < encoder->protected_->channels; i++) {
        encoder->private_->residual_workspace_unaligned[i][0] = encoder->private_->residual_workspace[i][0] = 0;
        encoder->private_->residual_workspace_unaligned[i][1] = encoder->private_->residual_workspace[i][1] = 0;
        encoder->private_->best_subframe[i] = 0;
    }
    for(i = 0; i < 2; i++) {
        encoder->private_->residual_workspace_mid_side_unaligned[i][0] = encoder->private_->residual_workspace_mid_side[i][0] = 0;
        encoder->private_->residual_workspace_mid_side_unaligned[i][1] = encoder->private_->residual_workspace_mid_side[i][1] = 0;
        encoder->private_->best_subframe_mid_side[i] = 0;
    }
    encoder->private_->abs_residual_partition_sums_unaligned = encoder->private_->abs_residual_partition_sums = 0;
    encoder->private_->raw_bits_per_partition_unaligned = encoder->private_->raw_bits_per_partition = 0;
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    encoder->private_->loose_mid_side_stereo_frames = (unsigned)((FLAC__double)encoder->protected_->sample_rate * 0.4 / (FLAC__double)encoder->protected_->blocksize + 0.5);
#else
    /* 26214 is the approximate fixed-point equivalent to 0.4 (0.4 * 2^16) */
    /* sample rate can be up to 655350 Hz, and thus use 20 bits, so we do the multiply&divide by hand */
    CHOC_ASSERT(FLAC__MAX_SAMPLE_RATE <= 655350);
    CHOC_ASSERT(FLAC__MAX_BLOCK_SIZE <= 65535);
    CHOC_ASSERT(encoder->protected_->sample_rate <= 655350);
    CHOC_ASSERT(encoder->protected_->blocksize <= 65535);
    encoder->private_->loose_mid_side_stereo_frames = (unsigned)FLAC__fixedpoint_trunc((((FLAC__uint64)(encoder->protected_->sample_rate) * (FLAC__uint64)(26214)) << 16) / (encoder->protected_->blocksize<<16) + FLAC__FP_ONE_HALF);
#endif
    if(encoder->private_->loose_mid_side_stereo_frames == 0)
        encoder->private_->loose_mid_side_stereo_frames = 1;
    encoder->private_->loose_mid_side_stereo_frame_count = 0;
    encoder->private_->current_sample_number = 0;
    encoder->private_->current_frame_number = 0;

    encoder->private_->use_wide_by_block = (encoder->protected_->bits_per_sample + FLAC__bitmath_ilog2(encoder->protected_->blocksize)+1 > 30);
    encoder->private_->use_wide_by_order = (encoder->protected_->bits_per_sample + FLAC__bitmath_ilog2(std::max(encoder->protected_->max_lpc_order, FLAC__MAX_FIXED_ORDER))+1 > 30); /*@@@ need to use this? */
    encoder->private_->use_wide_by_partition = (false); /*@@@ need to set this */

    /*
     * get the CPU info and set the function pointers
     */
    FLAC__cpu_info(&encoder->private_->cpuinfo);
    /* first default to the non-asm routines */
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation;
#endif
    encoder->private_->local_precompute_partition_info_sums = precompute_partition_info_sums_;
    encoder->private_->local_fixed_compute_best_predictor = FLAC__fixed_compute_best_predictor;
    encoder->private_->local_fixed_compute_best_predictor_wide = FLAC__fixed_compute_best_predictor_wide;
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    encoder->private_->local_lpc_compute_residual_from_qlp_coefficients = FLAC__lpc_compute_residual_from_qlp_coefficients;
    encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_64bit = FLAC__lpc_compute_residual_from_qlp_coefficients_wide;
    encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_16bit = FLAC__lpc_compute_residual_from_qlp_coefficients;
#endif
    /* now override with asm where appropriate */
#ifndef FLAC__INTEGER_ONLY_LIBRARY
# ifndef FLAC__NO_ASM
    if(encoder->private_->cpuinfo.use_asm) {
#  ifdef FLAC__CPU_IA32
        CHOC_ASSERT(encoder->private_->cpuinfo.type == FLAC__CPUINFO_TYPE_IA32);
#   ifdef FLAC__HAS_NASM
        if(encoder->private_->cpuinfo.ia32.sse) {
            if(encoder->protected_->max_lpc_order < 4)
                encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_asm_ia32_sse_lag_4;
            else if(encoder->protected_->max_lpc_order < 8)
                encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_asm_ia32_sse_lag_8;
            else if(encoder->protected_->max_lpc_order < 12)
                encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_asm_ia32_sse_lag_12;
            else if(encoder->protected_->max_lpc_order < 16)
                encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_asm_ia32_sse_lag_16;
            else
                encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_asm_ia32;
        }
        else
            encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_asm_ia32;

        encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_64bit = FLAC__lpc_compute_residual_from_qlp_coefficients_wide_asm_ia32; /* OPT_IA32: was really necessary for GCC < 4.9 */
        if(encoder->private_->cpuinfo.ia32.mmx) {
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients = FLAC__lpc_compute_residual_from_qlp_coefficients_asm_ia32;
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_16bit = FLAC__lpc_compute_residual_from_qlp_coefficients_asm_ia32_mmx;
        }
        else {
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients = FLAC__lpc_compute_residual_from_qlp_coefficients_asm_ia32;
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_16bit = FLAC__lpc_compute_residual_from_qlp_coefficients_asm_ia32;
        }

        if(encoder->private_->cpuinfo.ia32.mmx && encoder->private_->cpuinfo.ia32.cmov)
            encoder->private_->local_fixed_compute_best_predictor = FLAC__fixed_compute_best_predictor_asm_ia32_mmx_cmov;
#   endif /* FLAC__HAS_NASM */
#   ifdef FLAC__HAS_X86INTRIN
#    if defined FLAC__SSE_SUPPORTED
        if(encoder->private_->cpuinfo.ia32.sse) {
            if(encoder->protected_->max_lpc_order < 4)
                encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_intrin_sse_lag_4;
            else if(encoder->protected_->max_lpc_order < 8)
                encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_intrin_sse_lag_8;
            else if(encoder->protected_->max_lpc_order < 12)
                encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_intrin_sse_lag_12;
            else if(encoder->protected_->max_lpc_order < 16)
                encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_intrin_sse_lag_16;
            else
                encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation;
        }
#    endif

#    ifdef FLAC__SSE2_SUPPORTED
        if(encoder->private_->cpuinfo.ia32.sse2) {
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients       = FLAC__lpc_compute_residual_from_qlp_coefficients_intrin_sse2;
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_16bit = FLAC__lpc_compute_residual_from_qlp_coefficients_16_intrin_sse2;
        }
#    endif
#    ifdef FLAC__SSE4_1_SUPPORTED
        if(encoder->private_->cpuinfo.ia32.sse41) {
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients       = FLAC__lpc_compute_residual_from_qlp_coefficients_intrin_sse41;
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_64bit = FLAC__lpc_compute_residual_from_qlp_coefficients_wide_intrin_sse41;
        }
#    endif
#    ifdef FLAC__AVX2_SUPPORTED
        if(encoder->private_->cpuinfo.ia32.avx2) {
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_16bit = FLAC__lpc_compute_residual_from_qlp_coefficients_16_intrin_avx2;
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients       = FLAC__lpc_compute_residual_from_qlp_coefficients_intrin_avx2;
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_64bit = FLAC__lpc_compute_residual_from_qlp_coefficients_wide_intrin_avx2;
        }
#    endif

#    ifdef FLAC__SSE2_SUPPORTED
        if (encoder->private_->cpuinfo.ia32.sse2) {
            encoder->private_->local_fixed_compute_best_predictor      = FLAC__fixed_compute_best_predictor_intrin_sse2;
            encoder->private_->local_fixed_compute_best_predictor_wide = FLAC__fixed_compute_best_predictor_wide_intrin_sse2;
        }
#    endif
#    ifdef FLAC__SSSE3_SUPPORTED
        if (encoder->private_->cpuinfo.ia32.ssse3) {
            encoder->private_->local_fixed_compute_best_predictor      = FLAC__fixed_compute_best_predictor_intrin_ssse3;
            encoder->private_->local_fixed_compute_best_predictor_wide = FLAC__fixed_compute_best_predictor_wide_intrin_ssse3;
        }
#    endif
#   endif /* FLAC__HAS_X86INTRIN */
#  elif defined FLAC__CPU_X86_64
        CHOC_ASSERT(encoder->private_->cpuinfo.type == FLAC__CPUINFO_TYPE_X86_64);
#   ifdef FLAC__HAS_X86INTRIN
#    ifdef FLAC__SSE_SUPPORTED
        if(encoder->protected_->max_lpc_order < 4)
            encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_intrin_sse_lag_4;
        else if(encoder->protected_->max_lpc_order < 8)
            encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_intrin_sse_lag_8;
        else if(encoder->protected_->max_lpc_order < 12)
            encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_intrin_sse_lag_12;
        else if(encoder->protected_->max_lpc_order < 16)
            encoder->private_->local_lpc_compute_autocorrelation = FLAC__lpc_compute_autocorrelation_intrin_sse_lag_16;
#    endif

#    ifdef FLAC__SSE2_SUPPORTED
        encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_16bit = FLAC__lpc_compute_residual_from_qlp_coefficients_16_intrin_sse2;
#    endif
#    ifdef FLAC__SSE4_1_SUPPORTED
        if(encoder->private_->cpuinfo.x86.sse41) {
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients = FLAC__lpc_compute_residual_from_qlp_coefficients_intrin_sse41;
        }
#    endif
#    ifdef FLAC__AVX2_SUPPORTED
        if(encoder->private_->cpuinfo.x86.avx2) {
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_16bit = FLAC__lpc_compute_residual_from_qlp_coefficients_16_intrin_avx2;
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients       = FLAC__lpc_compute_residual_from_qlp_coefficients_intrin_avx2;
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_64bit = FLAC__lpc_compute_residual_from_qlp_coefficients_wide_intrin_avx2;
        }
#    endif

#    ifdef FLAC__SSE2_SUPPORTED
        encoder->private_->local_fixed_compute_best_predictor      = FLAC__fixed_compute_best_predictor_intrin_sse2;
        encoder->private_->local_fixed_compute_best_predictor_wide = FLAC__fixed_compute_best_predictor_wide_intrin_sse2;
#    endif
#    ifdef FLAC__SSSE3_SUPPORTED
        if (encoder->private_->cpuinfo.x86.ssse3) {
            encoder->private_->local_fixed_compute_best_predictor      = FLAC__fixed_compute_best_predictor_intrin_ssse3;
            encoder->private_->local_fixed_compute_best_predictor_wide = FLAC__fixed_compute_best_predictor_wide_intrin_ssse3;
        }
#    endif
#   endif /* FLAC__HAS_X86INTRIN */
#  endif /* FLAC__CPU_... */
    }
# endif /* !FLAC__NO_ASM */
#endif /* !FLAC__INTEGER_ONLY_LIBRARY */
#if !defined FLAC__NO_ASM && defined FLAC__HAS_X86INTRIN
    if(encoder->private_->cpuinfo.use_asm) {
# if defined FLAC__CPU_IA32
#  ifdef FLAC__SSE2_SUPPORTED
        if(encoder->private_->cpuinfo.ia32.sse2)
            encoder->private_->local_precompute_partition_info_sums = FLAC__precompute_partition_info_sums_intrin_sse2;
#  endif
#  ifdef FLAC__SSSE3_SUPPORTED
        if(encoder->private_->cpuinfo.ia32.ssse3)
            encoder->private_->local_precompute_partition_info_sums = FLAC__precompute_partition_info_sums_intrin_ssse3;
#  endif
#  ifdef FLAC__AVX2_SUPPORTED
        if(encoder->private_->cpuinfo.ia32.avx2)
            encoder->private_->local_precompute_partition_info_sums = FLAC__precompute_partition_info_sums_intrin_avx2;
#  endif
# elif defined FLAC__CPU_X86_64
#  ifdef FLAC__SSE2_SUPPORTED
        encoder->private_->local_precompute_partition_info_sums = FLAC__precompute_partition_info_sums_intrin_sse2;
#  endif
#  ifdef FLAC__SSSE3_SUPPORTED
        if(encoder->private_->cpuinfo.x86.ssse3)
            encoder->private_->local_precompute_partition_info_sums = FLAC__precompute_partition_info_sums_intrin_ssse3;
#  endif
#  ifdef FLAC__AVX2_SUPPORTED
        if(encoder->private_->cpuinfo.x86.avx2)
            encoder->private_->local_precompute_partition_info_sums = FLAC__precompute_partition_info_sums_intrin_avx2;
#  endif
# endif /* FLAC__CPU_... */
    }
#endif /* !FLAC__NO_ASM && FLAC__HAS_X86INTRIN */
    /* finally override based on wide-ness if necessary */
    if(encoder->private_->use_wide_by_block) {
        encoder->private_->local_fixed_compute_best_predictor = encoder->private_->local_fixed_compute_best_predictor_wide;
    }

    /* set state to OK; from here on, errors are fatal and we'll override the state then */
    encoder->protected_->state = FLAC__STREAM_ENCODER_OK;

#if FLAC__HAS_OGG
    encoder->private_->is_ogg = is_ogg;
    if(is_ogg && !FLAC__ogg_encoder_aspect_init(&encoder->protected_->ogg_encoder_aspect)) {
        encoder->protected_->state = FLAC__STREAM_ENCODER_OGG_ERROR;
        return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
    }
#endif

    encoder->private_->read_callback = read_callback;
    encoder->private_->write_callback = write_callback;
    encoder->private_->seek_callback = seek_callback;
    encoder->private_->tell_callback = tell_callback;
    encoder->private_->metadata_callback = metadata_callback;
    encoder->private_->client_data = client_data;

    if(!resize_buffers_(encoder, encoder->protected_->blocksize)) {
        /* the above function sets the state for us in case of an error */
        return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
    }

    if(!FLAC__bitwriter_init(encoder->private_->frame)) {
        encoder->protected_->state = FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR;
        return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
    }

    /*
     * Set up the verify stuff if necessary
     */
    if(encoder->protected_->verify) {
        /*
         * First, set up the fifo which will hold the
         * original signal to compare against
         */
        encoder->private_->verify.input_fifo.size = encoder->protected_->blocksize+OVERREAD_;
        for(i = 0; i < encoder->protected_->channels; i++) {
            if(0 == (encoder->private_->verify.input_fifo.data[i] = (FLAC__int32*) safe_malloc_mul_2op_p(sizeof(FLAC__int32), /*times*/encoder->private_->verify.input_fifo.size))) {
                encoder->protected_->state = FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR;
                return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
            }
        }
        encoder->private_->verify.input_fifo.tail = 0;

        /*
         * Now set up a stream decoder for verification
         */
        if(0 == encoder->private_->verify.decoder) {
            encoder->private_->verify.decoder = FLAC__stream_decoder_new();
            if(0 == encoder->private_->verify.decoder) {
                encoder->protected_->state = FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR;
                return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
            }
        }

        if(FLAC__stream_decoder_init_stream(encoder->private_->verify.decoder, verify_read_callback_, /*seek_callback=*/0, /*tell_callback=*/0, /*length_callback=*/0, /*eof_callback=*/0, verify_write_callback_, verify_metadata_callback_, verify_error_callback_, /*client_data=*/encoder) != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR;
            return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
        }
    }
    encoder->private_->verify.error_stats.absolute_sample = 0;
    encoder->private_->verify.error_stats.frame_number = 0;
    encoder->private_->verify.error_stats.channel = 0;
    encoder->private_->verify.error_stats.sample = 0;
    encoder->private_->verify.error_stats.expected = 0;
    encoder->private_->verify.error_stats.got = 0;

    /*
     * These must be done before we write any metadata, because that
     * calls the write_callback, which uses these values.
     */
    encoder->private_->first_seekpoint_to_check = 0;
    encoder->private_->samples_written = 0;
    encoder->protected_->streaminfo_offset = 0;
    encoder->protected_->seektable_offset = 0;
    encoder->protected_->audio_offset = 0;

    /*
     * write the stream header
     */
    if(encoder->protected_->verify)
        encoder->private_->verify.state_hint = ENCODER_IN_MAGIC;
    if(!FLAC__bitwriter_write_raw_uint32(encoder->private_->frame, FLAC__STREAM_SYNC, FLAC__STREAM_SYNC_LEN)) {
        encoder->protected_->state = FLAC__STREAM_ENCODER_FRAMING_ERROR;
        return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
    }
    if(!write_bitbuffer_(encoder, 0, /*is_last_block=*/false)) {
        /* the above function sets the state for us in case of an error */
        return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
    }

    /*
     * write the STREAMINFO metadata block
     */
    if(encoder->protected_->verify)
        encoder->private_->verify.state_hint = ENCODER_IN_METADATA;
    encoder->private_->streaminfo.type = FLAC__METADATA_TYPE_STREAMINFO;
    encoder->private_->streaminfo.is_last = false; /* we will have at a minimum a VORBIS_COMMENT afterwards */
    encoder->private_->streaminfo.length = FLAC__STREAM_METADATA_STREAMINFO_LENGTH;
    encoder->private_->streaminfo.data.stream_info.min_blocksize = encoder->protected_->blocksize; /* this encoder uses the same blocksize for the whole stream */
    encoder->private_->streaminfo.data.stream_info.max_blocksize = encoder->protected_->blocksize;
    encoder->private_->streaminfo.data.stream_info.min_framesize = 0; /* we don't know this yet; have to fill it in later */
    encoder->private_->streaminfo.data.stream_info.max_framesize = 0; /* we don't know this yet; have to fill it in later */
    encoder->private_->streaminfo.data.stream_info.sample_rate = encoder->protected_->sample_rate;
    encoder->private_->streaminfo.data.stream_info.channels = encoder->protected_->channels;
    encoder->private_->streaminfo.data.stream_info.bits_per_sample = encoder->protected_->bits_per_sample;
    encoder->private_->streaminfo.data.stream_info.total_samples = encoder->protected_->total_samples_estimate; /* we will replace this later with the real total */
    memset(encoder->private_->streaminfo.data.stream_info.md5sum, 0, 16); /* we don't know this yet; have to fill it in later */
    if(encoder->protected_->do_md5)
        FLAC__MD5Init(&encoder->private_->md5context);
    if(!FLAC__add_metadata_block(&encoder->private_->streaminfo, encoder->private_->frame)) {
        encoder->protected_->state = FLAC__STREAM_ENCODER_FRAMING_ERROR;
        return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
    }
    if(!write_bitbuffer_(encoder, 0, /*is_last_block=*/false)) {
        /* the above function sets the state for us in case of an error */
        return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
    }

    /*
     * Now that the STREAMINFO block is written, we can init this to an
     * absurdly-high value...
     */
    encoder->private_->streaminfo.data.stream_info.min_framesize = (1u << FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN) - 1;
    /* ... and clear this to 0 */
    encoder->private_->streaminfo.data.stream_info.total_samples = 0;

    /*
     * Check to see if the supplied metadata contains a VORBIS_COMMENT;
     * if not, we will write an empty one (FLAC__add_metadata_block()
     * automatically supplies the vendor string).
     *
     * WATCHOUT: the Ogg FLAC mapping requires us to write this block after
     * the STREAMINFO.  (In the case that metadata_has_vorbis_comment is
     * true it will have already insured that the metadata list is properly
     * ordered.)
     */
    if(!metadata_has_vorbis_comment) {
        FLAC__StreamMetadata vorbis_comment;
        vorbis_comment.type = FLAC__METADATA_TYPE_VORBIS_COMMENT;
        vorbis_comment.is_last = (encoder->protected_->num_metadata_blocks == 0);
        vorbis_comment.length = 4 + 4; /* MAGIC NUMBER */
        vorbis_comment.data.vorbis_comment.vendor_string.length = 0;
        vorbis_comment.data.vorbis_comment.vendor_string.entry = 0;
        vorbis_comment.data.vorbis_comment.num_comments = 0;
        vorbis_comment.data.vorbis_comment.comments = 0;
        if(!FLAC__add_metadata_block(&vorbis_comment, encoder->private_->frame)) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_FRAMING_ERROR;
            return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
        }
        if(!write_bitbuffer_(encoder, 0, /*is_last_block=*/false)) {
            /* the above function sets the state for us in case of an error */
            return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
        }
    }

    /*
     * write the user's metadata blocks
     */
    for(i = 0; i < encoder->protected_->num_metadata_blocks; i++) {
        encoder->protected_->metadata[i]->is_last = (i == encoder->protected_->num_metadata_blocks - 1);
        if(!FLAC__add_metadata_block(encoder->protected_->metadata[i], encoder->private_->frame)) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_FRAMING_ERROR;
            return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
        }
        if(!write_bitbuffer_(encoder, 0, /*is_last_block=*/false)) {
            /* the above function sets the state for us in case of an error */
            return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
        }
    }

    /* now that all the metadata is written, we save the stream offset */
    if(encoder->private_->tell_callback && encoder->private_->tell_callback(encoder, &encoder->protected_->audio_offset, encoder->private_->client_data) == FLAC__STREAM_ENCODER_TELL_STATUS_ERROR) { /* FLAC__STREAM_ENCODER_TELL_STATUS_UNSUPPORTED just means we didn't get the offset; no error */
        encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
        return FLAC__STREAM_ENCODER_INIT_STATUS_ENCODER_ERROR;
    }

    if(encoder->protected_->verify)
        encoder->private_->verify.state_hint = ENCODER_IN_AUDIO;

    return FLAC__STREAM_ENCODER_INIT_STATUS_OK;
}

FLAC_API FLAC__StreamEncoderInitStatus FLAC__stream_encoder_init_stream(
    FLAC__StreamEncoder *encoder,
    FLAC__StreamEncoderWriteCallback write_callback,
    FLAC__StreamEncoderSeekCallback seek_callback,
    FLAC__StreamEncoderTellCallback tell_callback,
    FLAC__StreamEncoderMetadataCallback metadata_callback,
    void *client_data
)
{
    return init_stream_internal_(
        encoder,
        /*read_callback=*/0,
        write_callback,
        seek_callback,
        tell_callback,
        metadata_callback,
        client_data,
        /*is_ogg=*/false
    );
}

FLAC_API FLAC__StreamEncoderInitStatus FLAC__stream_encoder_init_ogg_stream(
    FLAC__StreamEncoder *encoder,
    FLAC__StreamEncoderReadCallback read_callback,
    FLAC__StreamEncoderWriteCallback write_callback,
    FLAC__StreamEncoderSeekCallback seek_callback,
    FLAC__StreamEncoderTellCallback tell_callback,
    FLAC__StreamEncoderMetadataCallback metadata_callback,
    void *client_data
)
{
    return init_stream_internal_(
        encoder,
        read_callback,
        write_callback,
        seek_callback,
        tell_callback,
        metadata_callback,
        client_data,
        /*is_ogg=*/true
    );
}

FLAC_API FLAC__bool FLAC__stream_encoder_finish(FLAC__StreamEncoder *encoder)
{
    FLAC__bool error = false;

    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);

    if(encoder->protected_->state == FLAC__STREAM_ENCODER_UNINITIALIZED)
        return true;

    if(encoder->protected_->state == FLAC__STREAM_ENCODER_OK && !encoder->private_->is_being_deleted) {
        if(encoder->private_->current_sample_number != 0) {
            const FLAC__bool is_fractional_block = encoder->protected_->blocksize != encoder->private_->current_sample_number;
            encoder->protected_->blocksize = encoder->private_->current_sample_number;
            if(!process_frame_(encoder, is_fractional_block, /*is_last_block=*/true))
                error = true;
        }
    }

    if(encoder->protected_->do_md5)
        FLAC__MD5Final(encoder->private_->streaminfo.data.stream_info.md5sum, &encoder->private_->md5context);

    if(!encoder->private_->is_being_deleted) {
        if(encoder->protected_->state == FLAC__STREAM_ENCODER_OK) {
            if(encoder->private_->seek_callback) {
#if FLAC__HAS_OGG
                if(encoder->private_->is_ogg)
                    update_ogg_metadata_(encoder);
                else
#endif
                update_metadata_(encoder);

                /* check if an error occurred while updating metadata */
                if(encoder->protected_->state != FLAC__STREAM_ENCODER_OK)
                    error = true;
            }
            if(encoder->private_->metadata_callback)
                encoder->private_->metadata_callback(encoder, &encoder->private_->streaminfo, encoder->private_->client_data);
        }

        if(encoder->protected_->verify && 0 != encoder->private_->verify.decoder && !FLAC__stream_decoder_finish(encoder->private_->verify.decoder)) {
            if(!error)
                encoder->protected_->state = FLAC__STREAM_ENCODER_VERIFY_MISMATCH_IN_AUDIO_DATA;
            error = true;
        }
    }

    if(0 != encoder->private_->file) {
        if(encoder->private_->file != stdout)
            fclose(encoder->private_->file);
        encoder->private_->file = 0;
    }

#if FLAC__HAS_OGG
    if(encoder->private_->is_ogg)
        FLAC__ogg_encoder_aspect_finish(&encoder->protected_->ogg_encoder_aspect);
#endif

    free_(encoder);
    set_defaults_(encoder);

    if(!error)
        encoder->protected_->state = FLAC__STREAM_ENCODER_UNINITIALIZED;

    return !error;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_ogg_serial_number(FLAC__StreamEncoder *encoder, long value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
#if FLAC__HAS_OGG
    /* can't check encoder->private_->is_ogg since that's not set until init time */
    FLAC__ogg_encoder_aspect_set_serial_number(&encoder->protected_->ogg_encoder_aspect, value);
    return true;
#else
    (void)value;
    return false;
#endif
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_verify(FLAC__StreamEncoder *encoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
#ifndef FLAC__MANDATORY_VERIFY_WHILE_ENCODING
    encoder->protected_->verify = value;
#endif
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_streamable_subset(FLAC__StreamEncoder *encoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->streamable_subset = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_do_md5(FLAC__StreamEncoder *encoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->do_md5 = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_channels(FLAC__StreamEncoder *encoder, unsigned value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->channels = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_bits_per_sample(FLAC__StreamEncoder *encoder, unsigned value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->bits_per_sample = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_sample_rate(FLAC__StreamEncoder *encoder, unsigned value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->sample_rate = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_compression_level(FLAC__StreamEncoder *encoder, unsigned value)
{
    FLAC__bool ok = true;
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    if(value >= sizeof(compression_levels_)/sizeof(compression_levels_[0]))
        value = sizeof(compression_levels_)/sizeof(compression_levels_[0]) - 1;
    ok &= FLAC__stream_encoder_set_do_mid_side_stereo          (encoder, compression_levels_[value].do_mid_side_stereo);
    ok &= FLAC__stream_encoder_set_loose_mid_side_stereo       (encoder, compression_levels_[value].loose_mid_side_stereo);
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    ok &= FLAC__stream_encoder_set_apodization                 (encoder, compression_levels_[value].apodization);
#endif
    ok &= FLAC__stream_encoder_set_max_lpc_order               (encoder, compression_levels_[value].max_lpc_order);
    ok &= FLAC__stream_encoder_set_qlp_coeff_precision         (encoder, compression_levels_[value].qlp_coeff_precision);
    ok &= FLAC__stream_encoder_set_do_qlp_coeff_prec_search    (encoder, compression_levels_[value].do_qlp_coeff_prec_search);
    ok &= FLAC__stream_encoder_set_do_escape_coding            (encoder, compression_levels_[value].do_escape_coding);
    ok &= FLAC__stream_encoder_set_do_exhaustive_model_search  (encoder, compression_levels_[value].do_exhaustive_model_search);
    ok &= FLAC__stream_encoder_set_min_residual_partition_order(encoder, compression_levels_[value].min_residual_partition_order);
    ok &= FLAC__stream_encoder_set_max_residual_partition_order(encoder, compression_levels_[value].max_residual_partition_order);
    ok &= FLAC__stream_encoder_set_rice_parameter_search_dist  (encoder, compression_levels_[value].rice_parameter_search_dist);
    return ok;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_blocksize(FLAC__StreamEncoder *encoder, unsigned value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->blocksize = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_do_mid_side_stereo(FLAC__StreamEncoder *encoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->do_mid_side_stereo = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_loose_mid_side_stereo(FLAC__StreamEncoder *encoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->loose_mid_side_stereo = value;
    return true;
}

/*@@@@add to tests*/
FLAC_API FLAC__bool FLAC__stream_encoder_set_apodization(FLAC__StreamEncoder *encoder, const char *specification)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    CHOC_ASSERT(0 != specification);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
#ifdef FLAC__INTEGER_ONLY_LIBRARY
    (void)specification; /* silently ignore since we haven't integerized; will always use a rectangular window */
#else
    encoder->protected_->num_apodizations = 0;
    while(1) {
        const char *s = strchr(specification, ';');
        const size_t n = s? (size_t)(s - specification) : strlen(specification);
        if     (n==8  && 0 == strncmp("bartlett"     , specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_BARTLETT;
        else if(n==13 && 0 == strncmp("bartlett_hann", specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_BARTLETT_HANN;
        else if(n==8  && 0 == strncmp("blackman"     , specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_BLACKMAN;
        else if(n==26 && 0 == strncmp("blackman_harris_4term_92db", specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_BLACKMAN_HARRIS_4TERM_92DB_SIDELOBE;
        else if(n==6  && 0 == strncmp("connes"       , specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_CONNES;
        else if(n==7  && 0 == strncmp("flattop"      , specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_FLATTOP;
        else if(n>7   && 0 == strncmp("gauss("       , specification, 6)) {
            FLAC__real stddev = (FLAC__real)strtod(specification+6, 0);
            if (stddev > 0.0 && stddev <= 0.5) {
                encoder->protected_->apodizations[encoder->protected_->num_apodizations].parameters.gauss.stddev = stddev;
                encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_GAUSS;
            }
        }
        else if(n==7  && 0 == strncmp("hamming"      , specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_HAMMING;
        else if(n==4  && 0 == strncmp("hann"         , specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_HANN;
        else if(n==13 && 0 == strncmp("kaiser_bessel", specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_KAISER_BESSEL;
        else if(n==7  && 0 == strncmp("nuttall"      , specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_NUTTALL;
        else if(n==9  && 0 == strncmp("rectangle"    , specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_RECTANGLE;
        else if(n==8  && 0 == strncmp("triangle"     , specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_TRIANGLE;
        else if(n>7   && 0 == strncmp("tukey("       , specification, 6)) {
            FLAC__real p = (FLAC__real)strtod(specification+6, 0);
            if (p >= 0.0 && p <= 1.0) {
                encoder->protected_->apodizations[encoder->protected_->num_apodizations].parameters.tukey.p = p;
                encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_TUKEY;
            }
        }
        else if(n>15   && 0 == strncmp("partial_tukey("       , specification, 14)) {
            FLAC__int32 tukey_parts = (FLAC__int32)strtod(specification+14, 0);
            const char *si_1 = strchr(specification, '/');
            FLAC__real overlap = si_1?std::min((FLAC__real)strtod(si_1+1, 0),0.99f):0.1f;
            FLAC__real overlap_units = 1.0f/(1.0f - overlap) - 1.0f;
            const char *si_2 = strchr((si_1?(si_1+1):specification), '/');
            FLAC__real tukey_p = si_2?(FLAC__real)strtod(si_2+1, 0):0.2f;

            if (tukey_parts <= 1) {
                encoder->protected_->apodizations[encoder->protected_->num_apodizations].parameters.tukey.p = tukey_p;
                encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_TUKEY;
            }else if (encoder->protected_->num_apodizations + tukey_parts < 32){
                FLAC__int32 m;
                for(m = 0; m < tukey_parts; m++){
                    encoder->protected_->apodizations[encoder->protected_->num_apodizations].parameters.multiple_tukey.p = tukey_p;
                    encoder->protected_->apodizations[encoder->protected_->num_apodizations].parameters.multiple_tukey.start = m/(tukey_parts+overlap_units);
                    encoder->protected_->apodizations[encoder->protected_->num_apodizations].parameters.multiple_tukey.end = (m+1+overlap_units)/(tukey_parts+overlap_units);
                    encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_PARTIAL_TUKEY;
                }
            }
        }
        else if(n>16   && 0 == strncmp("punchout_tukey("       , specification, 15)) {
            FLAC__int32 tukey_parts = (FLAC__int32)strtod(specification+15, 0);
            const char *si_1 = strchr(specification, '/');
            FLAC__real overlap = si_1?std::min((FLAC__real)strtod(si_1+1, 0),0.99f):0.2f;
            FLAC__real overlap_units = 1.0f/(1.0f - overlap) - 1.0f;
            const char *si_2 = strchr((si_1?(si_1+1):specification), '/');
            FLAC__real tukey_p = si_2?(FLAC__real)strtod(si_2+1, 0):0.2f;

            if (tukey_parts <= 1) {
                encoder->protected_->apodizations[encoder->protected_->num_apodizations].parameters.tukey.p = tukey_p;
                encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_TUKEY;
            }else if (encoder->protected_->num_apodizations + tukey_parts < 32){
                FLAC__int32 m;
                for(m = 0; m < tukey_parts; m++){
                    encoder->protected_->apodizations[encoder->protected_->num_apodizations].parameters.multiple_tukey.p = tukey_p;
                    encoder->protected_->apodizations[encoder->protected_->num_apodizations].parameters.multiple_tukey.start = m/(tukey_parts+overlap_units);
                    encoder->protected_->apodizations[encoder->protected_->num_apodizations].parameters.multiple_tukey.end = (m+1+overlap_units)/(tukey_parts+overlap_units);
                    encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_PUNCHOUT_TUKEY;
                }
            }
        }
        else if(n==5  && 0 == strncmp("welch"        , specification, n))
            encoder->protected_->apodizations[encoder->protected_->num_apodizations++].type = FLAC__APODIZATION_WELCH;
        if (encoder->protected_->num_apodizations == 32)
            break;
        if (s)
            specification = s+1;
        else
            break;
    }
    if(encoder->protected_->num_apodizations == 0) {
        encoder->protected_->num_apodizations = 1;
        encoder->protected_->apodizations[0].type = FLAC__APODIZATION_TUKEY;
        encoder->protected_->apodizations[0].parameters.tukey.p = 0.5;
    }
#endif
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_max_lpc_order(FLAC__StreamEncoder *encoder, unsigned value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->max_lpc_order = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_qlp_coeff_precision(FLAC__StreamEncoder *encoder, unsigned value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->qlp_coeff_precision = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_do_qlp_coeff_prec_search(FLAC__StreamEncoder *encoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->do_qlp_coeff_prec_search = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_do_escape_coding(FLAC__StreamEncoder *encoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    (void)value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_do_exhaustive_model_search(FLAC__StreamEncoder *encoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->do_exhaustive_model_search = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_min_residual_partition_order(FLAC__StreamEncoder *encoder, unsigned value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->min_residual_partition_order = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_max_residual_partition_order(FLAC__StreamEncoder *encoder, unsigned value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->max_residual_partition_order = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_rice_parameter_search_dist(FLAC__StreamEncoder *encoder, unsigned value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    (void)value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_total_samples_estimate(FLAC__StreamEncoder *encoder, FLAC__uint64 value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->protected_->total_samples_estimate = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_set_metadata(FLAC__StreamEncoder *encoder, FLAC__StreamMetadata **metadata, unsigned num_blocks)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    if(0 == metadata)
        num_blocks = 0;
    if(0 == num_blocks)
        metadata = 0;
    /* realloc() does not do exactly what we want so... */
    if(encoder->protected_->metadata) {
        free(encoder->protected_->metadata);
        encoder->protected_->metadata = 0;
        encoder->protected_->num_metadata_blocks = 0;
    }
    if(num_blocks) {
        FLAC__StreamMetadata **m;
        if(0 == (m = (FLAC__StreamMetadata**) safe_malloc_mul_2op_p(sizeof(m[0]), /*times*/num_blocks)))
            return false;
        memcpy(m, metadata, sizeof(m[0]) * num_blocks);
        encoder->protected_->metadata = m;
        encoder->protected_->num_metadata_blocks = num_blocks;
    }
#if FLAC__HAS_OGG
    if(!FLAC__ogg_encoder_aspect_set_num_metadata(&encoder->protected_->ogg_encoder_aspect, num_blocks))
        return false;
#endif
    return true;
}

/*
 * These three functions are not static, but not publically exposed in
 * include/FLAC/ either.  They are used by the test suite.
 */
FLAC_API FLAC__bool FLAC__stream_encoder_disable_constant_subframes(FLAC__StreamEncoder *encoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->private_->disable_constant_subframes = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_disable_fixed_subframes(FLAC__StreamEncoder *encoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->private_->disable_fixed_subframes = value;
    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_disable_verbatim_subframes(FLAC__StreamEncoder *encoder, FLAC__bool value)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_UNINITIALIZED)
        return false;
    encoder->private_->disable_verbatim_subframes = value;
    return true;
}

FLAC_API FLAC__StreamEncoderState FLAC__stream_encoder_get_state(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->state;
}

FLAC_API FLAC__StreamDecoderState FLAC__stream_encoder_get_verify_decoder_state(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->verify)
        return FLAC__stream_decoder_get_state(encoder->private_->verify.decoder);
    else
        return FLAC__STREAM_DECODER_UNINITIALIZED;
}

FLAC_API const char *FLAC__stream_encoder_get_resolved_state_string(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(encoder->protected_->state != FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR)
        return FLAC__StreamEncoderStateString[encoder->protected_->state];
    else
        return FLAC__stream_decoder_get_resolved_state_string(encoder->private_->verify.decoder);
}

FLAC_API void FLAC__stream_encoder_get_verify_decoder_error_stats(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_sample, unsigned *frame_number, unsigned *channel, unsigned *sample, FLAC__int32 *expected, FLAC__int32 *got)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    if(0 != absolute_sample)
        *absolute_sample = encoder->private_->verify.error_stats.absolute_sample;
    if(0 != frame_number)
        *frame_number = encoder->private_->verify.error_stats.frame_number;
    if(0 != channel)
        *channel = encoder->private_->verify.error_stats.channel;
    if(0 != sample)
        *sample = encoder->private_->verify.error_stats.sample;
    if(0 != expected)
        *expected = encoder->private_->verify.error_stats.expected;
    if(0 != got)
        *got = encoder->private_->verify.error_stats.got;
}

FLAC_API FLAC__bool FLAC__stream_encoder_get_verify(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->verify;
}

FLAC_API FLAC__bool FLAC__stream_encoder_get_streamable_subset(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->streamable_subset;
}

FLAC_API FLAC__bool FLAC__stream_encoder_get_do_md5(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->do_md5;
}

FLAC_API unsigned FLAC__stream_encoder_get_channels(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->channels;
}

FLAC_API unsigned FLAC__stream_encoder_get_bits_per_sample(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->bits_per_sample;
}

FLAC_API unsigned FLAC__stream_encoder_get_sample_rate(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->sample_rate;
}

FLAC_API unsigned FLAC__stream_encoder_get_blocksize(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->blocksize;
}

FLAC_API FLAC__bool FLAC__stream_encoder_get_do_mid_side_stereo(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->do_mid_side_stereo;
}

FLAC_API FLAC__bool FLAC__stream_encoder_get_loose_mid_side_stereo(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->loose_mid_side_stereo;
}

FLAC_API unsigned FLAC__stream_encoder_get_max_lpc_order(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->max_lpc_order;
}

FLAC_API unsigned FLAC__stream_encoder_get_qlp_coeff_precision(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->qlp_coeff_precision;
}

FLAC_API FLAC__bool FLAC__stream_encoder_get_do_qlp_coeff_prec_search(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->do_qlp_coeff_prec_search;
}

FLAC_API FLAC__bool FLAC__stream_encoder_get_do_escape_coding(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->do_escape_coding;
}

FLAC_API FLAC__bool FLAC__stream_encoder_get_do_exhaustive_model_search(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->do_exhaustive_model_search;
}

FLAC_API unsigned FLAC__stream_encoder_get_min_residual_partition_order(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->min_residual_partition_order;
}

FLAC_API unsigned FLAC__stream_encoder_get_max_residual_partition_order(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->max_residual_partition_order;
}

FLAC_API unsigned FLAC__stream_encoder_get_rice_parameter_search_dist(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->rice_parameter_search_dist;
}

FLAC_API FLAC__uint64 FLAC__stream_encoder_get_total_samples_estimate(const FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    return encoder->protected_->total_samples_estimate;
}

FLAC_API FLAC__bool FLAC__stream_encoder_process(FLAC__StreamEncoder *encoder, const FLAC__int32 * const buffer[], unsigned samples)
{
    unsigned i, j = 0, channel;
    const unsigned channels = encoder->protected_->channels, blocksize = encoder->protected_->blocksize;

    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    CHOC_ASSERT(encoder->protected_->state == FLAC__STREAM_ENCODER_OK);

    do {
        const unsigned n = std::min(blocksize+OVERREAD_-encoder->private_->current_sample_number, samples-j);

        if(encoder->protected_->verify)
            append_to_verify_fifo_(&encoder->private_->verify.input_fifo, buffer, j, channels, n);

        for(channel = 0; channel < channels; channel++)
            memcpy(&encoder->private_->integer_signal[channel][encoder->private_->current_sample_number], &buffer[channel][j], sizeof(buffer[channel][0]) * n);

        if(encoder->protected_->do_mid_side_stereo) {
            CHOC_ASSERT(channels == 2);
            /* "i <= blocksize" to overread 1 sample; see comment in OVERREAD_ decl */
            for(i = encoder->private_->current_sample_number; i <= blocksize && j < samples; i++, j++) {
                encoder->private_->integer_signal_mid_side[1][i] = buffer[0][j] - buffer[1][j];
                encoder->private_->integer_signal_mid_side[0][i] = (buffer[0][j] + buffer[1][j]) >> 1; /* NOTE: not the same as 'mid = (buffer[0][j] + buffer[1][j]) / 2' ! */
            }
        }
        else
            j += n;

        encoder->private_->current_sample_number += n;

        /* we only process if we have a full block + 1 extra sample; final block is always handled by FLAC__stream_encoder_finish() */
        if(encoder->private_->current_sample_number > blocksize) {
            CHOC_ASSERT(encoder->private_->current_sample_number == blocksize+OVERREAD_);
            CHOC_ASSERT(OVERREAD_ == 1); /* assert we only overread 1 sample which simplifies the rest of the code below */
            if(!process_frame_(encoder, /*is_fractional_block=*/false, /*is_last_block=*/false))
                return false;
            /* move unprocessed overread samples to beginnings of arrays */
            for(channel = 0; channel < channels; channel++)
                encoder->private_->integer_signal[channel][0] = encoder->private_->integer_signal[channel][blocksize];
            if(encoder->protected_->do_mid_side_stereo) {
                encoder->private_->integer_signal_mid_side[0][0] = encoder->private_->integer_signal_mid_side[0][blocksize];
                encoder->private_->integer_signal_mid_side[1][0] = encoder->private_->integer_signal_mid_side[1][blocksize];
            }
            encoder->private_->current_sample_number = 1;
        }
    } while(j < samples);

    return true;
}

FLAC_API FLAC__bool FLAC__stream_encoder_process_interleaved(FLAC__StreamEncoder *encoder, const FLAC__int32 buffer[], unsigned samples)
{
    unsigned i, j, k, channel;
    FLAC__int32 x, mid, side;
    const unsigned channels = encoder->protected_->channels, blocksize = encoder->protected_->blocksize;

    CHOC_ASSERT(0 != encoder);
    CHOC_ASSERT(0 != encoder->private_);
    CHOC_ASSERT(0 != encoder->protected_);
    CHOC_ASSERT(encoder->protected_->state == FLAC__STREAM_ENCODER_OK);

    j = k = 0;
    /*
     * we have several flavors of the same basic loop, optimized for
     * different conditions:
     */
    if(encoder->protected_->do_mid_side_stereo && channels == 2) {
        /*
         * stereo coding: unroll channel loop
         */
        do {
            if(encoder->protected_->verify)
                append_to_verify_fifo_interleaved_(&encoder->private_->verify.input_fifo, buffer, j, channels, std::min(blocksize+OVERREAD_-encoder->private_->current_sample_number, samples-j));

            /* "i <= blocksize" to overread 1 sample; see comment in OVERREAD_ decl */
            for(i = encoder->private_->current_sample_number; i <= blocksize && j < samples; i++, j++) {
                encoder->private_->integer_signal[0][i] = mid = side = buffer[k++];
                x = buffer[k++];
                encoder->private_->integer_signal[1][i] = x;
                mid += x;
                side -= x;
                mid >>= 1; /* NOTE: not the same as 'mid = (left + right) / 2' ! */
                encoder->private_->integer_signal_mid_side[1][i] = side;
                encoder->private_->integer_signal_mid_side[0][i] = mid;
            }
            encoder->private_->current_sample_number = i;
            /* we only process if we have a full block + 1 extra sample; final block is always handled by FLAC__stream_encoder_finish() */
            if(i > blocksize) {
                if(!process_frame_(encoder, /*is_fractional_block=*/false, /*is_last_block=*/false))
                    return false;
                /* move unprocessed overread samples to beginnings of arrays */
                CHOC_ASSERT(i == blocksize+OVERREAD_);
                CHOC_ASSERT(OVERREAD_ == 1); /* assert we only overread 1 sample which simplifies the rest of the code below */
                encoder->private_->integer_signal[0][0] = encoder->private_->integer_signal[0][blocksize];
                encoder->private_->integer_signal[1][0] = encoder->private_->integer_signal[1][blocksize];
                encoder->private_->integer_signal_mid_side[0][0] = encoder->private_->integer_signal_mid_side[0][blocksize];
                encoder->private_->integer_signal_mid_side[1][0] = encoder->private_->integer_signal_mid_side[1][blocksize];
                encoder->private_->current_sample_number = 1;
            }
        } while(j < samples);
    }
    else {
        /*
         * independent channel coding: buffer each channel in inner loop
         */
        do {
            if(encoder->protected_->verify)
                append_to_verify_fifo_interleaved_(&encoder->private_->verify.input_fifo, buffer, j, channels, std::min(blocksize+OVERREAD_-encoder->private_->current_sample_number, samples-j));

            /* "i <= blocksize" to overread 1 sample; see comment in OVERREAD_ decl */
            for(i = encoder->private_->current_sample_number; i <= blocksize && j < samples; i++, j++) {
                for(channel = 0; channel < channels; channel++)
                    encoder->private_->integer_signal[channel][i] = buffer[k++];
            }
            encoder->private_->current_sample_number = i;
            /* we only process if we have a full block + 1 extra sample; final block is always handled by FLAC__stream_encoder_finish() */
            if(i > blocksize) {
                if(!process_frame_(encoder, /*is_fractional_block=*/false, /*is_last_block=*/false))
                    return false;
                /* move unprocessed overread samples to beginnings of arrays */
                CHOC_ASSERT(i == blocksize+OVERREAD_);
                CHOC_ASSERT(OVERREAD_ == 1); /* assert we only overread 1 sample which simplifies the rest of the code below */
                for(channel = 0; channel < channels; channel++)
                    encoder->private_->integer_signal[channel][0] = encoder->private_->integer_signal[channel][blocksize];
                encoder->private_->current_sample_number = 1;
            }
        } while(j < samples);
    }

    return true;
}

/***********************************************************************
 *
 * Private class methods
 *
 ***********************************************************************/

void set_defaults_(FLAC__StreamEncoder *encoder)
{
    CHOC_ASSERT(0 != encoder);

#ifdef FLAC__MANDATORY_VERIFY_WHILE_ENCODING
    encoder->protected_->verify = true;
#else
    encoder->protected_->verify = false;
#endif
    encoder->protected_->streamable_subset = true;
    encoder->protected_->do_md5 = true;
    encoder->protected_->do_mid_side_stereo = false;
    encoder->protected_->loose_mid_side_stereo = false;
    encoder->protected_->channels = 2;
    encoder->protected_->bits_per_sample = 16;
    encoder->protected_->sample_rate = 44100;
    encoder->protected_->blocksize = 0;
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    encoder->protected_->num_apodizations = 1;
    encoder->protected_->apodizations[0].type = FLAC__APODIZATION_TUKEY;
    encoder->protected_->apodizations[0].parameters.tukey.p = 0.5;
#endif
    encoder->protected_->max_lpc_order = 0;
    encoder->protected_->qlp_coeff_precision = 0;
    encoder->protected_->do_qlp_coeff_prec_search = false;
    encoder->protected_->do_exhaustive_model_search = false;
    encoder->protected_->do_escape_coding = false;
    encoder->protected_->min_residual_partition_order = 0;
    encoder->protected_->max_residual_partition_order = 0;
    encoder->protected_->rice_parameter_search_dist = 0;
    encoder->protected_->total_samples_estimate = 0;
    encoder->protected_->metadata = 0;
    encoder->protected_->num_metadata_blocks = 0;

    encoder->private_->seek_table = 0;
    encoder->private_->disable_constant_subframes = false;
    encoder->private_->disable_fixed_subframes = false;
    encoder->private_->disable_verbatim_subframes = false;
#if FLAC__HAS_OGG
    encoder->private_->is_ogg = false;
#endif
    encoder->private_->read_callback = 0;
    encoder->private_->write_callback = 0;
    encoder->private_->seek_callback = 0;
    encoder->private_->tell_callback = 0;
    encoder->private_->metadata_callback = 0;
    encoder->private_->progress_callback = 0;
    encoder->private_->client_data = 0;

#if FLAC__HAS_OGG
    FLAC__ogg_encoder_aspect_set_defaults(&encoder->protected_->ogg_encoder_aspect);
#endif

    FLAC__stream_encoder_set_compression_level(encoder, 5);
}

void free_(FLAC__StreamEncoder *encoder)
{
    unsigned i, channel;

    CHOC_ASSERT(0 != encoder);
    if(encoder->protected_->metadata) {
        free(encoder->protected_->metadata);
        encoder->protected_->metadata = 0;
        encoder->protected_->num_metadata_blocks = 0;
    }
    for(i = 0; i < encoder->protected_->channels; i++) {
        if(0 != encoder->private_->integer_signal_unaligned[i]) {
            free(encoder->private_->integer_signal_unaligned[i]);
            encoder->private_->integer_signal_unaligned[i] = 0;
        }
#ifndef FLAC__INTEGER_ONLY_LIBRARY
        if(0 != encoder->private_->real_signal_unaligned[i]) {
            free(encoder->private_->real_signal_unaligned[i]);
            encoder->private_->real_signal_unaligned[i] = 0;
        }
#endif
    }
    for(i = 0; i < 2; i++) {
        if(0 != encoder->private_->integer_signal_mid_side_unaligned[i]) {
            free(encoder->private_->integer_signal_mid_side_unaligned[i]);
            encoder->private_->integer_signal_mid_side_unaligned[i] = 0;
        }
#ifndef FLAC__INTEGER_ONLY_LIBRARY
        if(0 != encoder->private_->real_signal_mid_side_unaligned[i]) {
            free(encoder->private_->real_signal_mid_side_unaligned[i]);
            encoder->private_->real_signal_mid_side_unaligned[i] = 0;
        }
#endif
    }
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    for(i = 0; i < encoder->protected_->num_apodizations; i++) {
        if(0 != encoder->private_->window_unaligned[i]) {
            free(encoder->private_->window_unaligned[i]);
            encoder->private_->window_unaligned[i] = 0;
        }
    }
    if(0 != encoder->private_->windowed_signal_unaligned) {
        free(encoder->private_->windowed_signal_unaligned);
        encoder->private_->windowed_signal_unaligned = 0;
    }
#endif
    for(channel = 0; channel < encoder->protected_->channels; channel++) {
        for(i = 0; i < 2; i++) {
            if(0 != encoder->private_->residual_workspace_unaligned[channel][i]) {
                free(encoder->private_->residual_workspace_unaligned[channel][i]);
                encoder->private_->residual_workspace_unaligned[channel][i] = 0;
            }
        }
    }
    for(channel = 0; channel < 2; channel++) {
        for(i = 0; i < 2; i++) {
            if(0 != encoder->private_->residual_workspace_mid_side_unaligned[channel][i]) {
                free(encoder->private_->residual_workspace_mid_side_unaligned[channel][i]);
                encoder->private_->residual_workspace_mid_side_unaligned[channel][i] = 0;
            }
        }
    }
    if(0 != encoder->private_->abs_residual_partition_sums_unaligned) {
        free(encoder->private_->abs_residual_partition_sums_unaligned);
        encoder->private_->abs_residual_partition_sums_unaligned = 0;
    }
    if(0 != encoder->private_->raw_bits_per_partition_unaligned) {
        free(encoder->private_->raw_bits_per_partition_unaligned);
        encoder->private_->raw_bits_per_partition_unaligned = 0;
    }
    if(encoder->protected_->verify) {
        for(i = 0; i < encoder->protected_->channels; i++) {
            if(0 != encoder->private_->verify.input_fifo.data[i]) {
                free(encoder->private_->verify.input_fifo.data[i]);
                encoder->private_->verify.input_fifo.data[i] = 0;
            }
        }
    }
    FLAC__bitwriter_free(encoder->private_->frame);
}

FLAC__bool resize_buffers_(FLAC__StreamEncoder *encoder, unsigned new_blocksize)
{
    FLAC__bool ok;
    unsigned i, channel;

    CHOC_ASSERT(new_blocksize > 0);
    CHOC_ASSERT(encoder->protected_->state == FLAC__STREAM_ENCODER_OK);
    CHOC_ASSERT(encoder->private_->current_sample_number == 0);

    /* To avoid excessive malloc'ing, we only grow the buffer; no shrinking. */
    if(new_blocksize <= encoder->private_->input_capacity)
        return true;

    ok = true;

    /* WATCHOUT: FLAC__lpc_compute_residual_from_qlp_coefficients_asm_ia32_mmx() and ..._intrin_sse2()
     * require that the input arrays (in our case the integer signals)
     * have a buffer of up to 3 zeroes in front (at negative indices) for
     * alignment purposes; we use 4 in front to keep the data well-aligned.
     */

    for(i = 0; ok && i < encoder->protected_->channels; i++) {
        ok = ok && FLAC__memory_alloc_aligned_int32_array(new_blocksize+4+OVERREAD_, &encoder->private_->integer_signal_unaligned[i], &encoder->private_->integer_signal[i]);
        memset(encoder->private_->integer_signal[i], 0, sizeof(FLAC__int32)*4);
        encoder->private_->integer_signal[i] += 4;
#ifndef FLAC__INTEGER_ONLY_LIBRARY
#endif
    }
    for(i = 0; ok && i < 2; i++) {
        ok = ok && FLAC__memory_alloc_aligned_int32_array(new_blocksize+4+OVERREAD_, &encoder->private_->integer_signal_mid_side_unaligned[i], &encoder->private_->integer_signal_mid_side[i]);
        memset(encoder->private_->integer_signal_mid_side[i], 0, sizeof(FLAC__int32)*4);
        encoder->private_->integer_signal_mid_side[i] += 4;
#ifndef FLAC__INTEGER_ONLY_LIBRARY
#endif
    }
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    if(ok && encoder->protected_->max_lpc_order > 0) {
        for(i = 0; ok && i < encoder->protected_->num_apodizations; i++)
            ok = ok && FLAC__memory_alloc_aligned_real_array(new_blocksize, &encoder->private_->window_unaligned[i], &encoder->private_->window[i]);
        ok = ok && FLAC__memory_alloc_aligned_real_array(new_blocksize, &encoder->private_->windowed_signal_unaligned, &encoder->private_->windowed_signal);
    }
#endif
    for(channel = 0; ok && channel < encoder->protected_->channels; channel++) {
        for(i = 0; ok && i < 2; i++) {
            ok = ok && FLAC__memory_alloc_aligned_int32_array(new_blocksize, &encoder->private_->residual_workspace_unaligned[channel][i], &encoder->private_->residual_workspace[channel][i]);
        }
    }
    for(channel = 0; ok && channel < 2; channel++) {
        for(i = 0; ok && i < 2; i++) {
            ok = ok && FLAC__memory_alloc_aligned_int32_array(new_blocksize, &encoder->private_->residual_workspace_mid_side_unaligned[channel][i], &encoder->private_->residual_workspace_mid_side[channel][i]);
        }
    }
    /* the *2 is an approximation to the series 1 + 1/2 + 1/4 + ... that sums tree occupies in a flat array */
    /*@@@ new_blocksize*2 is too pessimistic, but to fix, we need smarter logic because a smaller new_blocksize can actually increase the # of partitions; would require moving this out into a separate function, then checking its capacity against the need of the current blocksize&min/max_partition_order (and maybe predictor order) */
    ok = ok && FLAC__memory_alloc_aligned_uint64_array(new_blocksize * 2, &encoder->private_->abs_residual_partition_sums_unaligned, &encoder->private_->abs_residual_partition_sums);
    if(encoder->protected_->do_escape_coding)
        ok = ok && FLAC__memory_alloc_aligned_unsigned_array(new_blocksize * 2, &encoder->private_->raw_bits_per_partition_unaligned, &encoder->private_->raw_bits_per_partition);

    /* now adjust the windows if the blocksize has changed */
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    if(ok && new_blocksize != encoder->private_->input_capacity && encoder->protected_->max_lpc_order > 0) {
        for(i = 0; ok && i < encoder->protected_->num_apodizations; i++) {
            switch(encoder->protected_->apodizations[i].type) {
                case FLAC__APODIZATION_BARTLETT:
                    FLAC__window_bartlett(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_BARTLETT_HANN:
                    FLAC__window_bartlett_hann(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_BLACKMAN:
                    FLAC__window_blackman(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_BLACKMAN_HARRIS_4TERM_92DB_SIDELOBE:
                    FLAC__window_blackman_harris_4term_92db_sidelobe(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_CONNES:
                    FLAC__window_connes(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_FLATTOP:
                    FLAC__window_flattop(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_GAUSS:
                    FLAC__window_gauss(encoder->private_->window[i], new_blocksize, encoder->protected_->apodizations[i].parameters.gauss.stddev);
                    break;
                case FLAC__APODIZATION_HAMMING:
                    FLAC__window_hamming(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_HANN:
                    FLAC__window_hann(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_KAISER_BESSEL:
                    FLAC__window_kaiser_bessel(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_NUTTALL:
                    FLAC__window_nuttall(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_RECTANGLE:
                    FLAC__window_rectangle(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_TRIANGLE:
                    FLAC__window_triangle(encoder->private_->window[i], new_blocksize);
                    break;
                case FLAC__APODIZATION_TUKEY:
                    FLAC__window_tukey(encoder->private_->window[i], new_blocksize, encoder->protected_->apodizations[i].parameters.tukey.p);
                    break;
                case FLAC__APODIZATION_PARTIAL_TUKEY:
                    FLAC__window_partial_tukey(encoder->private_->window[i], new_blocksize, encoder->protected_->apodizations[i].parameters.multiple_tukey.p, encoder->protected_->apodizations[i].parameters.multiple_tukey.start, encoder->protected_->apodizations[i].parameters.multiple_tukey.end);
                    break;
                case FLAC__APODIZATION_PUNCHOUT_TUKEY:
                    FLAC__window_punchout_tukey(encoder->private_->window[i], new_blocksize, encoder->protected_->apodizations[i].parameters.multiple_tukey.p, encoder->protected_->apodizations[i].parameters.multiple_tukey.start, encoder->protected_->apodizations[i].parameters.multiple_tukey.end);
                    break;
                case FLAC__APODIZATION_WELCH:
                    FLAC__window_welch(encoder->private_->window[i], new_blocksize);
                    break;
                default:
                    CHOC_ASSERT(0);
                    /* double protection */
                    FLAC__window_hann(encoder->private_->window[i], new_blocksize);
                    break;
            }
        }
    }
#endif

    if(ok)
        encoder->private_->input_capacity = new_blocksize;
    else
        encoder->protected_->state = FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR;

    return ok;
}

FLAC__bool write_bitbuffer_(FLAC__StreamEncoder *encoder, unsigned samples, FLAC__bool is_last_block)
{
    const FLAC__byte *buffer;
    size_t bytes;

    CHOC_ASSERT(FLAC__bitwriter_is_byte_aligned(encoder->private_->frame));

    if(!FLAC__bitwriter_get_buffer(encoder->private_->frame, &buffer, &bytes)) {
        encoder->protected_->state = FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR;
        return false;
    }

    if(encoder->protected_->verify) {
        encoder->private_->verify.output.data = buffer;
        encoder->private_->verify.output.bytes = bytes;
        if(encoder->private_->verify.state_hint == ENCODER_IN_MAGIC) {
            encoder->private_->verify.needs_magic_hack = true;
        }
        else {
            if(!FLAC__stream_decoder_process_single(encoder->private_->verify.decoder)) {
                FLAC__bitwriter_release_buffer(encoder->private_->frame);
                FLAC__bitwriter_clear(encoder->private_->frame);
                if(encoder->protected_->state != FLAC__STREAM_ENCODER_VERIFY_MISMATCH_IN_AUDIO_DATA)
                    encoder->protected_->state = FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR;
                return false;
            }
        }
    }

    if(write_frame_(encoder, buffer, bytes, samples, is_last_block) != FLAC__STREAM_ENCODER_WRITE_STATUS_OK) {
        FLAC__bitwriter_release_buffer(encoder->private_->frame);
        FLAC__bitwriter_clear(encoder->private_->frame);
        encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
        return false;
    }

    FLAC__bitwriter_release_buffer(encoder->private_->frame);
    FLAC__bitwriter_clear(encoder->private_->frame);

    if(samples > 0) {
        encoder->private_->streaminfo.data.stream_info.min_framesize = std::min(bytes, (size_t) encoder->private_->streaminfo.data.stream_info.min_framesize);
        encoder->private_->streaminfo.data.stream_info.max_framesize = std::max(bytes, (size_t) encoder->private_->streaminfo.data.stream_info.max_framesize);
    }

    return true;
}

FLAC__StreamEncoderWriteStatus write_frame_(FLAC__StreamEncoder *encoder, const FLAC__byte buffer[], size_t bytes, unsigned samples, FLAC__bool is_last_block)
{
    FLAC__StreamEncoderWriteStatus status;
    FLAC__uint64 output_position = 0;

#if FLAC__HAS_OGG == 0
    (void)is_last_block;
#endif

    /* FLAC__STREAM_ENCODER_TELL_STATUS_UNSUPPORTED just means we didn't get the offset; no error */
    if(encoder->private_->tell_callback && encoder->private_->tell_callback(encoder, &output_position, encoder->private_->client_data) == FLAC__STREAM_ENCODER_TELL_STATUS_ERROR) {
        encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
        return FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
    }

    /*
     * Watch for the STREAMINFO block and first SEEKTABLE block to go by and store their offsets.
     */
    if(samples == 0) {
        FLAC__MetadataType type = (FLAC__MetadataType) (buffer[0] & 0x7f);
        if(type == FLAC__METADATA_TYPE_STREAMINFO)
            encoder->protected_->streaminfo_offset = output_position;
        else if(type == FLAC__METADATA_TYPE_SEEKTABLE && encoder->protected_->seektable_offset == 0)
            encoder->protected_->seektable_offset = output_position;
    }

    /*
     * Mark the current seek point if hit (if audio_offset == 0 that
     * means we're still writing metadata and haven't hit the first
     * frame yet)
     */
    if(0 != encoder->private_->seek_table && encoder->protected_->audio_offset > 0 && encoder->private_->seek_table->num_points > 0) {
        const unsigned blocksize = FLAC__stream_encoder_get_blocksize(encoder);
        const FLAC__uint64 frame_first_sample = encoder->private_->samples_written;
        const FLAC__uint64 frame_last_sample = frame_first_sample + (FLAC__uint64)blocksize - 1;
        FLAC__uint64 test_sample;
        unsigned i;
        for(i = encoder->private_->first_seekpoint_to_check; i < encoder->private_->seek_table->num_points; i++) {
            test_sample = encoder->private_->seek_table->points[i].sample_number;
            if(test_sample > frame_last_sample) {
                break;
            }
            else if(test_sample >= frame_first_sample) {
                encoder->private_->seek_table->points[i].sample_number = frame_first_sample;
                encoder->private_->seek_table->points[i].stream_offset = output_position - encoder->protected_->audio_offset;
                encoder->private_->seek_table->points[i].frame_samples = blocksize;
                encoder->private_->first_seekpoint_to_check++;
                /* DO NOT: "break;" and here's why:
                 * The seektable template may contain more than one target
                 * sample for any given frame; we will keep looping, generating
                 * duplicate seekpoints for them, and we'll clean it up later,
                 * just before writing the seektable back to the metadata.
                 */
            }
            else {
                encoder->private_->first_seekpoint_to_check++;
            }
        }
    }

#if FLAC__HAS_OGG
    if(encoder->private_->is_ogg) {
        status = FLAC__ogg_encoder_aspect_write_callback_wrapper(
            &encoder->protected_->ogg_encoder_aspect,
            buffer,
            bytes,
            samples,
            encoder->private_->current_frame_number,
            is_last_block,
            (FLAC__OggEncoderAspectWriteCallbackProxy)encoder->private_->write_callback,
            encoder,
            encoder->private_->client_data
        );
    }
    else
#endif
    status = encoder->private_->write_callback(encoder, buffer, bytes, samples, encoder->private_->current_frame_number, encoder->private_->client_data);

    if(status == FLAC__STREAM_ENCODER_WRITE_STATUS_OK) {
        encoder->private_->bytes_written += bytes;
        encoder->private_->samples_written += samples;
        /* we keep a high watermark on the number of frames written because
         * when the encoder goes back to write metadata, 'current_frame'
         * will drop back to 0.
         */
        encoder->private_->frames_written = std::max(encoder->private_->frames_written, encoder->private_->current_frame_number+1);
    }
    else
        encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;

    return status;
}

/* Gets called when the encoding process has finished so that we can update the STREAMINFO and SEEKTABLE blocks.  */
void update_metadata_(const FLAC__StreamEncoder *encoder)
{
    FLAC__byte b[FLAC__STREAM_METADATA_SEEKPOINT_LENGTH];
    const FLAC__StreamMetadata *metadata = &encoder->private_->streaminfo;
    const FLAC__uint64 samples = metadata->data.stream_info.total_samples;
    const unsigned min_framesize = metadata->data.stream_info.min_framesize;
    const unsigned max_framesize = metadata->data.stream_info.max_framesize;
    const unsigned bps = metadata->data.stream_info.bits_per_sample;
    FLAC__StreamEncoderSeekStatus seek_status;

    CHOC_ASSERT(metadata->type == FLAC__METADATA_TYPE_STREAMINFO);

    /* All this is based on intimate knowledge of the stream header
     * layout, but a change to the header format that would break this
     * would also break all streams encoded in the previous format.
     */

    /*
     * Write MD5 signature
     */
    {
        const unsigned md5_offset =
            FLAC__STREAM_METADATA_HEADER_LENGTH +
            (
                FLAC__STREAM_METADATA_STREAMINFO_MIN_BLOCK_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MAX_FRAME_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_SAMPLE_RATE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_TOTAL_SAMPLES_LEN
            ) / 8;

        if((seek_status = encoder->private_->seek_callback(encoder, encoder->protected_->streaminfo_offset + md5_offset, encoder->private_->client_data)) != FLAC__STREAM_ENCODER_SEEK_STATUS_OK) {
            if(seek_status == FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR)
                encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
            return;
        }
        if(encoder->private_->write_callback(encoder, metadata->data.stream_info.md5sum, 16, 0, 0, encoder->private_->client_data) != FLAC__STREAM_ENCODER_WRITE_STATUS_OK) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
            return;
        }
    }

    /*
     * Write total samples
     */
    {
        const unsigned total_samples_byte_offset =
            FLAC__STREAM_METADATA_HEADER_LENGTH +
            (
                FLAC__STREAM_METADATA_STREAMINFO_MIN_BLOCK_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MAX_FRAME_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_SAMPLE_RATE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN
                - 4
            ) / 8;

        b[0] = ((FLAC__byte)(bps-1) << 4) | (FLAC__byte)((samples >> 32) & 0x0F);
        b[1] = (FLAC__byte)((samples >> 24) & 0xFF);
        b[2] = (FLAC__byte)((samples >> 16) & 0xFF);
        b[3] = (FLAC__byte)((samples >> 8) & 0xFF);
        b[4] = (FLAC__byte)(samples & 0xFF);
        if((seek_status = encoder->private_->seek_callback(encoder, encoder->protected_->streaminfo_offset + total_samples_byte_offset, encoder->private_->client_data)) != FLAC__STREAM_ENCODER_SEEK_STATUS_OK) {
            if(seek_status == FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR)
                encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
            return;
        }
        if(encoder->private_->write_callback(encoder, b, 5, 0, 0, encoder->private_->client_data) != FLAC__STREAM_ENCODER_WRITE_STATUS_OK) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
            return;
        }
    }

    /*
     * Write min/max framesize
     */
    {
        const unsigned min_framesize_offset =
            FLAC__STREAM_METADATA_HEADER_LENGTH +
            (
                FLAC__STREAM_METADATA_STREAMINFO_MIN_BLOCK_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN
            ) / 8;

        b[0] = (FLAC__byte)((min_framesize >> 16) & 0xFF);
        b[1] = (FLAC__byte)((min_framesize >> 8) & 0xFF);
        b[2] = (FLAC__byte)(min_framesize & 0xFF);
        b[3] = (FLAC__byte)((max_framesize >> 16) & 0xFF);
        b[4] = (FLAC__byte)((max_framesize >> 8) & 0xFF);
        b[5] = (FLAC__byte)(max_framesize & 0xFF);
        if((seek_status = encoder->private_->seek_callback(encoder, encoder->protected_->streaminfo_offset + min_framesize_offset, encoder->private_->client_data)) != FLAC__STREAM_ENCODER_SEEK_STATUS_OK) {
            if(seek_status == FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR)
                encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
            return;
        }
        if(encoder->private_->write_callback(encoder, b, 6, 0, 0, encoder->private_->client_data) != FLAC__STREAM_ENCODER_WRITE_STATUS_OK) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
            return;
        }
    }

    /*
     * Write seektable
     */
    if(0 != encoder->private_->seek_table && encoder->private_->seek_table->num_points > 0 && encoder->protected_->seektable_offset > 0) {
        unsigned i;

        FLAC__format_seektable_sort(encoder->private_->seek_table);

        CHOC_ASSERT(FLAC__format_seektable_is_legal(encoder->private_->seek_table));

        if((seek_status = encoder->private_->seek_callback(encoder, encoder->protected_->seektable_offset + FLAC__STREAM_METADATA_HEADER_LENGTH, encoder->private_->client_data)) != FLAC__STREAM_ENCODER_SEEK_STATUS_OK) {
            if(seek_status == FLAC__STREAM_ENCODER_SEEK_STATUS_ERROR)
                encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
            return;
        }

        for(i = 0; i < encoder->private_->seek_table->num_points; i++) {
            FLAC__uint64 xx;
            unsigned x;
            xx = encoder->private_->seek_table->points[i].sample_number;
            b[7] = (FLAC__byte)xx; xx >>= 8;
            b[6] = (FLAC__byte)xx; xx >>= 8;
            b[5] = (FLAC__byte)xx; xx >>= 8;
            b[4] = (FLAC__byte)xx; xx >>= 8;
            b[3] = (FLAC__byte)xx; xx >>= 8;
            b[2] = (FLAC__byte)xx; xx >>= 8;
            b[1] = (FLAC__byte)xx; xx >>= 8;
            b[0] = (FLAC__byte)xx; //xx >>= 8;
            xx = encoder->private_->seek_table->points[i].stream_offset;
            b[15] = (FLAC__byte)xx; xx >>= 8;
            b[14] = (FLAC__byte)xx; xx >>= 8;
            b[13] = (FLAC__byte)xx; xx >>= 8;
            b[12] = (FLAC__byte)xx; xx >>= 8;
            b[11] = (FLAC__byte)xx; xx >>= 8;
            b[10] = (FLAC__byte)xx; xx >>= 8;
            b[9] = (FLAC__byte)xx; xx >>= 8;
            b[8] = (FLAC__byte)xx; //xx >>= 8;
            x = encoder->private_->seek_table->points[i].frame_samples;
            b[17] = (FLAC__byte)x; x >>= 8;
            b[16] = (FLAC__byte)x; //x >>= 8;
            if(encoder->private_->write_callback(encoder, b, 18, 0, 0, encoder->private_->client_data) != FLAC__STREAM_ENCODER_WRITE_STATUS_OK) {
                encoder->protected_->state = FLAC__STREAM_ENCODER_CLIENT_ERROR;
                return;
            }
        }
    }
}

#if FLAC__HAS_OGG
/* Gets called when the encoding process has finished so that we can update the STREAMINFO and SEEKTABLE blocks.  */
void update_ogg_metadata_(FLAC__StreamEncoder *encoder)
{
    /* the # of bytes in the 1st packet that precede the STREAMINFO */
    static const unsigned FIRST_OGG_PACKET_STREAMINFO_PREFIX_LENGTH =
        FLAC__OGG_MAPPING_PACKET_TYPE_LENGTH +
        FLAC__OGG_MAPPING_MAGIC_LENGTH +
        FLAC__OGG_MAPPING_VERSION_MAJOR_LENGTH +
        FLAC__OGG_MAPPING_VERSION_MINOR_LENGTH +
        FLAC__OGG_MAPPING_NUM_HEADERS_LENGTH +
        FLAC__STREAM_SYNC_LENGTH
    ;
    FLAC__byte b[std::max(6u, FLAC__STREAM_METADATA_SEEKPOINT_LENGTH)];
    const FLAC__StreamMetadata *metadata = &encoder->private_->streaminfo;
    const FLAC__uint64 samples = metadata->data.stream_info.total_samples;
    const unsigned min_framesize = metadata->data.stream_info.min_framesize;
    const unsigned max_framesize = metadata->data.stream_info.max_framesize;
    ogg_page page;

    CHOC_ASSERT(metadata->type == FLAC__METADATA_TYPE_STREAMINFO);
    CHOC_ASSERT(0 != encoder->private_->seek_callback);

    /* Pre-check that client supports seeking, since we don't want the
     * ogg_helper code to ever have to deal with this condition.
     */
    if(encoder->private_->seek_callback(encoder, 0, encoder->private_->client_data) == FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED)
        return;

    /* All this is based on intimate knowledge of the stream header
     * layout, but a change to the header format that would break this
     * would also break all streams encoded in the previous format.
     */

    /**
     ** Write STREAMINFO stats
     **/
    simple_ogg_page__init(&page);
    if(!simple_ogg_page__get_at(encoder, encoder->protected_->streaminfo_offset, &page, encoder->private_->seek_callback, encoder->private_->read_callback, encoder->private_->client_data)) {
        simple_ogg_page__clear(&page);
        return; /* state already set */
    }

    /*
     * Write MD5 signature
     */
    {
        const unsigned md5_offset =
            FIRST_OGG_PACKET_STREAMINFO_PREFIX_LENGTH +
            FLAC__STREAM_METADATA_HEADER_LENGTH +
            (
                FLAC__STREAM_METADATA_STREAMINFO_MIN_BLOCK_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MAX_FRAME_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_SAMPLE_RATE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_TOTAL_SAMPLES_LEN
            ) / 8;

        if(md5_offset + 16 > (unsigned)page.body_len) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_OGG_ERROR;
            simple_ogg_page__clear(&page);
            return;
        }
        memcpy(page.body + md5_offset, metadata->data.stream_info.md5sum, 16);
    }

    /*
     * Write total samples
     */
    {
        const unsigned total_samples_byte_offset =
            FIRST_OGG_PACKET_STREAMINFO_PREFIX_LENGTH +
            FLAC__STREAM_METADATA_HEADER_LENGTH +
            (
                FLAC__STREAM_METADATA_STREAMINFO_MIN_BLOCK_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MAX_FRAME_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_SAMPLE_RATE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN
                - 4
            ) / 8;

        if(total_samples_byte_offset + 5 > (unsigned)page.body_len) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_OGG_ERROR;
            simple_ogg_page__clear(&page);
            return;
        }
        b[0] = (FLAC__byte)page.body[total_samples_byte_offset] & 0xF0;
        b[0] |= (FLAC__byte)((samples >> 32) & 0x0F);
        b[1] = (FLAC__byte)((samples >> 24) & 0xFF);
        b[2] = (FLAC__byte)((samples >> 16) & 0xFF);
        b[3] = (FLAC__byte)((samples >> 8) & 0xFF);
        b[4] = (FLAC__byte)(samples & 0xFF);
        memcpy(page.body + total_samples_byte_offset, b, 5);
    }

    /*
     * Write min/max framesize
     */
    {
        const unsigned min_framesize_offset =
            FIRST_OGG_PACKET_STREAMINFO_PREFIX_LENGTH +
            FLAC__STREAM_METADATA_HEADER_LENGTH +
            (
                FLAC__STREAM_METADATA_STREAMINFO_MIN_BLOCK_SIZE_LEN +
                FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN
            ) / 8;

        if(min_framesize_offset + 6 > (unsigned)page.body_len) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_OGG_ERROR;
            simple_ogg_page__clear(&page);
            return;
        }
        b[0] = (FLAC__byte)((min_framesize >> 16) & 0xFF);
        b[1] = (FLAC__byte)((min_framesize >> 8) & 0xFF);
        b[2] = (FLAC__byte)(min_framesize & 0xFF);
        b[3] = (FLAC__byte)((max_framesize >> 16) & 0xFF);
        b[4] = (FLAC__byte)((max_framesize >> 8) & 0xFF);
        b[5] = (FLAC__byte)(max_framesize & 0xFF);
        memcpy(page.body + min_framesize_offset, b, 6);
    }
    if(!simple_ogg_page__set_at(encoder, encoder->protected_->streaminfo_offset, &page, encoder->private_->seek_callback, encoder->private_->write_callback, encoder->private_->client_data)) {
        simple_ogg_page__clear(&page);
        return; /* state already set */
    }
    simple_ogg_page__clear(&page);

    /*
     * Write seektable
     */
    if(0 != encoder->private_->seek_table && encoder->private_->seek_table->num_points > 0 && encoder->protected_->seektable_offset > 0) {
        unsigned i;
        FLAC__byte *p;

        FLAC__format_seektable_sort(encoder->private_->seek_table);

        CHOC_ASSERT(FLAC__format_seektable_is_legal(encoder->private_->seek_table));

        simple_ogg_page__init(&page);
        if(!simple_ogg_page__get_at(encoder, encoder->protected_->seektable_offset, &page, encoder->private_->seek_callback, encoder->private_->read_callback, encoder->private_->client_data)) {
            simple_ogg_page__clear(&page);
            return; /* state already set */
        }

        if((FLAC__STREAM_METADATA_HEADER_LENGTH + 18*encoder->private_->seek_table->num_points) != (unsigned)page.body_len) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_OGG_ERROR;
            simple_ogg_page__clear(&page);
            return;
        }

        for(i = 0, p = page.body + FLAC__STREAM_METADATA_HEADER_LENGTH; i < encoder->private_->seek_table->num_points; i++, p += 18) {
            FLAC__uint64 xx;
            unsigned x;
            xx = encoder->private_->seek_table->points[i].sample_number;
            b[7] = (FLAC__byte)xx; xx >>= 8;
            b[6] = (FLAC__byte)xx; xx >>= 8;
            b[5] = (FLAC__byte)xx; xx >>= 8;
            b[4] = (FLAC__byte)xx; xx >>= 8;
            b[3] = (FLAC__byte)xx; xx >>= 8;
            b[2] = (FLAC__byte)xx; xx >>= 8;
            b[1] = (FLAC__byte)xx; xx >>= 8;
            b[0] = (FLAC__byte)xx; xx >>= 8;
            xx = encoder->private_->seek_table->points[i].stream_offset;
            b[15] = (FLAC__byte)xx; xx >>= 8;
            b[14] = (FLAC__byte)xx; xx >>= 8;
            b[13] = (FLAC__byte)xx; xx >>= 8;
            b[12] = (FLAC__byte)xx; xx >>= 8;
            b[11] = (FLAC__byte)xx; xx >>= 8;
            b[10] = (FLAC__byte)xx; xx >>= 8;
            b[9] = (FLAC__byte)xx; xx >>= 8;
            b[8] = (FLAC__byte)xx; xx >>= 8;
            x = encoder->private_->seek_table->points[i].frame_samples;
            b[17] = (FLAC__byte)x; x >>= 8;
            b[16] = (FLAC__byte)x; x >>= 8;
            memcpy(p, b, 18);
        }

        if(!simple_ogg_page__set_at(encoder, encoder->protected_->seektable_offset, &page, encoder->private_->seek_callback, encoder->private_->write_callback, encoder->private_->client_data)) {
            simple_ogg_page__clear(&page);
            return; /* state already set */
        }
        simple_ogg_page__clear(&page);
    }
}
#endif

FLAC__bool process_frame_(FLAC__StreamEncoder *encoder, FLAC__bool is_fractional_block, FLAC__bool is_last_block)
{
    FLAC__uint16 crc;
    CHOC_ASSERT(encoder->protected_->state == FLAC__STREAM_ENCODER_OK);

    /*
     * Accumulate raw signal to the MD5 signature
     */
    if(encoder->protected_->do_md5 && !FLAC__MD5Accumulate(&encoder->private_->md5context, (const FLAC__int32 * const *)encoder->private_->integer_signal, encoder->protected_->channels, encoder->protected_->blocksize, (encoder->protected_->bits_per_sample+7) / 8)) {
        encoder->protected_->state = FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR;
        return false;
    }

    /*
     * Process the frame header and subframes into the frame bitbuffer
     */
    if(!process_subframes_(encoder, is_fractional_block)) {
        /* the above function sets the state for us in case of an error */
        return false;
    }

    /*
     * Zero-pad the frame to a byte_boundary
     */
    if(!FLAC__bitwriter_zero_pad_to_byte_boundary(encoder->private_->frame)) {
        encoder->protected_->state = FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR;
        return false;
    }

    /*
     * CRC-16 the whole thing
     */
    CHOC_ASSERT(FLAC__bitwriter_is_byte_aligned(encoder->private_->frame));
    if(
        !FLAC__bitwriter_get_write_crc16(encoder->private_->frame, &crc) ||
        !FLAC__bitwriter_write_raw_uint32(encoder->private_->frame, crc, FLAC__FRAME_FOOTER_CRC_LEN)
    ) {
        encoder->protected_->state = FLAC__STREAM_ENCODER_MEMORY_ALLOCATION_ERROR;
        return false;
    }

    /*
     * Write it
     */
    if(!write_bitbuffer_(encoder, encoder->protected_->blocksize, is_last_block)) {
        /* the above function sets the state for us in case of an error */
        return false;
    }

    /*
     * Get ready for the next frame
     */
    encoder->private_->current_sample_number = 0;
    encoder->private_->current_frame_number++;
    encoder->private_->streaminfo.data.stream_info.total_samples += (FLAC__uint64)encoder->protected_->blocksize;

    return true;
}

FLAC__bool process_subframes_(FLAC__StreamEncoder *encoder, FLAC__bool is_fractional_block)
{
    FLAC__FrameHeader frame_header;
    unsigned channel, min_partition_order = encoder->protected_->min_residual_partition_order, max_partition_order;
    FLAC__bool do_independent, do_mid_side;

    /*
     * Calculate the min,max Rice partition orders
     */
    if(is_fractional_block) {
        max_partition_order = 0;
    }
    else {
        max_partition_order = FLAC__format_get_max_rice_partition_order_from_blocksize(encoder->protected_->blocksize);
        max_partition_order = std::min(max_partition_order, encoder->protected_->max_residual_partition_order);
    }
    min_partition_order = std::min(min_partition_order, max_partition_order);

    /*
     * Setup the frame
     */
    frame_header.blocksize = encoder->protected_->blocksize;
    frame_header.sample_rate = encoder->protected_->sample_rate;
    frame_header.channels = encoder->protected_->channels;
    frame_header.channel_assignment = FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT; /* the default unless the encoder determines otherwise */
    frame_header.bits_per_sample = encoder->protected_->bits_per_sample;
    frame_header.number_type = FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER;
    frame_header.number.frame_number = encoder->private_->current_frame_number;

    /*
     * Figure out what channel assignments to try
     */
    if(encoder->protected_->do_mid_side_stereo) {
        if(encoder->protected_->loose_mid_side_stereo) {
            if(encoder->private_->loose_mid_side_stereo_frame_count == 0) {
                do_independent = true;
                do_mid_side = true;
            }
            else {
                do_independent = (encoder->private_->last_channel_assignment == FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT);
                do_mid_side = !do_independent;
            }
        }
        else {
            do_independent = true;
            do_mid_side = true;
        }
    }
    else {
        do_independent = true;
        do_mid_side = false;
    }

    CHOC_ASSERT(do_independent || do_mid_side);

    /*
     * Check for wasted bits; set effective bps for each subframe
     */
    if(do_independent) {
        for(channel = 0; channel < encoder->protected_->channels; channel++) {
            const unsigned w = get_wasted_bits_(encoder->private_->integer_signal[channel], encoder->protected_->blocksize);
            encoder->private_->subframe_workspace[channel][0].wasted_bits = encoder->private_->subframe_workspace[channel][1].wasted_bits = w;
            encoder->private_->subframe_bps[channel] = encoder->protected_->bits_per_sample - w;
        }
    }
    if(do_mid_side) {
        CHOC_ASSERT(encoder->protected_->channels == 2);
        for(channel = 0; channel < 2; channel++) {
            const unsigned w = get_wasted_bits_(encoder->private_->integer_signal_mid_side[channel], encoder->protected_->blocksize);
            encoder->private_->subframe_workspace_mid_side[channel][0].wasted_bits = encoder->private_->subframe_workspace_mid_side[channel][1].wasted_bits = w;
            encoder->private_->subframe_bps_mid_side[channel] = encoder->protected_->bits_per_sample - w + (channel==0? 0:1);
        }
    }

    /*
     * First do a normal encoding pass of each independent channel
     */
    if(do_independent) {
        for(channel = 0; channel < encoder->protected_->channels; channel++) {
            if(!
                process_subframe_(
                    encoder,
                    min_partition_order,
                    max_partition_order,
                    &frame_header,
                    encoder->private_->subframe_bps[channel],
                    encoder->private_->integer_signal[channel],
                    encoder->private_->subframe_workspace_ptr[channel],
                    encoder->private_->partitioned_rice_contents_workspace_ptr[channel],
                    encoder->private_->residual_workspace[channel],
                    encoder->private_->best_subframe+channel,
                    encoder->private_->best_subframe_bits+channel
                )
            )
                return false;
        }
    }

    /*
     * Now do mid and side channels if requested
     */
    if(do_mid_side) {
        CHOC_ASSERT(encoder->protected_->channels == 2);

        for(channel = 0; channel < 2; channel++) {
            if(!
                process_subframe_(
                    encoder,
                    min_partition_order,
                    max_partition_order,
                    &frame_header,
                    encoder->private_->subframe_bps_mid_side[channel],
                    encoder->private_->integer_signal_mid_side[channel],
                    encoder->private_->subframe_workspace_ptr_mid_side[channel],
                    encoder->private_->partitioned_rice_contents_workspace_ptr_mid_side[channel],
                    encoder->private_->residual_workspace_mid_side[channel],
                    encoder->private_->best_subframe_mid_side+channel,
                    encoder->private_->best_subframe_bits_mid_side+channel
                )
            )
                return false;
        }
    }

    /*
     * Compose the frame bitbuffer
     */
    if(do_mid_side) {
        unsigned left_bps = 0, right_bps = 0; /* initialized only to prevent superfluous compiler warning */
        FLAC__Subframe *left_subframe = 0, *right_subframe = 0; /* initialized only to prevent superfluous compiler warning */
        FLAC__ChannelAssignment channel_assignment;

        CHOC_ASSERT(encoder->protected_->channels == 2);

        if(encoder->protected_->loose_mid_side_stereo && encoder->private_->loose_mid_side_stereo_frame_count > 0) {
            channel_assignment = (encoder->private_->last_channel_assignment == FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT? FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT : FLAC__CHANNEL_ASSIGNMENT_MID_SIDE);
        }
        else {
            unsigned bits[4]; /* WATCHOUT - indexed by FLAC__ChannelAssignment */
            unsigned min_bits;
            int ca;

            CHOC_ASSERT(FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT == 0);
            CHOC_ASSERT(FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE   == 1);
            CHOC_ASSERT(FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE  == 2);
            CHOC_ASSERT(FLAC__CHANNEL_ASSIGNMENT_MID_SIDE    == 3);
            CHOC_ASSERT(do_independent && do_mid_side);

            /* We have to figure out which channel assignent results in the smallest frame */
            bits[FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT] = encoder->private_->best_subframe_bits         [0] + encoder->private_->best_subframe_bits         [1];
            bits[FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE  ] = encoder->private_->best_subframe_bits         [0] + encoder->private_->best_subframe_bits_mid_side[1];
            bits[FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE ] = encoder->private_->best_subframe_bits         [1] + encoder->private_->best_subframe_bits_mid_side[1];
            bits[FLAC__CHANNEL_ASSIGNMENT_MID_SIDE   ] = encoder->private_->best_subframe_bits_mid_side[0] + encoder->private_->best_subframe_bits_mid_side[1];

            channel_assignment = FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT;
            min_bits = bits[channel_assignment];
            for(ca = 1; ca <= 3; ca++) {
                if(bits[ca] < min_bits) {
                    min_bits = bits[ca];
                    channel_assignment = (FLAC__ChannelAssignment)ca;
                }
            }
        }

        frame_header.channel_assignment = channel_assignment;

        if(!FLAC__frame_add_header(&frame_header, encoder->private_->frame)) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_FRAMING_ERROR;
            return false;
        }

        switch(channel_assignment) {
            case FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT:
                left_subframe  = &encoder->private_->subframe_workspace         [0][encoder->private_->best_subframe         [0]];
                right_subframe = &encoder->private_->subframe_workspace         [1][encoder->private_->best_subframe         [1]];
                break;
            case FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE:
                left_subframe  = &encoder->private_->subframe_workspace         [0][encoder->private_->best_subframe         [0]];
                right_subframe = &encoder->private_->subframe_workspace_mid_side[1][encoder->private_->best_subframe_mid_side[1]];
                break;
            case FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE:
                left_subframe  = &encoder->private_->subframe_workspace_mid_side[1][encoder->private_->best_subframe_mid_side[1]];
                right_subframe = &encoder->private_->subframe_workspace         [1][encoder->private_->best_subframe         [1]];
                break;
            case FLAC__CHANNEL_ASSIGNMENT_MID_SIDE:
                left_subframe  = &encoder->private_->subframe_workspace_mid_side[0][encoder->private_->best_subframe_mid_side[0]];
                right_subframe = &encoder->private_->subframe_workspace_mid_side[1][encoder->private_->best_subframe_mid_side[1]];
                break;
            default:
                CHOC_ASSERT(0);
        }

        switch(channel_assignment) {
            case FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT:
                left_bps  = encoder->private_->subframe_bps         [0];
                right_bps = encoder->private_->subframe_bps         [1];
                break;
            case FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE:
                left_bps  = encoder->private_->subframe_bps         [0];
                right_bps = encoder->private_->subframe_bps_mid_side[1];
                break;
            case FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE:
                left_bps  = encoder->private_->subframe_bps_mid_side[1];
                right_bps = encoder->private_->subframe_bps         [1];
                break;
            case FLAC__CHANNEL_ASSIGNMENT_MID_SIDE:
                left_bps  = encoder->private_->subframe_bps_mid_side[0];
                right_bps = encoder->private_->subframe_bps_mid_side[1];
                break;
            default:
                CHOC_ASSERT(0);
        }

        /* note that encoder_add_subframe_ sets the state for us in case of an error */
        if(!add_subframe_(encoder, frame_header.blocksize, left_bps , left_subframe , encoder->private_->frame))
            return false;
        if(!add_subframe_(encoder, frame_header.blocksize, right_bps, right_subframe, encoder->private_->frame))
            return false;
    }
    else {
        if(!FLAC__frame_add_header(&frame_header, encoder->private_->frame)) {
            encoder->protected_->state = FLAC__STREAM_ENCODER_FRAMING_ERROR;
            return false;
        }

        for(channel = 0; channel < encoder->protected_->channels; channel++) {
            if(!add_subframe_(encoder, frame_header.blocksize, encoder->private_->subframe_bps[channel], &encoder->private_->subframe_workspace[channel][encoder->private_->best_subframe[channel]], encoder->private_->frame)) {
                /* the above function sets the state for us in case of an error */
                return false;
            }
        }
    }

    if(encoder->protected_->loose_mid_side_stereo) {
        encoder->private_->loose_mid_side_stereo_frame_count++;
        if(encoder->private_->loose_mid_side_stereo_frame_count >= encoder->private_->loose_mid_side_stereo_frames)
            encoder->private_->loose_mid_side_stereo_frame_count = 0;
    }

    encoder->private_->last_channel_assignment = frame_header.channel_assignment;

    return true;
}

FLAC__bool process_subframe_(
    FLAC__StreamEncoder *encoder,
    unsigned min_partition_order,
    unsigned max_partition_order,
    const FLAC__FrameHeader *frame_header,
    unsigned subframe_bps,
    const FLAC__int32 integer_signal[],
    FLAC__Subframe *subframe[2],
    FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents[2],
    FLAC__int32 *residual[2],
    unsigned *best_subframe,
    unsigned *best_bits
)
{
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    FLAC__float fixed_residual_bits_per_sample[FLAC__MAX_FIXED_ORDER+1];
#else
    FLAC__fixedpoint fixed_residual_bits_per_sample[FLAC__MAX_FIXED_ORDER+1];
#endif
#ifndef FLAC__INTEGER_ONLY_LIBRARY
    FLAC__double lpc_residual_bits_per_sample;
    FLAC__real autoc[FLAC__MAX_LPC_ORDER+1]; /* WATCHOUT: the size is important even though encoder->protected_->max_lpc_order might be less; some asm and x86 intrinsic routines need all the space */
    FLAC__double lpc_error[FLAC__MAX_LPC_ORDER];
    unsigned min_lpc_order, max_lpc_order, lpc_order;
    unsigned min_qlp_coeff_precision, max_qlp_coeff_precision, qlp_coeff_precision;
#endif
    unsigned min_fixed_order, max_fixed_order, guess_fixed_order, fixed_order;
    unsigned rice_parameter;
    unsigned _candidate_bits, _best_bits;
    unsigned _best_subframe;
    /* only use RICE2 partitions if stream bps > 16 */
    const unsigned rice_parameter_limit = FLAC__stream_encoder_get_bits_per_sample(encoder) > 16? FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_ESCAPE_PARAMETER : FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ESCAPE_PARAMETER;

    CHOC_ASSERT(frame_header->blocksize > 0);

    /* verbatim subframe is the baseline against which we measure other compressed subframes */
    _best_subframe = 0;
    if(encoder->private_->disable_verbatim_subframes && frame_header->blocksize >= FLAC__MAX_FIXED_ORDER)
        _best_bits = UINT_MAX;
    else
        _best_bits = evaluate_verbatim_subframe_(encoder, integer_signal, frame_header->blocksize, subframe_bps, subframe[_best_subframe]);

    if(frame_header->blocksize >= FLAC__MAX_FIXED_ORDER) {
        unsigned signal_is_constant = false;
        guess_fixed_order = encoder->private_->local_fixed_compute_best_predictor(integer_signal+FLAC__MAX_FIXED_ORDER, frame_header->blocksize-FLAC__MAX_FIXED_ORDER, fixed_residual_bits_per_sample);
        /* check for constant subframe */
        if(
            !encoder->private_->disable_constant_subframes &&
#ifndef FLAC__INTEGER_ONLY_LIBRARY
            fixed_residual_bits_per_sample[1] == 0.0
#else
            fixed_residual_bits_per_sample[1] == FLAC__FP_ZERO
#endif
        ) {
            /* the above means it's possible all samples are the same value; now double-check it: */
            unsigned i;
            signal_is_constant = true;
            for(i = 1; i < frame_header->blocksize; i++) {
                if(integer_signal[0] != integer_signal[i]) {
                    signal_is_constant = false;
                    break;
                }
            }
        }
        if(signal_is_constant) {
            _candidate_bits = evaluate_constant_subframe_(encoder, integer_signal[0], frame_header->blocksize, subframe_bps, subframe[!_best_subframe]);
            if(_candidate_bits < _best_bits) {
                _best_subframe = !_best_subframe;
                _best_bits = _candidate_bits;
            }
        }
        else {
            if(!encoder->private_->disable_fixed_subframes || (encoder->protected_->max_lpc_order == 0 && _best_bits == UINT_MAX)) {
                /* encode fixed */
                if(encoder->protected_->do_exhaustive_model_search) {
                    min_fixed_order = 0;
                    max_fixed_order = FLAC__MAX_FIXED_ORDER;
                }
                else {
                    min_fixed_order = max_fixed_order = guess_fixed_order;
                }
                if(max_fixed_order >= frame_header->blocksize)
                    max_fixed_order = frame_header->blocksize - 1;
                for(fixed_order = min_fixed_order; fixed_order <= max_fixed_order; fixed_order++) {
#ifndef FLAC__INTEGER_ONLY_LIBRARY
                    if(fixed_residual_bits_per_sample[fixed_order] >= (FLAC__float)subframe_bps)
                        continue; /* don't even try */
                    rice_parameter = (fixed_residual_bits_per_sample[fixed_order] > 0.0)? (unsigned)(fixed_residual_bits_per_sample[fixed_order]+0.5) : 0; /* 0.5 is for rounding */
#else
                    if(FLAC__fixedpoint_trunc(fixed_residual_bits_per_sample[fixed_order]) >= (int)subframe_bps)
                        continue; /* don't even try */
                    rice_parameter = (fixed_residual_bits_per_sample[fixed_order] > FLAC__FP_ZERO)? (unsigned)FLAC__fixedpoint_trunc(fixed_residual_bits_per_sample[fixed_order]+FLAC__FP_ONE_HALF) : 0; /* 0.5 is for rounding */
#endif
                    rice_parameter++; /* to account for the signed->unsigned conversion during rice coding */
                    if(rice_parameter >= rice_parameter_limit) {
                        rice_parameter = rice_parameter_limit - 1;
                    }
                    _candidate_bits =
                        evaluate_fixed_subframe_(
                            encoder,
                            integer_signal,
                            residual[!_best_subframe],
                            encoder->private_->abs_residual_partition_sums,
                            encoder->private_->raw_bits_per_partition,
                            frame_header->blocksize,
                            subframe_bps,
                            fixed_order,
                            rice_parameter,
                            rice_parameter_limit,
                            min_partition_order,
                            max_partition_order,
                            encoder->protected_->do_escape_coding,
                            encoder->protected_->rice_parameter_search_dist,
                            subframe[!_best_subframe],
                            partitioned_rice_contents[!_best_subframe]
                        );
                    if(_candidate_bits < _best_bits) {
                        _best_subframe = !_best_subframe;
                        _best_bits = _candidate_bits;
                    }
                }
            }

#ifndef FLAC__INTEGER_ONLY_LIBRARY
            /* encode lpc */
            if(encoder->protected_->max_lpc_order > 0) {
                if(encoder->protected_->max_lpc_order >= frame_header->blocksize)
                    max_lpc_order = frame_header->blocksize-1;
                else
                    max_lpc_order = encoder->protected_->max_lpc_order;
                if(max_lpc_order > 0) {
                    unsigned a;
                    for (a = 0; a < encoder->protected_->num_apodizations; a++) {
                        FLAC__lpc_window_data(integer_signal, encoder->private_->window[a], encoder->private_->windowed_signal, frame_header->blocksize);
                        encoder->private_->local_lpc_compute_autocorrelation(encoder->private_->windowed_signal, frame_header->blocksize, max_lpc_order+1, autoc);
                        /* if autoc[0] == 0.0, the signal is constant and we usually won't get here, but it can happen */
                        if(autoc[0] != 0.0) {
                            FLAC__lpc_compute_lp_coefficients(autoc, &max_lpc_order, encoder->private_->lp_coeff, lpc_error);
                            if(encoder->protected_->do_exhaustive_model_search) {
                                min_lpc_order = 1;
                            }
                            else {
                                const unsigned guess_lpc_order =
                                    FLAC__lpc_compute_best_order(
                                        lpc_error,
                                        max_lpc_order,
                                        frame_header->blocksize,
                                        subframe_bps + (
                                            encoder->protected_->do_qlp_coeff_prec_search?
                                                FLAC__MIN_QLP_COEFF_PRECISION : /* have to guess; use the min possible size to avoid accidentally favoring lower orders */
                                                encoder->protected_->qlp_coeff_precision
                                        )
                                    );
                                min_lpc_order = max_lpc_order = guess_lpc_order;
                            }
                            if(max_lpc_order >= frame_header->blocksize)
                                max_lpc_order = frame_header->blocksize - 1;
                            for(lpc_order = min_lpc_order; lpc_order <= max_lpc_order; lpc_order++) {
                                lpc_residual_bits_per_sample = FLAC__lpc_compute_expected_bits_per_residual_sample(lpc_error[lpc_order-1], frame_header->blocksize-lpc_order);
                                if(lpc_residual_bits_per_sample >= (FLAC__double)subframe_bps)
                                    continue; /* don't even try */
                                rice_parameter = (lpc_residual_bits_per_sample > 0.0)? (unsigned)(lpc_residual_bits_per_sample+0.5) : 0; /* 0.5 is for rounding */
                                rice_parameter++; /* to account for the signed->unsigned conversion during rice coding */
                                if(rice_parameter >= rice_parameter_limit) {
                                    rice_parameter = rice_parameter_limit - 1;
                                }
                                if(encoder->protected_->do_qlp_coeff_prec_search) {
                                    min_qlp_coeff_precision = FLAC__MIN_QLP_COEFF_PRECISION;
                                    /* try to keep qlp coeff precision such that only 32-bit math is required for decode of <=16bps streams */
                                    if(subframe_bps <= 16) {
                                        max_qlp_coeff_precision = std::min(32 - subframe_bps - FLAC__bitmath_ilog2(lpc_order), FLAC__MAX_QLP_COEFF_PRECISION);
                                        max_qlp_coeff_precision = std::max(max_qlp_coeff_precision, min_qlp_coeff_precision);
                                    }
                                    else
                                        max_qlp_coeff_precision = FLAC__MAX_QLP_COEFF_PRECISION;
                                }
                                else {
                                    min_qlp_coeff_precision = max_qlp_coeff_precision = encoder->protected_->qlp_coeff_precision;
                                }
                                for(qlp_coeff_precision = min_qlp_coeff_precision; qlp_coeff_precision <= max_qlp_coeff_precision; qlp_coeff_precision++) {
                                    _candidate_bits =
                                        evaluate_lpc_subframe_(
                                            encoder,
                                            integer_signal,
                                            residual[!_best_subframe],
                                            encoder->private_->abs_residual_partition_sums,
                                            encoder->private_->raw_bits_per_partition,
                                            encoder->private_->lp_coeff[lpc_order-1],
                                            frame_header->blocksize,
                                            subframe_bps,
                                            lpc_order,
                                            qlp_coeff_precision,
                                            rice_parameter,
                                            rice_parameter_limit,
                                            min_partition_order,
                                            max_partition_order,
                                            encoder->protected_->do_escape_coding,
                                            encoder->protected_->rice_parameter_search_dist,
                                            subframe[!_best_subframe],
                                            partitioned_rice_contents[!_best_subframe]
                                        );
                                    if(_candidate_bits > 0) { /* if == 0, there was a problem quantizing the lpcoeffs */
                                        if(_candidate_bits < _best_bits) {
                                            _best_subframe = !_best_subframe;
                                            _best_bits = _candidate_bits;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
#endif /* !defined FLAC__INTEGER_ONLY_LIBRARY */
        }
    }

    /* under rare circumstances this can happen when all but lpc subframe types are disabled: */
    if(_best_bits == UINT_MAX) {
        CHOC_ASSERT(_best_subframe == 0);
        _best_bits = evaluate_verbatim_subframe_(encoder, integer_signal, frame_header->blocksize, subframe_bps, subframe[_best_subframe]);
    }

    *best_subframe = _best_subframe;
    *best_bits = _best_bits;

    return true;
}

FLAC__bool add_subframe_(
    FLAC__StreamEncoder *encoder,
    unsigned blocksize,
    unsigned subframe_bps,
    const FLAC__Subframe *subframe,
    FLAC__BitWriter *frame
)
{
    switch(subframe->type) {
        case FLAC__SUBFRAME_TYPE_CONSTANT:
            if(!FLAC__subframe_add_constant(&(subframe->data.constant), subframe_bps, subframe->wasted_bits, frame)) {
                encoder->protected_->state = FLAC__STREAM_ENCODER_FRAMING_ERROR;
                return false;
            }
            break;
        case FLAC__SUBFRAME_TYPE_FIXED:
            if(!FLAC__subframe_add_fixed(&(subframe->data.fixed), blocksize - subframe->data.fixed.order, subframe_bps, subframe->wasted_bits, frame)) {
                encoder->protected_->state = FLAC__STREAM_ENCODER_FRAMING_ERROR;
                return false;
            }
            break;
        case FLAC__SUBFRAME_TYPE_LPC:
            if(!FLAC__subframe_add_lpc(&(subframe->data.lpc), blocksize - subframe->data.lpc.order, subframe_bps, subframe->wasted_bits, frame)) {
                encoder->protected_->state = FLAC__STREAM_ENCODER_FRAMING_ERROR;
                return false;
            }
            break;
        case FLAC__SUBFRAME_TYPE_VERBATIM:
            if(!FLAC__subframe_add_verbatim(&(subframe->data.verbatim), blocksize, subframe_bps, subframe->wasted_bits, frame)) {
                encoder->protected_->state = FLAC__STREAM_ENCODER_FRAMING_ERROR;
                return false;
            }
            break;
        default:
            CHOC_ASSERT(0);
    }

    return true;
}

#if SPOTCHECK_ESTIMATE
static void spotcheck_subframe_estimate_(
    FLAC__StreamEncoder *encoder,
    unsigned blocksize,
    unsigned subframe_bps,
    const FLAC__Subframe *subframe,
    unsigned estimate
)
{
    FLAC__bool ret;
    FLAC__BitWriter *frame = FLAC__bitwriter_new();
    if(frame == 0) {
        fprintf(stderr, "EST: can't allocate frame\n");
        return;
    }
    if(!FLAC__bitwriter_init(frame)) {
        fprintf(stderr, "EST: can't init frame\n");
        return;
    }
    ret = add_subframe_(encoder, blocksize, subframe_bps, subframe, frame);
    CHOC_ASSERT(ret);
    {
        const unsigned actual = FLAC__bitwriter_get_input_bits_unconsumed(frame);
        if(estimate != actual)
            fprintf(stderr, "EST: bad, frame#%u sub#%%d type=%8s est=%u, actual=%u, delta=%d\n", encoder->private_->current_frame_number, FLAC__SubframeTypeString[subframe->type], estimate, actual, (int)actual-(int)estimate);
    }
    FLAC__bitwriter_delete(frame);
}
#endif

unsigned evaluate_constant_subframe_(
    FLAC__StreamEncoder *encoder,
    const FLAC__int32 signal,
    unsigned blocksize,
    unsigned subframe_bps,
    FLAC__Subframe *subframe
)
{
    unsigned estimate;
    subframe->type = FLAC__SUBFRAME_TYPE_CONSTANT;
    subframe->data.constant.value = signal;

    estimate = FLAC__SUBFRAME_ZERO_PAD_LEN + FLAC__SUBFRAME_TYPE_LEN + FLAC__SUBFRAME_WASTED_BITS_FLAG_LEN + subframe->wasted_bits + subframe_bps;

#if SPOTCHECK_ESTIMATE
    spotcheck_subframe_estimate_(encoder, blocksize, subframe_bps, subframe, estimate);
#else
    (void)encoder, (void)blocksize;
#endif

    return estimate;
}

unsigned evaluate_fixed_subframe_(
    FLAC__StreamEncoder *encoder,
    const FLAC__int32 signal[],
    FLAC__int32 residual[],
    FLAC__uint64 abs_residual_partition_sums[],
    unsigned raw_bits_per_partition[],
    unsigned blocksize,
    unsigned subframe_bps,
    unsigned order,
    unsigned rice_parameter,
    unsigned rice_parameter_limit,
    unsigned min_partition_order,
    unsigned max_partition_order,
    FLAC__bool do_escape_coding,
    unsigned rice_parameter_search_dist,
    FLAC__Subframe *subframe,
    FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents
)
{
    unsigned i, residual_bits, estimate;
    const unsigned residual_samples = blocksize - order;

    FLAC__fixed_compute_residual(signal+order, residual_samples, order, residual);

    subframe->type = FLAC__SUBFRAME_TYPE_FIXED;

    subframe->data.fixed.entropy_coding_method.type = FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE;
    subframe->data.fixed.entropy_coding_method.data.partitioned_rice.contents = partitioned_rice_contents;
    subframe->data.fixed.residual = residual;

    residual_bits =
        find_best_partition_order_(
            encoder->private_,
            residual,
            abs_residual_partition_sums,
            raw_bits_per_partition,
            residual_samples,
            order,
            rice_parameter,
            rice_parameter_limit,
            min_partition_order,
            max_partition_order,
            subframe_bps,
            do_escape_coding,
            rice_parameter_search_dist,
            &subframe->data.fixed.entropy_coding_method
        );

    subframe->data.fixed.order = order;
    for(i = 0; i < order; i++)
        subframe->data.fixed.warmup[i] = signal[i];

    estimate = FLAC__SUBFRAME_ZERO_PAD_LEN + FLAC__SUBFRAME_TYPE_LEN + FLAC__SUBFRAME_WASTED_BITS_FLAG_LEN + subframe->wasted_bits + (order * subframe_bps) + residual_bits;

#if SPOTCHECK_ESTIMATE
    spotcheck_subframe_estimate_(encoder, blocksize, subframe_bps, subframe, estimate);
#endif

    return estimate;
}

#ifndef FLAC__INTEGER_ONLY_LIBRARY
unsigned evaluate_lpc_subframe_(
    FLAC__StreamEncoder *encoder,
    const FLAC__int32 signal[],
    FLAC__int32 residual[],
    FLAC__uint64 abs_residual_partition_sums[],
    unsigned raw_bits_per_partition[],
    const FLAC__real lp_coeff[],
    unsigned blocksize,
    unsigned subframe_bps,
    unsigned order,
    unsigned qlp_coeff_precision,
    unsigned rice_parameter,
    unsigned rice_parameter_limit,
    unsigned min_partition_order,
    unsigned max_partition_order,
    FLAC__bool do_escape_coding,
    unsigned rice_parameter_search_dist,
    FLAC__Subframe *subframe,
    FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents
)
{
    FLAC__int32 qlp_coeff[FLAC__MAX_LPC_ORDER]; /* WATCHOUT: the size is important; some x86 intrinsic routines need more than lpc order elements */
    unsigned i, residual_bits, estimate;
    int quantization, ret;
    const unsigned residual_samples = blocksize - order;

    /* try to keep qlp coeff precision such that only 32-bit math is required for decode of <=16bps streams */
    if(subframe_bps <= 16) {
        CHOC_ASSERT(order > 0);
        CHOC_ASSERT(order <= FLAC__MAX_LPC_ORDER);
        qlp_coeff_precision = std::min(qlp_coeff_precision, 32 - subframe_bps - FLAC__bitmath_ilog2(order));
    }

    ret = FLAC__lpc_quantize_coefficients(lp_coeff, order, qlp_coeff_precision, qlp_coeff, &quantization);
    if(ret != 0)
        return 0; /* this is a hack to indicate to the caller that we can't do lp at this order on this subframe */

    if(subframe_bps + qlp_coeff_precision + FLAC__bitmath_ilog2(order) <= 32)
        if(subframe_bps <= 16 && qlp_coeff_precision <= 16)
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_16bit(signal+order, residual_samples, qlp_coeff, order, quantization, residual);
        else
            encoder->private_->local_lpc_compute_residual_from_qlp_coefficients(signal+order, residual_samples, qlp_coeff, order, quantization, residual);
    else
        encoder->private_->local_lpc_compute_residual_from_qlp_coefficients_64bit(signal+order, residual_samples, qlp_coeff, order, quantization, residual);

    subframe->type = FLAC__SUBFRAME_TYPE_LPC;

    subframe->data.lpc.entropy_coding_method.type = FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE;
    subframe->data.lpc.entropy_coding_method.data.partitioned_rice.contents = partitioned_rice_contents;
    subframe->data.lpc.residual = residual;

    residual_bits =
        find_best_partition_order_(
            encoder->private_,
            residual,
            abs_residual_partition_sums,
            raw_bits_per_partition,
            residual_samples,
            order,
            rice_parameter,
            rice_parameter_limit,
            min_partition_order,
            max_partition_order,
            subframe_bps,
            do_escape_coding,
            rice_parameter_search_dist,
            &subframe->data.lpc.entropy_coding_method
        );

    subframe->data.lpc.order = order;
    subframe->data.lpc.qlp_coeff_precision = qlp_coeff_precision;
    subframe->data.lpc.quantization_level = quantization;
    memcpy(subframe->data.lpc.qlp_coeff, qlp_coeff, sizeof(FLAC__int32)*FLAC__MAX_LPC_ORDER);
    for(i = 0; i < order; i++)
        subframe->data.lpc.warmup[i] = signal[i];

    estimate = FLAC__SUBFRAME_ZERO_PAD_LEN + FLAC__SUBFRAME_TYPE_LEN + FLAC__SUBFRAME_WASTED_BITS_FLAG_LEN + subframe->wasted_bits + FLAC__SUBFRAME_LPC_QLP_COEFF_PRECISION_LEN + FLAC__SUBFRAME_LPC_QLP_SHIFT_LEN + (order * (qlp_coeff_precision + subframe_bps)) + residual_bits;

#if SPOTCHECK_ESTIMATE
    spotcheck_subframe_estimate_(encoder, blocksize, subframe_bps, subframe, estimate);
#endif

    return estimate;
}
#endif

unsigned evaluate_verbatim_subframe_(
    FLAC__StreamEncoder *encoder,
    const FLAC__int32 signal[],
    unsigned blocksize,
    unsigned subframe_bps,
    FLAC__Subframe *subframe
)
{
    unsigned estimate;

    subframe->type = FLAC__SUBFRAME_TYPE_VERBATIM;

    subframe->data.verbatim.data = signal;

    estimate = FLAC__SUBFRAME_ZERO_PAD_LEN + FLAC__SUBFRAME_TYPE_LEN + FLAC__SUBFRAME_WASTED_BITS_FLAG_LEN + subframe->wasted_bits + (blocksize * subframe_bps);

#if SPOTCHECK_ESTIMATE
    spotcheck_subframe_estimate_(encoder, blocksize, subframe_bps, subframe, estimate);
#else
    (void)encoder;
#endif

    return estimate;
}

unsigned find_best_partition_order_(
    FLAC__StreamEncoderPrivate *private_,
    const FLAC__int32 residual[],
    FLAC__uint64 abs_residual_partition_sums[],
    unsigned raw_bits_per_partition[],
    unsigned residual_samples,
    unsigned predictor_order,
    unsigned rice_parameter,
    unsigned rice_parameter_limit,
    unsigned min_partition_order,
    unsigned max_partition_order,
    unsigned bps,
    FLAC__bool do_escape_coding,
    unsigned rice_parameter_search_dist,
    FLAC__EntropyCodingMethod *best_ecm
)
{
    unsigned residual_bits, best_residual_bits = 0;
    unsigned best_parameters_index = 0;
    unsigned best_partition_order = 0;
    const unsigned blocksize = residual_samples + predictor_order;

    max_partition_order = FLAC__format_get_max_rice_partition_order_from_blocksize_limited_max_and_predictor_order(max_partition_order, blocksize, predictor_order);
    min_partition_order = std::min(min_partition_order, max_partition_order);

    private_->local_precompute_partition_info_sums(residual, abs_residual_partition_sums, residual_samples, predictor_order, min_partition_order, max_partition_order, bps);

    if(do_escape_coding)
        precompute_partition_info_escapes_(residual, raw_bits_per_partition, residual_samples, predictor_order, min_partition_order, max_partition_order);

    {
        int partition_order;
        unsigned sum;

        for(partition_order = (int)max_partition_order, sum = 0; partition_order >= (int)min_partition_order; partition_order--) {
            if(!
                set_partitioned_rice_(
#ifdef EXACT_RICE_BITS_CALCULATION
                    residual,
#endif
                    abs_residual_partition_sums+sum,
                    raw_bits_per_partition ? (raw_bits_per_partition + sum) : nullptr,
                    residual_samples,
                    predictor_order,
                    rice_parameter,
                    rice_parameter_limit,
                    rice_parameter_search_dist,
                    (unsigned)partition_order,
                    do_escape_coding,
                    &private_->partitioned_rice_contents_extra[!best_parameters_index],
                    &residual_bits
                )
            )
            {
                CHOC_ASSERT(best_residual_bits != 0);
                break;
            }
            sum += 1u << partition_order;
            if(best_residual_bits == 0 || residual_bits < best_residual_bits) {
                best_residual_bits = residual_bits;
                best_parameters_index = !best_parameters_index;
                best_partition_order = partition_order;
            }
        }
    }

    best_ecm->data.partitioned_rice.order = best_partition_order;

    {
        /*
         * We are allowed to de-const the pointer based on our special
         * knowledge; it is const to the outside world.
         */
        FLAC__EntropyCodingMethod_PartitionedRiceContents* prc = (FLAC__EntropyCodingMethod_PartitionedRiceContents*)best_ecm->data.partitioned_rice.contents;
        unsigned partition;

        /* save best parameters and raw_bits */
        FLAC__format_entropy_coding_method_partitioned_rice_contents_ensure_size(prc, std::max(6u, best_partition_order));
        memcpy(prc->parameters, private_->partitioned_rice_contents_extra[best_parameters_index].parameters, sizeof(unsigned)*(1<<(best_partition_order)));
        if(do_escape_coding)
            memcpy(prc->raw_bits, private_->partitioned_rice_contents_extra[best_parameters_index].raw_bits, sizeof(unsigned)*(1<<(best_partition_order)));
        /*
         * Now need to check if the type should be changed to
         * FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2 based on the
         * size of the rice parameters.
         */
        for(partition = 0; partition < (1u<<best_partition_order); partition++) {
            if(prc->parameters[partition] >= FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ESCAPE_PARAMETER) {
                best_ecm->type = FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2;
                break;
            }
        }
    }

    return best_residual_bits;
}

void precompute_partition_info_sums_(
    const FLAC__int32 residual[],
    FLAC__uint64 abs_residual_partition_sums[],
    unsigned residual_samples,
    unsigned predictor_order,
    unsigned min_partition_order,
    unsigned max_partition_order,
    unsigned bps
)
{
    const unsigned default_partition_samples = (residual_samples + predictor_order) >> max_partition_order;
    unsigned partitions = 1u << max_partition_order;

    CHOC_ASSERT(default_partition_samples > predictor_order);

    /* first do max_partition_order */
    {
        unsigned partition, residual_sample, end = (unsigned)(-(int)predictor_order);
        /* WATCHOUT: "+ bps + FLAC__MAX_EXTRA_RESIDUAL_BPS" is the maximum
         * assumed size of the average residual magnitude */
        if(FLAC__bitmath_ilog2(default_partition_samples) + bps + FLAC__MAX_EXTRA_RESIDUAL_BPS < 32) {
            FLAC__uint32 abs_residual_partition_sum;

            for(partition = residual_sample = 0; partition < partitions; partition++) {
                end += default_partition_samples;
                abs_residual_partition_sum = 0;
                for( ; residual_sample < end; residual_sample++)
                    abs_residual_partition_sum += abs(residual[residual_sample]); /* abs(INT_MIN) is undefined, but if the residual is INT_MIN we have bigger problems */
                abs_residual_partition_sums[partition] = abs_residual_partition_sum;
            }
        }
        else { /* have to pessimistically use 64 bits for accumulator */
            FLAC__uint64 abs_residual_partition_sum;

            for(partition = residual_sample = 0; partition < partitions; partition++) {
                end += default_partition_samples;
                abs_residual_partition_sum = 0;
                for( ; residual_sample < end; residual_sample++)
                    abs_residual_partition_sum += abs(residual[residual_sample]); /* abs(INT_MIN) is undefined, but if the residual is INT_MIN we have bigger problems */
                abs_residual_partition_sums[partition] = abs_residual_partition_sum;
            }
        }
    }

    /* now merge partitions for lower orders */
    {
        unsigned from_partition = 0, to_partition = partitions;
        int partition_order;
        for(partition_order = (int)max_partition_order - 1; partition_order >= (int)min_partition_order; partition_order--) {
            unsigned i;
            partitions >>= 1;
            for(i = 0; i < partitions; i++) {
                abs_residual_partition_sums[to_partition++] =
                    abs_residual_partition_sums[from_partition  ] +
                    abs_residual_partition_sums[from_partition+1];
                from_partition += 2;
            }
        }
    }
}

void precompute_partition_info_escapes_(
    const FLAC__int32 residual[],
    unsigned raw_bits_per_partition[],
    unsigned residual_samples,
    unsigned predictor_order,
    unsigned min_partition_order,
    unsigned max_partition_order
)
{
    int partition_order;
    unsigned from_partition, to_partition = 0;
    const unsigned blocksize = residual_samples + predictor_order;

    /* first do max_partition_order */
    for(partition_order = (int)max_partition_order; partition_order >= 0; partition_order--) {
        FLAC__int32 r;
        FLAC__uint32 rmax;
        unsigned partition, partition_sample, partition_samples, residual_sample;
        const unsigned partitions = 1u << partition_order;
        const unsigned default_partition_samples = blocksize >> partition_order;

        CHOC_ASSERT(default_partition_samples > predictor_order);

        for(partition = residual_sample = 0; partition < partitions; partition++) {
            partition_samples = default_partition_samples;
            if(partition == 0)
                partition_samples -= predictor_order;
            rmax = 0;
            for(partition_sample = 0; partition_sample < partition_samples; partition_sample++) {
                r = residual[residual_sample++];
                /* OPT: maybe faster: rmax |= r ^ (r>>31) */
                if(r < 0)
                    rmax |= ~r;
                else
                    rmax |= r;
            }
            /* now we know all residual values are in the range [-rmax-1,rmax] */
            raw_bits_per_partition[partition] = rmax? FLAC__bitmath_ilog2(rmax) + 2 : 1;
        }
        to_partition = partitions;
        break; /*@@@ yuck, should remove the 'for' loop instead */
    }

    /* now merge partitions for lower orders */
    for(from_partition = 0, --partition_order; partition_order >= (int)min_partition_order; partition_order--) {
        unsigned m;
        unsigned i;
        const unsigned partitions = 1u << partition_order;
        for(i = 0; i < partitions; i++) {
            m = raw_bits_per_partition[from_partition];
            from_partition++;
            raw_bits_per_partition[to_partition] = std::max(m, raw_bits_per_partition[from_partition]);
            from_partition++;
            to_partition++;
        }
    }
}

#ifdef EXACT_RICE_BITS_CALCULATION
static inline unsigned count_rice_bits_in_partition_(
    const unsigned rice_parameter,
    const unsigned partition_samples,
    const FLAC__int32 *residual
)
{
    unsigned i, partition_bits =
        FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN + /* actually could end up being FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_PARAMETER_LEN but err on side of 16bps */
        (1+rice_parameter) * partition_samples /* 1 for unary stop bit + rice_parameter for the binary portion */
    ;
    for(i = 0; i < partition_samples; i++)
        partition_bits += ( (FLAC__uint32)((residual[i]<<1)^(residual[i]>>31)) >> rice_parameter );
    return partition_bits;
}
#else
static inline unsigned count_rice_bits_in_partition_(
    const unsigned rice_parameter,
    const unsigned partition_samples,
    const FLAC__uint64 abs_residual_partition_sum
)
{
    return
        FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN + /* actually could end up being FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_PARAMETER_LEN but err on side of 16bps */
        (1+rice_parameter) * partition_samples + /* 1 for unary stop bit + rice_parameter for the binary portion */
        (
            rice_parameter?
                (unsigned)(abs_residual_partition_sum >> (rice_parameter-1)) /* rice_parameter-1 because the real coder sign-folds instead of using a sign bit */
                : (unsigned)(abs_residual_partition_sum << 1) /* can't shift by negative number, so reverse */
        )
        - (partition_samples >> 1)
        /* -(partition_samples>>1) to subtract out extra contributions to the abs_residual_partition_sum.
         * The actual number of bits used is closer to the sum(for all i in the partition) of  abs(residual[i])>>(rice_parameter-1)
         * By using the abs_residual_partition sum, we also add in bits in the LSBs that would normally be shifted out.
         * So the subtraction term tries to guess how many extra bits were contributed.
         * If the LSBs are randomly distributed, this should average to 0.5 extra bits per sample.
         */
    ;
}
#endif

FLAC__bool set_partitioned_rice_(
#ifdef EXACT_RICE_BITS_CALCULATION
    const FLAC__int32 residual[],
#endif
    const FLAC__uint64 abs_residual_partition_sums[],
    const unsigned raw_bits_per_partition[],
    const unsigned residual_samples,
    const unsigned predictor_order,
    const unsigned suggested_rice_parameter,
    const unsigned rice_parameter_limit,
    const unsigned rice_parameter_search_dist,
    const unsigned partition_order,
    const FLAC__bool search_for_escapes,
    FLAC__EntropyCodingMethod_PartitionedRiceContents *partitioned_rice_contents,
    unsigned *bits
)
{
    unsigned rice_parameter, partition_bits;
    unsigned best_partition_bits, best_rice_parameter = 0;
    unsigned bits_ = FLAC__ENTROPY_CODING_METHOD_TYPE_LEN + FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ORDER_LEN;
    unsigned *parameters, *raw_bits;
#ifdef ENABLE_RICE_PARAMETER_SEARCH
    unsigned min_rice_parameter, max_rice_parameter;
#else
    (void)rice_parameter_search_dist;
#endif

    CHOC_ASSERT(suggested_rice_parameter < FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_ESCAPE_PARAMETER);
    CHOC_ASSERT(rice_parameter_limit <= FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_ESCAPE_PARAMETER);

    FLAC__format_entropy_coding_method_partitioned_rice_contents_ensure_size(partitioned_rice_contents, std::max(6u, partition_order));
    parameters = partitioned_rice_contents->parameters;
    raw_bits = partitioned_rice_contents->raw_bits;

    if(partition_order == 0) {
        best_partition_bits = (unsigned)(-1);
#ifdef ENABLE_RICE_PARAMETER_SEARCH
        if(rice_parameter_search_dist) {
            if(suggested_rice_parameter < rice_parameter_search_dist)
                min_rice_parameter = 0;
            else
                min_rice_parameter = suggested_rice_parameter - rice_parameter_search_dist;
            max_rice_parameter = suggested_rice_parameter + rice_parameter_search_dist;
            if(max_rice_parameter >= rice_parameter_limit) {
                max_rice_parameter = rice_parameter_limit - 1;
            }
        }
        else
            min_rice_parameter = max_rice_parameter = suggested_rice_parameter;

        for(rice_parameter = min_rice_parameter; rice_parameter <= max_rice_parameter; rice_parameter++) {
#else
            rice_parameter = suggested_rice_parameter;
#endif
#ifdef EXACT_RICE_BITS_CALCULATION
            partition_bits = count_rice_bits_in_partition_(rice_parameter, residual_samples, residual);
#else
            partition_bits = count_rice_bits_in_partition_(rice_parameter, residual_samples, abs_residual_partition_sums[0]);
#endif
            if(partition_bits < best_partition_bits) {
                best_rice_parameter = rice_parameter;
                best_partition_bits = partition_bits;
            }
#ifdef ENABLE_RICE_PARAMETER_SEARCH
        }
#endif
        if(search_for_escapes) {
            partition_bits = FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_PARAMETER_LEN + FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_RAW_LEN + raw_bits_per_partition[0] * residual_samples;
            if(partition_bits <= best_partition_bits) {
                raw_bits[0] = raw_bits_per_partition[0];
                best_rice_parameter = 0; /* will be converted to appropriate escape parameter later */
                best_partition_bits = partition_bits;
            }
            else
                raw_bits[0] = 0;
        }
        parameters[0] = best_rice_parameter;
        bits_ += best_partition_bits;
    }
    else {
        unsigned partition, residual_sample;
        unsigned partition_samples;
        FLAC__uint64 mean, k;
        const unsigned partitions = 1u << partition_order;
        for(partition = residual_sample = 0; partition < partitions; partition++) {
            partition_samples = (residual_samples+predictor_order) >> partition_order;
            if(partition == 0) {
                if(partition_samples <= predictor_order)
                    return false;
                else
                    partition_samples -= predictor_order;
            }
            mean = abs_residual_partition_sums[partition];
            /* we are basically calculating the size in bits of the
             * average residual magnitude in the partition:
             *   rice_parameter = floor(log2(mean/partition_samples))
             * 'mean' is not a good name for the variable, it is
             * actually the sum of magnitudes of all residual values
             * in the partition, so the actual mean is
             * mean/partition_samples
             */
#if defined FLAC__CPU_X86_64 /* and other 64-bit arch, too */
            if(mean <= 0x80000000/512) { /* 512: more or less optimal for both 16- and 24-bit input */
#else
            if(mean <= 0x80000000/8) { /* 32-bit arch: use 32-bit math if possible */
#endif
                FLAC__uint32 k2, mean2 = (FLAC__uint32) mean;
                rice_parameter = 0; k2 = partition_samples;
                while(k2*8 < mean2) { /* requires: mean <= (2^31)/8 */
                    rice_parameter += 4; k2 <<= 4; /* tuned for 16-bit input */
                }
                while(k2 < mean2) { /* requires: mean <= 2^31 */
                    rice_parameter++; k2 <<= 1;
                }
            }
            else {
                rice_parameter = 0; k = partition_samples;
                if(mean <= FLAC__U64L(0x8000000000000000)/128) /* usually mean is _much_ smaller than this value */
                    while(k*128 < mean) { /* requires: mean <= (2^63)/128 */
                        rice_parameter += 8; k <<= 8; /* tuned for 24-bit input */
                    }
                while(k < mean) { /* requires: mean <= 2^63 */
                    rice_parameter++; k <<= 1;
                }
            }
            if(rice_parameter >= rice_parameter_limit) {
                rice_parameter = rice_parameter_limit - 1;
            }

            best_partition_bits = (unsigned)(-1);
#ifdef ENABLE_RICE_PARAMETER_SEARCH
            if(rice_parameter_search_dist) {
                if(rice_parameter < rice_parameter_search_dist)
                    min_rice_parameter = 0;
                else
                    min_rice_parameter = rice_parameter - rice_parameter_search_dist;
                max_rice_parameter = rice_parameter + rice_parameter_search_dist;
                if(max_rice_parameter >= rice_parameter_limit) {
                    max_rice_parameter = rice_parameter_limit - 1;
                }
            }
            else
                min_rice_parameter = max_rice_parameter = rice_parameter;

            for(rice_parameter = min_rice_parameter; rice_parameter <= max_rice_parameter; rice_parameter++) {
#endif
#ifdef EXACT_RICE_BITS_CALCULATION
                partition_bits = count_rice_bits_in_partition_(rice_parameter, partition_samples, residual+residual_sample);
#else
                partition_bits = count_rice_bits_in_partition_(rice_parameter, partition_samples, abs_residual_partition_sums[partition]);
#endif
                if(partition_bits < best_partition_bits) {
                    best_rice_parameter = rice_parameter;
                    best_partition_bits = partition_bits;
                }
#ifdef ENABLE_RICE_PARAMETER_SEARCH
            }
#endif
            if(search_for_escapes) {
                partition_bits = FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_PARAMETER_LEN + FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_RAW_LEN + raw_bits_per_partition[partition] * partition_samples;
                if(partition_bits <= best_partition_bits) {
                    raw_bits[partition] = raw_bits_per_partition[partition];
                    best_rice_parameter = 0; /* will be converted to appropriate escape parameter later */
                    best_partition_bits = partition_bits;
                }
                else
                    raw_bits[partition] = 0;
            }
            parameters[partition] = best_rice_parameter;
            bits_ += best_partition_bits;
            residual_sample += partition_samples;
        }
    }

    *bits = bits_;
    return true;
}

unsigned get_wasted_bits_(FLAC__int32 signal[], unsigned samples)
{
    unsigned i, shift;
    FLAC__int32 x = 0;

    for(i = 0; i < samples && !(x&1); i++)
        x |= signal[i];

    if(x == 0) {
        shift = 0;
    }
    else {
        for(shift = 0; !(x&1); shift++)
            x >>= 1;
    }

    if(shift > 0) {
        for(i = 0; i < samples; i++)
             signal[i] >>= shift;
    }

    return shift;
}

void append_to_verify_fifo_(verify_input_fifo *fifo, const FLAC__int32 * const input[], unsigned input_offset, unsigned channels, unsigned wide_samples)
{
    unsigned channel;

    for(channel = 0; channel < channels; channel++)
        memcpy(&fifo->data[channel][fifo->tail], &input[channel][input_offset], sizeof(FLAC__int32) * wide_samples);

    fifo->tail += wide_samples;

    CHOC_ASSERT(fifo->tail <= fifo->size);
}

void append_to_verify_fifo_interleaved_(verify_input_fifo *fifo, const FLAC__int32 input[], unsigned input_offset, unsigned channels, unsigned wide_samples)
{
    unsigned channel;
    unsigned sample, wide_sample;
    unsigned tail = fifo->tail;

    sample = input_offset * channels;
    for(wide_sample = 0; wide_sample < wide_samples; wide_sample++) {
        for(channel = 0; channel < channels; channel++)
            fifo->data[channel][tail] = input[sample++];
        tail++;
    }
    fifo->tail = tail;

    CHOC_ASSERT(fifo->tail <= fifo->size);
}

FLAC__StreamDecoderReadStatus verify_read_callback_(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
    FLAC__StreamEncoder *encoder = (FLAC__StreamEncoder*)client_data;
    const size_t encoded_bytes = encoder->private_->verify.output.bytes;
    (void)decoder;

    if(encoder->private_->verify.needs_magic_hack) {
        CHOC_ASSERT(*bytes >= FLAC__STREAM_SYNC_LENGTH);
        *bytes = FLAC__STREAM_SYNC_LENGTH;
        memcpy(buffer, FLAC__STREAM_SYNC_STRING, *bytes);
        encoder->private_->verify.needs_magic_hack = false;
    }
    else {
        if(encoded_bytes == 0) {
            /*
             * If we get here, a FIFO underflow has occurred,
             * which means there is a bug somewhere.
             */
            CHOC_ASSERT(0);
            return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
        }
        else if(encoded_bytes < *bytes)
            *bytes = encoded_bytes;
        memcpy(buffer, encoder->private_->verify.output.data, *bytes);
        encoder->private_->verify.output.data += *bytes;
        encoder->private_->verify.output.bytes -= *bytes;
    }

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}

FLAC__StreamDecoderWriteStatus verify_write_callback_(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 * const buffer[], void *client_data)
{
    FLAC__StreamEncoder *encoder = (FLAC__StreamEncoder *)client_data;
    unsigned channel;
    const unsigned channels = frame->header.channels;
    const unsigned blocksize = frame->header.blocksize;
    const unsigned bytes_per_block = sizeof(FLAC__int32) * blocksize;

    (void)decoder;

    for(channel = 0; channel < channels; channel++) {
        if(0 != memcmp(buffer[channel], encoder->private_->verify.input_fifo.data[channel], bytes_per_block)) {
            unsigned i, sample = 0;
            FLAC__int32 expect = 0, got = 0;

            for(i = 0; i < blocksize; i++) {
                if(buffer[channel][i] != encoder->private_->verify.input_fifo.data[channel][i]) {
                    sample = i;
                    expect = (FLAC__int32)encoder->private_->verify.input_fifo.data[channel][i];
                    got = (FLAC__int32)buffer[channel][i];
                    break;
                }
            }
            CHOC_ASSERT(i < blocksize);
            CHOC_ASSERT(frame->header.number_type == FLAC__FRAME_NUMBER_TYPE_SAMPLE_NUMBER);
            encoder->private_->verify.error_stats.absolute_sample = frame->header.number.sample_number + sample;
            encoder->private_->verify.error_stats.frame_number = (unsigned)(frame->header.number.sample_number / blocksize);
            encoder->private_->verify.error_stats.channel = channel;
            encoder->private_->verify.error_stats.sample = sample;
            encoder->private_->verify.error_stats.expected = expect;
            encoder->private_->verify.error_stats.got = got;
            encoder->protected_->state = FLAC__STREAM_ENCODER_VERIFY_MISMATCH_IN_AUDIO_DATA;
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }
    }
    /* dequeue the frame from the fifo */
    encoder->private_->verify.input_fifo.tail -= blocksize;
    CHOC_ASSERT(encoder->private_->verify.input_fifo.tail <= OVERREAD_);
    for(channel = 0; channel < channels; channel++)
        memmove(&encoder->private_->verify.input_fifo.data[channel][0], &encoder->private_->verify.input_fifo.data[channel][blocksize], encoder->private_->verify.input_fifo.tail * sizeof(encoder->private_->verify.input_fifo.data[0][0]));
    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void verify_metadata_callback_(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
    (void)decoder, (void)metadata, (void)client_data;
}

void verify_error_callback_(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
    FLAC__StreamEncoder *encoder = (FLAC__StreamEncoder*)client_data;
    (void)decoder, (void)status;
    encoder->protected_->state = FLAC__STREAM_ENCODER_VERIFY_DECODER_ERROR;
}

static FLAC__bool add_entropy_coding_method_(FLAC__BitWriter *bw, const FLAC__EntropyCodingMethod *method);
static FLAC__bool add_residual_partitioned_rice_(FLAC__BitWriter *bw, const FLAC__int32 residual[], const unsigned residual_samples, const unsigned predictor_order, const unsigned rice_parameters[], const unsigned raw_bits[], const unsigned partition_order, const FLAC__bool is_extended);

inline FLAC__bool FLAC__add_metadata_block(const FLAC__StreamMetadata *metadata, FLAC__BitWriter *bw)
{
    unsigned i, j;
    const unsigned vendor_string_length = (unsigned)strlen(FLAC__VENDOR_STRING);

    if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->is_last, FLAC__STREAM_METADATA_IS_LAST_LEN))
        return false;

    if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->type, FLAC__STREAM_METADATA_TYPE_LEN))
        return false;

    /*
     * First, for VORBIS_COMMENTs, adjust the length to reflect our vendor string
     */
    i = metadata->length;
    if(metadata->type == FLAC__METADATA_TYPE_VORBIS_COMMENT) {
        CHOC_ASSERT(metadata->data.vorbis_comment.vendor_string.length == 0 || 0 != metadata->data.vorbis_comment.vendor_string.entry);
        i -= metadata->data.vorbis_comment.vendor_string.length;
        i += vendor_string_length;
    }
    CHOC_ASSERT(i < (1u << FLAC__STREAM_METADATA_LENGTH_LEN));
    if(!FLAC__bitwriter_write_raw_uint32(bw, i, FLAC__STREAM_METADATA_LENGTH_LEN))
        return false;

    switch(metadata->type) {
        case FLAC__METADATA_TYPE_STREAMINFO:
            CHOC_ASSERT(metadata->data.stream_info.min_blocksize < (1u << FLAC__STREAM_METADATA_STREAMINFO_MIN_BLOCK_SIZE_LEN));
            if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.stream_info.min_blocksize, FLAC__STREAM_METADATA_STREAMINFO_MIN_BLOCK_SIZE_LEN))
                return false;
            CHOC_ASSERT(metadata->data.stream_info.max_blocksize < (1u << FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN));
            if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.stream_info.max_blocksize, FLAC__STREAM_METADATA_STREAMINFO_MAX_BLOCK_SIZE_LEN))
                return false;
            CHOC_ASSERT(metadata->data.stream_info.min_framesize < (1u << FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN));
            if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.stream_info.min_framesize, FLAC__STREAM_METADATA_STREAMINFO_MIN_FRAME_SIZE_LEN))
                return false;
            CHOC_ASSERT(metadata->data.stream_info.max_framesize < (1u << FLAC__STREAM_METADATA_STREAMINFO_MAX_FRAME_SIZE_LEN));
            if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.stream_info.max_framesize, FLAC__STREAM_METADATA_STREAMINFO_MAX_FRAME_SIZE_LEN))
                return false;
            CHOC_ASSERT(FLAC__format_sample_rate_is_valid(metadata->data.stream_info.sample_rate));
            if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.stream_info.sample_rate, FLAC__STREAM_METADATA_STREAMINFO_SAMPLE_RATE_LEN))
                return false;
            CHOC_ASSERT(metadata->data.stream_info.channels > 0);
            CHOC_ASSERT(metadata->data.stream_info.channels <= (1u << FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN));
            if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.stream_info.channels-1, FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN))
                return false;
            CHOC_ASSERT(metadata->data.stream_info.bits_per_sample > 0);
            CHOC_ASSERT(metadata->data.stream_info.bits_per_sample <= (1u << FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN));
            if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.stream_info.bits_per_sample-1, FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN))
                return false;
            if(!FLAC__bitwriter_write_raw_uint64(bw, metadata->data.stream_info.total_samples, FLAC__STREAM_METADATA_STREAMINFO_TOTAL_SAMPLES_LEN))
                return false;
            if(!FLAC__bitwriter_write_byte_block(bw, metadata->data.stream_info.md5sum, 16))
                return false;
            break;
        case FLAC__METADATA_TYPE_PADDING:
            if(!FLAC__bitwriter_write_zeroes(bw, metadata->length * 8))
                return false;
            break;
        case FLAC__METADATA_TYPE_APPLICATION:
            if(!FLAC__bitwriter_write_byte_block(bw, metadata->data.application.id, FLAC__STREAM_METADATA_APPLICATION_ID_LEN / 8))
                return false;
            if(!FLAC__bitwriter_write_byte_block(bw, metadata->data.application.data, metadata->length - (FLAC__STREAM_METADATA_APPLICATION_ID_LEN / 8)))
                return false;
            break;
        case FLAC__METADATA_TYPE_SEEKTABLE:
            for(i = 0; i < metadata->data.seek_table.num_points; i++) {
                if(!FLAC__bitwriter_write_raw_uint64(bw, metadata->data.seek_table.points[i].sample_number, FLAC__STREAM_METADATA_SEEKPOINT_SAMPLE_NUMBER_LEN))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint64(bw, metadata->data.seek_table.points[i].stream_offset, FLAC__STREAM_METADATA_SEEKPOINT_STREAM_OFFSET_LEN))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.seek_table.points[i].frame_samples, FLAC__STREAM_METADATA_SEEKPOINT_FRAME_SAMPLES_LEN))
                    return false;
            }
            break;
        case FLAC__METADATA_TYPE_VORBIS_COMMENT:
            if(!FLAC__bitwriter_write_raw_uint32_little_endian(bw, vendor_string_length))
                return false;
            if(!FLAC__bitwriter_write_byte_block(bw, (const FLAC__byte*)FLAC__VENDOR_STRING, vendor_string_length))
                return false;
            if(!FLAC__bitwriter_write_raw_uint32_little_endian(bw, metadata->data.vorbis_comment.num_comments))
                return false;
            for(i = 0; i < metadata->data.vorbis_comment.num_comments; i++) {
                if(!FLAC__bitwriter_write_raw_uint32_little_endian(bw, metadata->data.vorbis_comment.comments[i].length))
                    return false;
                if(!FLAC__bitwriter_write_byte_block(bw, metadata->data.vorbis_comment.comments[i].entry, metadata->data.vorbis_comment.comments[i].length))
                    return false;
            }
            break;
        case FLAC__METADATA_TYPE_CUESHEET:
            CHOC_ASSERT(FLAC__STREAM_METADATA_CUESHEET_MEDIA_CATALOG_NUMBER_LEN % 8 == 0);
            if(!FLAC__bitwriter_write_byte_block(bw, (const FLAC__byte*)metadata->data.cue_sheet.media_catalog_number, FLAC__STREAM_METADATA_CUESHEET_MEDIA_CATALOG_NUMBER_LEN/8))
                return false;
            if(!FLAC__bitwriter_write_raw_uint64(bw, metadata->data.cue_sheet.lead_in, FLAC__STREAM_METADATA_CUESHEET_LEAD_IN_LEN))
                return false;
            if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.cue_sheet.is_cd? 1 : 0, FLAC__STREAM_METADATA_CUESHEET_IS_CD_LEN))
                return false;
            if(!FLAC__bitwriter_write_zeroes(bw, FLAC__STREAM_METADATA_CUESHEET_RESERVED_LEN))
                return false;
            if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.cue_sheet.num_tracks, FLAC__STREAM_METADATA_CUESHEET_NUM_TRACKS_LEN))
                return false;
            for(i = 0; i < metadata->data.cue_sheet.num_tracks; i++) {
                const FLAC__StreamMetadata_CueSheet_Track *track = metadata->data.cue_sheet.tracks + i;

                if(!FLAC__bitwriter_write_raw_uint64(bw, track->offset, FLAC__STREAM_METADATA_CUESHEET_TRACK_OFFSET_LEN))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint32(bw, track->number, FLAC__STREAM_METADATA_CUESHEET_TRACK_NUMBER_LEN))
                    return false;
                CHOC_ASSERT(FLAC__STREAM_METADATA_CUESHEET_TRACK_ISRC_LEN % 8 == 0);
                if(!FLAC__bitwriter_write_byte_block(bw, (const FLAC__byte*)track->isrc, FLAC__STREAM_METADATA_CUESHEET_TRACK_ISRC_LEN/8))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint32(bw, track->type, FLAC__STREAM_METADATA_CUESHEET_TRACK_TYPE_LEN))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint32(bw, track->pre_emphasis, FLAC__STREAM_METADATA_CUESHEET_TRACK_PRE_EMPHASIS_LEN))
                    return false;
                if(!FLAC__bitwriter_write_zeroes(bw, FLAC__STREAM_METADATA_CUESHEET_TRACK_RESERVED_LEN))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint32(bw, track->num_indices, FLAC__STREAM_METADATA_CUESHEET_TRACK_NUM_INDICES_LEN))
                    return false;
                for(j = 0; j < track->num_indices; j++) {
                    const FLAC__StreamMetadata_CueSheet_Index *indx = track->indices + j;

                    if(!FLAC__bitwriter_write_raw_uint64(bw, indx->offset, FLAC__STREAM_METADATA_CUESHEET_INDEX_OFFSET_LEN))
                        return false;
                    if(!FLAC__bitwriter_write_raw_uint32(bw, indx->number, FLAC__STREAM_METADATA_CUESHEET_INDEX_NUMBER_LEN))
                        return false;
                    if(!FLAC__bitwriter_write_zeroes(bw, FLAC__STREAM_METADATA_CUESHEET_INDEX_RESERVED_LEN))
                        return false;
                }
            }
            break;
        case FLAC__METADATA_TYPE_PICTURE:
            {
                size_t len;
                if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.picture.type, FLAC__STREAM_METADATA_PICTURE_TYPE_LEN))
                    return false;
                len = strlen(metadata->data.picture.mime_type);
                if(!FLAC__bitwriter_write_raw_uint32(bw, len, FLAC__STREAM_METADATA_PICTURE_MIME_TYPE_LENGTH_LEN))
                    return false;
                if(!FLAC__bitwriter_write_byte_block(bw, (const FLAC__byte*)metadata->data.picture.mime_type, len))
                    return false;
                len = strlen((const char *)metadata->data.picture.description);
                if(!FLAC__bitwriter_write_raw_uint32(bw, len, FLAC__STREAM_METADATA_PICTURE_DESCRIPTION_LENGTH_LEN))
                    return false;
                if(!FLAC__bitwriter_write_byte_block(bw, metadata->data.picture.description, len))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.picture.width, FLAC__STREAM_METADATA_PICTURE_WIDTH_LEN))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.picture.height, FLAC__STREAM_METADATA_PICTURE_HEIGHT_LEN))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.picture.depth, FLAC__STREAM_METADATA_PICTURE_DEPTH_LEN))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.picture.colors, FLAC__STREAM_METADATA_PICTURE_COLORS_LEN))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint32(bw, metadata->data.picture.data_length, FLAC__STREAM_METADATA_PICTURE_DATA_LENGTH_LEN))
                    return false;
                if(!FLAC__bitwriter_write_byte_block(bw, metadata->data.picture.data, metadata->data.picture.data_length))
                    return false;
            }
            break;
        default:
            if(!FLAC__bitwriter_write_byte_block(bw, metadata->data.unknown.data, metadata->length))
                return false;
            break;
    }

    CHOC_ASSERT(FLAC__bitwriter_is_byte_aligned(bw));
    return true;
}

inline FLAC__bool FLAC__frame_add_header(const FLAC__FrameHeader *header, FLAC__BitWriter *bw)
{
    unsigned u, blocksize_hint, sample_rate_hint;
    FLAC__byte crc;

    CHOC_ASSERT(FLAC__bitwriter_is_byte_aligned(bw));

    if(!FLAC__bitwriter_write_raw_uint32(bw, FLAC__FRAME_HEADER_SYNC, FLAC__FRAME_HEADER_SYNC_LEN))
        return false;

    if(!FLAC__bitwriter_write_raw_uint32(bw, 0, FLAC__FRAME_HEADER_RESERVED_LEN))
        return false;

    if(!FLAC__bitwriter_write_raw_uint32(bw, (header->number_type == FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER)? 0 : 1, FLAC__FRAME_HEADER_BLOCKING_STRATEGY_LEN))
        return false;

    CHOC_ASSERT(header->blocksize > 0 && header->blocksize <= FLAC__MAX_BLOCK_SIZE);
    /* when this assertion holds true, any legal blocksize can be expressed in the frame header */
    CHOC_ASSERT(FLAC__MAX_BLOCK_SIZE <= 65535u);
    blocksize_hint = 0;
    switch(header->blocksize) {
        case   192: u = 1; break;
        case   576: u = 2; break;
        case  1152: u = 3; break;
        case  2304: u = 4; break;
        case  4608: u = 5; break;
        case   256: u = 8; break;
        case   512: u = 9; break;
        case  1024: u = 10; break;
        case  2048: u = 11; break;
        case  4096: u = 12; break;
        case  8192: u = 13; break;
        case 16384: u = 14; break;
        case 32768: u = 15; break;
        default:
            if(header->blocksize <= 0x100)
                blocksize_hint = u = 6;
            else
                blocksize_hint = u = 7;
            break;
    }
    if(!FLAC__bitwriter_write_raw_uint32(bw, u, FLAC__FRAME_HEADER_BLOCK_SIZE_LEN))
        return false;

    CHOC_ASSERT(FLAC__format_sample_rate_is_valid(header->sample_rate));
    sample_rate_hint = 0;
    switch(header->sample_rate) {
        case  88200: u = 1; break;
        case 176400: u = 2; break;
        case 192000: u = 3; break;
        case   8000: u = 4; break;
        case  16000: u = 5; break;
        case  22050: u = 6; break;
        case  24000: u = 7; break;
        case  32000: u = 8; break;
        case  44100: u = 9; break;
        case  48000: u = 10; break;
        case  96000: u = 11; break;
        default:
            if(header->sample_rate <= 255000 && header->sample_rate % 1000 == 0)
                sample_rate_hint = u = 12;
            else if(header->sample_rate % 10 == 0)
                sample_rate_hint = u = 14;
            else if(header->sample_rate <= 0xffff)
                sample_rate_hint = u = 13;
            else
                u = 0;
            break;
    }
    if(!FLAC__bitwriter_write_raw_uint32(bw, u, FLAC__FRAME_HEADER_SAMPLE_RATE_LEN))
        return false;

    CHOC_ASSERT(header->channels > 0 && header->channels <= (1u << FLAC__STREAM_METADATA_STREAMINFO_CHANNELS_LEN) && header->channels <= FLAC__MAX_CHANNELS);
    switch(header->channel_assignment) {
        case FLAC__CHANNEL_ASSIGNMENT_INDEPENDENT:
            u = header->channels - 1;
            break;
        case FLAC__CHANNEL_ASSIGNMENT_LEFT_SIDE:
            CHOC_ASSERT(header->channels == 2);
            u = 8;
            break;
        case FLAC__CHANNEL_ASSIGNMENT_RIGHT_SIDE:
            CHOC_ASSERT(header->channels == 2);
            u = 9;
            break;
        case FLAC__CHANNEL_ASSIGNMENT_MID_SIDE:
            CHOC_ASSERT(header->channels == 2);
            u = 10;
            break;
        default:
            CHOC_ASSERT(0);
    }
    if(!FLAC__bitwriter_write_raw_uint32(bw, u, FLAC__FRAME_HEADER_CHANNEL_ASSIGNMENT_LEN))
        return false;

    CHOC_ASSERT(header->bits_per_sample > 0 && header->bits_per_sample <= (1u << FLAC__STREAM_METADATA_STREAMINFO_BITS_PER_SAMPLE_LEN));
    switch(header->bits_per_sample) {
        case 8 : u = 1; break;
        case 12: u = 2; break;
        case 16: u = 4; break;
        case 20: u = 5; break;
        case 24: u = 6; break;
        default: u = 0; break;
    }
    if(!FLAC__bitwriter_write_raw_uint32(bw, u, FLAC__FRAME_HEADER_BITS_PER_SAMPLE_LEN))
        return false;

    if(!FLAC__bitwriter_write_raw_uint32(bw, 0, FLAC__FRAME_HEADER_ZERO_PAD_LEN))
        return false;

    if(header->number_type == FLAC__FRAME_NUMBER_TYPE_FRAME_NUMBER) {
        if(!FLAC__bitwriter_write_utf8_uint32(bw, header->number.frame_number))
            return false;
    }
    else {
        if(!FLAC__bitwriter_write_utf8_uint64(bw, header->number.sample_number))
            return false;
    }

    if(blocksize_hint)
        if(!FLAC__bitwriter_write_raw_uint32(bw, header->blocksize-1, (blocksize_hint==6)? 8:16))
            return false;

    switch(sample_rate_hint) {
        case 12:
            if(!FLAC__bitwriter_write_raw_uint32(bw, header->sample_rate / 1000, 8))
                return false;
            break;
        case 13:
            if(!FLAC__bitwriter_write_raw_uint32(bw, header->sample_rate, 16))
                return false;
            break;
        case 14:
            if(!FLAC__bitwriter_write_raw_uint32(bw, header->sample_rate / 10, 16))
                return false;
            break;
    }

    /* write the CRC */
    if(!FLAC__bitwriter_get_write_crc8(bw, &crc))
        return false;
    if(!FLAC__bitwriter_write_raw_uint32(bw, crc, FLAC__FRAME_HEADER_CRC_LEN))
        return false;

    return true;
}

inline FLAC__bool FLAC__subframe_add_constant(const FLAC__Subframe_Constant *subframe, unsigned subframe_bps, unsigned wasted_bits, FLAC__BitWriter *bw)
{
    FLAC__bool ok;

    ok =
        FLAC__bitwriter_write_raw_uint32(bw, FLAC__SUBFRAME_TYPE_CONSTANT_BYTE_ALIGNED_MASK | (wasted_bits? 1:0), FLAC__SUBFRAME_ZERO_PAD_LEN + FLAC__SUBFRAME_TYPE_LEN + FLAC__SUBFRAME_WASTED_BITS_FLAG_LEN) &&
        (wasted_bits? FLAC__bitwriter_write_unary_unsigned(bw, wasted_bits-1) : true) &&
        FLAC__bitwriter_write_raw_int32(bw, subframe->value, subframe_bps)
    ;

    return ok;
}

inline FLAC__bool FLAC__subframe_add_fixed(const FLAC__Subframe_Fixed *subframe, unsigned residual_samples, unsigned subframe_bps, unsigned wasted_bits, FLAC__BitWriter *bw)
{
    unsigned i;

    if(!FLAC__bitwriter_write_raw_uint32(bw, FLAC__SUBFRAME_TYPE_FIXED_BYTE_ALIGNED_MASK | (subframe->order<<1) | (wasted_bits? 1:0), FLAC__SUBFRAME_ZERO_PAD_LEN + FLAC__SUBFRAME_TYPE_LEN + FLAC__SUBFRAME_WASTED_BITS_FLAG_LEN))
        return false;
    if(wasted_bits)
        if(!FLAC__bitwriter_write_unary_unsigned(bw, wasted_bits-1))
            return false;

    for(i = 0; i < subframe->order; i++)
        if(!FLAC__bitwriter_write_raw_int32(bw, subframe->warmup[i], subframe_bps))
            return false;

    if(!add_entropy_coding_method_(bw, &subframe->entropy_coding_method))
        return false;
    switch(subframe->entropy_coding_method.type) {
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE:
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2:
            if(!add_residual_partitioned_rice_(
                bw,
                subframe->residual,
                residual_samples,
                subframe->order,
                subframe->entropy_coding_method.data.partitioned_rice.contents->parameters,
                subframe->entropy_coding_method.data.partitioned_rice.contents->raw_bits,
                subframe->entropy_coding_method.data.partitioned_rice.order,
                /*is_extended=*/subframe->entropy_coding_method.type == FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2
            ))
                return false;
            break;
        default:
            CHOC_ASSERT(0);
    }

    return true;
}

inline FLAC__bool FLAC__subframe_add_lpc(const FLAC__Subframe_LPC *subframe, unsigned residual_samples, unsigned subframe_bps, unsigned wasted_bits, FLAC__BitWriter *bw)
{
    unsigned i;

    if(!FLAC__bitwriter_write_raw_uint32(bw, FLAC__SUBFRAME_TYPE_LPC_BYTE_ALIGNED_MASK | ((subframe->order-1)<<1) | (wasted_bits? 1:0), FLAC__SUBFRAME_ZERO_PAD_LEN + FLAC__SUBFRAME_TYPE_LEN + FLAC__SUBFRAME_WASTED_BITS_FLAG_LEN))
        return false;
    if(wasted_bits)
        if(!FLAC__bitwriter_write_unary_unsigned(bw, wasted_bits-1))
            return false;

    for(i = 0; i < subframe->order; i++)
        if(!FLAC__bitwriter_write_raw_int32(bw, subframe->warmup[i], subframe_bps))
            return false;

    if(!FLAC__bitwriter_write_raw_uint32(bw, subframe->qlp_coeff_precision-1, FLAC__SUBFRAME_LPC_QLP_COEFF_PRECISION_LEN))
        return false;
    if(!FLAC__bitwriter_write_raw_int32(bw, subframe->quantization_level, FLAC__SUBFRAME_LPC_QLP_SHIFT_LEN))
        return false;
    for(i = 0; i < subframe->order; i++)
        if(!FLAC__bitwriter_write_raw_int32(bw, subframe->qlp_coeff[i], subframe->qlp_coeff_precision))
            return false;

    if(!add_entropy_coding_method_(bw, &subframe->entropy_coding_method))
        return false;
    switch(subframe->entropy_coding_method.type) {
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE:
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2:
            if(!add_residual_partitioned_rice_(
                bw,
                subframe->residual,
                residual_samples,
                subframe->order,
                subframe->entropy_coding_method.data.partitioned_rice.contents->parameters,
                subframe->entropy_coding_method.data.partitioned_rice.contents->raw_bits,
                subframe->entropy_coding_method.data.partitioned_rice.order,
                /*is_extended=*/subframe->entropy_coding_method.type == FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2
            ))
                return false;
            break;
        default:
            CHOC_ASSERT(0);
    }

    return true;
}

inline FLAC__bool FLAC__subframe_add_verbatim(const FLAC__Subframe_Verbatim *subframe, unsigned samples, unsigned subframe_bps, unsigned wasted_bits, FLAC__BitWriter *bw)
{
    unsigned i;
    const FLAC__int32 *signal = subframe->data;

    if(!FLAC__bitwriter_write_raw_uint32(bw, FLAC__SUBFRAME_TYPE_VERBATIM_BYTE_ALIGNED_MASK | (wasted_bits? 1:0), FLAC__SUBFRAME_ZERO_PAD_LEN + FLAC__SUBFRAME_TYPE_LEN + FLAC__SUBFRAME_WASTED_BITS_FLAG_LEN))
        return false;
    if(wasted_bits)
        if(!FLAC__bitwriter_write_unary_unsigned(bw, wasted_bits-1))
            return false;

    for(i = 0; i < samples; i++)
        if(!FLAC__bitwriter_write_raw_int32(bw, signal[i], subframe_bps))
            return false;

    return true;
}

inline FLAC__bool add_entropy_coding_method_(FLAC__BitWriter *bw, const FLAC__EntropyCodingMethod *method)
{
    if(!FLAC__bitwriter_write_raw_uint32(bw, method->type, FLAC__ENTROPY_CODING_METHOD_TYPE_LEN))
        return false;
    switch(method->type) {
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE:
        case FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2:
            if(!FLAC__bitwriter_write_raw_uint32(bw, method->data.partitioned_rice.order, FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ORDER_LEN))
                return false;
            break;
        default:
            CHOC_ASSERT(0);
    }
    return true;
}

inline FLAC__bool add_residual_partitioned_rice_(FLAC__BitWriter *bw, const FLAC__int32 residual[], const unsigned residual_samples, const unsigned predictor_order, const unsigned rice_parameters[], const unsigned raw_bits[], const unsigned partition_order, const FLAC__bool is_extended)
{
    const unsigned plen = is_extended? FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_PARAMETER_LEN : FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_PARAMETER_LEN;
    const unsigned pesc = is_extended? FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE2_ESCAPE_PARAMETER : FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_ESCAPE_PARAMETER;

    if(partition_order == 0) {
        unsigned i;

        if(raw_bits[0] == 0) {
            if(!FLAC__bitwriter_write_raw_uint32(bw, rice_parameters[0], plen))
                return false;
            if(!FLAC__bitwriter_write_rice_signed_block(bw, residual, residual_samples, rice_parameters[0]))
                return false;
        }
        else {
            CHOC_ASSERT(rice_parameters[0] == 0);
            if(!FLAC__bitwriter_write_raw_uint32(bw, pesc, plen))
                return false;
            if(!FLAC__bitwriter_write_raw_uint32(bw, raw_bits[0], FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_RAW_LEN))
                return false;
            for(i = 0; i < residual_samples; i++) {
                if(!FLAC__bitwriter_write_raw_int32(bw, residual[i], raw_bits[0]))
                    return false;
            }
        }
        return true;
    }
    else {
        unsigned i, j, k = 0, k_last = 0;
        unsigned partition_samples;
        const unsigned default_partition_samples = (residual_samples+predictor_order) >> partition_order;
        for(i = 0; i < (1u<<partition_order); i++) {
            partition_samples = default_partition_samples;
            if(i == 0)
                partition_samples -= predictor_order;
            k += partition_samples;
            if(raw_bits[i] == 0) {
                if(!FLAC__bitwriter_write_raw_uint32(bw, rice_parameters[i], plen))
                    return false;
                if(!FLAC__bitwriter_write_rice_signed_block(bw, residual+k_last, k-k_last, rice_parameters[i]))
                    return false;
            }
            else {
                if(!FLAC__bitwriter_write_raw_uint32(bw, pesc, plen))
                    return false;
                if(!FLAC__bitwriter_write_raw_uint32(bw, raw_bits[i], FLAC__ENTROPY_CODING_METHOD_PARTITIONED_RICE_RAW_LEN))
                    return false;
                for(j = k_last; j < k; j++) {
                    if(!FLAC__bitwriter_write_raw_int32(bw, residual[j], raw_bits[i]))
                        return false;
                }
            }
            k_last = k;
        }
        return true;
    }
}


#ifndef FLAC__INTEGER_ONLY_LIBRARY

inline void FLAC__window_bartlett(FLAC__real *window, const FLAC__int32 L)
{
    const FLAC__int32 N = L - 1;
    FLAC__int32 n;

    if (L & 1) {
        for (n = 0; n <= N/2; n++)
            window[n] = 2.0f * n / (float)N;
        for (; n <= N; n++)
            window[n] = 2.0f - 2.0f * n / (float)N;
    }
    else {
        for (n = 0; n <= L/2-1; n++)
            window[n] = 2.0f * n / (float)N;
        for (; n <= N; n++)
            window[n] = 2.0f - 2.0f * n / (float)N;
    }
}

inline void FLAC__window_bartlett_hann(FLAC__real *window, const FLAC__int32 L)
{
    const FLAC__int32 N = L - 1;
    FLAC__int32 n;

    for (n = 0; n < L; n++)
        window[n] = (FLAC__real)(0.62f - 0.48f * fabs((float)n/(float)N-0.5f) - 0.38f * cos(2.0f * M_PI * ((float)n/(float)N)));
}

inline void FLAC__window_blackman(FLAC__real *window, const FLAC__int32 L)
{
    const FLAC__int32 N = L - 1;
    FLAC__int32 n;

    for (n = 0; n < L; n++)
        window[n] = (FLAC__real)(0.42f - 0.5f * cos(2.0f * M_PI * n / N) + 0.08f * cos(4.0f * M_PI * n / N));
}

/* 4-term -92dB side-lobe */
inline void FLAC__window_blackman_harris_4term_92db_sidelobe(FLAC__real *window, const FLAC__int32 L)
{
    const FLAC__int32 N = L - 1;
    FLAC__int32 n;

    for (n = 0; n <= N; n++)
        window[n] = (FLAC__real)(0.35875f - 0.48829f * cos(2.0f * M_PI * n / N) + 0.14128f * cos(4.0f * M_PI * n / N) - 0.01168f * cos(6.0f * M_PI * n / N));
}

inline void FLAC__window_connes(FLAC__real *window, const FLAC__int32 L)
{
    const FLAC__int32 N = L - 1;
    const double N2 = (double)N / 2.;
    FLAC__int32 n;

    for (n = 0; n <= N; n++) {
        double k = ((double)n - N2) / N2;
        k = 1.0f - k * k;
        window[n] = (FLAC__real)(k * k);
    }
}

inline void FLAC__window_flattop(FLAC__real *window, const FLAC__int32 L)
{
    const FLAC__int32 N = L - 1;
    FLAC__int32 n;

    for (n = 0; n < L; n++)
        window[n] = (FLAC__real)(1.0f - 1.93f * cos(2.0f * M_PI * n / N) + 1.29f * cos(4.0f * M_PI * n / N) - 0.388f * cos(6.0f * M_PI * n / N) + 0.0322f * cos(8.0f * M_PI * n / N));
}

inline void FLAC__window_gauss(FLAC__real *window, const FLAC__int32 L, const FLAC__real stddev)
{
    const FLAC__int32 N = L - 1;
    const double N2 = (double)N / 2.;
    FLAC__int32 n;

    for (n = 0; n <= N; n++) {
        const double k = ((double)n - N2) / (stddev * N2);
        window[n] = (FLAC__real)exp(-0.5f * k * k);
    }
}

inline void FLAC__window_hamming(FLAC__real *window, const FLAC__int32 L)
{
    const FLAC__int32 N = L - 1;
    FLAC__int32 n;

    for (n = 0; n < L; n++)
        window[n] = (FLAC__real)(0.54f - 0.46f * cos(2.0f * M_PI * n / N));
}

inline void FLAC__window_hann(FLAC__real *window, const FLAC__int32 L)
{
    const FLAC__int32 N = L - 1;
    FLAC__int32 n;

    for (n = 0; n < L; n++)
        window[n] = (FLAC__real)(0.5f - 0.5f * cos(2.0f * M_PI * n / N));
}

inline void FLAC__window_kaiser_bessel(FLAC__real *window, const FLAC__int32 L)
{
    const FLAC__int32 N = L - 1;
    FLAC__int32 n;

    for (n = 0; n < L; n++)
        window[n] = (FLAC__real)(0.402f - 0.498f * cos(2.0f * M_PI * n / N) + 0.098f * cos(4.0f * M_PI * n / N) - 0.001f * cos(6.0f * M_PI * n / N));
}

inline void FLAC__window_nuttall(FLAC__real *window, const FLAC__int32 L)
{
    const FLAC__int32 N = L - 1;
    FLAC__int32 n;

    for (n = 0; n < L; n++)
        window[n] = (FLAC__real)(0.3635819f - 0.4891775f*cos(2.0f*M_PI*n/N) + 0.1365995f*cos(4.0f*M_PI*n/N) - 0.0106411f*cos(6.0f*M_PI*n/N));
}

inline void FLAC__window_rectangle(FLAC__real *window, const FLAC__int32 L)
{
    FLAC__int32 n;

    for (n = 0; n < L; n++)
        window[n] = 1.0f;
}

inline void FLAC__window_triangle(FLAC__real *window, const FLAC__int32 L)
{
    FLAC__int32 n;

    if (L & 1) {
        for (n = 1; n <= (L+1)/2; n++)
            window[n-1] = 2.0f * n / ((float)L + 1.0f);
        for (; n <= L; n++)
            window[n-1] = (float)(2 * (L - n + 1)) / ((float)L + 1.0f);
    }
    else {
        for (n = 1; n <= L/2; n++)
            window[n-1] = 2.0f * n / ((float)L + 1.0f);
        for (; n <= L; n++)
            window[n-1] = (float)(2 * (L - n + 1)) / ((float)L + 1.0f);
    }
}

inline void FLAC__window_tukey(FLAC__real *window, const FLAC__int32 L, const FLAC__real p)
{
    if (p <= 0.0)
        FLAC__window_rectangle(window, L);
    else if (p >= 1.0)
        FLAC__window_hann(window, L);
    else {
        const FLAC__int32 Np = (FLAC__int32)(p / 2.0f * L) - 1;
        FLAC__int32 n;
        /* start with rectangle... */
        FLAC__window_rectangle(window, L);
        /* ...replace ends with hann */
        if (Np > 0) {
            for (n = 0; n <= Np; n++) {
                window[n] = (FLAC__real)(0.5f - 0.5f * cos(M_PI * n / Np));
                window[L-Np-1+n] = (FLAC__real)(0.5f - 0.5f * cos(M_PI * (n+Np) / Np));
            }
        }
    }
}

inline void FLAC__window_partial_tukey(FLAC__real *window, const FLAC__int32 L, const FLAC__real p, const FLAC__real start, const FLAC__real end)
{
    const FLAC__int32 start_n = (FLAC__int32)(start * L);
    const FLAC__int32 end_n = (FLAC__int32)(end * L);
    const FLAC__int32 N = end_n - start_n;
    FLAC__int32 Np, n, i;

    if (p <= 0.0f)
        FLAC__window_partial_tukey(window, L, 0.05f, start, end);
    else if (p >= 1.0f)
        FLAC__window_partial_tukey(window, L, 0.95f, start, end);
    else {

        Np = (FLAC__int32)(p / 2.0f * N);

        for (n = 0; n < start_n && n < L; n++)
            window[n] = 0.0f;
        for (i = 1; n < (start_n+Np) && n < L; n++, i++)
            window[n] = (FLAC__real)(0.5f - 0.5f * cos(M_PI * i / Np));
        for (; n < (end_n-Np) && n < L; n++)
            window[n] = 1.0f;
        for (i = Np; n < end_n && n < L; n++, i--)
            window[n] = (FLAC__real)(0.5f - 0.5f * cos(M_PI * i / Np));
        for (; n < L; n++)
            window[n] = 0.0f;
    }
}

inline void FLAC__window_punchout_tukey(FLAC__real *window, const FLAC__int32 L, const FLAC__real p, const FLAC__real start, const FLAC__real end)
{
    const FLAC__int32 start_n = (FLAC__int32)(start * L);
    const FLAC__int32 end_n = (FLAC__int32)(end * L);
    FLAC__int32 Ns, Ne, n, i;

    if (p <= 0.0f)
        FLAC__window_punchout_tukey(window, L, 0.05f, start, end);
    else if (p >= 1.0f)
        FLAC__window_punchout_tukey(window, L, 0.95f, start, end);
    else {

        Ns = (FLAC__int32)(p / 2.0f * start_n);
        Ne = (FLAC__int32)(p / 2.0f * (L - end_n));

        for (n = 0, i = 1; n < Ns && n < L; n++, i++)
            window[n] = (FLAC__real)(0.5f - 0.5f * cos(M_PI * i / Ns));
        for (; n < start_n-Ns && n < L; n++)
            window[n] = 1.0f;
        for (i = Ns; n < start_n && n < L; n++, i--)
            window[n] = (FLAC__real)(0.5f - 0.5f * cos(M_PI * i / Ns));
        for (; n < end_n && n < L; n++)
            window[n] = 0.0f;
        for (i = 1; n < end_n+Ne && n < L; n++, i++)
            window[n] = (FLAC__real)(0.5f - 0.5f * cos(M_PI * i / Ne));
        for (; n < L - (Ne) && n < L; n++)
            window[n] = 1.0f;
        for (i = Ne; n < L; n++, i--)
            window[n] = (FLAC__real)(0.5f - 0.5f * cos(M_PI * i / Ne));
    }
}

inline void FLAC__window_welch(FLAC__real *window, const FLAC__int32 L)
{
    const FLAC__int32 N = L - 1;
    const double N2 = (double)N / 2.;
    FLAC__int32 n;

    for (n = 0; n <= N; n++) {
        const double k = ((double)n - N2) / N2;
        window[n] = (FLAC__real)(1.0f - k * k);
    }
}

#endif /* !defined FLAC__INTEGER_ONLY_LIBRARY */

} // namespace flac

#include "../platform/choc_ReenableAllWarnings.h"

//==============================================================================
//==============================================================================
//
// Finally back to choc code now....
//
//==============================================================================
//==============================================================================

template <bool supportWriting>
struct FLACAudioFileFormat<supportWriting>::Implementation
{
    struct FormatException {};
    [[noreturn]] static void throwFormatException()    { throw FormatException(); }

    //==============================================================================
    struct FLACReader : public AudioFileReader
    {
        FLACReader (std::shared_ptr<std::istream> s)  : stream (std::move (s))
        {
            decoder = flac::FLAC__stream_decoder_new();
        }

        ~FLACReader() override
        {
            flac::FLAC__stream_decoder_delete (decoder);
        }

        bool initialise()
        {
            stream->exceptions (std::istream::failbit);

            properties.formatName  = "FLAC";

            if (flac::FLAC__stream_decoder_init_stream (decoder,
                                                        readCallback, seekCallback, tellCallback, lengthCallback,
                                                        eofCallback, writeCallback, metadataCallback, errorCallback,
                                                        this) != flac::FLAC__STREAM_DECODER_INIT_STATUS_OK)
                return false;

            if (! flac::FLAC__stream_decoder_process_until_end_of_metadata (decoder))
                return false;

            if (errorOccurred || properties.sampleRate <= 0)
                return false;

            if (properties.numFrames == 0)
            {
                // If the metadata doesn't contain a valid length, this plods through
                // the whole file and counts the frames
                isDummyLengthScan = true;
                flac::FLAC__stream_decoder_process_until_end_of_stream (decoder);
                isDummyLengthScan = false;
                flac::FLAC__stream_decoder_reset (decoder);
                flac::FLAC__stream_decoder_process_until_end_of_metadata (decoder);
            }

            return true;
        }

        const AudioFileProperties& getProperties() override   { return properties; }

        bool readFrames (uint64_t frameIndex, choc::buffer::ChannelArrayView<float> buffer) override
        {
            return readFramesForType (frameIndex, buffer);
        }

        bool readFrames (uint64_t frameIndex, choc::buffer::ChannelArrayView<double> buffer) override
        {
            return readFramesForType (frameIndex, buffer);
        }

        template <typename FloatType>
        bool readFramesForType (uint64_t frameIndex, choc::buffer::ChannelArrayView<FloatType> buffer)
        {
            auto numChannels = buffer.getNumChannels();

            if (numChannels != properties.numChannels)
                return false;

            if (auto numFrames = buffer.getNumFrames())
            {
                if (frameIndex + numFrames > properties.numFrames)
                {
                    if (frameIndex >= properties.numFrames)
                    {
                        buffer.clear();
                        return true;
                    }

                    auto trailingEmptyFrames = static_cast<choc::buffer::FrameCount> (frameIndex + numFrames - properties.numFrames);
                    buffer.getEnd (trailingEmptyFrames).clear();
                    numFrames -= trailingEmptyFrames;
                    buffer = buffer.getStart (numFrames);
                }

                while (numFrames != 0)
                {
                    if (auto numRead = readFromCache (buffer, frameIndex))
                    {
                        frameIndex += numRead;
                        numFrames -= numRead;
                        buffer = buffer.fromFrame (numRead);
                    }

                    if (numFrames != 0 && ! fillCache (frameIndex))
                        return false;
                }
            }

            return true;
        }

        template <typename DestBufferType>
        uint32_t readFromCache (DestBufferType& destBuffer, uint64_t frameIndex)
        {
            if (frameIndex >= cacheStart && frameIndex < cacheStart + validCacheFrames)
            {
                auto startInCache = static_cast<uint32_t> (frameIndex - cacheStart);
                auto numFrames = std::min (destBuffer.getNumFrames(), static_cast<uint32_t> (cacheStart + validCacheFrames - frameIndex));
                using SampleType = typename DestBufferType::Sample;
                auto scale = static_cast<SampleType> (intToFloatScaleFactor);
                auto numChannels = properties.numChannels;

                for (uint32_t chan = 0; chan < numChannels; ++chan)
                {
                    auto src = getCacheChannel (chan) + startInCache;
                    auto dst = destBuffer.getChannel (chan).data.data;

                    for (uint32_t i = 0; i < numFrames; ++i)
                        dst[i] = scale * static_cast<SampleType> (src[i]);
                }

                return numFrames;
            }

            return 0;
        }

        bool fillCache (uint64_t frameIndex)
        {
            if (frameIndex != nextReadPosition)
            {
                FLAC__stream_decoder_seek_absolute (decoder, frameIndex);
                nextReadPosition = frameIndex;
            }

            validCacheFrames = 0;
            FLAC__stream_decoder_process_single (decoder);
            return validCacheFrames != 0;
        }

        void handleStreamInfo (const flac::FLAC__StreamMetadata_StreamInfo& info)
        {
            if (info.total_samples != 0) // (must avoid resetting a valid length after a dummy length scan)
                properties.numFrames = static_cast<uint64_t> (info.total_samples);

            properties.numChannels = static_cast<uint32_t> (info.channels);
            properties.sampleRate  = static_cast<double> (info.sample_rate);
            properties.bitDepth    = getIntegerBitDepth (static_cast<uint16_t> (info.bits_per_sample));

            intToFloatScaleFactor = 1.0 / ((1u << static_cast<uint16_t> (info.bits_per_sample - 1)) - 1);

            setCacheSize (static_cast<uint32_t> (info.max_blocksize));
        }

        void handleFrames (const int32_t* const* buffer, uint32_t numFrames)
        {
            if (isDummyLengthScan)
            {
                properties.numFrames += numFrames;
                return;
            }

            if (numFrames > numCacheFrames)
                setCacheSize (numFrames);

            for (uint32_t chan = 0; chan < properties.numChannels; ++chan)
            {
                CHOC_ASSERT (buffer[chan] != nullptr);
                std::memcpy (getCacheChannel (chan), buffer[chan], sizeof (int32_t) * numFrames);
            }

            validCacheFrames = numFrames;
            cacheStart = nextReadPosition;
            nextReadPosition += numFrames;
        }

        static flac::FLAC__StreamDecoderReadStatus readCallback (const flac::FLAC__StreamDecoder*, flac::FLAC__byte* dest, size_t* bytes, void* context)
        {
            if (static_cast<FLACReader*> (context)->errorOccurred)
                return flac::FLAC__STREAM_DECODER_READ_STATUS_ABORT;

            auto& s = *static_cast<FLACReader*> (context)->stream;

            try
            {
                s.read (reinterpret_cast<char*> (dest), static_cast<std::streamsize> (*bytes));
                return flac::FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
            }
            catch (...) {}

            if (s.eof())
            {
                s.clear();
                *bytes = (size_t) s.gcount();
            }

            return flac::FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
        }

        static flac::FLAC__StreamDecoderSeekStatus seekCallback (const flac::FLAC__StreamDecoder*, flac::FLAC__uint64 absolute_byte_offset, void* context)
        {
            if (static_cast<FLACReader*> (context)->errorOccurred)
                return flac::FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;

            auto& s = *static_cast<FLACReader*> (context)->stream;

            try
            {
                s.seekg (static_cast<std::istream::off_type> (absolute_byte_offset), std::ios_base::beg);
                return flac::FLAC__STREAM_DECODER_SEEK_STATUS_OK;
            }
            catch (...) {}

            s.clear();
            return flac::FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
        }

        static flac::FLAC__StreamDecoderTellStatus tellCallback (const flac::FLAC__StreamDecoder*, flac::FLAC__uint64* absolute_byte_offset, void* context)
        {
            auto& s = *static_cast<FLACReader*> (context)->stream;
            *absolute_byte_offset = static_cast<flac::FLAC__uint64> (s.tellg());
            return flac::FLAC__STREAM_DECODER_TELL_STATUS_OK;
        }

        static flac::FLAC__StreamDecoderLengthStatus lengthCallback (const flac::FLAC__StreamDecoder*, flac::FLAC__uint64* stream_length, void* context)
        {
            auto& s = *static_cast<FLACReader*> (context)->stream;

            try
            {
                auto currentPos = s.tellg();
                s.seekg (0, std::ios::end);
                auto end = s.tellg();
                s.seekg (currentPos);
                *stream_length = static_cast<flac::FLAC__uint64> (end);
                return flac::FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
            }
            catch (...) {}

            return flac::FLAC__STREAM_DECODER_LENGTH_STATUS_ERROR;
        }

        static flac::FLAC__bool eofCallback (const flac::FLAC__StreamDecoder*, void* context)
        {
            auto& s = *static_cast<FLACReader*> (context)->stream;
            return s.eof();
        }

        static flac::FLAC__StreamDecoderWriteStatus writeCallback (const flac::FLAC__StreamDecoder*, const flac::FLAC__Frame* frame, const flac::FLAC__int32* const* buffer, void* context)
        {
            static_cast<FLACReader*> (context)->handleFrames (buffer, static_cast<uint32_t> (frame->header.blocksize));
            return flac::FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
        }

        static void metadataCallback (const flac::FLAC__StreamDecoder*, const flac::FLAC__StreamMetadata* metadata, void* context)
        {
            static_cast<FLACReader*> (context)->handleStreamInfo (metadata->data.stream_info);
        }

        static void errorCallback (const flac::FLAC__StreamDecoder*, flac::FLAC__StreamDecoderErrorStatus, void* context)
        {
            static_cast<FLACReader*> (context)->errorOccurred = true;
        }

        void setCacheSize (uint32_t numFrames)
        {
            numCacheFrames = numFrames;
            cache.resize (properties.numChannels * numFrames);
        }

        int32_t* getCacheChannel (uint32_t channel)         { return cache.data() + numCacheFrames * channel; }

        std::shared_ptr<std::istream> stream;
        AudioFileProperties properties;

        flac::FLAC__StreamDecoder* decoder = {};
        bool isDummyLengthScan = false;
        bool errorOccurred = false;
        double intToFloatScaleFactor = 0;

        std::vector<int32_t> cache;
        uint64_t nextReadPosition = 0, cacheStart = 0;
        uint32_t numCacheFrames = 0, validCacheFrames = 0;
    };

    //==============================================================================
    //==============================================================================
    struct FLACWriter   : public AudioFileWriter
    {
        FLACWriter (std::shared_ptr<std::ostream> s, AudioFileProperties&& p)
            : stream (std::move (s)), properties (std::move (p))
        {
            CHOC_ASSERT (stream != nullptr && ! stream->fail());
            CHOC_ASSERT (properties.sampleRate > 0 && properties.numChannels != 0 && properties.bitDepth != BitDepth::unknown);
            stream->exceptions (std::ostream::failbit);
            originalStreamPos = stream->tellp();

            encoder = flac::FLAC__stream_encoder_new();

            if (auto q = getQualityIndex(); q >= 0)
                FLAC__stream_encoder_set_compression_level (encoder, static_cast<unsigned> (q));

            FLAC__stream_encoder_set_channels (encoder, properties.numChannels);
            FLAC__stream_encoder_set_bits_per_sample (encoder, getBytesPerSample (properties.bitDepth) * 8u);
            FLAC__stream_encoder_set_sample_rate (encoder, static_cast<unsigned> (properties.sampleRate));
            FLAC__stream_encoder_set_blocksize (encoder, 0);

            FLAC__stream_encoder_set_do_escape_coding (encoder, true);
            FLAC__stream_encoder_set_do_mid_side_stereo (encoder, properties.numChannels == 2u);
            FLAC__stream_encoder_set_loose_mid_side_stereo (encoder, properties.numChannels == 2u);

            if (FLAC__stream_encoder_init_stream (encoder,
                                                  writeCallback, seekCallback, encodeTellCallback, metadataCallback,
                                                  this) != flac::FLAC__STREAM_ENCODER_INIT_STATUS_OK)
                deleteEncoder();

            cache.resize (properties.numChannels * cacheNumFrames);
            cacheChannels.resize (properties.numChannels);

            for (uint32_t i = 0; i < properties.numChannels; ++i)
                cacheChannels[i] = cache.data() + cacheNumFrames * i;

            auto bits = encoder->protected_->bits_per_sample;
            CHOC_ASSERT (bits != 0);
            floatToIntScaleFactor = static_cast<double> ((1u << (31u - bits)) - 1);
        }

        ~FLACWriter() override
        {
            if (encoder != nullptr)
            {
                flac::FLAC__stream_encoder_finish (encoder);
                deleteEncoder();
            }

            stream.reset();
        }

        void deleteEncoder()
        {
            if (encoder != nullptr)
            {
                flac::FLAC__stream_encoder_delete (encoder);
                encoder = nullptr;
            }
        }

        const AudioFileProperties& getProperties() override    { return properties; }

        bool appendFrames (choc::buffer::ChannelArrayView<const float>  source) override { return appendFramesForType (source); }
        bool appendFrames (choc::buffer::ChannelArrayView<const double> source) override { return appendFramesForType (source); }

        template <typename FloatType>
        bool appendFramesForType (choc::buffer::ChannelArrayView<const FloatType> source)
        {
            auto numChannels = source.getNumChannels();

            if (encoder == nullptr || numChannels != properties.numChannels)
                return false;

            try
            {
                auto framesToDo = source.getNumFrames();
                properties.numFrames += framesToDo;
                auto scale = static_cast<FloatType> (floatToIntScaleFactor);

                while (framesToDo != 0)
                {
                    auto framesThisTime = std::min (framesToDo, cacheNumFrames);

                    for (uint32_t chan = 0; chan < numChannels; ++chan)
                    {
                        auto src = source.getChannel (chan).data.data;
                        auto dst = cacheChannels[chan];

                        for (uint32_t i = 0; i < framesThisTime; ++i)
                        {
                            auto s = src[i];
                            if (s < -1.0)     s = -1.0;
                            else if (s > 1.0) s = 1.0;

                            dst[i] = static_cast<int32_t> (scale * s);
                        }
                    }

                    FLAC__stream_encoder_process (encoder, cacheChannels.data(), (unsigned) framesThisTime);

                    framesToDo -= framesThisTime;
                    source = source.fromFrame (framesThisTime);
                }

                return true;
            }
            catch (...) {}

            return false;
        }

        bool flush() override
        {
            if (encoder != nullptr)
            {
                stream->flush();
                return true;
            }

            return false;
        }

        int getQualityIndex() const
        {
            auto p = choc::text::trim (properties.quality);

            if (p.empty())
                return -1;

            auto number = std::stoi (p);
            return (number >= 0 && number <= 8) ? number : -1;
        }

        void writeStreamInfo (const flac::FLAC__StreamMetadata_StreamInfo& info)
        {
            auto writeInt = [this] (uint32_t n, uint32_t numBytes)
            {
                uint8_t data[8];

                for (auto i = numBytes; i > 0;)
                {
                    data[--i] = static_cast<uint8_t> (n);
                    n >>= 8;
                }

                write (data, numBytes);
            };

            stream->seekp (static_cast<std::ostream::pos_type> (static_cast<int64_t> (originalStreamPos) + 4));

            writeInt (flac::FLAC__STREAM_METADATA_STREAMINFO_LENGTH, 4);
            writeInt (info.min_blocksize, 2);
            writeInt (info.max_blocksize, 2);
            writeInt (info.min_framesize, 3);
            writeInt (info.max_framesize, 3);

            auto bitsMinus1 = info.bits_per_sample - 1;

            uint8_t format[4] = { static_cast<uint8_t> (info.sample_rate >> 12),
                                  static_cast<uint8_t> (info.sample_rate >> 4),
                                  static_cast<uint8_t> (((info.sample_rate & 15) << 4) | ((info.channels - 1) << 1) | (bitsMinus1 >> 4)),
                                  static_cast<uint8_t> (((bitsMinus1 & 15) << 4) | ((info.total_samples >> 32) & 15)) };
            write (format, 4);

            writeInt (static_cast<uint32_t> (info.total_samples), 4);
            write (info.md5sum, 16);
        }

        //==============================================================================
        static flac::FLAC__StreamEncoderWriteStatus writeCallback (const flac::FLAC__StreamEncoder*,
                                                                   const flac::FLAC__byte* buffer, size_t size,
                                                                   unsigned int, unsigned int, void* context)
        {
            return static_cast<FLACWriter*> (context)->write (buffer, size) ? flac::FLAC__STREAM_ENCODER_WRITE_STATUS_OK
                                                                            : flac::FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;
        }

        static flac::FLAC__StreamEncoderSeekStatus seekCallback (const flac::FLAC__StreamEncoder*, flac::FLAC__uint64, void*)
        {
            return flac::FLAC__STREAM_ENCODER_SEEK_STATUS_UNSUPPORTED;
        }

        static flac::FLAC__StreamEncoderTellStatus encodeTellCallback (const flac::FLAC__StreamEncoder*, flac::FLAC__uint64* absolute_byte_offset, void* context)
        {
            *absolute_byte_offset = static_cast<flac::FLAC__uint64> (static_cast<FLACWriter*> (context)->stream->tellp());
            return flac::FLAC__STREAM_ENCODER_TELL_STATUS_OK;
        }

        static void metadataCallback (const flac::FLAC__StreamEncoder*, const flac::FLAC__StreamMetadata* metadata, void* context)
        {
            static_cast<FLACWriter*> (context)->writeStreamInfo (metadata->data.stream_info);
        }

        bool write (const void* data, size_t size)
        {
            try
            {
                stream->write (static_cast<const char*> (data), static_cast<std::streamsize> (size));
                return true;
            }
            catch (...) {}

            return false;
        }

        std::shared_ptr<std::ostream> stream;
        std::ostream::pos_type originalStreamPos = 0;
        AudioFileProperties properties;
        flac::FLAC__StreamEncoder* encoder = nullptr;
        double floatToIntScaleFactor = 0;
        std::vector<int32_t> cache;
        std::vector<int32_t*> cacheChannels;
        static constexpr uint32_t cacheNumFrames = 1024;
    };
};

//==============================================================================
template <bool supportWriting>
std::string FLACAudioFileFormat<supportWriting>::getFileSuffixes()
{
    return "flac";
}

template <bool supportWriting>
std::vector<double> FLACAudioFileFormat<supportWriting>::getSupportedSampleRates()
{
    return { 8000, 16000, 22050, 24000, 32000, 44100, 48000, 88200, 176400, 192000 };
}

template <bool supportWriting>
uint32_t FLACAudioFileFormat<supportWriting>::getMaximumNumChannels()
{
    return 256;
}

template <bool supportWriting>
std::vector<BitDepth> FLACAudioFileFormat<supportWriting>::getSupportedBitDepths()
{
    return { BitDepth::int16, BitDepth::int24 };
}

template <bool supportWriting>
std::vector<std::string> FLACAudioFileFormat<supportWriting>::getQualityLevels()
{
    return { "0 (Fastest)", "1", "2", "3", "4", "5", "6", "7", "8 (Smallest)" };
}

template <bool supportWriting>
bool FLACAudioFileFormat<supportWriting>::supportsWriting() const        { return supportWriting; }

template <bool supportWriting>
std::unique_ptr<AudioFileReader> FLACAudioFileFormat<supportWriting>::createReader (std::shared_ptr<std::istream> s)
{
    if (s == nullptr || s->fail())
        return {};

    auto r = std::make_unique<typename Implementation::FLACReader> (std::move (s));

    if (r->initialise())
        return r;

    return {};
}

template <bool supportWriting>
std::unique_ptr<AudioFileWriter> FLACAudioFileFormat<supportWriting>::createWriter (std::shared_ptr<std::ostream> s,
                                                                                    AudioFileProperties p)
{
    if constexpr (supportWriting)
    {
        auto w = std::make_unique<typename Implementation::FLACWriter> (std::move (s), std::move (p));

        if (w->encoder != nullptr)
            return w;
    }

    return {};
}

} // namespace choc::audio

#endif // CHOC_AUDIOFILEFORMAT_FLAC_HEADER_INCLUDED
