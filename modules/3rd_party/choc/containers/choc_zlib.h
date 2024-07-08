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

#ifndef CHOC_GZIPDECOMPRESS_HEADER_INCLUDED
#define CHOC_GZIPDECOMPRESS_HEADER_INCLUDED

#include <memory>
#include <iostream>
#include <streambuf>
#include <vector>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../platform/choc_Assert.h"
#include "../text/choc_OpenSourceLicenseList.h"

namespace choc::zlib
{

/// This std::istream decompresses data that was written with zlib or gzip.
/// @see DeflaterStream
class InflaterStream   : public std::istream,
                         private std::streambuf
{
public:
    enum class FormatType
    {
        zlib = 0,
        deflate,
        gzip
    };

    InflaterStream (std::shared_ptr<std::istream> compressedData,
                    FormatType);

    ~InflaterStream() override;

    using pos_type = std::streambuf::pos_type;
    using off_type = std::streambuf::off_type;
    using char_type = std::streambuf::char_type;

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    pos_type seekoff (off_type, std::ios_base::seekdir, std::ios_base::openmode) override;
    pos_type seekpos (pos_type, std::ios_base::openmode) override;
    std::streambuf::int_type underflow() override;
    pos_type getPosition() const;
};

//==============================================================================
/// This std::ostream writes compressed data to a stream, which can be later
/// decompressed with the InflaterStream.
/// @see InflaterStream
class DeflaterStream  : public std::ostream,
                        private std::streambuf
{
public:
    enum CompressionLevel
    {
        none          = 0,
        fastest       = 1,
        medium        = 6,
        best          = 9,
        defaultLevel  = -1
    };

    DeflaterStream (std::shared_ptr<std::ostream> destData,
                    CompressionLevel compressionLevel,
                    int windowBits);
    ~DeflaterStream() override;

    using pos_type = std::streambuf::pos_type;
    using off_type = std::streambuf::off_type;
    using char_type = std::streambuf::char_type;

private:
    struct Pimpl;
    std::unique_ptr<Pimpl> pimpl;

    int overflow (int) override;
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

struct zlib
{

// This struct contains a very very mangled version of good old zlib,
// where I've purged almost all the macros and global definitions. Keeping
// all the symbols and nastiness locked inside this single struct means
// that it should play nice in a header-only world.
// I even started trying to improve some of the appalling variable names,
// but honestly, life's too short..

CHOC_REGISTER_OPEN_SOURCE_LICENCE (QuickJS, R"(
==============================================================================
ZLIB License:

Copyright (C) 1995-2005 Jean-loup Gailly and Mark Adler

This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.

Jean-loup Gailly        Mark Adler
jloup@gzip.org          madler@alumni.caltech.edu

The data format used by the zlib library is described by RFCs (Request for
Comments) 1950 to 1952 in the files http://www.ietf.org/rfc/rfc1950.txt
(zlib format), rfc1951.txt (deflate format) and rfc1952.txt (gzip format).
)")

static constexpr auto ZLIB_VERSION = "1.2.3";

static constexpr const char deflate_copyright[] = " deflate 1.2.3 Copyright 1995-2005 Jean-loup Gailly ";
static constexpr const char inflate_copyright[] = " inflate 1.2.3 Copyright 1995-2005 Mark Adler ";

enum class ErrorCode  : int
{
    OK                  = 0,
    STREAM_END          = 1,
    NEED_DICT           = 2,
    ERRNO               = -1,
    STREAM_ERROR        = -2,
    DATA_ERROR          = -3,
    MEM_ERROR           = -4,
    BUF_ERROR           = -5,
    VERSION_ERROR       = -6
};

enum class DataType  : uint32_t
{
    Z_BINARY   = 0,
    Z_TEXT     = 1,
    Z_UNKNOWN  = 2
};

static constexpr uint8_t Z_DEFLATED = 8;
static constexpr int MAX_WBITS = 15;
static constexpr int MAX_MEM_LEVEL = 9;
static constexpr int DEF_MEM_LEVEL = 8;

static constexpr int MIN_MATCH = 3;
static constexpr int MAX_MATCH = 258;
static constexpr int MIN_LOOKAHEAD = MAX_MATCH + MIN_MATCH + 1;

enum class FlushState
{
    Z_NO_FLUSH      = 0,
    Z_PARTIAL_FLUSH = 1, /* will be removed, use Z_SYNC_FLUSH instead */
    Z_SYNC_FLUSH    = 2,
    Z_FULL_FLUSH    = 3,
    Z_FINISH        = 4,
    Z_BLOCK         = 5,
    unknown         = -1
};

//==============================================================================
/*
     gzip header information passed to and from zlib routines.  See RFC 1952
  for more details on the meanings of these fields.
*/
struct gz_header
{
    int text;       /* true if compressed data believed to be text */
    uint64_t time;       /* modification time */
    int xflags;     /* extra flags (not used when writing a gzip file) */
    int os;         /* operating system */
    uint8_t* extra;     /* pointer to extra field or nullptr if none */
    uint32_t extra_len;  /* extra field length (valid if extra != nullptr) */
    uint32_t extra_max;  /* space at extra (only when reading header) */
    uint8_t* name;      /* pointer to zero-terminated file name or nullptr */
    uint32_t name_max;   /* space at name (only when reading header) */
    uint8_t* comment;   /* pointer to zero-terminated comment or nullptr */
    uint32_t comm_max;   /* space at comment (only when reading header) */
    int hcrc;       /* true if there was or will be a header crc */
    int done;       /* true when done reading gzip header (not used
                           when writing a gzip file) */
};

//==============================================================================
static constexpr uint32_t LENGTH_CODES = 29;
static constexpr uint32_t LITERALS = 256;
static constexpr uint32_t L_CODES = LITERALS + 1 + LENGTH_CODES;
static constexpr uint32_t D_CODES  = 30;
static constexpr uint32_t BL_CODES = 19;

/* Data structure describing a single value and its code string. */
struct ct_data
{
    union {
        uint16_t  freq;       /* frequency count */
        uint16_t  code;       /* bit string */
    } fc;

    union {
        uint16_t  dad;        /* father node in Huffman tree */
        uint16_t  len;        /* length of bit string */
    } dl;
};

struct static_tree_desc
{
    const ct_data* static_tree;  /* static tree or nullptr */
    const uint32_t* extra_bits;      /* extra bits for each code or nullptr */
    int extra_base;          /* base index for extra_bits */
    int elems;               /* max number of elements in the tree */
    uint32_t max_length;          /* max bit length for the codes */
};

struct tree_desc
{
    ct_data* dyn_tree;           /* the dynamic tree */
    uint32_t max_code;            /* largest code with non zero frequency */
    const static_tree_desc* stat_desc; /* the corresponding static tree */
};

struct code
{
    uint8_t op;           /* operation, extra bits, table bits */
    uint8_t bits;         /* bits in this part of the code */
    uint16_t val;         /* offset in table or code value */
};

//==============================================================================
struct DeflateStream
{
    uint8_t* next_in = nullptr;
    uint32_t avail_in = 0;
    size_t total_in = 0;

    uint8_t* next_out = nullptr;
    uint32_t avail_out = 0;
    size_t total_out = 0;

    //==============================================================================
    ErrorCode initialise (int compressionLevel, uint8_t methodToUse, int windowBits, int memLevel)
    {
        wrap = 1;
        msg = nullptr;

        if (windowBits < 0)
        {
            wrap = 0;
            windowBits = -windowBits;
        }
        else if (windowBits > 15)
        {
            wrap = 2; /* write gzip wrapper instead */
            windowBits -= 16;
        }

        if (memLevel < 1 || memLevel > MAX_MEM_LEVEL
            || methodToUse != Z_DEFLATED
            || windowBits < 8 || windowBits > 15
            || compressionLevel < 0 || compressionLevel > 9)
            return ErrorCode::STREAM_ERROR;

        if (windowBits == 8)
            windowBits = 9; /* until 256-byte window bug fixed */

        gzhead = nullptr;
        w_bits = (uint32_t) windowBits;
        w_size = 1U << w_bits;
        w_mask = w_size - 1;

        hash_bits = (uint32_t) memLevel + 7;
        hash_size = 1U << hash_bits;
        hash_mask = hash_size - 1;
        hash_shift = ((hash_bits + MIN_MATCH - 1) / MIN_MATCH);

        window.resize (w_size * 2);
        prev.resize (w_size);
        head.resize (hash_size);

        lit_bufsize = 1U << (memLevel + 6); /* 16K elements by default */

        pending_buf.resize (lit_bufsize * (sizeof (uint16_t) + 2));

        if (window.empty() || prev.empty() || head.empty() || pending_buf.empty())
        {
            status = Status::FINISH_STATE;
            setError (ErrorCode::MEM_ERROR);
            return ErrorCode::MEM_ERROR;
        }

        d_buf = reinterpret_cast<uint16_t*> (pending_buf.data() + lit_bufsize);
        l_buf = pending_buf.data() + (1 + sizeof (uint16_t)) * lit_bufsize;

        level = compressionLevel;
        method = methodToUse;

        total_in = total_out = 0;
        msg = nullptr;
        data_type = DataType::Z_UNKNOWN;

        pending = 0;
        pending_out = pending_buf.data();

        if (wrap < 0)
            wrap = -wrap; /* was made negative by deflate(..., Z_FINISH); */

        status = wrap ? Status::INIT_STATE : Status::BUSY_STATE;
        adler = wrap == 2 ? Checksum::crc32 (0, nullptr, 0)
                          : Checksum::adler32 (0, nullptr, 0);
        last_flush = FlushState::Z_NO_FLUSH;
        initialiseTrees();
        initLongestMatch();

        return ErrorCode::OK;
    }

    ErrorCode deflateParams (int compressionLevel)
    {
        compress_func func;
        auto err = ErrorCode::OK;

        if (compressionLevel < 0 || compressionLevel > 9)
            return ErrorCode::STREAM_ERROR;

        func = compressionConfigs[level].func;

        if (func != compressionConfigs[compressionLevel].func && total_in != 0)
            err = deflate (FlushState::Z_PARTIAL_FLUSH);

        if (level != compressionLevel)
        {
            level = compressionLevel;
            max_lazy_match = compressionConfigs[compressionLevel].max_lazy;
            good_match = compressionConfigs[compressionLevel].good_length;
            nice_match = compressionConfigs[compressionLevel].nice_length;
            max_chain_length = compressionConfigs[compressionLevel].max_chain;
        }

        return err;
    }

    ErrorCode deflate (FlushState flush)
    {
        if (next_out == nullptr || (next_in == nullptr && avail_in != 0)
            || (status == Status::FINISH_STATE && flush != FlushState::Z_FINISH))
            return setError (ErrorCode::STREAM_ERROR);

        if (avail_out == 0)
            return setError (ErrorCode::BUF_ERROR);

        auto old_flush = last_flush;
        last_flush = flush;

        if (status == Status::INIT_STATE)
        {
            if (wrap == 2)
            {
                adler = Checksum::crc32 (0, nullptr, 0);
                putByte (31);
                putByte (139);
                putByte (8);

                if (gzhead == nullptr)
                {
                    putByte (0);
                    putByte (0);
                    putByte (0);
                    putByte (0);
                    putByte (0);
                    putByte (level == 9 ? 2 : (level < 2 ? 4 : 0));
                    putByte (OS_CODE);
                    status = Status::BUSY_STATE;
                }
                else
                {
                    putByte ((gzhead->text ? 1 : 0) + (gzhead->hcrc ? 2 : 0) + (gzhead->extra == nullptr ? 0 : 4) + (gzhead->name == nullptr ? 0 : 8) + (gzhead->comment == nullptr ? 0 : 16));
                    putInt32 (gzhead->time);
                    putByte (level == 9 ? 2 : (level < 2 ? 4 : 0));
                    putByte (gzhead->os);

                    if (gzhead->extra != nullptr)
                        putInt16 (gzhead->extra_len);

                    if (gzhead->hcrc)
                        adler = Checksum::crc32 (adler, pending_buf.data(), pending);

                    gzindex = 0;
                    status = Status::EXTRA_STATE;
                }
            }
            else
            {
                uint32_t header = (Z_DEFLATED + ((w_bits - 8) << 4)) << 8;
                uint32_t level_flags;

                if (level < 2)
                    level_flags = 0;
                else if (level < 6)
                    level_flags = 1;
                else if (level == 6)
                    level_flags = 2;
                else
                    level_flags = 3;
                header |= (level_flags << 6);
                if (strstart != 0)
                    header |= 0x20; // preset dictionary flag
                header += 31 - (header % 31);

                status = Status::BUSY_STATE;

                putInt16MSB (header);

                if (strstart != 0)
                    putInt32MSB (adler);

                adler = Checksum::adler32 (0, nullptr, 0);
            }
        }

        if (status == Status::EXTRA_STATE)
        {
            if (gzhead->extra != nullptr)
            {
                uint32_t beg = pending; /* start of bytes to update crc */

                while (gzindex < (gzhead->extra_len & 0xffff))
                {
                    if (pending == pending_buf.size())
                    {
                        if (gzhead->hcrc && pending > beg)
                            adler = Checksum::crc32 (adler, pending_buf.data() + beg, pending - beg);
                        flushPending();
                        beg = pending;
                        if (pending == pending_buf.size())
                            break;
                    }
                    putByte (gzhead->extra[gzindex]);
                    gzindex++;
                }
                if (gzhead->hcrc && pending > beg)
                    adler = Checksum::crc32 (adler, pending_buf.data() + beg, pending - beg);
                if (gzindex == gzhead->extra_len)
                {
                    gzindex = 0;
                    status = Status::NAME_STATE;
                }
            }
            else
                status = Status::NAME_STATE;
        }

        if (status == Status::NAME_STATE)
        {
            if (gzhead->name != nullptr)
            {
                uint32_t beg = pending; /* start of bytes to update crc */
                int val;

                do
                {
                    if (pending == pending_buf.size())
                    {
                        if (gzhead->hcrc && pending > beg)
                            adler = Checksum::crc32 (adler, pending_buf.data() + beg, pending - beg);
                        flushPending();
                        beg = pending;
                        if (pending == pending_buf.size())
                        {
                            val = 1;
                            break;
                        }
                    }
                    val = gzhead->name[gzindex++];
                    putByte (val);
                } while (val != 0);
                if (gzhead->hcrc && pending > beg)
                    adler = Checksum::crc32 (adler, pending_buf.data() + beg, pending - beg);
                if (val == 0)
                {
                    gzindex = 0;
                    status = Status::COMMENT_STATE;
                }
            }
            else
                status = Status::COMMENT_STATE;
        }

        if (status == Status::COMMENT_STATE)
        {
            if (gzhead->comment != nullptr)
            {
                uint32_t beg = pending; /* start of bytes to update crc */
                int val;

                do
                {
                    if (pending == pending_buf.size())
                    {
                        if (gzhead->hcrc && pending > beg)
                            adler = Checksum::crc32 (adler, pending_buf.data() + beg, pending - beg);
                        flushPending();
                        beg = pending;
                        if (pending == pending_buf.size())
                        {
                            val = 1;
                            break;
                        }
                    }
                    val = gzhead->comment[gzindex++];
                    putByte (val);
                } while (val != 0);

                if (gzhead->hcrc && pending > beg)
                    adler = Checksum::crc32 (adler, pending_buf.data() + beg, pending - beg);
                if (val == 0)
                    status = Status::HCRC_STATE;
            }
            else
                status = Status::HCRC_STATE;
        }

        if (status == Status::HCRC_STATE)
        {
            if (gzhead->hcrc)
            {
                if (pending + 2 > pending_buf.size())
                    flushPending();

                if (pending + 2 <= pending_buf.size())
                {
                    putInt16 (adler);
                    adler = Checksum::crc32 (0, nullptr, 0);
                    status = Status::BUSY_STATE;
                }
            }
            else
                status = Status::BUSY_STATE;
        }

        if (pending != 0)
        {
            flushPending();

            if (avail_out == 0)
            {
                /* Since avail_out is 0, deflate will be called again with
                * more output space, but possibly with both pending and
                * avail_in equal to zero. There won't be anything to do,
                * but this is not an error situation so make sure we
                * return OK instead of BUF_ERROR at next call of deflate:
                */
                last_flush = FlushState::unknown;
                return ErrorCode::OK;
            }
        }
        else if (avail_in == 0 && flush <= old_flush && flush != FlushState::Z_FINISH)
        {
            return setError (ErrorCode::BUF_ERROR);
        }

        /* User must not provide more input after the first FINISH: */
        if (status == Status::FINISH_STATE && avail_in != 0)
            return setError (ErrorCode::BUF_ERROR);

        if (avail_in != 0 || lookahead != 0 || (flush != FlushState::Z_NO_FLUSH && status != Status::FINISH_STATE))
        {
            auto blockStatus = (*(compressionConfigs[level].func)) (this, flush);

            if (blockStatus == BlockStatus::finish_started || blockStatus == BlockStatus::finish_done)
                status = Status::FINISH_STATE;

            if (blockStatus == BlockStatus::need_more || blockStatus == BlockStatus::finish_started)
            {
                if (avail_out == 0)
                    last_flush = FlushState::unknown; /* avoid BUF_ERROR next call, see above */

                return ErrorCode::OK;
            }

            if (blockStatus == BlockStatus::block_done)
            {
                if (flush == FlushState::Z_PARTIAL_FLUSH)
                {
                    alignBuffer();
                }
                else
                {
                    sendStoredBlock (nullptr, 0L, false);

                    if (flush == FlushState::Z_FULL_FLUSH)
                        clearHash();
                }

                flushPending();

                if (avail_out == 0)
                {
                    last_flush = FlushState::unknown; /* avoid BUF_ERROR at next call, see above */
                    return ErrorCode::OK;
                }
            }
        }

        CHOC_ASSERT (avail_out > 0);

        if (flush != FlushState::Z_FINISH)
            return ErrorCode::OK;

        if (wrap <= 0)
            return ErrorCode::STREAM_END;

        if (wrap == 2)
        {
            putInt32 (adler);
            putInt32 (total_in);
        }
        else
        {
            putInt32MSB (adler);
        }

        flushPending();

        if (wrap > 0)
            wrap = -wrap;

        return pending != 0 ? ErrorCode::OK : ErrorCode::STREAM_END;
    }

private:
    const char* msg = nullptr;

    DataType data_type = DataType::Z_BINARY;
    unsigned long adler = 0;

    enum class Status
    {
        INIT_STATE,
        EXTRA_STATE,
        NAME_STATE,
        COMMENT_STATE,
        HCRC_STATE,
        BUSY_STATE,
        FINISH_STATE
    };

    Status status = Status::INIT_STATE;

    std::vector<uint8_t> pending_buf;
    uint8_t* pending_out = nullptr;
    uint32_t pending = 0;
    int wrap = 0;
    gz_header* gzhead = nullptr;
    uint32_t gzindex = 0;
    uint8_t  method = 0;
    FlushState last_flush = FlushState::Z_NO_FLUSH;

    uint32_t w_size = 0;
    uint32_t w_bits = 0;
    uint32_t w_mask = 0;

    std::vector<uint8_t> window;
    unsigned long window_size = 0;

    using Pos = uint16_t;
    using IPos = uint32_t;

    std::vector<Pos> prev, head;

    uint32_t ins_h = 0;
    uint32_t hash_size = 0;
    uint32_t hash_bits = 0;
    uint32_t hash_mask = 0;
    uint32_t hash_shift = 0;

    long block_start = 0;
    uint32_t match_length = 0;
    IPos prev_match = 0;
    int match_available = 0;
    uint32_t strstart = 0;
    uint32_t match_start = 0;
    uint32_t lookahead = 0;

    uint32_t prev_length = 0;
    uint32_t max_chain_length = 0;
    uint32_t max_lazy_match = 0;
    int level = 0;

    uint32_t good_match = 0;
    int nice_match = 0;

    static constexpr uint32_t HEAP_SIZE = 2 * L_CODES + 1;

    ct_data dyn_ltree[HEAP_SIZE] = {};
    ct_data dyn_dtree[2 * D_CODES + 1] = {};
    ct_data bl_tree[2 * BL_CODES + 1] = {};

    tree_desc l_desc = {};
    tree_desc d_desc = {};
    tree_desc bl_desc = {};

    static constexpr uint32_t MAX_BITS = 15;

    uint16_t bl_count[MAX_BITS + 1] = {};
    int heap[2 * L_CODES + 1] = {};
    int heap_len = 0;
    uint32_t heap_max = 0;
    uint8_t depth[2 * L_CODES + 1] = {};

    uint8_t* l_buf = nullptr;
    uint32_t lit_bufsize = 0;
    uint32_t last_lit = 0;
    uint16_t* d_buf = nullptr;

    unsigned long opt_len = 0;
    unsigned long static_len = 0;
    uint32_t last_eob_len = 0;

    uint16_t outputBitBuffer = 0;
    uint32_t outputBitBufferSize = 0;

    static constexpr uint32_t SMALLEST = 1;
    static constexpr uint32_t END_BLOCK = 256;
    static constexpr uint32_t REP_3_6 = 16;
    static constexpr uint32_t REPZ_3_10 = 17;
    static constexpr uint32_t REPZ_11_138 = 18;
    static constexpr uint32_t MAX_BL_BITS = 7;

   #if defined (_WIN32) || defined (_WIN64)
    static constexpr uint8_t OS_CODE = 0x0b;
   #else
    static constexpr uint8_t OS_CODE = 0x03;
   #endif

    static constexpr uint32_t STORED_BLOCK = 0;
    static constexpr uint32_t STATIC_TREES = 1;
    static constexpr uint32_t DYN_TREES    = 2;

    static constexpr uint32_t extra_lbits[LENGTH_CODES] /* extra bits for each length code */
        = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};

    static constexpr uint32_t extra_dbits[D_CODES] /* extra bits for each distance code */
        = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

    static constexpr uint32_t extra_blbits[BL_CODES] /* extra bits for each bit length code */
        = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 3, 7};

    static constexpr uint8_t bl_order[BL_CODES]
        = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

    enum class BlockStatus
    {
        need_more,      /* block not completed, need more input or more output */
        block_done,     /* block flush performed */
        finish_started, /* finish started, need only more output at next deflate */
        finish_done     /* finish done, accept no more input or output */
    };

    uint32_t read_buf (uint8_t* buf, uint32_t size)
    {
        uint32_t len = avail_in;

        if (len > size)
            len = size;

        if (len == 0)
            return 0;

        avail_in -= len;

        if (wrap == 1)
            adler = Checksum::adler32 (adler, next_in, len);
        else if (wrap == 2)
            adler = Checksum::crc32 (adler, next_in, len);

        memcpy (buf, next_in, len);
        next_in += len;
        total_in += len;

        return len;
    }

    void flushPending()
    {
        auto len = pending;

        if (len > avail_out)
            len = avail_out;

        if (len == 0)
            return;

        memcpy (next_out, pending_out, len);
        next_out += len;
        pending_out += len;
        total_out += len;
        avail_out -= len;
        pending -= len;

        if (pending == 0)
            pending_out = pending_buf.data();
    }

    static const char* getErrorMessage (ErrorCode errorCode)
    {
        switch (errorCode)
        {
            case ErrorCode::NEED_DICT:       return "need dictionary";
            case ErrorCode::STREAM_END:      return "stream end";
            case ErrorCode::OK:              return "";
            case ErrorCode::ERRNO:           return "file error";
            case ErrorCode::STREAM_ERROR:    return "stream error";
            case ErrorCode::DATA_ERROR:      return "data error";
            case ErrorCode::MEM_ERROR:       return "insufficient memory";
            case ErrorCode::BUF_ERROR:       return "buffer error";
            case ErrorCode::VERSION_ERROR:   return "incompatible version";
            default:                           return "";
        }
    }

    ErrorCode setError (ErrorCode errorCode)
    {
        msg = getErrorMessage (errorCode);
        return errorCode;
    }

    /* ===========================================================================
    * Copy without compression as much as possible from the input stream, return
    * the current block state.
    * This function does not insert new strings in the dictionary since
    * uncompressible data is probably not useful. This function is used
    * only for the level=0 compression option.
    * NOTE: this function should be optimized to avoid extra copying from
    * window to pending_buf.
    */
    static BlockStatus deflate_stored (DeflateStream* s, FlushState flush)
    {
        /* Stored blocks are limited to 0xffff bytes, pending_buf is limited
        * to pending_buf_size, and each stored block has a 5 byte header:
        */
        unsigned long max_block_size = 0xffff;
        unsigned long max_start;

        if (max_block_size > (unsigned long) (s->pending_buf.size() - 5))
            max_block_size = (unsigned long) (s->pending_buf.size() - 5);

        /* Copy as much as possible from input to output: */
        for (;;)
        {
            /* Fill the window as much as possible: */
            if (s->lookahead <= 1)
            {
                CHOC_ASSERT (s->strstart < s->w_size + s->MAX_DIST() || s->block_start >= (long) s->w_size);

                s->fill_window();

                if (s->lookahead == 0 && flush == FlushState::Z_NO_FLUSH)
                    return BlockStatus::need_more;

                if (s->lookahead == 0)
                    break; /* flush the current block */
            }

            CHOC_ASSERT (s->block_start >= 0L);

            s->strstart += s->lookahead;
            s->lookahead = 0;

            /* Emit a stored block if pending_buf will be full: */
            max_start = (unsigned long) s->block_start + max_block_size;

            if (s->strstart == 0 || (unsigned long) s->strstart >= max_start)
            {
                /* strstart == 0 is possible when wraparound on 16-bit machine */
                s->lookahead = (uint32_t) (s->strstart - max_start);
                s->strstart = (uint32_t) max_start;
                s->flushBlock (0);
            }
            /* Flush if we may have to slide, otherwise block_start may become
            * negative and the data will be gone:
            */
            if (s->strstart - (uint32_t) s->block_start >= s->MAX_DIST())
                s->flushBlock (0);
        }
        s->flushBlock (flush == FlushState::Z_FINISH);
        return flush == FlushState::Z_FINISH ? BlockStatus::finish_done
                                             : BlockStatus::block_done;
    }

    /* ===========================================================================
    * Compress as much as possible from the input stream, return the current
    * block state.
    * This function does not perform lazy evaluation of matches and inserts
    * new strings in the dictionary only for unmatched strings or for short
    * matches. It is used only for the fast compression options.
    */
    static BlockStatus deflate_fast (DeflateStream* s, FlushState flush)
    {
        IPos hash_head = 0; /* head of the hash chain */
        bool bflush; /* set if current block must be flushed */

        for (;;)
        {
            /* Make sure that we always have enough lookahead, except
            * at the end of the input file. We need MAX_MATCH bytes
            * for the next match, plus MIN_MATCH bytes to insert the
            * string following the next match.
            */
            if (s->lookahead < MIN_LOOKAHEAD)
            {
                s->fill_window();

                if (s->lookahead < MIN_LOOKAHEAD && flush == FlushState::Z_NO_FLUSH)
                    return BlockStatus::need_more;

                if (s->lookahead == 0)
                    break; /* flush the current block */
            }

            /* Insert the string window[strstart .. strstart+2] in the
            * dictionary, and set hash_head to the head of the hash chain:
            */
            if (s->lookahead >= MIN_MATCH)
                s->insertString (s->strstart, hash_head);

            /* Find the longest match, discarding those <= prev_length.
            * At this point we have always match_length < MIN_MATCH
            */
            if (hash_head != 0 && s->strstart - hash_head <= s->MAX_DIST())
            {
                s->match_length = s->longest_match (hash_head);
            }
            if (s->match_length >= MIN_MATCH)
            {
                s->tallyDistance (s->strstart - s->match_start, s->match_length - MIN_MATCH, bflush);

                s->lookahead -= s->match_length;

                /* Insert new strings in the hash table only if the match length
                * is not too large. This saves time but degrades compression.
                */
                if (s->match_length <= s->max_lazy_match && s->lookahead >= MIN_MATCH)
                {
                    s->match_length--; /* string at strstart already in table */
                    do
                    {
                        s->strstart++;
                        s->insertString (s->strstart, hash_head);
                        /* strstart never exceeds WSIZE-MAX_MATCH, so there are
                        * always MIN_MATCH bytes ahead.
                        */
                    } while (--s->match_length != 0);
                    s->strstart++;
                }
                else
                {
                    s->strstart += s->match_length;
                    s->match_length = 0;
                    s->ins_h = s->window[s->strstart];
                    s->updateHash (s->ins_h, s->window[s->strstart + 1]);

                    /* If lookahead < MIN_MATCH, ins_h is garbage, but it does not
                    * matter since it will be recomputed at next deflate call.
                    */
                }
            }
            else
            {
                /* No match, output a literal byte */
                s->tallyLiteral (s->window[s->strstart], bflush);
                s->lookahead--;
                s->strstart++;
            }
            if (bflush)
                s->flushBlock (0);
        }

        s->flushBlock (flush == FlushState::Z_FINISH);
        return flush == FlushState::Z_FINISH ? BlockStatus::finish_done : BlockStatus::block_done;
    }

    /* ===========================================================================
    * Same as above, but achieves better compression. We use a lazy
    * evaluation for matches: a match is finally adopted only if there is
    * no better match at the next window position.
    */
    static BlockStatus deflate_slow (DeflateStream* s, FlushState flush)
    {
        IPos hash_head = 0; /* head of hash chain */
        bool bflush; /* set if current block must be flushed */

        for (;;)
        {
            /* Make sure that we always have enough lookahead, except
            * at the end of the input file. We need MAX_MATCH bytes
            * for the next match, plus MIN_MATCH bytes to insert the
            * string following the next match.
            */
            if (s->lookahead < MIN_LOOKAHEAD)
            {
                s->fill_window();
                if (s->lookahead < MIN_LOOKAHEAD && flush == FlushState::Z_NO_FLUSH)
                {
                    return BlockStatus::need_more;
                }
                if (s->lookahead == 0)
                    break; /* flush the current block */
            }

            /* Insert the string window[strstart .. strstart+2] in the
            * dictionary, and set hash_head to the head of the hash chain:
            */
            if (s->lookahead >= MIN_MATCH)
            {
                s->insertString (s->strstart, hash_head);
            }

            /* Find the longest match, discarding those <= prev_length.
            */
            s->prev_length = s->match_length, s->prev_match = s->match_start;
            s->match_length = MIN_MATCH - 1;

            if (hash_head != 0 && s->prev_length < s->max_lazy_match && s->strstart - hash_head <= s->MAX_DIST())
            {
                s->match_length = s->longest_match (hash_head);

                constexpr uint32_t TOO_FAR = 4096;

                if (s->match_length <= 5
                    && (s->match_length == MIN_MATCH && s->strstart - s->match_start > TOO_FAR))
                {
                    /* If prev_match is also MIN_MATCH, match_start is garbage
                    * but we will ignore the current match anyway.
                    */
                    s->match_length = MIN_MATCH - 1;
                }
            }
            /* If there was a match at the previous step and the current
            * match is not better, output the previous match:
            */
            if (s->prev_length >= MIN_MATCH && s->match_length <= s->prev_length)
            {
                uint32_t max_insert = s->strstart + s->lookahead - MIN_MATCH;
                /* Do not insert strings in hash table beyond this. */

                s->tallyDistance (s->strstart - 1 - s->prev_match, s->prev_length - MIN_MATCH, bflush);

                /* Insert in hash table all strings up to the end of the match.
                * strstart-1 and strstart are already inserted. If there is not
                * enough lookahead, the last two strings are not inserted in
                * the hash table.
                */
                s->lookahead -= s->prev_length - 1;
                s->prev_length -= 2;
                do
                {
                    if (++s->strstart <= max_insert)
                    {
                        s->insertString (s->strstart, hash_head);
                    }
                } while (--s->prev_length != 0);
                s->match_available = 0;
                s->match_length = MIN_MATCH - 1;
                s->strstart++;

                if (bflush)
                    s->flushBlock (0);
            }
            else if (s->match_available)
            {
                /* If there was no match at the previous position, output a
                * single literal. If there was a match but the current match
                * is longer, truncate the previous match to a single literal.
                */
                s->tallyLiteral (s->window[s->strstart - 1], bflush);
                if (bflush)
                {
                    s->flushBlock (0);
                }
                s->strstart++;
                s->lookahead--;
                if (s->avail_out == 0)
                    return BlockStatus::need_more;
            }
            else
            {
                /* There is no previous match to compare with, wait for
                * the next step to decide.
                */
                s->match_available = 1;
                s->strstart++;
                s->lookahead--;
            }
        }
        CHOC_ASSERT (flush != FlushState::Z_NO_FLUSH);
        if (s->match_available)
        {
            s->tallyLiteral (s->window[s->strstart - 1], bflush);
            s->match_available = 0;
        }
        s->flushBlock (flush == FlushState::Z_FINISH);
        return flush == FlushState::Z_FINISH ? BlockStatus::finish_done : BlockStatus::block_done;
    }

    //==============================================================================
    using compress_func = BlockStatus(*)(DeflateStream*, FlushState);

    /* Values for max_lazy_match, good_match and max_chain_length, depending on
    * the desired pack level (0..9). The values given below have been tuned to
    * exclude worst case performance for pathological files. Better values may be
    * found for specific files.
    */
    struct config
    {
        uint16_t good_length; /* reduce lazy search above this match length */
        uint16_t max_lazy;    /* do not perform lazy search above this match length */
        uint16_t nice_length; /* quit search above this match length */
        uint16_t max_chain;
        compress_func func;
    };

    static constexpr config compressionConfigs[10] =
    {
        {  0,   0,    0,    0,  DeflateStream::deflate_stored },  // store only
        {  4,   4,    8,    4,  DeflateStream::deflate_fast },  // max speed, no lazy matches
        {  4,   5,   16,    8,  DeflateStream::deflate_fast },
        {  4,   6,   32,   32,  DeflateStream::deflate_fast },
        {  4,   4,   16,   16,  DeflateStream::deflate_slow },  // lazy matches
        {  8,  16,   32,   32,  DeflateStream::deflate_slow },
        {  8,  16,  128,  128,  DeflateStream::deflate_slow },
        {  8,  32,  128,  256,  DeflateStream::deflate_slow },
        { 32, 128,  258, 1024,  DeflateStream::deflate_slow },
        { 32, 258,  258, 4096,  DeflateStream::deflate_slow }
    };

    template <typename Type> void putByte (Type n)      { pending_buf[pending++] = static_cast<uint8_t> (n); }
    template <typename Type> void putInt16 (Type n)     { putByte (static_cast<uint16_t> (n)); putByte (static_cast<uint16_t> (n) >> 8); }
    template <typename Type> void putInt32 (Type n)     { putInt16 (static_cast<uint32_t> (n)); putInt16 (static_cast<uint32_t> (n) >> 16); }
    template <typename Type> void putInt16MSB (Type n)  { putByte (static_cast<uint16_t> (n) >> 8); putByte (static_cast<uint16_t> (n)); }
    template <typename Type> void putInt32MSB (Type n)  { putInt16MSB (static_cast<uint32_t> (n) >> 16); putInt16MSB (static_cast<uint32_t> (n)); }

    void send_bits (uint32_t value, uint32_t length)
    {
        constexpr uint32_t bufSize = 8 * 2;

        if (outputBitBufferSize > bufSize - length)
        {
            outputBitBuffer |= (uint16_t) (value << outputBitBufferSize);
            putInt16 (outputBitBuffer);
            outputBitBuffer = (uint16_t) value >> (bufSize - outputBitBufferSize);
            outputBitBufferSize += length - bufSize;
        }
        else
        {
            outputBitBuffer |= (uint16_t) (value << outputBitBufferSize);
            outputBitBufferSize += length;
        }
    }

    void send_code (uint32_t c, const ct_data* tree)
    {
        send_bits (tree[c].fc.code, tree[c].dl.len);
    }

    /*
    * Send the header for a block using dynamic Huffman trees: the counts, the
    * lengths of the bit length codes, the literal tree and the distance tree.
    * IN assertion: lcodes >= 257, dcodes >= 1, blcodes >= 4.
    */
    void send_all_trees (uint32_t lcodes, uint32_t dcodes, uint32_t blcodes)
    {
        CHOC_ASSERT (lcodes >= 257 && dcodes >= 1 && blcodes >= 4);
        CHOC_ASSERT (lcodes <= L_CODES && dcodes <= D_CODES && blcodes <= BL_CODES);
        send_bits (lcodes - 257u, 5); /* not +255 as stated in appnote.txt */
        send_bits (dcodes - 1u,   5);
        send_bits (blcodes - 4u,  4); /* not -3 as stated in appnote.txt */

        for (uint32_t rank = 0; rank < blcodes; rank++)
            send_bits (bl_tree[bl_order[rank]].dl.len, 3);

        sendTree (dyn_ltree, lcodes - 1); /* literal tree */
        sendTree (dyn_dtree, dcodes - 1); /* distance tree */
    }

    void sendTree (const ct_data* tree, uint32_t max_code)
    {
        uint32_t prevlen = 0xffffffff;          /* last emitted length */
        uint32_t curlen;                /* length of current code */
        auto nextlen = tree[0].dl.len; /* length of next code */
        uint32_t count = 0;             /* repeat count of the current code */
        uint32_t max_count = 7;         /* max repeat count */
        uint32_t min_count = 4;         /* min repeat count */

        /* tree[max_code+1].dl.len = -1; */  /* guard already set */
        if (nextlen == 0)
        {
            max_count = 138;
            min_count = 3;
        }

        for (uint32_t n = 0; n <= max_code; n++)
        {
            curlen = nextlen;
            nextlen = tree[n + 1].dl.len;

            if (++count < max_count && curlen == nextlen)
                continue;

            if (count < min_count)
            {
                do { send_code (curlen, bl_tree); } while (--count != 0);
            }
            else if (curlen != 0)
            {
                if (curlen != prevlen)
                {
                    send_code (curlen, bl_tree);
                    count--;
                }

                CHOC_ASSERT(count >= 3 && count <= 6);
                send_code (REP_3_6, bl_tree); send_bits (count - 3, 2);
            }
            else if (count <= 10)
            {
                send_code (REPZ_3_10, bl_tree); send_bits (count - 3, 3);
            }
            else
            {
                send_code (REPZ_11_138, bl_tree); send_bits (count - 11, 7);
            }

            count = 0;
            prevlen = curlen;

            if (nextlen == 0)
                max_count = 138, min_count = 3;
            else if (curlen == nextlen)
                max_count = 6, min_count = 3;
            else
                max_count = 7, min_count = 4;
        }
    }

    void build_tree (tree_desc& desc)
    {
        auto* tree  = desc.dyn_tree;
        auto* stree = desc.stat_desc->static_tree;
        int elems   = desc.stat_desc->elems;
        int n, m;          /* iterate over heap elements */
        int max_code = -1; /* largest code with non zero frequency */
        int node;          /* new node being created */

        /* Construct the initial heap, with least frequent element in
        * heap[SMALLEST]. The sons of heap[n] are heap[2*n] and heap[2*n+1].
        * heap[0] is not used.
        */
        heap_len = 0, heap_max = HEAP_SIZE;

        for (n = 0; n < elems; n++) {
            if (tree[n].fc.freq != 0) {
                heap[++(heap_len)] = max_code = n;
                depth[n] = 0;
            } else {
                tree[n].dl.len = 0;
            }
        }

        /* The pkzip format requires that at least one distance code exists,
        * and that at least one bit should be sent even if there is only one
        * possible code. So to avoid special checks later on we force at least
        * two codes of non zero frequency.
        */
        while (heap_len < 2) {
            node = heap[++(heap_len)] = (max_code < 2 ? ++max_code : 0);
            tree[node].fc.freq = 1;
            depth[node] = 0;
            opt_len--; if (stree) static_len -= stree[node].dl.len;
            /* node is 0 or 1 so it does not have extra bits */
        }
        desc.max_code = (uint32_t) max_code;

        /* The elements heap[heap_len/2+1 .. heap_len] are leaves of the tree,
        * establish sub-heaps of increasing lengths:
        */
        for (n = heap_len/2; n >= 1; n--) pqdownheap (tree, n);

        /* Construct the Huffman tree by repeatedly combining the least two
        * frequent nodes.
        */
        node = elems;              /* next internal node of the tree */

        do {
            n = heap[SMALLEST];
            heap[SMALLEST] = heap[heap_len--];
            pqdownheap (tree, SMALLEST);

            m = heap[SMALLEST]; /* m = node of next least frequency */

            heap[--(heap_max)] = n; /* keep the nodes sorted by frequency */
            heap[--(heap_max)] = m;

            /* Create a new node father of n and m */
            tree[node].fc.freq = tree[n].fc.freq + tree[m].fc.freq;
            depth[node] = (uint8_t)((depth[n] >= depth[m] ?
                                        depth[n] : depth[m]) + 1);
            tree[n].dl.dad = tree[m].dl.dad = (uint16_t)node;
            /* and insert the new node in the heap */
            heap[SMALLEST] = node++;
            pqdownheap (tree, SMALLEST);

        } while (heap_len >= 2);

        heap[--(heap_max)] = heap[SMALLEST];

        computeOptimalBitLengthForTree (desc);

        /* The field len is now set, we can generate the bit codes */
        gen_codes (tree, max_code, bl_count);
    }

    /* ===========================================================================
    * Generate the codes for a given tree and bit counts (which need not be
    * optimal).
    * IN assertion: the array bl_count contains the bit length statistics for
    * the given tree and the field len is set for all tree elements.
    * OUT assertion: the field code is set for all tree elements of non
    *     zero code length.
    */
    static void gen_codes (ct_data* tree, int max_code, uint16_t* bl_count)
    {
        uint16_t next_code[MAX_BITS + 1]; /* next code value for each bit length */
        uint16_t code_ = 0;              /* running code value */

        /* The distribution counts are first used to generate the code values
        * without bit reversal.
        */
        for (uint32_t bits = 1; bits <= MAX_BITS; bits++)
            next_code[bits] = code_ = (uint16_t) ((code_ + bl_count[bits-1]) << 1);

        /* Check that the bit counts in bl_count are consistent. The last code
        * must be all ones.
        */
        CHOC_ASSERT (code_ + bl_count[MAX_BITS] - 1 == (1 << MAX_BITS) - 1);

        for (int n = 0;  n <= max_code; n++)
        {
            int len = tree[n].dl.len;
            if (len == 0) continue;
            /* Now reverse the bits */
            tree[n].fc.code = bi_reverse(next_code[len]++, len);
        }
    }

    static uint16_t bi_reverse (uint16_t code_, int len)
    {
        uint32_t res = 0;
        do {
            res |= code_ & 1;
            code_ >>= 1, res <<= 1;
        } while (--len > 0);
        return (uint16_t) (res >> 1);
    }

    void computeOptimalBitLengthForTree (tree_desc& desc)
    {
        ct_data* tree = desc.dyn_tree;
        auto max_code = (int) desc.max_code;
        auto* stree = desc.stat_desc->static_tree;
        auto* extra = desc.stat_desc->extra_bits;
        int base = desc.stat_desc->extra_base;
        uint32_t max_length = desc.stat_desc->max_length;
        uint32_t h; /* heap index */
        int n, m; /* iterate over the tree elements */
        uint32_t bits; /* bit length */
        uint32_t xbits; /* extra bits */
        uint16_t f; /* frequency */
        int overflow = 0; /* number of elements with bit length too large */

        for (bits = 0; bits <= MAX_BITS; bits++)
            bl_count[bits] = 0;

        /* In a first pass, compute the optimal bit lengths (which may
        * overflow in the case of the bit length tree).
        */
        tree[heap[heap_max]].dl.len = 0; /* root of the heap */

        for (h = heap_max + 1; h < HEAP_SIZE; h++)
        {
            n = heap[h];
            bits = tree[tree[n].dl.dad].dl.len + 1U;
            if (bits > max_length)
                bits = max_length, overflow++;
            tree[n].dl.len = (uint16_t) bits;
            /* We overwrite tree[n].dl.dad which is no longer needed */

            if (n > max_code)
                continue; /* not a leaf node */

            bl_count[bits]++;
            xbits = 0;
            if (n >= base)
                xbits = extra[n - base];
            f = tree[n].fc.freq;
            opt_len += (unsigned long) f * (bits + xbits);
            if (stree)
                static_len += (unsigned long) f * (stree[n].dl.len + xbits);
        }

        if (overflow == 0)
            return;

        /* This happens for example on obj2 and pic of the Calgary corpus */

        /* Find the first bit length which could increase: */
        do
        {
            bits = max_length - 1;
            while (bl_count[bits] == 0)
                bits--;
            bl_count[bits]--; /* move one leaf down the tree */
            bl_count[bits + 1] += 2; /* move one overflow item as its brother */
            bl_count[max_length]--;
            /* The brother of the overflow item also moves one step up,
            * but this does not affect bl_count[max_length]
            */
            overflow -= 2;
        } while (overflow > 0);

        /* Now recompute all bit lengths, scanning in increasing frequency.
        * h is still equal to HEAP_SIZE. (It is simpler to reconstruct all
        * lengths instead of fixing only the wrong ones. This idea is taken
        * from 'ar' written by Haruhiko Okumura.)
        */
        for (bits = max_length; bits != 0; bits--)
        {
            n = bl_count[bits];
            while (n != 0)
            {
                m = heap[--h];
                if (m > max_code)
                    continue;
                if ((uint32_t) tree[m].dl.len != (uint32_t) bits)
                {
                    opt_len = (unsigned long) ((long) opt_len + ((long) bits - (long) tree[m].dl.len) * (long) tree[m].fc.freq);
                    tree[m].dl.len = (uint16_t) bits;
                }
                n--;
            }
        }
    }

    void initialiseBlock()
    {
        for (uint32_t n = 0; n < L_CODES;  n++) dyn_ltree[n].fc.freq = 0;
        for (uint32_t n = 0; n < D_CODES;  n++) dyn_dtree[n].fc.freq = 0;
        for (uint32_t n = 0; n < BL_CODES; n++) bl_tree[n].fc.freq = 0;

        dyn_ltree[END_BLOCK].fc.freq = 1;
        opt_len = 0;
        static_len = 0;
        last_lit = 0;
    }

    void flushBlock (bool eof)
    {
        flushCurrentBlock (block_start >= 0 ? (uint8_t*) &window[(uint32_t) block_start]
                                            : (uint8_t*) nullptr,
                           (unsigned long) ((long) strstart - block_start),
                           eof);
        block_start = (long) strstart;
        flushPending();
    }

    void initialiseTrees()
    {
        constexpr static static_tree_desc static_l_desc  = { static_ltree, extra_lbits, LITERALS + 1, L_CODES, MAX_BITS };
        constexpr static static_tree_desc static_d_desc  = { static_dtree, extra_dbits, 0, D_CODES, MAX_BITS };
        constexpr static static_tree_desc static_bl_desc = { nullptr, extra_blbits, 0, BL_CODES, MAX_BL_BITS};

        l_desc.dyn_tree = dyn_ltree;
        l_desc.stat_desc = &static_l_desc;

        d_desc.dyn_tree = dyn_dtree;
        d_desc.stat_desc = &static_d_desc;

        bl_desc.dyn_tree = bl_tree;
        bl_desc.stat_desc = &static_bl_desc;

        outputBitBuffer = 0;
        outputBitBufferSize = 0;
        last_eob_len = 8; /* enough lookahead for inflate */

        initialiseBlock();
    }

    void sendStoredBlock (const uint8_t* buf, unsigned long len, bool eof)
    {
        send_bits ((STORED_BLOCK << 1) + (eof ? 1 : 0), 3);

        flushBitBufferAndAlign();
        last_eob_len = 8;

        putInt16 (len);
        putInt16 (~len);

        while (len--)
            putByte (*buf++);
    }

    void tallyLiteral (uint8_t c, bool& flush)
    {
        d_buf[last_lit] = 0;
        l_buf[last_lit++] = c;
        dyn_ltree[c].fc.freq++;
        flush = (last_lit == lit_bufsize - 1);
    }

    void tallyDistance (uint32_t distance, uint32_t length, bool& flush)
    {
        d_buf[last_lit] = (uint16_t) distance;
        l_buf[last_lit++] = (uint8_t) length;
        distance--;
        dyn_ltree[lengthCodes[length] + LITERALS + 1].fc.freq++;
        dyn_dtree[getDistanceCode(distance)].fc.freq++;
        flush = (last_lit == lit_bufsize - 1);
    }

    /*
    * Send one empty static block to give enough lookahead for inflate.
    * This takes 10 bits, of which 7 may remain in the bit buffer.
    * The current inflate code requires 9 bits of lookahead. If the
    * last two codes for the previous block (real code plus EOB) were coded
    * on 5 bits or less, inflate may have only 5+3 bits of lookahead to decode
    * the last real code. In this case we send two empty static blocks instead
    * of one. (There are no problems if the previous block is stored or fixed.)
    * To simplify the code, we assume the worst case of last real code encoded
    * on one bit only.
    */
    void alignBuffer()
    {
        send_bits (STATIC_TREES << 1, 3);
        send_code (END_BLOCK, static_ltree);
        flushBitBuffer();

        /* Of the 10 bits for the empty block, we have already sent
        * (10 - outputBitBufferSize) bits. The lookahead for the last real code (before
        * the EOB of the previous block) was thus at least one plus the length
        * of the EOB plus what we have just sent of the empty static block.
        */
        if (1 + last_eob_len + 10 - outputBitBufferSize < 9)
        {
            send_bits (STATIC_TREES<<1, 3);
            send_code (END_BLOCK, static_ltree);
            flushBitBuffer();
        }
        last_eob_len = 7;
    }

    void flushCurrentBlock (uint8_t* inputBlock, unsigned long stored_len, bool eof)
    {
        unsigned long opt_lenb, static_lenb; /* opt_len and static_len in bytes */
        uint32_t max_blindex = 0; /* index of last bit length code of non zero freq */

        if (level > 0)
        {
            if (stored_len > 0 && data_type == DataType::Z_UNKNOWN)
                set_data_type();

            build_tree (l_desc);
            build_tree (d_desc);

            max_blindex = build_bl_tree();

            opt_lenb = (opt_len + 3 + 7) >> 3;
            static_lenb = (static_len + 3 + 7) >> 3;

            if (static_lenb <= opt_lenb)
                opt_lenb = static_lenb;
        }
        else
        {
            CHOC_ASSERT (inputBlock);
            opt_lenb = static_lenb = stored_len + 5; /* force a stored block */
        }

        if (stored_len + 4 <= opt_lenb && inputBlock != nullptr)
        {
            sendStoredBlock (inputBlock, stored_len, eof);
        }
        else if (static_lenb == opt_lenb)
        {
            send_bits ((STATIC_TREES << 1) + (eof ? 1 : 0), 3);
            compress_block (static_ltree, static_dtree);
        }
        else
        {
            send_bits ((DYN_TREES << 1) + (eof ? 1 : 0), 3);
            send_all_trees (l_desc.max_code + 1, d_desc.max_code + 1, max_blindex + 1);
            compress_block (dyn_ltree, dyn_dtree);
        }

        initialiseBlock();

        if (eof)
            flushBitBufferAndAlign();
    }

    void flushBitBuffer()
    {
        if (outputBitBufferSize == 16)
        {
            putInt16 (outputBitBuffer);
            outputBitBuffer = 0;
            outputBitBufferSize = 0;
        }
        else if (outputBitBufferSize >= 8)
        {
            putByte (outputBitBuffer);
            outputBitBuffer >>= 8;
            outputBitBufferSize -= 8;
        }
    }

    void flushBitBufferAndAlign()
    {
        if (outputBitBufferSize > 8)
            putInt16 (outputBitBuffer);
        else if (outputBitBufferSize > 0)
            putByte (outputBitBuffer);

        outputBitBuffer = 0;
        outputBitBufferSize = 0;
    }

    void clearHash()
    {
        std::fill (head.begin(), head.end(), Pos());
    }

    void updateHash (uint32_t& h, uint8_t c) const
    {
        h = ((h << hash_shift) ^ c) & hash_mask;
    }

    void insertString (uint32_t str, IPos& match_head)
    {
        updateHash (ins_h, window[(str) + (MIN_MATCH - 1)]);
        match_head = prev[str & w_mask] = head[ins_h];
        head[ins_h] = (Pos) str;
    }

    void initLongestMatch()
    {
        window_size = (unsigned long) 2 * w_size;

        clearHash();

        max_lazy_match = DeflateStream::compressionConfigs[level].max_lazy;
        good_match = DeflateStream::compressionConfigs[level].good_length;
        nice_match = DeflateStream::compressionConfigs[level].nice_length;
        max_chain_length = DeflateStream::compressionConfigs[level].max_chain;

        strstart = 0;
        block_start = 0;
        lookahead = 0;
        match_length = prev_length = MIN_MATCH - 1;
        match_available = 0;
        ins_h = 0;
    }

    /*
    * Set the data type to BINARY or TEXT, using a crude approximation:
    * set it to Z_TEXT if all symbols are either printable characters (33 to 255)
    * or white spaces (9 to 13, or 32); or set it to Z_BINARY otherwise.
    * IN assertion: the fields Freq of dyn_ltree are set.
    */
    void set_data_type()
    {
        int n;

        for (n = 0; n < 9; n++)
            if (dyn_ltree[n].fc.freq != 0)
                break;

        if (n == 9)
            for (n = 14; n < 32; n++)
                if (dyn_ltree[n].fc.freq != 0)
                    break;

        data_type = (n == 32) ? DataType::Z_TEXT : DataType::Z_BINARY;
    }

    /*
    * Construct the Huffman tree for the bit lengths and return the index in
    * bl_order of the last bit length code to send.
    */
    uint32_t build_bl_tree()
    {
        uint32_t max_blindex;  /* index of last bit length code of non zero freq */

        /* Determine the bit length frequencies for literal and distance trees */
        scan_tree (dyn_ltree, l_desc.max_code);
        scan_tree (dyn_dtree, d_desc.max_code);

        /* Build the bit length tree: */
        build_tree (bl_desc);
        /* opt_len now includes the length of the tree representations, except
        * the lengths of the bit lengths codes and the 5+5+4 bits for the counts.
        */

        /* Determine the number of bit length codes to send. The pkzip format
        * requires that at least 4 bit length codes be sent. (appnote.txt says
        * 3 but the actual value used is 4.)
        */
        for (max_blindex = BL_CODES - 1; max_blindex >= 3; max_blindex--)
            if (bl_tree[bl_order[max_blindex]].dl.len != 0)
                break;

        /* Update opt_len to include the bit length tree and counts */
        opt_len += 3u * (max_blindex + 1u) + 5u + 5u + 4u;
        return max_blindex;
    }

    /*
    * Scan a literal or distance tree to determine the frequencies of the codes
    * in the bit length tree.
    */
    void scan_tree (ct_data* tree, uint32_t max_code)
    {
        uint32_t prevlen = 0xffffffff;          /* last emitted length */
        uint32_t curlen;                /* length of current code */
        auto nextlen = tree[0].dl.len; /* length of next code */
        uint32_t count = 0;             /* repeat count of the current code */
        uint32_t max_count = 7;         /* max repeat count */
        uint32_t min_count = 4;         /* min repeat count */

        if (nextlen == 0)
        {
            max_count = 138;
            min_count = 3;
        }

        tree[max_code + 1].dl.len = (uint16_t)0xffff;

        for (uint32_t n = 0; n <= max_code; n++)
        {
            curlen = nextlen; nextlen = tree[n + 1].dl.len;

            if (++count < max_count && curlen == nextlen)
                continue;

            if (count < min_count)
            {
                bl_tree[curlen].fc.freq += (uint16_t) count;
            }
            else if (curlen != 0)
            {
                if (curlen != prevlen) bl_tree[curlen].fc.freq++;
                bl_tree[REP_3_6].fc.freq++;
            }
            else if (count <= 10)
            {
                bl_tree[REPZ_3_10].fc.freq++;
            }
            else
            {
                bl_tree[REPZ_11_138].fc.freq++;
            }

            count = 0;
            prevlen = curlen;

            if (nextlen == 0)
            {
                max_count = 138;
                min_count = 3;
            }
            else if (curlen == nextlen)
            {
                max_count = 6;
                min_count = 3;
            }
            else
            {
                max_count = 7;
                min_count = 4;
            }
        }
    }

    void compress_block (const ct_data* ltree, const ct_data* dtree)
    {
        static constexpr uint32_t base_length[LENGTH_CODES]
            = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 0 };

        static constexpr uint32_t base_dist[D_CODES]
            = { 0, 1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576 };

        uint32_t lx = 0; /* running index in l_buf */

        if (last_lit != 0)
            do
            {
                auto dist = d_buf[lx];
                auto lc = l_buf[lx++];
                if (dist == 0)
                {
                    send_code (lc, ltree); /* send a literal byte */
                }
                else
                {
                    /* Here, lc is the match length - MIN_MATCH */
                    auto code_ = lengthCodes[lc];
                    send_code (code_ + LITERALS + 1, ltree); /* send the length code */
                    auto extra = extra_lbits[code_];
                    if (extra != 0)
                    {
                        lc -= (uint8_t) base_length[code_];
                        send_bits (lc, extra); /* send the extra length bits */
                    }
                    dist--; /* dist is now the match distance - 1 */
                    code_ = getDistanceCode (dist);
                    CHOC_ASSERT (code_ < D_CODES);

                    send_code (code_, dtree); /* send the distance code */
                    extra = extra_dbits[code_];
                    if (extra != 0)
                    {
                        dist -= (unsigned short) base_dist[code_];
                        send_bits (dist, extra); /* send the extra distance bits */
                    }
                }

                CHOC_ASSERT ((uint32_t) (pending) < lit_bufsize + 2 * lx);

            } while (lx < last_lit);

        send_code (END_BLOCK, ltree);
        last_eob_len = ltree[END_BLOCK].dl.len;
    }

    /* ===========================================================================
    * Set match_start to the longest match starting at the given string and
    * return its length. Matches shorter or equal to prev_length are discarded,
    * in which case the result is equal to prev_length and match_start is
    * garbage.
    * IN assertions: cur_match is the head of the hash chain for the current
    *   string (strstart) and its distance is <= MAX_DIST, and prev_length >= 1
    * OUT assertion: the match length is not greater than s->lookahead.
    */

    /* For 80x86 and 680x0, an optimized version will be provided in match.asm or
    * match.S. The code will be functionally equivalent.
    */
    uint32_t longest_match (IPos cur_match)
    {
        uint32_t chain_length = max_chain_length; /* max hash chain length */
        auto scan = window.data() + strstart; /* current string */
        auto best_len = prev_length; /* best match length so far */
        IPos limit = strstart > (IPos) MAX_DIST() ? strstart - (IPos) MAX_DIST() : 0;

        auto strend = window.data() + strstart + MAX_MATCH;
        uint8_t scan_end1 = scan[best_len - 1];
        uint8_t scan_end = scan[best_len];

        /* The code is optimized for HASH_BITS >= 8 and MAX_MATCH-2 multiple of 16.
        * It is easy to get rid of this optimization if necessary.
        */
        CHOC_ASSERT (hash_bits >= 8 && MAX_MATCH == 258);

        /* Do not waste too much time if we already have a good match: */
        if (prev_length >= good_match)
        {
            chain_length >>= 2;
        }

        auto longEnoughMatch = nice_match; /* stop if match long enough */

        if ((uint32_t) longEnoughMatch > lookahead)
            longEnoughMatch = (int) lookahead;

        CHOC_ASSERT ((unsigned long) strstart <= window_size - MIN_LOOKAHEAD);

        do
        {
            CHOC_ASSERT (cur_match < strstart);
            auto match = window.data() + cur_match;

            if (match[best_len] != scan_end || match[best_len - 1] != scan_end1 || *match != *scan || *++match != scan[1])
                continue;

            /* The check at best_len-1 can be removed because it will be made
            * again later. (This heuristic is not always a win.)
            * It is not necessary to compare scan[2] and match[2] since they
            * are always equal when the other bytes match, given that
            * the hash keys are equal and that HASH_BITS >= 8.
            */
            scan += 2, match++;
            CHOC_ASSERT (*scan == *match);

            /* We check for insufficient lookahead only every 8th comparison;
            * the 256th check will be made at strstart+258.
            */
            do
            {
            } while (*++scan == *++match && *++scan == *++match && *++scan == *++match
                        && *++scan == *++match && *++scan == *++match && *++scan == *++match
                        && *++scan == *++match && *++scan == *++match && scan < strend);

            CHOC_ASSERT (scan <= window.data() + (uint32_t) (window_size - 1));

            auto len = MAX_MATCH - (int) (strend - scan);
            scan = strend - MAX_MATCH;

            if (len > (int) best_len)
            {
                match_start = cur_match;
                best_len = (uint32_t) len;
                if (len >= longEnoughMatch)
                    break;
                scan_end1 = scan[best_len - 1];
                scan_end = scan[best_len];
            }
        } while ((cur_match = prev[cur_match & w_mask]) > limit
                 && --chain_length != 0);

        if ((uint32_t) best_len <= lookahead)
            return (uint32_t) best_len;

        return lookahead;
    }

    /* ---------------------------------------------------------------------------
    * Optimized version for level == 1 or strategy == Z_RLE only
    */
    uint32_t longest_match_fast (IPos cur_match)
    {
        auto scan = window.data() + strstart; /* current string */
        uint8_t* match; /* matched string */
        auto strend = window.data() + strstart + MAX_MATCH;

        CHOC_ASSERT (hash_bits >= 8 && MAX_MATCH == 258);
        CHOC_ASSERT ((unsigned long) strstart <= window_size - MIN_LOOKAHEAD);
        CHOC_ASSERT (cur_match < strstart);

        match = window.data() + cur_match;

        /* Return failure if the match length is less than 2:
        */
        if (match[0] != scan[0] || match[1] != scan[1])
            return MIN_MATCH - 1;

        /* The check at best_len-1 can be removed because it will be made
        * again later. (This heuristic is not always a win.)
        * It is not necessary to compare scan[2] and match[2] since they
        * are always equal when the other bytes match, given that
        * the hash keys are equal and that HASH_BITS >= 8.
        */
        scan += 2, match += 2;
        CHOC_ASSERT (*scan == *match);

        /* We check for insufficient lookahead only every 8th comparison;
        * the 256th check will be made at strstart+258.
        */
        do
        {
        } while (*++scan == *++match && *++scan == *++match
                 && *++scan == *++match && *++scan == *++match
                 && *++scan == *++match && *++scan == *++match
                 && *++scan == *++match && *++scan == *++match
                 && scan < strend);

        CHOC_ASSERT (scan <= window.data() + (uint32_t) (window_size - 1));

        auto len = MAX_MATCH - (int) (strend - scan);

        if (len < MIN_MATCH)
            return MIN_MATCH - 1;

        match_start = cur_match;
        return (uint32_t) len <= lookahead ? (uint32_t) len : lookahead;
    }

    /* ===========================================================================
    * Fill the window when the lookahead becomes insufficient.
    * Updates strstart and lookahead.
    *
    * IN assertion: lookahead < MIN_LOOKAHEAD
    * OUT assertions: strstart <= window_size-MIN_LOOKAHEAD
    *    At least one byte has been read, or avail_in == 0; reads are
    *    performed for at least two bytes (required for the zip translate_eol
    *    option -- not supported here).
    */
    void fill_window()
    {
        uint32_t n, m;
        uint32_t wsize = w_size;

        do
        {
            auto more = (uint32_t) (window_size - (unsigned long) lookahead - (unsigned long) strstart);

            /* If the window is almost full and there is insufficient lookahead,
            * move the upper half to the lower one to make room in the upper half.
            */
            if (strstart >= wsize + MAX_DIST())
            {
                memcpy (window.data(), window.data() + wsize, (uint32_t) wsize);
                match_start -= wsize;
                strstart -= wsize; /* we now have strstart >= MAX_DIST */
                block_start -= (long) wsize;

                /* Slide the hash table (could be avoided with 32 bit values
                at the expense of memory usage). We slide even when level == 0
                to keep the hash table consistent if we switch back to level > 0
                later. (Using level 0 permanently is not an optimal usage of
                zlib, so we don't care about this pathological case.)
                */
                /* %%% avoid this when Z_RLE */
                n = hash_size;
                auto p = &head[0] + n;
                do
                {
                    m = *--p;
                    *p = (Pos) (m >= wsize ? m - wsize : 0);
                } while (--n);

                n = wsize;
                p = &prev[0] + n;

                do
                {
                    m = *--p;
                    *p = (Pos) (m >= wsize ? m - wsize : 0);
                    /* If n is not on any hash chain, prev[n] is garbage but
                    * its value will never be used.
                    */
                } while (--n);

                more += wsize;
            }

            if (avail_in == 0)
                return;

            CHOC_ASSERT (more >= 2);
            n = read_buf (window.data() + strstart + lookahead, more);
            lookahead += n;

            if (lookahead >= MIN_MATCH)
            {
                ins_h = window[strstart];
                updateHash (ins_h, window[strstart + 1]);
            }

        } while (lookahead < MIN_LOOKAHEAD && avail_in != 0);
    }

    void pqdownheap (const ct_data* tree, int k)
    {
        auto v = heap[k];
        auto j = k << 1;

        while (j <= heap_len)
        {
            if (j < heap_len && smaller(tree, heap[j + 1], heap[j]))
                j++;

            if (smaller (tree, v, heap[j]))
                break;

            heap[k] = heap[j];
            k = j;
            j <<= 1;
        }

        heap[k] = v;
    }

    bool smaller (const ct_data* tree, int n, int m) const
    {
        return tree[n].fc.freq < tree[m].fc.freq
                 || (tree[n].fc.freq == tree[m].fc.freq && depth[n] <= depth[m]);
    }

    uint32_t MAX_DIST() const  { return w_size - MIN_LOOKAHEAD; }

    static constexpr ct_data static_ltree[L_CODES + 2] =
    {
        {{ 12},{  8}}, {{140},{  8}}, {{ 76},{  8}}, {{204},{  8}}, {{ 44},{  8}}, {{172},{  8}}, {{108},{  8}}, {{236},{  8}}, {{ 28},{  8}}, {{156},{  8}},
        {{ 92},{  8}}, {{220},{  8}}, {{ 60},{  8}}, {{188},{  8}}, {{124},{  8}}, {{252},{  8}}, {{  2},{  8}}, {{130},{  8}}, {{ 66},{  8}}, {{194},{  8}},
        {{ 34},{  8}}, {{162},{  8}}, {{ 98},{  8}}, {{226},{  8}}, {{ 18},{  8}}, {{146},{  8}}, {{ 82},{  8}}, {{210},{  8}}, {{ 50},{  8}}, {{178},{  8}},
        {{114},{  8}}, {{242},{  8}}, {{ 10},{  8}}, {{138},{  8}}, {{ 74},{  8}}, {{202},{  8}}, {{ 42},{  8}}, {{170},{  8}}, {{106},{  8}}, {{234},{  8}},
        {{ 26},{  8}}, {{154},{  8}}, {{ 90},{  8}}, {{218},{  8}}, {{ 58},{  8}}, {{186},{  8}}, {{122},{  8}}, {{250},{  8}}, {{  6},{  8}}, {{134},{  8}},
        {{ 70},{  8}}, {{198},{  8}}, {{ 38},{  8}}, {{166},{  8}}, {{102},{  8}}, {{230},{  8}}, {{ 22},{  8}}, {{150},{  8}}, {{ 86},{  8}}, {{214},{  8}},
        {{ 54},{  8}}, {{182},{  8}}, {{118},{  8}}, {{246},{  8}}, {{ 14},{  8}}, {{142},{  8}}, {{ 78},{  8}}, {{206},{  8}}, {{ 46},{  8}}, {{174},{  8}},
        {{110},{  8}}, {{238},{  8}}, {{ 30},{  8}}, {{158},{  8}}, {{ 94},{  8}}, {{222},{  8}}, {{ 62},{  8}}, {{190},{  8}}, {{126},{  8}}, {{254},{  8}},
        {{  1},{  8}}, {{129},{  8}}, {{ 65},{  8}}, {{193},{  8}}, {{ 33},{  8}}, {{161},{  8}}, {{ 97},{  8}}, {{225},{  8}}, {{ 17},{  8}}, {{145},{  8}},
        {{ 81},{  8}}, {{209},{  8}}, {{ 49},{  8}}, {{177},{  8}}, {{113},{  8}}, {{241},{  8}}, {{  9},{  8}}, {{137},{  8}}, {{ 73},{  8}}, {{201},{  8}},
        {{ 41},{  8}}, {{169},{  8}}, {{105},{  8}}, {{233},{  8}}, {{ 25},{  8}}, {{153},{  8}}, {{ 89},{  8}}, {{217},{  8}}, {{ 57},{  8}}, {{185},{  8}},
        {{121},{  8}}, {{249},{  8}}, {{  5},{  8}}, {{133},{  8}}, {{ 69},{  8}}, {{197},{  8}}, {{ 37},{  8}}, {{165},{  8}}, {{101},{  8}}, {{229},{  8}},
        {{ 21},{  8}}, {{149},{  8}}, {{ 85},{  8}}, {{213},{  8}}, {{ 53},{  8}}, {{181},{  8}}, {{117},{  8}}, {{245},{  8}}, {{ 13},{  8}}, {{141},{  8}},
        {{ 77},{  8}}, {{205},{  8}}, {{ 45},{  8}}, {{173},{  8}}, {{109},{  8}}, {{237},{  8}}, {{ 29},{  8}}, {{157},{  8}}, {{ 93},{  8}}, {{221},{  8}},
        {{ 61},{  8}}, {{189},{  8}}, {{125},{  8}}, {{253},{  8}}, {{ 19},{  9}}, {{275},{  9}}, {{147},{  9}}, {{403},{  9}}, {{ 83},{  9}}, {{339},{  9}},
        {{211},{  9}}, {{467},{  9}}, {{ 51},{  9}}, {{307},{  9}}, {{179},{  9}}, {{435},{  9}}, {{115},{  9}}, {{371},{  9}}, {{243},{  9}}, {{499},{  9}},
        {{ 11},{  9}}, {{267},{  9}}, {{139},{  9}}, {{395},{  9}}, {{ 75},{  9}}, {{331},{  9}}, {{203},{  9}}, {{459},{  9}}, {{ 43},{  9}}, {{299},{  9}},
        {{171},{  9}}, {{427},{  9}}, {{107},{  9}}, {{363},{  9}}, {{235},{  9}}, {{491},{  9}}, {{ 27},{  9}}, {{283},{  9}}, {{155},{  9}}, {{411},{  9}},
        {{ 91},{  9}}, {{347},{  9}}, {{219},{  9}}, {{475},{  9}}, {{ 59},{  9}}, {{315},{  9}}, {{187},{  9}}, {{443},{  9}}, {{123},{  9}}, {{379},{  9}},
        {{251},{  9}}, {{507},{  9}}, {{  7},{  9}}, {{263},{  9}}, {{135},{  9}}, {{391},{  9}}, {{ 71},{  9}}, {{327},{  9}}, {{199},{  9}}, {{455},{  9}},
        {{ 39},{  9}}, {{295},{  9}}, {{167},{  9}}, {{423},{  9}}, {{103},{  9}}, {{359},{  9}}, {{231},{  9}}, {{487},{  9}}, {{ 23},{  9}}, {{279},{  9}},
        {{151},{  9}}, {{407},{  9}}, {{ 87},{  9}}, {{343},{  9}}, {{215},{  9}}, {{471},{  9}}, {{ 55},{  9}}, {{311},{  9}}, {{183},{  9}}, {{439},{  9}},
        {{119},{  9}}, {{375},{  9}}, {{247},{  9}}, {{503},{  9}}, {{ 15},{  9}}, {{271},{  9}}, {{143},{  9}}, {{399},{  9}}, {{ 79},{  9}}, {{335},{  9}},
        {{207},{  9}}, {{463},{  9}}, {{ 47},{  9}}, {{303},{  9}}, {{175},{  9}}, {{431},{  9}}, {{111},{  9}}, {{367},{  9}}, {{239},{  9}}, {{495},{  9}},
        {{ 31},{  9}}, {{287},{  9}}, {{159},{  9}}, {{415},{  9}}, {{ 95},{  9}}, {{351},{  9}}, {{223},{  9}}, {{479},{  9}}, {{ 63},{  9}}, {{319},{  9}},
        {{191},{  9}}, {{447},{  9}}, {{127},{  9}}, {{383},{  9}}, {{255},{  9}}, {{511},{  9}}, {{  0},{  7}}, {{ 64},{  7}}, {{ 32},{  7}}, {{ 96},{  7}},
        {{ 16},{  7}}, {{ 80},{  7}}, {{ 48},{  7}}, {{112},{  7}}, {{  8},{  7}}, {{ 72},{  7}}, {{ 40},{  7}}, {{104},{  7}}, {{ 24},{  7}}, {{ 88},{  7}},
        {{ 56},{  7}}, {{120},{  7}}, {{  4},{  7}}, {{ 68},{  7}}, {{ 36},{  7}}, {{100},{  7}}, {{ 20},{  7}}, {{ 84},{  7}}, {{ 52},{  7}}, {{116},{  7}},
        {{  3},{  8}}, {{131},{  8}}, {{ 67},{  8}}, {{195},{  8}}, {{ 35},{  8}}, {{163},{  8}}, {{ 99},{  8}}, {{227},{  8}}
    };

    static constexpr ct_data static_dtree[D_CODES] =
    {
        {{ 0},{ 5}}, {{16},{ 5}}, {{ 8},{ 5}}, {{24},{ 5}}, {{ 4},{ 5}},
        {{20},{ 5}}, {{12},{ 5}}, {{28},{ 5}}, {{ 2},{ 5}}, {{18},{ 5}},
        {{10},{ 5}}, {{26},{ 5}}, {{ 6},{ 5}}, {{22},{ 5}}, {{14},{ 5}},
        {{30},{ 5}}, {{ 1},{ 5}}, {{17},{ 5}}, {{ 9},{ 5}}, {{25},{ 5}},
        {{ 5},{ 5}}, {{21},{ 5}}, {{13},{ 5}}, {{29},{ 5}}, {{ 3},{ 5}},
        {{19},{ 5}}, {{11},{ 5}}, {{27},{ 5}}, {{ 7},{ 5}}, {{23},{ 5}}
    };

    static constexpr uint8_t lengthCodes[MAX_MATCH - MIN_MATCH + 1] =
    {
        0,  1,  2,  3,  4,  5,  6,  7,  8,  8,  9,  9, 10, 10, 11, 11, 12, 12, 12, 12, 13, 13, 13, 13, 14, 14, 14, 14, 15, 15, 15, 15, 16, 16, 16, 16, 16, 16, 16, 16,
        17, 17, 17, 17, 17, 17, 17, 17, 18, 18, 18, 18, 18, 18, 18, 18, 19, 19, 19, 19, 19, 19, 19, 19, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 23, 23, 23, 23, 23, 23, 23, 23,
        23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
        25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 26, 26, 26, 26, 26, 26, 26, 26,
        26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
        27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28
    };

    static uint8_t getDistanceCode (uint32_t dist)
    {
        static constexpr uint8_t codes[] =
        {
            0,  1,  2,  3,  4,  4,  5,  5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  8, 8,  8,  8,  8,  9,  9,  9,  9,  9,  9,  9,  9, 10, 10, 10, 10, 10, 10, 10, 10,
            10, 10, 10, 10, 10, 10, 10, 10, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
            12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
            13, 13, 13, 13, 13, 13, 13, 13, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
            14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
            15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,  0,  0, 16, 17, 18, 18, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 22, 22, 22, 22, 22, 22, 22, 22,
            23, 23, 23, 23, 23, 23, 23, 23, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
            26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 27, 27, 27, 27, 27, 27, 27, 27,
            27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
            28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28,
            28, 28, 28, 28, 28, 28, 28, 28, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
            29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29
        };

        return codes[dist < 256 ? dist : (256 + (dist >> 7))];
    }
};

//==============================================================================
//==============================================================================
//==============================================================================

struct InflateStream
{
    enum class Mode
    {
        HEAD,       /* i: waiting for magic header */
        FLAGS,      /* i: waiting for method and flags (gzip) */
        TIME,       /* i: waiting for modification time (gzip) */
        OS,         /* i: waiting for extra flags and operating system (gzip) */
        EXLEN,      /* i: waiting for extra length (gzip) */
        EXTRA,      /* i: waiting for extra bytes (gzip) */
        NAME,       /* i: waiting for end of file name (gzip) */
        COMMENT,    /* i: waiting for end of comment (gzip) */
        HCRC,       /* i: waiting for header crc (gzip) */
        DICTID,     /* i: waiting for dictionary check value */
        DICT,       /* waiting for inflateSetDictionary() call */
            TYPEDO,     /* i: same, but skip check to exit inflate on new block */
            STORED,     /* i: waiting for stored size (length and complement) */
            COPY,       /* i/o: waiting for input or output to copy stored block */
            TABLE,      /* i: waiting for dynamic block table lengths */
            LENLENS,    /* i: waiting for code length code lengths */
            CODELENS,   /* i: waiting for length/lit and distance code lengths */
                LEN,        /* i: waiting for length/lit code */
                LENEXT,     /* i: waiting for length extra bits */
                DIST,       /* i: waiting for distance code */
                DISTEXT,    /* i: waiting for distance extra bits */
                MATCH,      /* o: waiting for output space to copy string */
                LIT,        /* o: waiting for output space to write literal */
        CHECK,      /* i: waiting for 32-bit check value */
        LENGTH,     /* i: waiting for 32-bit length (gzip) */
        DONE,       /* finished check, done -- remain here until reset */
        BAD,        /* got a data error -- remain here until reset */
        MEM,        /* got an inflate() memory error -- remain here until reset */
        SYNC        /* looking for synchronization bytes to restart inflate() */
    };

    //==============================================================================
    uint8_t* next_in = nullptr; /* next input byte */
    uint32_t avail_in = 0; /* number of bytes available at next_in */
    size_t total_in = 0; /* total nb of input bytes read so far */

    uint8_t* next_out = nullptr; /* next output byte should be put there */
    uint32_t avail_out = 0; /* remaining free space at next_out */
    size_t total_out = 0; /* total nb of bytes output so far */

    const char* msg = nullptr;

    DataType data_type; /* best guess about the data type: binary or text */
    unsigned long adler; /* adler32 value of the uncompressed data */
    Mode mode; /* current inflate mode */
    bool last; /* true if processing last block */
    int wrap; /* bit 0 true for zlib, bit 1 true for gzip */
    int havedict; /* true if dictionary provided */
    int flags; /* gzip header method and flags (0 if zlib) */
    uint32_t dmax; /* zlib header max distance (INFLATE_STRICT) */
    unsigned long check; /* protected copy of check value */
    unsigned long total; /* protected copy of output count */
    gz_header* head; /* where to save gzip header information */
    /* sliding window */
    uint32_t wbits; /* log base 2 of requested window size */
    uint32_t wsize; /* window size or zero if not using window */
    uint32_t whave; /* valid bytes in the window */
    uint32_t write; /* window write index */
    std::vector<uint8_t> window; /* allocated sliding window, if needed */
    /* bit accumulator */
    unsigned long hold; /* input bit accumulator */
    uint32_t bits; /* number of bits in "in" */
    /* for string and stored block copying */
    uint32_t length; /* literal or length of data to copy */
    uint32_t offset; /* distance back to copy string from */
    /* for table and code decoding */
    uint32_t extra; /* extra bits needed */
    /* fixed and dynamic code tables */
    code const* lencode; /* starting table for length/literal codes */
    code const* distcode; /* starting table for distance codes */
    uint32_t lenbits; /* index bits for lencode */
    uint32_t distbits; /* index bits for distcode */
    /* dynamic table building */
    uint32_t ncode; /* number of code length code lengths */
    uint32_t nlen; /* number of length code lengths */
    uint32_t ndist; /* number of distance code lengths */
    uint32_t have; /* number of code lengths in lens[] */
    code* next; /* next available space in codes[] */
    uint16_t lens[320]; /* temporary storage for code lengths */
    uint16_t work[288]; /* work area for code table building */

    static constexpr uint32_t ENOUGH = 2048;

    code codes[ENOUGH]; /* space for code tables */

    //==============================================================================
    ErrorCode initialise (int windowBits)
    {
        msg = nullptr; /* in case we return an error */

        if (windowBits < 0)
        {
            wrap = 0;
            windowBits = -windowBits;
        }
        else
        {
            wrap = (windowBits >> 4) + 1;
        }

        if (windowBits < 8 || windowBits > 15)
            return ErrorCode::STREAM_ERROR;

        wbits = (uint32_t) windowBits;
        window.clear();
        return reset();
    }

    ErrorCode reset()
    {
        total_in = total_out = total = 0;
        msg = nullptr;
        adler = 1;        /* to support ill-conceived Java test suite */
        mode = Mode::HEAD;
        last = false;
        havedict = 0;
        dmax = 32768U;
        head = nullptr;
        wsize = 0;
        whave = 0;
        write = 0;
        hold = 0;
        bits = 0;
        lencode = distcode = next = codes;
        return ErrorCode::OK;
    }

    ErrorCode inflateSetDictionary (const uint8_t* dictionary, uint32_t dictLength)
    {
        if (wrap != 0 && mode != Mode::DICT)
            return ErrorCode::STREAM_ERROR;

        /* check for correct dictionary id */
        if (mode == Mode::DICT)
        {
            auto crc = Checksum::adler32 (0, nullptr, 0);
            crc = Checksum::adler32 (crc, dictionary, dictLength);
            if (crc != check)
                return ErrorCode::DATA_ERROR;
        }

        if (updatewindow (avail_out))
        {
            mode = Mode::MEM;
            return ErrorCode::MEM_ERROR;
        }

        if (dictLength > wsize)
        {
            memcpy (window.data(), dictionary + dictLength - wsize, wsize);
            whave = wsize;
        }
        else
        {
            memcpy (window.data() + wsize - dictLength, dictionary, dictLength);
            whave = dictLength;
        }

        havedict = 1;
        return ErrorCode::OK;
    }

    ErrorCode inflateGetHeader (gz_header* header)
    {
        if ((wrap & 2) == 0)
            return ErrorCode::STREAM_ERROR;

        head = header;
        head->done = 0;
        return ErrorCode::OK;
    }

    /*
        Update the window with the last wsize (normally 32K) bytes written before
        returning.  If window does not exist yet, create it.  This is only called
        when a window is already in use, or when output has been written during this
        inflate call, but the end of the deflate stream has not been reached yet.
        It is also called to create a window for dictionary data when a dictionary
        is loaded.

        Providing output buffers larger than 32K to inflate() should provide a speed
        advantage, since only the last 32K of output is copied to the sliding window
        upon return from inflate(), and since all distances after the first 32K of
        output will fall in the output data, making match copies simpler and faster.
        The advantage may be dependent on the size of the processor's data caches.
    */
    int updatewindow (uint32_t out)
    {
        /* if it hasn't been done already, allocate space for the window */
        if (window.empty())
            window.resize ((size_t) (1ull << wbits));

        /* if window not in use yet, initialize */
        if (wsize == 0)
        {
            wsize = 1U << wbits;
            write = 0;
            whave = 0;
        }

        /* copy wsize or less output bytes into the circular window */
        auto copy = out - avail_out;

        if (copy >= wsize)
        {
            memcpy (window.data(), next_out - wsize, wsize);
            write = 0;
            whave = wsize;
        }
        else
        {
            auto dist = wsize - write;
            if (dist > copy) dist = copy;
            memcpy (window.data() + write, next_out - copy, dist);
            copy -= dist;

            if (copy)
            {
                memcpy (window.data(), next_out - copy, copy);
                write = copy;
                whave = wsize;
            }
            else
            {
                write += dist;
                if (write == wsize) write = 0;
                if (whave < wsize) whave += dist;
            }
        }
        return 0;
    }

    /*
    Search buf[0..len-1] for the pattern: 0, 0, 0xff, 0xff.  Return when found
    or when out of input.  When called, *have is the number of pattern bytes
    found in order so far, in 0..3.  On return *have is updated to the new
    state.  If on return *have equals four, then the pattern was found and the
    return value is how many bytes were read including the last byte of the
    pattern.  If *have is less than four, then the pattern has not been found
    yet and the return value is len.  In the latter case, syncsearch() can be
    called again with more data and the *have state.  *have is initialized to
    zero for the first call.
    */
    static uint32_t syncsearch (unsigned* have, uint8_t* buf, uint32_t len)
    {
        auto got = *have;
        uint32_t next = 0;
        while (next < len && got < 4)
        {
            if ((int) (buf[next]) == (got < 2 ? 0 : 0xff))
                got++;
            else if (buf[next])
                got = 0;
            else
                got = 4 - got;
            next++;
        }
        *have = got;
        return next;
    }

    ErrorCode inflateSync()
    {
        uint32_t len; /* number of bytes to look at or looked at */
        uint64_t in, out; /* temporary to save total_in and total_out */
        uint8_t buf[4]; /* to restore bit buffer to byte string */

        if (avail_in == 0 && bits < 8)
            return ErrorCode::BUF_ERROR;

        /* if first time, start search in bit buffer */
        if (mode != Mode::SYNC)
        {
            mode = Mode::SYNC;
            hold <<= bits & 7;
            bits -= bits & 7;
            len = 0;
            while (bits >= 8)
            {
                buf[len++] = (uint8_t) hold;
                hold >>= 8;
                bits -= 8;
            }
            have = 0;
            syncsearch (&(have), buf, len);
        }

        /* search available input */
        len = syncsearch (&(have), next_in, avail_in);
        avail_in -= len;
        next_in += len;
        total_in += len;

        /* return no joy or set up to restart inflate() on a new block */
        if (have != 4)
            return ErrorCode::DATA_ERROR;

        in = total_in;
        out = total_out;
        reset();
        total_in = in;
        total_out = out;
        mode = Mode::TYPEDO;
        return ErrorCode::OK;
    }

    /*
    Decode literal, length, and distance codes and write out the resulting
    literal and match bytes until either not enough input or output is
    available, an end-of-block is encountered, or a data error is encountered.
    When large enough input and output buffers are supplied to inflate(), for
    example, a 16K input buffer and a 64K output buffer, more than 95% of the
    inflate execution time is spent in this routine.

    Entry assumptions:

            state->mode == LEN
            strm->avail_in >= 6
            strm->avail_out >= 258
            start >= strm->avail_out
            state->bits < 8

    On return, state->mode is one of:

            LEN -- ran out of enough output space or enough available input
            TYPE -- reached end of block code, inflate() to interpret next block
            BAD -- error in block data

    Notes:

        - The maximum input bits used by a length/distance pair is 15 bits for the
        length code, 5 bits for the length extra, 15 bits for the distance code,
        and 13 bits for the distance extra.  This totals 48 bits, or six bytes.
        Therefore if strm->avail_in >= 6, then there is enough input to avoid
        checking for available input while decoding.

        - The maximum bytes that a single length/distance pair can output is 258
        bytes, which is the maximum length that can be coded.  inflate_fast()
        requires strm->avail_out >= 258 for each loop to avoid checking for
        output space.
    */
    void inflate_fast (uint32_t start)
    {
        uint8_t* in = next_in - 1;
        uint8_t* last2 = in + (avail_in - 5);
        uint8_t* out = next_out - 1;
        uint8_t* beg = out - (start - avail_out);
        uint8_t* end = out + (avail_out - 257);
        auto wsize2 = wsize;
        auto whave2 = whave;
        auto write2 = write;
        auto window2 = window.data();
        auto hold2 = hold;
        auto bits2 = bits;
        auto lcode = lencode;
        auto dcode = distcode;
        uint32_t lmask = (1U << lenbits) - 1;
        uint32_t dmask = (1U << distbits) - 1;

        uint32_t op; /* code bits, operation, extra bits, or */
        /*  window position, window bytes to copy */
        uint32_t len; /* match length, unused bytes */
        uint32_t dist; /* match distance */
        uint8_t* from; /* where to copy match from */

        /* decode literals and length/distances until end-of-block or not enough
        input data or output space */
        do
        {
            if (bits2 < 15)
            {
                hold2 += (unsigned long) (*++in << bits2);
                bits2 += 8;
                hold2 += (unsigned long) (*++in << bits2);
                bits2 += 8;
            }
            auto thisx = lcode[hold2 & lmask];

        dolen:
            {
                op = (uint32_t) thisx.bits;
                hold2 >>= op;
                bits2 -= op;
                op = (uint32_t) thisx.op;
                if (op == 0)
                { /* literal */
                    *++out = (uint8_t) thisx.val;
                }
                else if (op & 16)
                { /* length base */
                    len = (uint32_t) thisx.val;
                    op &= 15; /* number of extra bits */
                    if (op)
                    {
                        if (bits2 < op)
                        {
                            hold2 += (unsigned long) (*++in << bits2);
                            bits2 += 8;
                        }
                        len += (uint32_t) hold2 & ((1U << op) - 1);
                        hold2 >>= op;
                        bits2 -= op;
                    }
                    if (bits2 < 15)
                    {
                        hold2 += (unsigned long) (*++in << bits2);
                        bits2 += 8;
                        hold2 += (unsigned long) (*++in << bits2);
                        bits2 += 8;
                    }
                    thisx = dcode[hold2 & dmask];

                dodist:
                    {
                        op = (uint32_t) thisx.bits;
                        hold2 >>= op;
                        bits2 -= op;
                        op = (uint32_t) thisx.op;
                        if (op & 16)
                        { /* distance base */
                            dist = (uint32_t) thisx.val;
                            op &= 15; /* number of extra bits */
                            if (bits2 < op)
                            {
                                hold2 += (unsigned long) (*++in << bits2);
                                bits2 += 8;
                                if (bits2 < op)
                                {
                                    hold2 += (unsigned long) (*++in << bits2);
                                    bits2 += 8;
                                }
                            }
                            dist += (uint32_t) hold2 & ((1U << op) - 1);
                            hold2 >>= op;
                            bits2 -= op;
                            op = (uint32_t) (out - beg); /* max distance in output */
                            if (dist > op)
                            { /* see if copy from window */
                                op = dist - op; /* distance back in window */
                                if (op > whave2)
                                {
                                    msg = "invalid distance too far back";
                                    mode = Mode::BAD;
                                    break;
                                }
                                from = window2 - 1;
                                if (write2 == 0)
                                { /* very common case */
                                    from += wsize2 - op;
                                    if (op < len)
                                    { /* some from window */
                                        len -= op;
                                        do
                                        {
                                            *++out = *++from;
                                        } while (--op);
                                        from = out - dist; /* rest from output */
                                    }
                                }
                                else if (write2 < op)
                                { /* wrap around window */
                                    from += wsize2 + write2 - op;
                                    op -= write2;
                                    if (op < len)
                                    { /* some from end of window */
                                        len -= op;
                                        do
                                        {
                                            *++out = *++from;
                                        } while (--op);
                                        from = window2 - 1;
                                        if (write2 < len)
                                        { /* some from start of window */
                                            op = write2;
                                            len -= op;
                                            do
                                            {
                                                *++out = *++from;
                                            } while (--op);
                                            from = out - dist; /* rest from output */
                                        }
                                    }
                                }
                                else
                                { /* contiguous in window */
                                    from += write2 - op;
                                    if (op < len)
                                    { /* some from window */
                                        len -= op;
                                        do
                                        {
                                            *++out = *++from;
                                        } while (--op);
                                        from = out - dist; /* rest from output */
                                    }
                                }
                                while (len > 2)
                                {
                                    *++out = *++from;
                                    *++out = *++from;
                                    *++out = *++from;
                                    len -= 3;
                                }
                                if (len)
                                {
                                    *++out = *++from;
                                    if (len > 1)
                                        *++out = *++from;
                                }
                            }
                            else
                            {
                                from = out - dist; /* copy direct from output */
                                do
                                { /* minimum length is three */
                                    *++out = *++from;
                                    *++out = *++from;
                                    *++out = *++from;
                                    len -= 3;
                                } while (len > 2);
                                if (len)
                                {
                                    *++out = *++from;
                                    if (len > 1)
                                        *++out = *++from;
                                }
                            }
                        }
                        else if ((op & 64) == 0)
                        { /* 2nd level distance code */
                            thisx = dcode[thisx.val + (hold2 & ((1U << op) - 1))];
                            goto dodist;
                        }
                        else
                        {
                            msg = "invalid distance code";
                            mode = Mode::BAD;
                            break;
                        }
                    }
                }
                else if ((op & 64) == 0)
                { /* 2nd level length code */
                    thisx = lcode[thisx.val + (hold2 & ((1U << op) - 1))];
                    goto dolen;
                }
                else if (op & 32)
                { /* end-of-block */
                    mode = Mode::TYPEDO;
                    break;
                }
                else
                {
                    msg = "invalid literal/length code";
                    mode = Mode::BAD;
                    break;
                }
            }
        } while (in < last2 && out < end);

        /* return unused bytes (on entry, bits < 8, so in won't go too far back) */
        len = bits2 >> 3;
        in -= len;
        bits2 -= len << 3;
        hold2 &= (1U << bits2) - 1;

        /* update state and return */
        next_in = in + 1;
        next_out = out + 1;
        avail_in = (uint32_t) (in < last2 ? 5 + (last2 - in) : 5 - (in - last2));
        avail_out = (uint32_t) (out < end ? 257 + (end - out) : 257 - (out - end));
        hold = hold2;
        bits = bits2;
    }

    void initialiseFixedTables()
    {
        static constexpr code lengths[512] =
        {
            {96,7,0}, {0,8,80}, {0,8,16}, {20,8,115}, {18,7,31}, {0,8,112}, {0,8,48}, {0,9,192}, {16,7,10}, {0,8,96}, {0,8,32}, {0,9,160}, {0,8,0}, {0,8,128},
            {0,8,64}, {0,9,224}, {16,7,6}, {0,8,88}, {0,8,24}, {0,9,144}, {19,7,59}, {0,8,120}, {0,8,56}, {0,9,208}, {17,7,17}, {0,8,104}, {0,8,40}, {0,9,176},
            {0,8,8}, {0,8,136}, {0,8,72}, {0,9,240}, {16,7,4}, {0,8,84}, {0,8,20}, {21,8,227}, {19,7,43}, {0,8,116}, {0,8,52}, {0,9,200}, {17,7,13}, {0,8,100},
            {0,8,36}, {0,9,168}, {0,8,4}, {0,8,132}, {0,8,68}, {0,9,232}, {16,7,8}, {0,8,92}, {0,8,28}, {0,9,152}, {20,7,83}, {0,8,124}, {0,8,60}, {0,9,216},
            {18,7,23}, {0,8,108}, {0,8,44}, {0,9,184}, {0,8,12}, {0,8,140}, {0,8,76}, {0,9,248}, {16,7,3}, {0,8,82}, {0,8,18}, {21,8,163}, {19,7,35}, {0,8,114},
            {0,8,50}, {0,9,196}, {17,7,11}, {0,8,98}, {0,8,34}, {0,9,164}, {0,8,2}, {0,8,130}, {0,8,66}, {0,9,228}, {16,7,7}, {0,8,90}, {0,8,26}, {0,9,148},
            {20,7,67}, {0,8,122}, {0,8,58}, {0,9,212}, {18,7,19}, {0,8,106}, {0,8,42}, {0,9,180}, {0,8,10}, {0,8,138}, {0,8,74}, {0,9,244}, {16,7,5}, {0,8,86},
            {0,8,22}, {64,8,0}, {19,7,51}, {0,8,118}, {0,8,54}, {0,9,204}, {17,7,15}, {0,8,102}, {0,8,38}, {0,9,172}, {0,8,6}, {0,8,134}, {0,8,70}, {0,9,236},
            {16,7,9}, {0,8,94}, {0,8,30}, {0,9,156}, {20,7,99}, {0,8,126}, {0,8,62}, {0,9,220}, {18,7,27}, {0,8,110}, {0,8,46}, {0,9,188}, {0,8,14}, {0,8,142},
            {0,8,78}, {0,9,252}, {96,7,0}, {0,8,81}, {0,8,17}, {21,8,131}, {18,7,31}, {0,8,113}, {0,8,49}, {0,9,194}, {16,7,10}, {0,8,97}, {0,8,33}, {0,9,162},
            {0,8,1}, {0,8,129}, {0,8,65}, {0,9,226}, {16,7,6}, {0,8,89}, {0,8,25}, {0,9,146}, {19,7,59}, {0,8,121}, {0,8,57}, {0,9,210}, {17,7,17}, {0,8,105},
            {0,8,41}, {0,9,178}, {0,8,9}, {0,8,137}, {0,8,73}, {0,9,242}, {16,7,4}, {0,8,85}, {0,8,21}, {16,8,258}, {19,7,43}, {0,8,117}, {0,8,53}, {0,9,202},
            {17,7,13}, {0,8,101}, {0,8,37}, {0,9,170}, {0,8,5}, {0,8,133}, {0,8,69}, {0,9,234}, {16,7,8}, {0,8,93}, {0,8,29}, {0,9,154}, {20,7,83}, {0,8,125},
            {0,8,61}, {0,9,218}, {18,7,23}, {0,8,109}, {0,8,45}, {0,9,186}, {0,8,13}, {0,8,141}, {0,8,77}, {0,9,250}, {16,7,3}, {0,8,83}, {0,8,19}, {21,8,195},
            {19,7,35}, {0,8,115}, {0,8,51}, {0,9,198}, {17,7,11}, {0,8,99}, {0,8,35}, {0,9,166}, {0,8,3}, {0,8,131}, {0,8,67}, {0,9,230}, {16,7,7}, {0,8,91},
            {0,8,27}, {0,9,150}, {20,7,67}, {0,8,123}, {0,8,59}, {0,9,214}, {18,7,19}, {0,8,107}, {0,8,43}, {0,9,182}, {0,8,11}, {0,8,139}, {0,8,75}, {0,9,246},
            {16,7,5}, {0,8,87}, {0,8,23}, {64,8,0}, {19,7,51}, {0,8,119}, {0,8,55}, {0,9,206}, {17,7,15}, {0,8,103}, {0,8,39}, {0,9,174}, {0,8,7}, {0,8,135},
            {0,8,71}, {0,9,238}, {16,7,9}, {0,8,95}, {0,8,31}, {0,9,158}, {20,7,99}, {0,8,127}, {0,8,63}, {0,9,222}, {18,7,27}, {0,8,111}, {0,8,47}, {0,9,190},
            {0,8,15}, {0,8,143}, {0,8,79}, {0,9,254}, {96,7,0}, {0,8,80}, {0,8,16}, {20,8,115}, {18,7,31}, {0,8,112}, {0,8,48}, {0,9,193}, {16,7,10}, {0,8,96},
            {0,8,32}, {0,9,161}, {0,8,0}, {0,8,128}, {0,8,64}, {0,9,225}, {16,7,6}, {0,8,88}, {0,8,24}, {0,9,145}, {19,7,59}, {0,8,120}, {0,8,56}, {0,9,209},
            {17,7,17}, {0,8,104}, {0,8,40}, {0,9,177}, {0,8,8}, {0,8,136}, {0,8,72}, {0,9,241}, {16,7,4}, {0,8,84}, {0,8,20}, {21,8,227}, {19,7,43}, {0,8,116},
            {0,8,52}, {0,9,201}, {17,7,13}, {0,8,100}, {0,8,36}, {0,9,169}, {0,8,4}, {0,8,132}, {0,8,68}, {0,9,233}, {16,7,8}, {0,8,92}, {0,8,28}, {0,9,153},
            {20,7,83}, {0,8,124}, {0,8,60}, {0,9,217}, {18,7,23}, {0,8,108}, {0,8,44}, {0,9,185}, {0,8,12}, {0,8,140}, {0,8,76}, {0,9,249}, {16,7,3}, {0,8,82},
            {0,8,18}, {21,8,163}, {19,7,35}, {0,8,114}, {0,8,50}, {0,9,197}, {17,7,11}, {0,8,98}, {0,8,34}, {0,9,165}, {0,8,2}, {0,8,130}, {0,8,66}, {0,9,229},
            {16,7,7}, {0,8,90}, {0,8,26}, {0,9,149}, {20,7,67}, {0,8,122}, {0,8,58}, {0,9,213}, {18,7,19}, {0,8,106}, {0,8,42}, {0,9,181}, {0,8,10}, {0,8,138},
            {0,8,74}, {0,9,245}, {16,7,5}, {0,8,86}, {0,8,22}, {64,8,0}, {19,7,51}, {0,8,118}, {0,8,54}, {0,9,205}, {17,7,15}, {0,8,102}, {0,8,38}, {0,9,173},
            {0,8,6}, {0,8,134}, {0,8,70}, {0,9,237}, {16,7,9}, {0,8,94}, {0,8,30}, {0,9,157}, {20,7,99}, {0,8,126}, {0,8,62}, {0,9,221}, {18,7,27}, {0,8,110},
            {0,8,46}, {0,9,189}, {0,8,14}, {0,8,142}, {0,8,78}, {0,9,253}, {96,7,0}, {0,8,81}, {0,8,17}, {21,8,131}, {18,7,31}, {0,8,113}, {0,8,49}, {0,9,195},
            {16,7,10}, {0,8,97}, {0,8,33}, {0,9,163}, {0,8,1}, {0,8,129}, {0,8,65}, {0,9,227}, {16,7,6}, {0,8,89}, {0,8,25}, {0,9,147}, {19,7,59}, {0,8,121},
            {0,8,57}, {0,9,211}, {17,7,17}, {0,8,105}, {0,8,41}, {0,9,179}, {0,8,9}, {0,8,137}, {0,8,73}, {0,9,243}, {16,7,4}, {0,8,85}, {0,8,21}, {16,8,258},
            {19,7,43}, {0,8,117}, {0,8,53}, {0,9,203}, {17,7,13}, {0,8,101}, {0,8,37}, {0,9,171}, {0,8,5}, {0,8,133}, {0,8,69}, {0,9,235}, {16,7,8}, {0,8,93},
            {0,8,29}, {0,9,155}, {20,7,83}, {0,8,125}, {0,8,61}, {0,9,219}, {18,7,23}, {0,8,109}, {0,8,45}, {0,9,187}, {0,8,13}, {0,8,141}, {0,8,77}, {0,9,251},
            {16,7,3}, {0,8,83}, {0,8,19}, {21,8,195}, {19,7,35}, {0,8,115}, {0,8,51}, {0,9,199}, {17,7,11}, {0,8,99}, {0,8,35}, {0,9,167}, {0,8,3}, {0,8,131},
            {0,8,67}, {0,9,231}, {16,7,7}, {0,8,91}, {0,8,27}, {0,9,151}, {20,7,67}, {0,8,123}, {0,8,59}, {0,9,215}, {18,7,19}, {0,8,107}, {0,8,43}, {0,9,183},
            {0,8,11}, {0,8,139}, {0,8,75}, {0,9,247}, {16,7,5}, {0,8,87}, {0,8,23}, {64,8,0}, {19,7,51}, {0,8,119}, {0,8,55}, {0,9,207}, {17,7,15}, {0,8,103},
            {0,8,39}, {0,9,175}, {0,8,7}, {0,8,135}, {0,8,71}, {0,9,239}, {16,7,9}, {0,8,95}, {0,8,31}, {0,9,159}, {20,7,99}, {0,8,127}, {0,8,63}, {0,9,223},
            {18,7,27}, {0,8,111}, {0,8,47}, {0,9,191}, {0,8,15}, {0,8,143}, {0,8,79}, {0,9,255}
        };

        static constexpr code distances[32] =
        {
            {16,5,1}, {23,5,257}, {19,5,17}, {27,5,4097}, {17,5,5}, {25,5,1025},
            {21,5,65}, {29,5,16385}, {16,5,3}, {24,5,513}, {20,5,33}, {28,5,8193},
            {18,5,9}, {26,5,2049}, {22,5,129}, {64,5,0}, {16,5,2}, {23,5,385},
            {19,5,25}, {27,5,6145}, {17,5,7}, {25,5,1537}, {21,5,97}, {29,5,24577},
            {16,5,4}, {24,5,769}, {20,5,49}, {28,5,12289}, {18,5,13}, {26,5,3073},
            {22,5,193}, {64,5,0}
        };

        lencode = lengths;
        lenbits = 9;
        distcode = distances;
        distbits = 5;
    }

    enum class CodeType
    {
        CODES,
        LENS,
        DISTS
    };

    static ErrorCode createTables (CodeType type, uint16_t* lens, uint32_t codes, code** table, uint32_t* bits, uint16_t* work)
    {
        static constexpr uint32_t MAXBITS = 15;

        uint32_t min, max; /* minimum and maximum code lengths */
        uint32_t root; /* number of index bits for root table */
        uint32_t curr; /* number of index bits for current table */
        uint32_t drop; /* code bits to drop for sub-table */
        int left; /* number of prefix codes available */
        uint32_t used; /* code entries in table used */
        uint32_t huff; /* Huffman code */
        uint32_t incr; /* for incrementing code, index */
        uint32_t fill; /* index for replicating entries */
        uint32_t low; /* low bits for current root entry */
        uint32_t mask; /* mask for low root bits */
        code thisx; /* table entry for duplication */
        code* next; /* next available space in table */
        const uint16_t* base; /* base value table to use */
        const uint16_t* extra; /* extra bits table to use */
        int end; /* use base and extra for symbol > end */
        uint16_t count[MAXBITS + 1]; /* number of codes of each length */
        uint16_t offs[MAXBITS + 1]; /* offsets in table for each length */

        static constexpr uint16_t lbase[31] = { /* Length codes 257..285 base */
            3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
            35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
        static constexpr uint16_t lext[31] = { /* Length codes 257..285 extra */
            16, 16, 16, 16, 16, 16, 16, 16, 17, 17, 17, 17, 18, 18, 18, 18,
            19, 19, 19, 19, 20, 20, 20, 20, 21, 21, 21, 21, 16, 201, 196};
        static constexpr uint16_t dbase[32] = { /* Distance codes 0..29 base */
            1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
            257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
            8193, 12289, 16385, 24577, 0, 0};
        static constexpr uint16_t dext[32] = { /* Distance codes 0..29 extra */
            16, 16, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22,
            23, 23, 24, 24, 25, 25, 26, 26, 27, 27,
            28, 28, 29, 29, 64, 64};

        for (uint32_t len = 0; len <= MAXBITS; len++)
            count[len] = 0;
        for (uint32_t sym = 0; sym < codes; sym++)
            count[lens[sym]]++;

        root = *bits;
        for (max = MAXBITS; max >= 1; max--)
            if (count[max] != 0)
                break;
        if (root > max)
            root = max;
        if (max == 0)
        { /* no symbols to code at all */
            thisx.op = 64; /* invalid code marker */
            thisx.bits = 1;
            thisx.val = (uint16_t) 0;
            *(*table)++ = thisx; /* make a table to force an error */
            *(*table)++ = thisx;
            *bits = 1;
            return ErrorCode::OK; /* no symbols, but wait for decoding to report error */
        }
        for (min = 1; min <= MAXBITS; min++)
            if (count[min] != 0)
                break;
        if (root < min)
            root = min;

        left = 1;
        for (uint32_t len = 1; len <= MAXBITS; len++)
        {
            left <<= 1;
            left -= count[len];
            if (left < 0)
                return ErrorCode::ERRNO; /* over-subscribed */
        }
        if (left > 0 && (type == CodeType::CODES || max != 1))
            return ErrorCode::ERRNO; /* incomplete set */

        offs[1] = 0;
        for (uint32_t len = 1; len < MAXBITS; len++)
            offs[len + 1] = offs[len] + count[len];

        for (uint32_t sym = 0; sym < codes; sym++)
            if (lens[sym] != 0)
                work[offs[lens[sym]]++] = (uint16_t) sym;

        switch (type)
        {
            case CodeType::CODES:
                base = extra = work;
                end = 19;
                break;
            case CodeType::LENS:
                base = lbase;
                base -= 257;
                extra = lext;
                extra -= 257;
                end = 256;
                break;
            case CodeType::DISTS:
            default:
                base = dbase;
                extra = dext;
                end = -1;
                break;
        }

        huff = 0; /* starting code */
        uint32_t sym = 0; /* starting code symbol */
        auto len = min; /* starting code length */
        next = *table; /* current table to fill in */
        curr = root; /* current table index bits */
        drop = 0; /* current bits to drop from code for index */
        low = (uint32_t) -1; /* trigger new sub-table when len > root */
        used = 1U << root; /* use root table entries */
        mask = used - 1; /* mask for comparing low */

        static constexpr uint32_t MAXD = 592;

        /* check available table space */
        if (type == CodeType::LENS && used >= ENOUGH - MAXD)
            return ErrorCode::STREAM_END;

        /* process all codes and make table entries */
        for (;;)
        {
            /* create table entry */
            thisx.bits = (uint8_t) (len - drop);
            if ((int) (work[sym]) < end)
            {
                thisx.op = 0;
                thisx.val = work[sym];
            }
            else if ((int) (work[sym]) > end)
            {
                thisx.op = (uint8_t) extra[work[sym]];
                thisx.val = base[work[sym]];
            }
            else
            {
                thisx.op = 32 + 64; /* end of block */
                thisx.val = 0;
            }

            /* replicate for those indices with low len bits equal to huff */
            incr = 1U << (len - drop);
            fill = 1U << curr;
            min = fill; /* save offset to next table */
            do
            {
                fill -= incr;
                next[(huff >> drop) + fill] = thisx;
            } while (fill != 0);

            /* backwards increment the len-bit code huff */
            incr = 1U << (len - 1);
            while (huff & incr)
                incr >>= 1;
            if (incr != 0)
            {
                huff &= incr - 1;
                huff += incr;
            }
            else
                huff = 0;

            /* go to next symbol, update count, len */
            sym++;
            if (--(count[len]) == 0)
            {
                if (len == max)
                    break;
                len = lens[work[sym]];
            }

            /* create new sub-table if needed */
            if (len > root && (huff & mask) != low)
            {
                /* if first time, transition to sub-tables */
                if (drop == 0)
                    drop = root;

                /* increment past last table */
                next += min; /* here min is 1 << curr */

                /* determine length of next table */
                curr = len - drop;
                left = (int) (1 << curr);
                while (curr + drop < max)
                {
                    left -= count[curr + drop];
                    if (left <= 0)
                        break;
                    curr++;
                    left <<= 1;
                }

                /* check for enough space */
                used += 1U << curr;
                if (type == CodeType::LENS && used >= ENOUGH - MAXD)
                    return ErrorCode::STREAM_END;

                /* point entry in root table to sub-table */
                low = huff & mask;
                (*table)[low].op = (uint8_t) curr;
                (*table)[low].bits = (uint8_t) root;
                (*table)[low].val = (uint16_t) (next - *table);
            }
        }

        thisx.op = 64;
        thisx.bits = (uint8_t) (len - drop);
        thisx.val = 0;

        while (huff != 0)
        {
            /* when done with sub-table, drop back to root table */
            if (drop != 0 && (huff & mask) != low)
            {
                drop = 0;
                len = root;
                next = *table;
                thisx.bits = (uint8_t) len;
            }

            /* put invalid code marker in table */
            next[huff >> drop] = thisx;

            /* backwards increment the len-bit code huff */
            incr = 1U << (len - 1);
            while (huff & incr)
                incr >>= 1;
            if (incr != 0)
            {
                huff &= incr - 1;
                huff += incr;
            }
            else
                huff = 0;
        }

        *table += used;
        *bits = root;
        return ErrorCode::OK;
    }

    ErrorCode inflate()
    {
        uint8_t* next2;    /* next input */
        uint8_t* put;     /* next output */
        uint32_t have2, left;        /* available input and output */
        unsigned long hold2;         /* bit buffer */
        uint32_t bits2;              /* bits in bit buffer */
        uint32_t copy;              /* number of stored or match bytes to copy */
        uint8_t* from;    /* where to copy match bytes from */
        code thisx;                  /* current decoding table entry */
        code last2;                  /* parent table entry */
        uint32_t len;               /* length to copy for repeats, bits to drop */
        ErrorCode ret;                    /* return code */

        static constexpr uint16_t order[19] = /* permutation of code lengths */
            { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };

        if (next_out == nullptr || (next_in == nullptr && avail_in != 0))
            return ErrorCode::STREAM_ERROR;

        /* Load registers with state in inflate() for speed */
        auto LOAD = [&]
        {
            put = next_out;
            left = avail_out;
            next2 = next_in;
            have2 = avail_in;
            hold2 = hold;
            bits2 = bits;
        };

        /* Restore state from registers in inflate() */
        auto RESTORE = [&]
        {
            next_out = put;
            avail_out = left;
            next_in = next2;
            avail_in = have2;
            hold = hold2;
            bits = bits2;
        };

        /* Clear the input bit accumulator */
        auto INITBITS = [&]
        {
            hold2 = 0;
            bits2 = 0;
        };

        /* Get a byte of input into the bit accumulator, or return from inflate()
        if there is no input available. */
        #define CHOC_ZLIB_PULLBYTE() \
            { \
                if (have2 == 0) return cleanUp(); \
                have2--; \
                hold2 += (unsigned long)(*next2++) << bits2; \
                bits2 += 8; \
            }

        /* Assure that there are at least n bits in the bit accumulator.  If there is
        not enough available input to do that, then return from inflate(). */
        #define CHOC_ZLIB_NEEDBITS(n) \
            { \
                while (bits2 < (uint32_t) (n)) \
                    CHOC_ZLIB_PULLBYTE(); \
            }

        /* Return the low n bits of the bit accumulator (n < 16) */
        auto BITS = [&] (uint32_t n) -> uint32_t
        {
            return (uint32_t) hold2 & ((1u << n) - 1);
        };

        /* Remove n bits from the bit accumulator */
        auto DROPBITS = [&] (uint32_t n)
        {
            hold2 >>= n;
            bits2 -= n;
        };

        /* Remove zero to seven bits as needed to go to a byte boundary */
        auto BYTEBITS = [&]
        {
            hold2 >>= bits2 & 7;
            bits2 -= bits2 & 7;
        };

        auto REVERSE = [] (unsigned long n) -> unsigned long
        {
            return ((n >> 24) & 0xff) + ((n >> 8) & 0xff00) + ((n & 0xff00) << 8) + ((n & 0xff) << 24);
        };

        LOAD();
        auto in = have2;
        auto out = left;
        ret = ErrorCode::OK;

        auto cleanUp = [&] () -> ErrorCode
        {
            RESTORE();

            if (wsize || (mode < Mode::CHECK && out != avail_out))
                if (updatewindow (out)) {
                    mode = Mode::MEM;
                    return ErrorCode::MEM_ERROR;
                }

            in -= avail_in;
            out -= avail_out;
            total_in += in;
            total_out += out;
            total += out;

            if (wrap && out)
                adler = check = Checksum::adler32 (check, next_out - out, out);

            data_type = (DataType) (bits + (last ? 64 : 0) + (mode == Mode::TYPEDO ? 128 : 0));

            if (in == 0 && out == 0)
                return ErrorCode::BUF_ERROR;

            return ret;
        };

        auto fail = [&] (const char* failMessage) -> ErrorCode
        {
            msg = failMessage;
            mode = Mode::BAD;
            return ErrorCode::DATA_ERROR;
        };

        for (;;)
        {
            switch (mode)
            {
            case Mode::HEAD:
                if (wrap == 0) {
                    mode = Mode::TYPEDO;
                    break;
                }
                CHOC_ZLIB_NEEDBITS(16);
                if (((BITS(8) << 8) + (hold2 >> 8)) % 31)
                    return fail ("incorrect header check");

                if (BITS(4) != Z_DEFLATED)
                    return fail ("unknown compression method");

                DROPBITS(4);
                len = BITS(4) + 8;
                if (len > wbits)
                    return fail ("invalid window size");

                dmax = 1U << len;
                adler = check = Checksum::adler32 (0, nullptr, 0);
                mode = hold2 & 0x200 ? Mode::DICTID : Mode::TYPEDO;
                INITBITS();
                break;

            case Mode::DICTID:
                CHOC_ZLIB_NEEDBITS(32);
                adler = check = REVERSE(hold2);
                INITBITS();
                mode = Mode::DICT;
                [[fallthrough]];
            case Mode::DICT:
                if (havedict == 0) {
                    RESTORE();
                    return ErrorCode::NEED_DICT;
                }
                adler = check = Checksum::adler32 (0, nullptr, 0);
                mode = Mode::TYPEDO;
                [[fallthrough]];
            case Mode::TYPEDO:
                if (last) {
                    BYTEBITS();
                    mode = Mode::CHECK;
                    break;
                }
                CHOC_ZLIB_NEEDBITS(3);
                last = BITS(1) != 0;
                DROPBITS(1);
                switch (BITS(2)) {
                case 0:
                    mode = Mode::STORED;
                    break;
                case 1:
                    initialiseFixedTables();
                    mode = Mode::LEN;
                    break;
                case 2:
                    mode = Mode::TABLE;
                    break;
                case 3:
                    return fail ("invalid block type");
                }

                DROPBITS(2);
                break;
            case Mode::STORED:
                BYTEBITS();
                CHOC_ZLIB_NEEDBITS(32);
                if ((hold2 & 0xffff) != ((hold2 >> 16) ^ 0xffff))
                    return fail ("invalid stored block lengths");

                length = (uint32_t) hold2 & 0xffff;
                INITBITS();
                mode = Mode::COPY;
                [[fallthrough]];
            case Mode::COPY:
                copy = length;
                if (copy) {
                    if (copy > have2) copy = have2;
                    if (copy > left) copy = left;
                    if (copy == 0) return cleanUp();
                    memcpy (put, next2, copy);
                    have2 -= copy;
                    next2 += copy;
                    left -= copy;
                    put += copy;
                    length -= copy;
                    break;
                }
                mode = Mode::TYPEDO;
                break;
            case Mode::TABLE:
                CHOC_ZLIB_NEEDBITS(14);
                nlen = BITS(5) + 257;
                DROPBITS(5);
                ndist = BITS(5) + 1;
                DROPBITS(5);
                ncode = BITS(4) + 4;
                DROPBITS(4);

                if (nlen > 286 || ndist > 30)
                    return fail ("too many length or distance symbols");

                have = 0;
                mode = Mode::LENLENS;
                [[fallthrough]];
            case Mode::LENLENS:
                while (have < ncode) {
                    CHOC_ZLIB_NEEDBITS(3);
                    lens[order[have++]] = (uint16_t)BITS(3);
                    DROPBITS(3);
                }
                while (have < 19)
                    lens[order[have++]] = 0;
                next = codes;
                lencode = next;
                lenbits = 7;
                ret = createTables (CodeType::CODES, lens, 19, &next, &lenbits, work);

                if (ret != ErrorCode::OK)
                    return fail ("invalid code lengths set");

                have = 0;
                mode = Mode::CODELENS;
                [[fallthrough]];
            case Mode::CODELENS:
                while (have < nlen + ndist)
                {
                    for (;;) {
                        thisx = lencode[BITS(lenbits)];
                        if ((uint32_t) thisx.bits <= bits2)
                            break;
                        CHOC_ZLIB_PULLBYTE();
                    }
                    if (thisx.val < 16) {
                        CHOC_ZLIB_NEEDBITS(thisx.bits);
                        DROPBITS(thisx.bits);
                        lens[have++] = thisx.val;
                    }
                    else {
                        if (thisx.val == 16) {
                            CHOC_ZLIB_NEEDBITS(thisx.bits + 2);
                            DROPBITS(thisx.bits);
                            if (have == 0)
                                return fail ("invalid bit length repeat");

                            len = lens[have - 1];
                            copy = 3 + BITS(2);
                            DROPBITS(2);
                        }
                        else if (thisx.val == 17) {
                            CHOC_ZLIB_NEEDBITS(thisx.bits + 3);
                            DROPBITS(thisx.bits);
                            len = 0;
                            copy = 3 + BITS(3);
                            DROPBITS(3);
                        }
                        else {
                            CHOC_ZLIB_NEEDBITS(thisx.bits + 7);
                            DROPBITS(thisx.bits);
                            len = 0;
                            copy = 11 + BITS(7);
                            DROPBITS(7);
                        }
                        if (have + copy > nlen + ndist)
                            return fail ("invalid bit length repeat");

                        while (copy--)
                            lens[have++] = (uint16_t)len;
                    }
                }

                /* build code tables */
                next = codes;
                lencode = next;
                lenbits = 9;
                ret = createTables (CodeType::LENS, lens, nlen, &next, &lenbits, work);

                if (ret != ErrorCode::OK)
                    return fail ("invalid literal/lengths set");

                distcode = next;
                distbits = 6;
                ret = createTables (CodeType::DISTS, lens + nlen, ndist, &next, &distbits, work);

                if (ret != ErrorCode::OK)
                    return fail ("invalid distances set");

                mode = Mode::LEN;
                [[fallthrough]];
            case Mode::LEN:
                if (have2 >= 6 && left >= 258) {
                    RESTORE();
                    inflate_fast (out);
                    LOAD();
                    break;
                }
                for (;;) {
                    thisx = lencode[BITS(lenbits)];
                    if ((uint32_t) thisx.bits <= bits2)
                        break;
                    CHOC_ZLIB_PULLBYTE();
                }
                if (thisx.op && (thisx.op & 0xf0) == 0) {
                    last2 = thisx;
                    for (;;) {
                        thisx = lencode[last2.val + (BITS((uint32_t)last2.bits + last2.op) >> last2.bits)];
                        if ((uint32_t) (last2.bits + thisx.bits) <= bits2)
                            break;
                        CHOC_ZLIB_PULLBYTE();
                    }
                    DROPBITS(last2.bits);
                }
                DROPBITS(thisx.bits);
                length = (uint32_t) thisx.val;
                if ((int)(thisx.op) == 0) {
                    mode = Mode::LIT;
                    break;
                }
                if (thisx.op & 32) {
                    mode = Mode::TYPEDO;
                    break;
                }
                if (thisx.op & 64)
                    return fail ("invalid literal/length code");

                extra = (uint32_t) thisx.op & 15;
                mode = Mode::LENEXT;
                [[fallthrough]];
            case Mode::LENEXT:
                if (extra) {
                    CHOC_ZLIB_NEEDBITS(extra);
                    length += BITS(extra);
                    DROPBITS(extra);
                }
                mode = Mode::DIST;
                [[fallthrough]];
            case Mode::DIST:
                for (;;) {
                    thisx = distcode[BITS(distbits)];
                    if ((uint32_t) thisx.bits <= bits2)
                        break;
                    CHOC_ZLIB_PULLBYTE();
                }
                if ((thisx.op & 0xf0) == 0) {
                    last2 = thisx;
                    for (;;) {
                        thisx = distcode[last2.val + (BITS((uint32_t)last2.bits + last2.op) >> last2.bits)];
                        if ((uint32_t) (last2.bits + thisx.bits) <= bits2)
                            break;
                        CHOC_ZLIB_PULLBYTE();
                    }
                    DROPBITS(last2.bits);
                }
                DROPBITS(thisx.bits);
                if (thisx.op & 64)
                    return fail ("invalid distance code");

                offset = (uint32_t) thisx.val;
                extra = (uint32_t) thisx.op & 15;
                mode = Mode::DISTEXT;
                [[fallthrough]];
            case Mode::DISTEXT:
                if (extra) {
                    CHOC_ZLIB_NEEDBITS(extra);
                    offset += BITS(extra);
                    DROPBITS(extra);
                }

                if (offset > whave + out - left)
                    return fail ("invalid distance too far back");

                mode = Mode::MATCH;
                [[fallthrough]];
            case Mode::MATCH:
                if (left == 0) return cleanUp();
                copy = out - left;
                if (offset > copy) {         /* copy from window */
                    copy = offset - copy;
                    if (copy > write) {
                        copy -= write;
                        from = window.data() + (wsize - copy);
                    }
                    else
                        from = window.data() + (write - copy);
                    if (copy > length) copy = length;
                }
                else {                              /* copy from output */
                    from = put - offset;
                    copy = length;
                }
                if (copy > left) copy = left;
                left -= copy;
                length -= copy;
                do {
                    *put++ = *from++;
                } while (--copy);
                if (length == 0) mode = Mode::LEN;
                break;
            case Mode::LIT:
                if (left == 0) return cleanUp();
                *put++ = (uint8_t) length;
                left--;
                mode = Mode::LEN;
                break;
            case Mode::CHECK:
                if (wrap) {
                    CHOC_ZLIB_NEEDBITS(32);
                    out -= left;
                    total_out += out;
                    total += out;
                    if (out)
                        adler = check = Checksum::adler32 (check, put - out, out);
                    out = left;

                    if ((REVERSE(hold2)) != check)
                        return fail ("incorrect data check");

                    INITBITS();
                }
                mode = Mode::DONE;
                [[fallthrough]];
            case Mode::DONE:
                ret = ErrorCode::STREAM_END;
                return cleanUp();
            case Mode::BAD:
                ret = ErrorCode::DATA_ERROR;
                return cleanUp();
            case Mode::MEM:
                return ErrorCode::MEM_ERROR;
            case Mode::SYNC:
            case Mode::FLAGS:
            case Mode::TIME:
            case Mode::OS:
            case Mode::EXLEN:
            case Mode::EXTRA:
            case Mode::NAME:
            case Mode::COMMENT:
            case Mode::HCRC:
            case Mode::LENGTH:
            default:
                return ErrorCode::STREAM_ERROR;
            }
        }

        #undef CHOC_ZLIB_PULLBYTE
        #undef CHOC_ZLIB_NEEDBITS
    }
};


//==============================================================================
//==============================================================================
struct Checksum
{
    static unsigned long crc32 (unsigned long crc, const uint8_t* buf, unsigned len)
    {
        if (buf == nullptr)
            return 0;

        crc = crc ^ 0xffffffffUL;

        while (len >= 8)
        {
            crc = crc_table[((int) crc ^ *buf++) & 0xff] ^ (crc >> 8);
            crc = crc_table[((int) crc ^ *buf++) & 0xff] ^ (crc >> 8);
            crc = crc_table[((int) crc ^ *buf++) & 0xff] ^ (crc >> 8);
            crc = crc_table[((int) crc ^ *buf++) & 0xff] ^ (crc >> 8);
            crc = crc_table[((int) crc ^ *buf++) & 0xff] ^ (crc >> 8);
            crc = crc_table[((int) crc ^ *buf++) & 0xff] ^ (crc >> 8);
            crc = crc_table[((int) crc ^ *buf++) & 0xff] ^ (crc >> 8);
            crc = crc_table[((int) crc ^ *buf++) & 0xff] ^ (crc >> 8);
            len -= 8;
        }

        while (len > 0)
        {
            crc = crc_table[((int) crc ^ *buf++) & 0xff] ^ (crc >> 8);
            --len;
        }

        return crc ^ 0xffffffffUL;
    }

    static unsigned long adler32 (unsigned long adler, const uint8_t* buf, uint32_t len)
    {
        static constexpr uint32_t BASE = 65521u; // largest prime smaller than 65536
        static constexpr uint32_t NMAX = 5552u;

        unsigned long sum2 = (adler >> 16) & 0xffff;
        adler &= 0xffff;

        if (len == 1)
        {
            adler += buf[0];
            if (adler >= BASE)
                adler -= BASE;
            sum2 += adler;
            if (sum2 >= BASE)
                sum2 -= BASE;
            return adler | (sum2 << 16);
        }

        if (buf == nullptr)
            return 1;

        while (len >= NMAX)
        {
            len -= NMAX;
            auto n = NMAX / 16;

            do
            {
                for (uint32_t i = 0; i < 16; ++i)
                {
                    adler += buf[i];
                    sum2 += adler;
                }

                buf += 16;
            }
            while (--n);

            adler %= BASE;
            sum2 %= BASE;
        }

        while (len >= 16)
        {
            for (uint32_t i = 0; i < 16; ++i)
            {
                adler += buf[i];
                sum2 += adler;
            }

            len -= 16;
            buf += 16;
        }

        while (len--)
        {
            adler += *buf++;
            sum2 += adler;
        }

        adler %= BASE;
        sum2 %= BASE;

        return adler | (sum2 << 16);
    }

private:
    static constexpr unsigned long crc_table[256] =
    {
        0x00000000ul, 0x77073096ul, 0xee0e612cul, 0x990951baul, 0x076dc419ul, 0x706af48ful, 0xe963a535ul, 0x9e6495a3ul, 0x0edb8832ul, 0x79dcb8a4ul, 0xe0d5e91eul, 0x97d2d988ul, 0x09b64c2bul, 0x7eb17cbdul, 0xe7b82d07ul, 0x90bf1d91ul,
        0x1db71064ul, 0x6ab020f2ul, 0xf3b97148ul, 0x84be41deul, 0x1adad47dul, 0x6ddde4ebul, 0xf4d4b551ul, 0x83d385c7ul, 0x136c9856ul, 0x646ba8c0ul, 0xfd62f97aul, 0x8a65c9ecul, 0x14015c4ful, 0x63066cd9ul, 0xfa0f3d63ul, 0x8d080df5ul,
        0x3b6e20c8ul, 0x4c69105eul, 0xd56041e4ul, 0xa2677172ul, 0x3c03e4d1ul, 0x4b04d447ul, 0xd20d85fdul, 0xa50ab56bul, 0x35b5a8faul, 0x42b2986cul, 0xdbbbc9d6ul, 0xacbcf940ul, 0x32d86ce3ul, 0x45df5c75ul, 0xdcd60dcful, 0xabd13d59ul,
        0x26d930acul, 0x51de003aul, 0xc8d75180ul, 0xbfd06116ul, 0x21b4f4b5ul, 0x56b3c423ul, 0xcfba9599ul, 0xb8bda50ful, 0x2802b89eul, 0x5f058808ul, 0xc60cd9b2ul, 0xb10be924ul, 0x2f6f7c87ul, 0x58684c11ul, 0xc1611dabul, 0xb6662d3dul,
        0x76dc4190ul, 0x01db7106ul, 0x98d220bcul, 0xefd5102aul, 0x71b18589ul, 0x06b6b51ful, 0x9fbfe4a5ul, 0xe8b8d433ul, 0x7807c9a2ul, 0x0f00f934ul, 0x9609a88eul, 0xe10e9818ul, 0x7f6a0dbbul, 0x086d3d2dul, 0x91646c97ul, 0xe6635c01ul,
        0x6b6b51f4ul, 0x1c6c6162ul, 0x856530d8ul, 0xf262004eul, 0x6c0695edul, 0x1b01a57bul, 0x8208f4c1ul, 0xf50fc457ul, 0x65b0d9c6ul, 0x12b7e950ul, 0x8bbeb8eaul, 0xfcb9887cul, 0x62dd1ddful, 0x15da2d49ul, 0x8cd37cf3ul, 0xfbd44c65ul,
        0x4db26158ul, 0x3ab551ceul, 0xa3bc0074ul, 0xd4bb30e2ul, 0x4adfa541ul, 0x3dd895d7ul, 0xa4d1c46dul, 0xd3d6f4fbul, 0x4369e96aul, 0x346ed9fcul, 0xad678846ul, 0xda60b8d0ul, 0x44042d73ul, 0x33031de5ul, 0xaa0a4c5ful, 0xdd0d7cc9ul,
        0x5005713cul, 0x270241aaul, 0xbe0b1010ul, 0xc90c2086ul, 0x5768b525ul, 0x206f85b3ul, 0xb966d409ul, 0xce61e49ful, 0x5edef90eul, 0x29d9c998ul, 0xb0d09822ul, 0xc7d7a8b4ul, 0x59b33d17ul, 0x2eb40d81ul, 0xb7bd5c3bul, 0xc0ba6cadul,
        0xedb88320ul, 0x9abfb3b6ul, 0x03b6e20cul, 0x74b1d29aul, 0xead54739ul, 0x9dd277aful, 0x04db2615ul, 0x73dc1683ul, 0xe3630b12ul, 0x94643b84ul, 0x0d6d6a3eul, 0x7a6a5aa8ul, 0xe40ecf0bul, 0x9309ff9dul, 0x0a00ae27ul, 0x7d079eb1ul,
        0xf00f9344ul, 0x8708a3d2ul, 0x1e01f268ul, 0x6906c2feul, 0xf762575dul, 0x806567cbul, 0x196c3671ul, 0x6e6b06e7ul, 0xfed41b76ul, 0x89d32be0ul, 0x10da7a5aul, 0x67dd4accul, 0xf9b9df6ful, 0x8ebeeff9ul, 0x17b7be43ul, 0x60b08ed5ul,
        0xd6d6a3e8ul, 0xa1d1937eul, 0x38d8c2c4ul, 0x4fdff252ul, 0xd1bb67f1ul, 0xa6bc5767ul, 0x3fb506ddul, 0x48b2364bul, 0xd80d2bdaul, 0xaf0a1b4cul, 0x36034af6ul, 0x41047a60ul, 0xdf60efc3ul, 0xa867df55ul, 0x316e8eeful, 0x4669be79ul,
        0xcb61b38cul, 0xbc66831aul, 0x256fd2a0ul, 0x5268e236ul, 0xcc0c7795ul, 0xbb0b4703ul, 0x220216b9ul, 0x5505262ful, 0xc5ba3bbeul, 0xb2bd0b28ul, 0x2bb45a92ul, 0x5cb36a04ul, 0xc2d7ffa7ul, 0xb5d0cf31ul, 0x2cd99e8bul, 0x5bdeae1dul,
        0x9b64c2b0ul, 0xec63f226ul, 0x756aa39cul, 0x026d930aul, 0x9c0906a9ul, 0xeb0e363ful, 0x72076785ul, 0x05005713ul, 0x95bf4a82ul, 0xe2b87a14ul, 0x7bb12baeul, 0x0cb61b38ul, 0x92d28e9bul, 0xe5d5be0dul, 0x7cdcefb7ul, 0x0bdbdf21ul,
        0x86d3d2d4ul, 0xf1d4e242ul, 0x68ddb3f8ul, 0x1fda836eul, 0x81be16cdul, 0xf6b9265bul, 0x6fb077e1ul, 0x18b74777ul, 0x88085ae6ul, 0xff0f6a70ul, 0x66063bcaul, 0x11010b5cul, 0x8f659efful, 0xf862ae69ul, 0x616bffd3ul, 0x166ccf45ul,
        0xa00ae278ul, 0xd70dd2eeul, 0x4e048354ul, 0x3903b3c2ul, 0xa7672661ul, 0xd06016f7ul, 0x4969474dul, 0x3e6e77dbul, 0xaed16a4aul, 0xd9d65adcul, 0x40df0b66ul, 0x37d83bf0ul, 0xa9bcae53ul, 0xdebb9ec5ul, 0x47b2cf7ful, 0x30b5ffe9ul,
        0xbdbdf21cul, 0xcabac28aul, 0x53b39330ul, 0x24b4a3a6ul, 0xbad03605ul, 0xcdd70693ul, 0x54de5729ul, 0x23d967bful, 0xb3667a2eul, 0xc4614ab8ul, 0x5d681b02ul, 0x2a6f2b94ul, 0xb40bbe37ul, 0xc30c8ea1ul, 0x5a05df1bul, 0x2d02ef8dul
    };
};
};


//==============================================================================
//==============================================================================
struct InflaterStream::Pimpl
{
    Pimpl (std::shared_ptr<std::istream> s, FormatType type)
       : source (std::move (s)), formatType (type)
    {
        buffer.resize (32768);
        decompressed.resize (32768);
        originalSourcePos = source->tellg();
        CHOC_ASSERT (originalSourcePos >= 0); // must provide a stream that supports seeking
        createStream();
    }

    ~Pimpl()
    {
        deleteStream();
    }

    void createStream()
    {
        source->seekg (originalSourcePos);
        inflateStream = {};
        streamIsValid = (inflateStream.initialise (getNumBits()) == zlib::ErrorCode::OK);
        finished = error = ! streamIsValid;
    }

    void deleteStream()
    {
        decompressedPosition = {};
        decompressedSize = 0;
        finished = true;
        needsDictionary = false;
        error = false;
        streamIsValid = false;
    }

    size_t fillBuffer()
    {
        if (finished || ! streamIsValid)
            return 0;

        decompressedPosition += static_cast<off_type> (decompressedSize);
        decompressedSize = 0;

        if (inflateStream.avail_in == 0)
        {
            try
            {
                source->read (buffer.data(), static_cast<std::streamsize> (buffer.size()));
            }
            catch (...) {}

            auto numRead = static_cast<size_t> (source->gcount());

            if (numRead == 0)
                return 0;

            inflateStream.next_in   = reinterpret_cast<uint8_t*> (buffer.data());
            inflateStream.avail_in  = static_cast<decltype(inflateStream.avail_in)> (numRead);

            if (source->eof())
                source->clear();
        }

        inflateStream.next_out  = reinterpret_cast<uint8_t*> (decompressed.data());
        inflateStream.avail_out = static_cast<decltype(inflateStream.avail_out)> (decompressed.size());

        switch (inflateStream.inflate())
        {
            case zlib::ErrorCode::OK:            break;
            case zlib::ErrorCode::STREAM_END:    finished = true; break;
            case zlib::ErrorCode::NEED_DICT:     needsDictionary = true; break;

            case zlib::ErrorCode::DATA_ERROR:
            case zlib::ErrorCode::MEM_ERROR:
            case zlib::ErrorCode::ERRNO:
            case zlib::ErrorCode::STREAM_ERROR:
            case zlib::ErrorCode::BUF_ERROR:
            case zlib::ErrorCode::VERSION_ERROR:
            default:                             error = true; return 0;
        }

        decompressedSize = decompressed.size() - inflateStream.avail_out;
        return decompressedSize;
    }

    int getNumBits() const
    {
        if (formatType == FormatType::deflate)  return -zlib::MAX_WBITS;
        if (formatType == FormatType::gzip)     return zlib::MAX_WBITS | 16;
        return zlib::MAX_WBITS;
    }

    std::shared_ptr<std::istream> source;
    const FormatType formatType;

    zlib::InflateStream inflateStream;
    std::streamoff originalSourcePos = {};

    bool finished = true, needsDictionary = false, error = false, streamIsValid = false;

    std::vector<char_type> buffer, decompressed;
    off_type decompressedPosition = {};
    size_t decompressedSize = 0;
};

inline InflaterStream::InflaterStream (std::shared_ptr<std::istream> source, FormatType format)
   : std::istream (this),
     pimpl (std::make_unique<Pimpl> (std::move (source), format))
{
}

inline InflaterStream::~InflaterStream() = default;

inline InflaterStream::pos_type InflaterStream::seekoff (off_type off, std::ios_base::seekdir dir, std::ios_base::openmode mode)
{
    if (off == 0 && dir == std::ios_base::cur)
        return getPosition();

    if (dir == std::ios_base::end)
        return pos_type (off_type (-1));

    return seekpos (dir == std::ios_base::cur ? static_cast<pos_type> (getPosition() + off)
                                              : static_cast<pos_type> (off), mode);
}

inline InflaterStream::pos_type InflaterStream::seekpos (pos_type newPosition, std::ios_base::openmode)
{
    auto currentPos = getPosition();

    if (newPosition < currentPos)
    {
        pimpl->deleteStream();
        pimpl->createStream();
        setg ({}, {}, {});
        clear();
        currentPos = 0;
    }

    while (currentPos < newPosition)
    {
        if (auto avail = in_avail())
        {
            auto needed = newPosition - currentPos;

            if (avail >= needed)
            {
                gbump (static_cast<int> (needed));
                return newPosition;
            }
        }

        setg ({}, {}, {});

        if (underflow() == std::streambuf::traits_type::eof())
            break;

        currentPos = getPosition();
    }

    return currentPos;
}

inline std::streambuf::int_type InflaterStream::underflow()
{
    if (in_avail() != 0)
        return std::streambuf::traits_type::to_int_type (*gptr());

    auto numAvail = pimpl->fillBuffer();
    setg (pimpl->decompressed.data(), pimpl->decompressed.data(), pimpl->decompressed.data() + numAvail);

    if (numAvail != 0)
        return std::streambuf::traits_type::to_int_type (*gptr());

    return std::streambuf::traits_type::eof();
}

inline InflaterStream::pos_type InflaterStream::getPosition() const
{
    return gptr() != nullptr ? pimpl->decompressedPosition + (gptr() - eback()) : 0;
}

//==============================================================================
//==============================================================================

struct DeflaterStream::Pimpl
{
    Pimpl (std::shared_ptr<std::ostream> d, CompressionLevel comp, int windowBits)
       : dest (std::move (d)),
         compressionLevel (comp >= 0 && comp <= 10 ? comp : 6)
    {
        if (windowBits <= 0)
            windowBits = zlib::MAX_WBITS;

        streamIsValid = (deflateStream.initialise (compressionLevel, zlib::Z_DEFLATED,
                                                   windowBits, zlib::DEF_MEM_LEVEL)
                           == zlib::ErrorCode::OK);
    }

    ~Pimpl()
    {
        if (streamIsValid)
            flush();
    }

    std::shared_ptr<std::ostream> dest;
    zlib::DeflateStream deflateStream;

    const int compressionLevel;
    bool isFirstDeflate = true, streamIsValid = false, finished = false;
    DeflaterStream::char_type buffer[32768];

    int overflow (int c)
    {
        auto data = static_cast<uint8_t> (c);
        return write (&data, 1) ? 0 : std::streambuf::traits_type::eof();
    }

    bool write (const uint8_t* data, size_t dataSize)
    {
        if (finished)
            return false;

        while (dataSize > 0)
            if (! doNextBlock (data, dataSize, zlib::FlushState::Z_NO_FLUSH))
                return false;

        return true;
    }

    void flush()
    {
        const uint8_t* data = nullptr;
        size_t dataSize = 0;

        while (! finished)
            doNextBlock (data, dataSize, zlib::FlushState::Z_FINISH);
    }

    bool doNextBlock (const uint8_t*& data, size_t& dataSize, zlib::FlushState flushMode)
    {
        if (! streamIsValid)
            return false;

        deflateStream.next_in   = const_cast<uint8_t*> (data);
        deflateStream.next_out  = reinterpret_cast<uint8_t*> (buffer);
        deflateStream.avail_in  = (uint32_t) dataSize;
        deflateStream.avail_out = (uint32_t) sizeof (buffer);

        auto result = isFirstDeflate ? deflateStream.deflateParams (compressionLevel)
                                     : deflateStream.deflate (flushMode);
        isFirstDeflate = false;

        switch (result)
        {
            case zlib::ErrorCode::OK:          break;
            case zlib::ErrorCode::STREAM_END:  finished = true; break;
            case zlib::ErrorCode::NEED_DICT:
            case zlib::ErrorCode::ERRNO:
            case zlib::ErrorCode::STREAM_ERROR:
            case zlib::ErrorCode::DATA_ERROR:
            case zlib::ErrorCode::MEM_ERROR:
            case zlib::ErrorCode::BUF_ERROR:
            case zlib::ErrorCode::VERSION_ERROR:
            default:                           return false;
        }

        data += dataSize - deflateStream.avail_in;
        dataSize = deflateStream.avail_in;

        auto bytesDone = (std::streamsize) sizeof (buffer) - (std::streamsize) deflateStream.avail_out;

        if (bytesDone > 0)
            dest->write (buffer, bytesDone);

        return true;
    }
};

inline DeflaterStream::DeflaterStream (std::shared_ptr<std::ostream> d, CompressionLevel c, int w)
    : std::ostream (this),
      pimpl (std::make_unique<Pimpl> (std::move (d), c, w))
{}

inline DeflaterStream::~DeflaterStream() = default;

inline int DeflaterStream::overflow (int c) { return pimpl->overflow (c); }


} // namespace choc::gzip


#endif // CHOC_GZIPDECOMPRESS_HEADER_INCLUDED
