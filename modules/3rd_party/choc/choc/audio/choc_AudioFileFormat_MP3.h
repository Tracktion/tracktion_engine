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

#ifndef CHOC_AUDIOFILEFORMAT_MP3_HEADER_INCLUDED
#define CHOC_AUDIOFILEFORMAT_MP3_HEADER_INCLUDED

#include "../platform/choc_Assert.h"
#include "choc_AudioFileFormat.h"
#include "choc_AudioSampleData.h"

namespace choc::audio
{

//==============================================================================
/**
    An AudioFormat class which can read (but not yet write!) MP3 files.
*/
class MP3AudioFileFormat  : public AudioFileFormat
{
public:
    MP3AudioFileFormat() = default;
    ~MP3AudioFileFormat() override {}

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

#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))) || ((defined(__i386__) || defined(__x86_64__)) && defined(__SSE2__))
 #if defined(_MSC_VER)
  #include <intrin.h>
 #endif /* defined(_MSC_VER) */
 #include <immintrin.h>
#elif defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
 #include <arm_neon.h>
#endif

namespace choc::audio
{

#include "../platform/choc_DisableAllWarnings.h"

namespace minimp3
{
/*
    The chunk of code inside this minimp3 namespace is taken from the
    minimp3 project, whose homepage is here: https://github.com/lieff/minimp3

    The original license for minimp3 is:

    > To the extent possible under law, the author(s) have dedicated all
    > copyright and related and neighboring rights to this software to the
    > public domain worldwide.
    > This software is distributed without any warranty.
    > See <http://creativecommons.org/publicdomain/zero/1.0/>.

    I've made a few hacks to it to make it fit the choc format better:
    I've removed and renamed many macros to avoid polluting the global namespace.
    I've also deleted some chunks of code that weren't needed for the platforms
    choc runs on, and have made some functions inline.

    If you skip to the bottom of this file, you'll find the wrapper code that
    uses the minimp3 library to implement the choc AudioFileFormat classes...
*/

#ifdef CHOC_REGISTER_OPEN_SOURCE_LICENCE
 CHOC_REGISTER_OPEN_SOURCE_LICENCE (minimp3, R"(
==============================================================================
minimp3 license:

To the extent possible under law, the author(s) have dedicated all
copyright and related and neighboring rights to this software to the
public domain worldwide.
This software is distributed without any warranty.
See <http://creativecommons.org/publicdomain/zero/1.0/>.
)")
#endif

typedef struct
{
    int frame_bytes, frame_offset, channels, hz, layer, bitrate_kbps;
} mp3dec_frame_info_t;

typedef struct
{
    float mdct_overlap[2][9*32], qmf_state[15*2*32];
    int reserv, free_format_bytes;
    unsigned char header[4], reserv_buf[511];
} mp3dec_t;

typedef float mp3d_sample_t;

static constexpr int MINIMP3_MAX_SAMPLES_PER_FRAME = (1152*2);
static constexpr int MAX_FREE_FORMAT_FRAME_SIZE = 2304;    /* more than ISO spec's */
static constexpr int MAX_FRAME_SYNC_MATCHES = 10;

static constexpr auto MAX_L3_FRAME_PAYLOAD_BYTES = MAX_FREE_FORMAT_FRAME_SIZE; /* MUST be >= 320000/8/32000*1152 = 1440 */

static constexpr int MAX_BITRESERVOIR_BYTES  = 511;
static constexpr int SHORT_BLOCK_TYPE        = 2;
static constexpr int STOP_BLOCK_TYPE         = 3;
static constexpr int MODE_MONO               = 3;
static constexpr int MODE_JOINT_STEREO       = 1;
static constexpr int HDR_SIZE                = 4;
#define MINIMP3_HDR_IS_MONO(h)              (((h[3]) & 0xC0) == 0xC0)
#define MINIMP3_HDR_IS_MS_STEREO(h)         (((h[3]) & 0xE0) == 0x60)
#define MINIMP3_HDR_IS_FREE_FORMAT(h)       (((h[2]) & 0xF0) == 0)
#define MINIMP3_HDR_IS_CRC(h)               (!((h[1]) & 1))
#define MINIMP3_HDR_TEST_PADDING(h)         ((h[2]) & 0x2)
#define MINIMP3_HDR_TEST_MPEG1(h)           ((h[1]) & 0x8)
#define MINIMP3_HDR_TEST_NOT_MPEG25(h)      ((h[1]) & 0x10)
#define MINIMP3_HDR_TEST_I_STEREO(h)        ((h[3]) & 0x10)
#define MINIMP3_HDR_TEST_MS_STEREO(h)       ((h[3]) & 0x20)
#define MINIMP3_HDR_GET_STEREO_MODE(h)      (((h[3]) >> 6) & 3)
#define MINIMP3_HDR_GET_STEREO_MODE_EXT(h)  (((h[3]) >> 4) & 3)
#define MINIMP3_HDR_GET_LAYER(h)            (((h[1]) >> 1) & 3)
#define MINIMP3_HDR_GET_BITRATE(h)          ((h[2]) >> 4)
#define MINIMP3_HDR_GET_SAMPLE_RATE(h)      (((h[2]) >> 2) & 3)
#define MINIMP3_HDR_GET_MY_SAMPLE_RATE(h)   (MINIMP3_HDR_GET_SAMPLE_RATE(h) + (((h[1] >> 3) & 1) + ((h[1] >> 4) & 1))*3)
#define MINIMP3_HDR_IS_FRAME_576(h)         ((h[1] & 14) == 2)
#define MINIMP3_HDR_IS_LAYER_1(h)           ((h[1] & 6) == 6)

static constexpr int BITS_DEQUANTIZER_OUT = -1;
static constexpr int MAX_SCF  = (255 + BITS_DEQUANTIZER_OUT*4 - 210);
static constexpr int MAX_SCFI = ((MAX_SCF + 3) & ~3);

#if !defined(MINIMP3_NO_SIMD)

#if !defined(MINIMP3_ONLY_SIMD) && (defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__) || defined(_M_ARM64))
/* x64 always have SSE2, arm64 always have neon, no need for generic code */
#define MINIMP3_ONLY_SIMD
#endif /* SIMD checks... */

#if (defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))) || ((defined(__i386__) || defined(__x86_64__)) && defined(__SSE2__))
#define MINIMP3_HAVE_SSE 1
#define MINIMP3_HAVE_SIMD 1
#define MINIMP3_VSTORE _mm_storeu_ps
#define MINIMP3_VLD _mm_loadu_ps
#define MINIMP3_VSET _mm_set1_ps
#define MINIMP3_VADD _mm_add_ps
#define MINIMP3_VSUB _mm_sub_ps
#define MINIMP3_VMUL _mm_mul_ps
#define MINIMP3_VMAC(a, x, y) _mm_add_ps(a, _mm_mul_ps(x, y))
#define MINIMP3_VMSB(a, x, y) _mm_sub_ps(a, _mm_mul_ps(x, y))
#define MINIMP3_VMUL_S(x, s)  _mm_mul_ps(x, _mm_set1_ps(s))
#define MINIMP3_VREV(x) _mm_shuffle_ps(x, x, _MM_SHUFFLE(0, 1, 2, 3))
typedef __m128 f4;
#if defined(_MSC_VER) || defined(MINIMP3_ONLY_SIMD)
#define minimp3_cpuid __cpuid
#else /* defined(_MSC_VER) || defined(MINIMP3_ONLY_SIMD) */
static __inline__ __attribute__((always_inline)) void minimp3_cpuid(int CPUInfo[], const int InfoType)
{
#if defined(__PIC__)
    __asm__ __volatile__(
#if defined(__x86_64__)
        "push %%rbx\n"
        "cpuid\n"
        "xchgl %%ebx, %1\n"
        "pop  %%rbx\n"
#else /* defined(__x86_64__) */
        "xchgl %%ebx, %1\n"
        "cpuid\n"
        "xchgl %%ebx, %1\n"
#endif /* defined(__x86_64__) */
        : "=a" (CPUInfo[0]), "=r" (CPUInfo[1]), "=c" (CPUInfo[2]), "=d" (CPUInfo[3])
        : "a" (InfoType));
#else /* defined(__PIC__) */
    __asm__ __volatile__(
        "cpuid"
        : "=a" (CPUInfo[0]), "=b" (CPUInfo[1]), "=c" (CPUInfo[2]), "=d" (CPUInfo[3])
        : "a" (InfoType));
#endif /* defined(__PIC__)*/
}
#endif /* defined(_MSC_VER) || defined(MINIMP3_ONLY_SIMD) */
static int have_simd(void)
{
#ifdef MINIMP3_ONLY_SIMD
    return 1;
#else /* MINIMP3_ONLY_SIMD */
    static int g_have_simd;
    int CPUInfo[4];
#ifdef MINIMP3_TEST
    static int g_counter;
    if (g_counter++ > 100)
        return 0;
#endif /* MINIMP3_TEST */
    if (g_have_simd)
        goto end;
    minimp3_cpuid(CPUInfo, 0);
    g_have_simd = 1;
    if (CPUInfo[0] > 0)
    {
        minimp3_cpuid(CPUInfo, 1);
        g_have_simd = (CPUInfo[3] & (1 << 26)) + 1; /* SSE2 */
    }
end:
    return g_have_simd - 1;
#endif /* MINIMP3_ONLY_SIMD */
}
#elif defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64)
#define MINIMP3_HAVE_SSE 0
#define MINIMP3_HAVE_SIMD 1
#define MINIMP3_VSTORE vst1q_f32
#define MINIMP3_VLD vld1q_f32
#define MINIMP3_VSET vmovq_n_f32
#define MINIMP3_VADD vaddq_f32
#define MINIMP3_VSUB vsubq_f32
#define MINIMP3_VMUL vmulq_f32
#define MINIMP3_VMAC(a, x, y) vmlaq_f32(a, x, y)
#define MINIMP3_VMSB(a, x, y) vmlsq_f32(a, x, y)
#define MINIMP3_VMUL_S(x, s)  vmulq_f32(x, vmovq_n_f32(s))
#define MINIMP3_VREV(x) vcombine_f32(vget_high_f32(vrev64q_f32(x)), vget_low_f32(vrev64q_f32(x)))
typedef float32x4_t f4;
static int have_simd()
{   /* TODO: detect neon for !MINIMP3_ONLY_SIMD */
    return 1;
}
#else /* SIMD checks... */
#define MINIMP3_HAVE_SSE 0
#define MINIMP3_HAVE_SIMD 0
#ifdef MINIMP3_ONLY_SIMD
#error MINIMP3_ONLY_SIMD used, but SSE/NEON not enabled
#endif /* MINIMP3_ONLY_SIMD */
#endif /* SIMD checks... */
#else /* !defined(MINIMP3_NO_SIMD) */
#define MINIMP3_HAVE_SIMD 0
#endif /* !defined(MINIMP3_NO_SIMD) */

typedef struct
{
    const uint8_t *buf;
    int pos, limit;
} bs_t;

typedef struct
{
    float scf[3*64];
    uint8_t total_bands, stereo_bands, bitalloc[64], scfcod[64];
} L12_scale_info;

typedef struct
{
    uint8_t tab_offset, code_tab_width, band_count;
} L12_subband_alloc_t;

typedef struct
{
    const uint8_t *sfbtab;
    uint16_t part_23_length, big_values, scalefac_compress;
    uint8_t global_gain, block_type, mixed_block_flag, n_long_sfb, n_short_sfb;
    uint8_t table_select[3], region_count[3], subblock_gain[3];
    uint8_t preflag, scalefac_scale, count1_table, scfsi;
} L3_gr_info_t;

typedef struct
{
    bs_t bs;
    uint8_t maindata[MAX_BITRESERVOIR_BYTES + MAX_L3_FRAME_PAYLOAD_BYTES];
    L3_gr_info_t gr_info[4];
    float grbuf[2][576], scf[40], syn[18 + 15][2*32];
    uint8_t ist_pos[2][39];
} mp3dec_scratch_t;

static void bs_init(bs_t *bs, const uint8_t *data, int bytes)
{
    bs->buf   = data;
    bs->pos   = 0;
    bs->limit = bytes*8;
}

static uint32_t get_bits(bs_t *bs, int n)
{
    uint32_t next, cache = 0, s = bs->pos & 7;
    int shl = n + s;
    const uint8_t *p = bs->buf + (bs->pos >> 3);
    if ((bs->pos += n) > bs->limit)
        return 0;
    next = *p++ & (255 >> s);
    while ((shl -= 8) > 0)
    {
        cache |= next << shl;
        next = *p++;
    }
    return cache | (next >> -shl);
}

static int hdr_valid(const uint8_t *h)
{
    return h[0] == 0xff &&
        ((h[1] & 0xF0) == 0xf0 || (h[1] & 0xFE) == 0xe2) &&
        (MINIMP3_HDR_GET_LAYER(h) != 0) &&
        (MINIMP3_HDR_GET_BITRATE(h) != 15) &&
        (MINIMP3_HDR_GET_SAMPLE_RATE(h) != 3);
}

static int hdr_compare(const uint8_t *h1, const uint8_t *h2)
{
    return hdr_valid(h2) &&
        ((h1[1] ^ h2[1]) & 0xFE) == 0 &&
        ((h1[2] ^ h2[2]) & 0x0C) == 0 &&
        !(MINIMP3_HDR_IS_FREE_FORMAT(h1) ^ MINIMP3_HDR_IS_FREE_FORMAT(h2));
}

static unsigned hdr_bitrate_kbps(const uint8_t *h)
{
    static const uint8_t halfrate[2][3][15] = {
        { { 0,4,8,12,16,20,24,28,32,40,48,56,64,72,80 }, { 0,4,8,12,16,20,24,28,32,40,48,56,64,72,80 }, { 0,16,24,28,32,40,48,56,64,72,80,88,96,112,128 } },
        { { 0,16,20,24,28,32,40,48,56,64,80,96,112,128,160 }, { 0,16,24,28,32,40,48,56,64,80,96,112,128,160,192 }, { 0,16,32,48,64,80,96,112,128,144,160,176,192,208,224 } },
    };
    return 2*halfrate[!!MINIMP3_HDR_TEST_MPEG1(h)][MINIMP3_HDR_GET_LAYER(h) - 1][MINIMP3_HDR_GET_BITRATE(h)];
}

static unsigned hdr_sample_rate_hz(const uint8_t *h)
{
    static const unsigned g_hz[3] = { 44100, 48000, 32000 };
    return g_hz[MINIMP3_HDR_GET_SAMPLE_RATE(h)] >> (int)!MINIMP3_HDR_TEST_MPEG1(h) >> (int)!MINIMP3_HDR_TEST_NOT_MPEG25(h);
}

static unsigned hdr_frame_samples(const uint8_t *h)
{
    return MINIMP3_HDR_IS_LAYER_1(h) ? 384 : (1152 >> (int)MINIMP3_HDR_IS_FRAME_576(h));
}

static int hdr_frame_bytes(const uint8_t *h, int free_format_size)
{
    int frame_bytes = hdr_frame_samples(h)*hdr_bitrate_kbps(h)*125/hdr_sample_rate_hz(h);
    if (MINIMP3_HDR_IS_LAYER_1(h))
    {
        frame_bytes &= ~3; /* slot align */
    }
    return frame_bytes ? frame_bytes : free_format_size;
}

static int hdr_padding(const uint8_t *h)
{
    return MINIMP3_HDR_TEST_PADDING(h) ? (MINIMP3_HDR_IS_LAYER_1(h) ? 4 : 1) : 0;
}

#ifndef MINIMP3_ONLY_MP3
static const L12_subband_alloc_t *L12_subband_alloc_table(const uint8_t *hdr, L12_scale_info *sci)
{
    const L12_subband_alloc_t *alloc;
    int mode = MINIMP3_HDR_GET_STEREO_MODE(hdr);
    int nbands, stereo_bands = (mode == MODE_MONO) ? 0 : (mode == MODE_JOINT_STEREO) ? (MINIMP3_HDR_GET_STEREO_MODE_EXT(hdr) << 2) + 4 : 32;

    if (MINIMP3_HDR_IS_LAYER_1(hdr))
    {
        static const L12_subband_alloc_t g_alloc_L1[] = { { 76, 4, 32 } };
        alloc = g_alloc_L1;
        nbands = 32;
    } else if (!MINIMP3_HDR_TEST_MPEG1(hdr))
    {
        static const L12_subband_alloc_t g_alloc_L2M2[] = { { 60, 4, 4 }, { 44, 3, 7 }, { 44, 2, 19 } };
        alloc = g_alloc_L2M2;
        nbands = 30;
    } else
    {
        static const L12_subband_alloc_t g_alloc_L2M1[] = { { 0, 4, 3 }, { 16, 4, 8 }, { 32, 3, 12 }, { 40, 2, 7 } };
        int sample_rate_idx = MINIMP3_HDR_GET_SAMPLE_RATE(hdr);
        unsigned kbps = hdr_bitrate_kbps(hdr) >> (int)(mode != MODE_MONO);
        if (!kbps) /* free-format */
        {
            kbps = 192;
        }

        alloc = g_alloc_L2M1;
        nbands = 27;
        if (kbps < 56)
        {
            static const L12_subband_alloc_t g_alloc_L2M1_lowrate[] = { { 44, 4, 2 }, { 44, 3, 10 } };
            alloc = g_alloc_L2M1_lowrate;
            nbands = sample_rate_idx == 2 ? 12 : 8;
        } else if (kbps >= 96 && sample_rate_idx != 1)
        {
            nbands = 30;
        }
    }

    sci->total_bands = (uint8_t)nbands;
    sci->stereo_bands = (uint8_t) std::min (stereo_bands, nbands);

    return alloc;
}

static void L12_read_scalefactors(bs_t *bs, uint8_t *pba, uint8_t *scfcod, int bands, float *scf)
{
    static const float g_deq_L12[18*3] = {
#define MINIMP3_DQ(x) 9.53674316e-07f/x, 7.56931807e-07f/x, 6.00777173e-07f/x
        MINIMP3_DQ(3),MINIMP3_DQ(7),MINIMP3_DQ(15),MINIMP3_DQ(31),MINIMP3_DQ(63),MINIMP3_DQ(127),MINIMP3_DQ(255),MINIMP3_DQ(511),MINIMP3_DQ(1023),
        MINIMP3_DQ(2047),MINIMP3_DQ(4095),MINIMP3_DQ(8191),MINIMP3_DQ(16383),MINIMP3_DQ(32767),MINIMP3_DQ(65535),MINIMP3_DQ(3),MINIMP3_DQ(5),MINIMP3_DQ(9)
    };
    int i, m;
    for (i = 0; i < bands; i++)
    {
        float s = 0;
        int ba = *pba++;
        int mask = ba ? 4 + ((19 >> scfcod[i]) & 3) : 0;
        for (m = 4; m; m >>= 1)
        {
            if (mask & m)
            {
                int b = get_bits(bs, 6);
                s = g_deq_L12[ba*3 - 6 + b % 3]*(1 << 21 >> b/3);
            }
            *scf++ = s;
        }
    }
}

static void L12_read_scale_info(const uint8_t *hdr, bs_t *bs, L12_scale_info *sci)
{
    static const uint8_t g_bitalloc_code_tab[] = {
        0,17, 3, 4, 5,6,7, 8,9,10,11,12,13,14,15,16,
        0,17,18, 3,19,4,5, 6,7, 8, 9,10,11,12,13,16,
        0,17,18, 3,19,4,5,16,
        0,17,18,16,
        0,17,18,19, 4,5,6, 7,8, 9,10,11,12,13,14,15,
        0,17,18, 3,19,4,5, 6,7, 8, 9,10,11,12,13,14,
        0, 2, 3, 4, 5,6,7, 8,9,10,11,12,13,14,15,16
    };
    const L12_subband_alloc_t *subband_alloc = L12_subband_alloc_table(hdr, sci);

    int i, k = 0, ba_bits = 0;
    const uint8_t *ba_code_tab = g_bitalloc_code_tab;

    for (i = 0; i < sci->total_bands; i++)
    {
        uint8_t ba;
        if (i == k)
        {
            k += subband_alloc->band_count;
            ba_bits = subband_alloc->code_tab_width;
            ba_code_tab = g_bitalloc_code_tab + subband_alloc->tab_offset;
            subband_alloc++;
        }
        ba = ba_code_tab[get_bits(bs, ba_bits)];
        sci->bitalloc[2*i] = ba;
        if (i < sci->stereo_bands)
        {
            ba = ba_code_tab[get_bits(bs, ba_bits)];
        }
        sci->bitalloc[2*i + 1] = sci->stereo_bands ? ba : 0;
    }

    for (i = 0; i < 2*sci->total_bands; i++)
    {
        sci->scfcod[i] = sci->bitalloc[i] ? MINIMP3_HDR_IS_LAYER_1(hdr) ? 2 : get_bits(bs, 2) : 6;
    }

    L12_read_scalefactors(bs, sci->bitalloc, sci->scfcod, sci->total_bands*2, sci->scf);

    for (i = sci->stereo_bands; i < sci->total_bands; i++)
    {
        sci->bitalloc[2*i + 1] = 0;
    }
}

static int L12_dequantize_granule(float *grbuf, bs_t *bs, L12_scale_info *sci, int group_size)
{
    int i, j, k, choff = 576;
    for (j = 0; j < 4; j++)
    {
        float *dst = grbuf + group_size*j;
        for (i = 0; i < 2*sci->total_bands; i++)
        {
            int ba = sci->bitalloc[i];
            if (ba != 0)
            {
                if (ba < 17)
                {
                    int half = (1 << (ba - 1)) - 1;
                    for (k = 0; k < group_size; k++)
                    {
                        dst[k] = (float)((int)get_bits(bs, ba) - half);
                    }
                } else
                {
                    unsigned mod = (2 << (ba - 17)) + 1;    /* 3, 5, 9 */
                    unsigned code = get_bits(bs, mod + 2 - (mod >> 3));  /* 5, 7, 10 */
                    for (k = 0; k < group_size; k++, code /= mod)
                    {
                        dst[k] = (float)((int)(code % mod - mod/2));
                    }
                }
            }
            dst += choff;
            choff = 18 - choff;
        }
    }
    return group_size*4;
}

static void L12_apply_scf_384(L12_scale_info *sci, const float *scf, float *dst)
{
    int i, k;
    memcpy(dst + 576 + sci->stereo_bands*18, dst + sci->stereo_bands*18, (sci->total_bands - sci->stereo_bands)*18*sizeof(float));
    for (i = 0; i < sci->total_bands; i++, dst += 18, scf += 6)
    {
        for (k = 0; k < 12; k++)
        {
            dst[k + 0]   *= scf[0];
            dst[k + 576] *= scf[3];
        }
    }
}
#endif /* MINIMP3_ONLY_MP3 */

static int L3_read_side_info(bs_t *bs, L3_gr_info_t *gr, const uint8_t *hdr)
{
    static const uint8_t g_scf_long[8][23] = {
        { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
        { 12,12,12,12,12,12,16,20,24,28,32,40,48,56,64,76,90,2,2,2,2,2,0 },
        { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
        { 6,6,6,6,6,6,8,10,12,14,16,18,22,26,32,38,46,54,62,70,76,36,0 },
        { 6,6,6,6,6,6,8,10,12,14,16,20,24,28,32,38,46,52,60,68,58,54,0 },
        { 4,4,4,4,4,4,6,6,8,8,10,12,16,20,24,28,34,42,50,54,76,158,0 },
        { 4,4,4,4,4,4,6,6,6,8,10,12,16,18,22,28,34,40,46,54,54,192,0 },
        { 4,4,4,4,4,4,6,6,8,10,12,16,20,24,30,38,46,56,68,84,102,26,0 }
    };
    static const uint8_t g_scf_short[8][40] = {
        { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 8,8,8,8,8,8,8,8,8,12,12,12,16,16,16,20,20,20,24,24,24,28,28,28,36,36,36,2,2,2,2,2,2,2,2,2,26,26,26,0 },
        { 4,4,4,4,4,4,4,4,4,6,6,6,6,6,6,8,8,8,10,10,10,14,14,14,18,18,18,26,26,26,32,32,32,42,42,42,18,18,18,0 },
        { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,32,32,32,44,44,44,12,12,12,0 },
        { 4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,22,22,22,30,30,30,56,56,56,0 },
        { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,6,6,6,10,10,10,12,12,12,14,14,14,16,16,16,20,20,20,26,26,26,66,66,66,0 },
        { 4,4,4,4,4,4,4,4,4,4,4,4,6,6,6,8,8,8,12,12,12,16,16,16,20,20,20,26,26,26,34,34,34,42,42,42,12,12,12,0 }
    };
    static const uint8_t g_scf_mixed[8][40] = {
        { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 12,12,12,4,4,4,8,8,8,12,12,12,16,16,16,20,20,20,24,24,24,28,28,28,36,36,36,2,2,2,2,2,2,2,2,2,26,26,26,0 },
        { 6,6,6,6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,14,14,14,18,18,18,26,26,26,32,32,32,42,42,42,18,18,18,0 },
        { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,32,32,32,44,44,44,12,12,12,0 },
        { 6,6,6,6,6,6,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,24,24,24,30,30,30,40,40,40,18,18,18,0 },
        { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,8,8,8,10,10,10,12,12,12,14,14,14,18,18,18,22,22,22,30,30,30,56,56,56,0 },
        { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,6,6,6,10,10,10,12,12,12,14,14,14,16,16,16,20,20,20,26,26,26,66,66,66,0 },
        { 4,4,4,4,4,4,6,6,4,4,4,6,6,6,8,8,8,12,12,12,16,16,16,20,20,20,26,26,26,34,34,34,42,42,42,12,12,12,0 }
    };

    unsigned tables, scfsi = 0;
    int main_data_begin, part_23_sum = 0;
    int sr_idx = MINIMP3_HDR_GET_MY_SAMPLE_RATE(hdr); sr_idx -= (sr_idx != 0);
    int gr_count = MINIMP3_HDR_IS_MONO(hdr) ? 1 : 2;

    if (MINIMP3_HDR_TEST_MPEG1(hdr))
    {
        gr_count *= 2;
        main_data_begin = get_bits(bs, 9);
        scfsi = get_bits(bs, 7 + gr_count);
    } else
    {
        main_data_begin = get_bits(bs, 8 + gr_count) >> gr_count;
    }

    do
    {
        if (MINIMP3_HDR_IS_MONO(hdr))
        {
            scfsi <<= 4;
        }
        gr->part_23_length = (uint16_t)get_bits(bs, 12);
        part_23_sum += gr->part_23_length;
        gr->big_values = (uint16_t)get_bits(bs,  9);
        if (gr->big_values > 288)
        {
            return -1;
        }
        gr->global_gain = (uint8_t)get_bits(bs, 8);
        gr->scalefac_compress = (uint16_t)get_bits(bs, MINIMP3_HDR_TEST_MPEG1(hdr) ? 4 : 9);
        gr->sfbtab = g_scf_long[sr_idx];
        gr->n_long_sfb  = 22;
        gr->n_short_sfb = 0;
        if (get_bits(bs, 1))
        {
            gr->block_type = (uint8_t)get_bits(bs, 2);
            if (!gr->block_type)
            {
                return -1;
            }
            gr->mixed_block_flag = (uint8_t)get_bits(bs, 1);
            gr->region_count[0] = 7;
            gr->region_count[1] = 255;
            if (gr->block_type == SHORT_BLOCK_TYPE)
            {
                scfsi &= 0x0F0F;
                if (!gr->mixed_block_flag)
                {
                    gr->region_count[0] = 8;
                    gr->sfbtab = g_scf_short[sr_idx];
                    gr->n_long_sfb = 0;
                    gr->n_short_sfb = 39;
                } else
                {
                    gr->sfbtab = g_scf_mixed[sr_idx];
                    gr->n_long_sfb = MINIMP3_HDR_TEST_MPEG1(hdr) ? 8 : 6;
                    gr->n_short_sfb = 30;
                }
            }
            tables = get_bits(bs, 10);
            tables <<= 5;
            gr->subblock_gain[0] = (uint8_t)get_bits(bs, 3);
            gr->subblock_gain[1] = (uint8_t)get_bits(bs, 3);
            gr->subblock_gain[2] = (uint8_t)get_bits(bs, 3);
        } else
        {
            gr->block_type = 0;
            gr->mixed_block_flag = 0;
            tables = get_bits(bs, 15);
            gr->region_count[0] = (uint8_t)get_bits(bs, 4);
            gr->region_count[1] = (uint8_t)get_bits(bs, 3);
            gr->region_count[2] = 255;
        }
        gr->table_select[0] = (uint8_t)(tables >> 10);
        gr->table_select[1] = (uint8_t)((tables >> 5) & 31);
        gr->table_select[2] = (uint8_t)((tables) & 31);
        gr->preflag = MINIMP3_HDR_TEST_MPEG1(hdr) ? get_bits(bs, 1) : (gr->scalefac_compress >= 500);
        gr->scalefac_scale = (uint8_t)get_bits(bs, 1);
        gr->count1_table = (uint8_t)get_bits(bs, 1);
        gr->scfsi = (uint8_t)((scfsi >> 12) & 15);
        scfsi <<= 4;
        gr++;
    } while(--gr_count);

    if (part_23_sum + bs->pos > bs->limit + main_data_begin*8)
    {
        return -1;
    }

    return main_data_begin;
}

static void L3_read_scalefactors(uint8_t *scf, uint8_t *ist_pos, const uint8_t *scf_size, const uint8_t *scf_count, bs_t *bitbuf, int scfsi)
{
    int i, k;
    for (i = 0; i < 4 && scf_count[i]; i++, scfsi *= 2)
    {
        int cnt = scf_count[i];
        if (scfsi & 8)
        {
            memcpy(scf, ist_pos, cnt);
        } else
        {
            int bits = scf_size[i];
            if (!bits)
            {
                memset(scf, 0, cnt);
                memset(ist_pos, 0, cnt);
            } else
            {
                int max_scf = (scfsi < 0) ? (1 << bits) - 1 : -1;
                for (k = 0; k < cnt; k++)
                {
                    int s = get_bits(bitbuf, bits);
                    ist_pos[k] = (s == max_scf ? -1 : s);
                    scf[k] = s;
                }
            }
        }
        ist_pos += cnt;
        scf += cnt;
    }
    scf[0] = scf[1] = scf[2] = 0;
}

static float L3_ldexp_q2(float y, int exp_q2)
{
    static const float g_expfrac[4] = { 9.31322575e-10f,7.83145814e-10f,6.58544508e-10f,5.53767716e-10f };
    int e;
    do
    {
        e = std::min (30*4, exp_q2);
        y *= g_expfrac[e & 3]*(1 << 30 >> (e >> 2));
    } while ((exp_q2 -= e) > 0);
    return y;
}

static void L3_decode_scalefactors(const uint8_t *hdr, uint8_t *ist_pos, bs_t *bs, const L3_gr_info_t *gr, float *scf, int ch)
{
    static const uint8_t g_scf_partitions[3][28] = {
        { 6,5,5, 5,6,5,5,5,6,5, 7,3,11,10,0,0, 7, 7, 7,0, 6, 6,6,3, 8, 8,5,0 },
        { 8,9,6,12,6,9,9,9,6,9,12,6,15,18,0,0, 6,15,12,0, 6,12,9,6, 6,18,9,0 },
        { 9,9,6,12,9,9,9,9,9,9,12,6,18,18,0,0,12,12,12,0,12, 9,9,6,15,12,9,0 }
    };
    const uint8_t *scf_partition = g_scf_partitions[!!gr->n_short_sfb + !gr->n_long_sfb];
    uint8_t scf_size[4], iscf[40];
    int i, scf_shift = gr->scalefac_scale + 1, gain_exp, scfsi = gr->scfsi;
    float gain;

    if (MINIMP3_HDR_TEST_MPEG1(hdr))
    {
        static const uint8_t g_scfc_decode[16] = { 0,1,2,3, 12,5,6,7, 9,10,11,13, 14,15,18,19 };
        int part = g_scfc_decode[gr->scalefac_compress];
        scf_size[1] = scf_size[0] = (uint8_t)(part >> 2);
        scf_size[3] = scf_size[2] = (uint8_t)(part & 3);
    } else
    {
        static const uint8_t g_mod[6*4] = { 5,5,4,4,5,5,4,1,4,3,1,1,5,6,6,1,4,4,4,1,4,3,1,1 };
        int k, modprod, sfc, ist = MINIMP3_HDR_TEST_I_STEREO(hdr) && ch;
        sfc = gr->scalefac_compress >> ist;
        for (k = ist*3*4; sfc >= 0; sfc -= modprod, k += 4)
        {
            for (modprod = 1, i = 3; i >= 0; i--)
            {
                scf_size[i] = (uint8_t)(sfc / modprod % g_mod[k + i]);
                modprod *= g_mod[k + i];
            }
        }
        scf_partition += k;
        scfsi = -16;
    }
    L3_read_scalefactors(iscf, ist_pos, scf_size, scf_partition, bs, scfsi);

    if (gr->n_short_sfb)
    {
        int sh = 3 - scf_shift;
        for (i = 0; i < gr->n_short_sfb; i += 3)
        {
            iscf[gr->n_long_sfb + i + 0] += gr->subblock_gain[0] << sh;
            iscf[gr->n_long_sfb + i + 1] += gr->subblock_gain[1] << sh;
            iscf[gr->n_long_sfb + i + 2] += gr->subblock_gain[2] << sh;
        }
    } else if (gr->preflag)
    {
        static const uint8_t g_preamp[10] = { 1,1,1,1,2,2,3,3,3,2 };
        for (i = 0; i < 10; i++)
        {
            iscf[11 + i] += g_preamp[i];
        }
    }

    gain_exp = gr->global_gain + BITS_DEQUANTIZER_OUT*4 - 210 - (MINIMP3_HDR_IS_MS_STEREO(hdr) ? 2 : 0);
    gain = L3_ldexp_q2(1 << (MAX_SCFI/4),  MAX_SCFI - gain_exp);
    for (i = 0; i < (int)(gr->n_long_sfb + gr->n_short_sfb); i++)
    {
        scf[i] = L3_ldexp_q2(gain, iscf[i] << scf_shift);
    }
}

static const float g_pow43[129 + 16] = {
    0,-1,-2.519842f,-4.326749f,-6.349604f,-8.549880f,-10.902724f,-13.390518f,-16.000000f,-18.720754f,-21.544347f,-24.463781f,-27.473142f,-30.567351f,-33.741992f,-36.993181f,
    0,1,2.519842f,4.326749f,6.349604f,8.549880f,10.902724f,13.390518f,16.000000f,18.720754f,21.544347f,24.463781f,27.473142f,30.567351f,33.741992f,36.993181f,40.317474f,43.711787f,47.173345f,50.699631f,54.288352f,57.937408f,61.644865f,65.408941f,69.227979f,73.100443f,77.024898f,81.000000f,85.024491f,89.097188f,93.216975f,97.382800f,101.593667f,105.848633f,110.146801f,114.487321f,118.869381f,123.292209f,127.755065f,132.257246f,136.798076f,141.376907f,145.993119f,150.646117f,155.335327f,160.060199f,164.820202f,169.614826f,174.443577f,179.305980f,184.201575f,189.129918f,194.090580f,199.083145f,204.107210f,209.162385f,214.248292f,219.364564f,224.510845f,229.686789f,234.892058f,240.126328f,245.389280f,250.680604f,256.000000f,261.347174f,266.721841f,272.123723f,277.552547f,283.008049f,288.489971f,293.998060f,299.532071f,305.091761f,310.676898f,316.287249f,321.922592f,327.582707f,333.267377f,338.976394f,344.709550f,350.466646f,356.247482f,362.051866f,367.879608f,373.730522f,379.604427f,385.501143f,391.420496f,397.362314f,403.326427f,409.312672f,415.320884f,421.350905f,427.402579f,433.475750f,439.570269f,445.685987f,451.822757f,457.980436f,464.158883f,470.357960f,476.577530f,482.817459f,489.077615f,495.357868f,501.658090f,507.978156f,514.317941f,520.677324f,527.056184f,533.454404f,539.871867f,546.308458f,552.764065f,559.238575f,565.731879f,572.243870f,578.774440f,585.323483f,591.890898f,598.476581f,605.080431f,611.702349f,618.342238f,625.000000f,631.675540f,638.368763f,645.079578f
};

static float L3_pow_43(int x)
{
    float frac;
    int sign, mult = 256;

    if (x < 129)
    {
        return g_pow43[16 + x];
    }

    if (x < 1024)
    {
        mult = 16;
        x <<= 3;
    }

    sign = 2*x & 64;
    frac = (float)((x & 63) - sign) / ((x & ~63) + sign);
    return g_pow43[16 + ((x + sign) >> 6)]*(1.f + frac*((4.f/3) + frac*(2.f/9)))*mult;
}

static void L3_huffman(float *dst, bs_t *bs, const L3_gr_info_t *gr_info, const float *scf, int layer3gr_limit)
{
    static const int16_t tabs[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        785,785,785,785,784,784,784,784,513,513,513,513,513,513,513,513,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,
        -255,1313,1298,1282,785,785,785,785,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,290,288,
        -255,1313,1298,1282,769,769,769,769,529,529,529,529,529,529,529,529,528,528,528,528,528,528,528,528,512,512,512,512,512,512,512,512,290,288,
        -253,-318,-351,-367,785,785,785,785,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,819,818,547,547,275,275,275,275,561,560,515,546,289,274,288,258,
        -254,-287,1329,1299,1314,1312,1057,1057,1042,1042,1026,1026,784,784,784,784,529,529,529,529,529,529,529,529,769,769,769,769,768,768,768,768,563,560,306,306,291,259,
        -252,-413,-477,-542,1298,-575,1041,1041,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-383,-399,1107,1092,1106,1061,849,849,789,789,1104,1091,773,773,1076,1075,341,340,325,309,834,804,577,577,532,532,516,516,832,818,803,816,561,561,531,531,515,546,289,289,288,258,
        -252,-429,-493,-559,1057,1057,1042,1042,529,529,529,529,529,529,529,529,784,784,784,784,769,769,769,769,512,512,512,512,512,512,512,512,-382,1077,-415,1106,1061,1104,849,849,789,789,1091,1076,1029,1075,834,834,597,581,340,340,339,324,804,833,532,532,832,772,818,803,817,787,816,771,290,290,290,290,288,258,
        -253,-349,-414,-447,-463,1329,1299,-479,1314,1312,1057,1057,1042,1042,1026,1026,785,785,785,785,784,784,784,784,769,769,769,769,768,768,768,768,-319,851,821,-335,836,850,805,849,341,340,325,336,533,533,579,579,564,564,773,832,578,548,563,516,321,276,306,291,304,259,
        -251,-572,-733,-830,-863,-879,1041,1041,784,784,784,784,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-511,-527,-543,1396,1351,1381,1366,1395,1335,1380,-559,1334,1138,1138,1063,1063,1350,1392,1031,1031,1062,1062,1364,1363,1120,1120,1333,1348,881,881,881,881,375,374,359,373,343,358,341,325,791,791,1123,1122,-703,1105,1045,-719,865,865,790,790,774,774,1104,1029,338,293,323,308,-799,-815,833,788,772,818,803,816,322,292,307,320,561,531,515,546,289,274,288,258,
        -251,-525,-605,-685,-765,-831,-846,1298,1057,1057,1312,1282,785,785,785,785,784,784,784,784,769,769,769,769,512,512,512,512,512,512,512,512,1399,1398,1383,1367,1382,1396,1351,-511,1381,1366,1139,1139,1079,1079,1124,1124,1364,1349,1363,1333,882,882,882,882,807,807,807,807,1094,1094,1136,1136,373,341,535,535,881,775,867,822,774,-591,324,338,-671,849,550,550,866,864,609,609,293,336,534,534,789,835,773,-751,834,804,308,307,833,788,832,772,562,562,547,547,305,275,560,515,290,290,
        -252,-397,-477,-557,-622,-653,-719,-735,-750,1329,1299,1314,1057,1057,1042,1042,1312,1282,1024,1024,785,785,785,785,784,784,784,784,769,769,769,769,-383,1127,1141,1111,1126,1140,1095,1110,869,869,883,883,1079,1109,882,882,375,374,807,868,838,881,791,-463,867,822,368,263,852,837,836,-543,610,610,550,550,352,336,534,534,865,774,851,821,850,805,593,533,579,564,773,832,578,578,548,548,577,577,307,276,306,291,516,560,259,259,
        -250,-2107,-2507,-2764,-2909,-2974,-3007,-3023,1041,1041,1040,1040,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-767,-1052,-1213,-1277,-1358,-1405,-1469,-1535,-1550,-1582,-1614,-1647,-1662,-1694,-1726,-1759,-1774,-1807,-1822,-1854,-1886,1565,-1919,-1935,-1951,-1967,1731,1730,1580,1717,-1983,1729,1564,-1999,1548,-2015,-2031,1715,1595,-2047,1714,-2063,1610,-2079,1609,-2095,1323,1323,1457,1457,1307,1307,1712,1547,1641,1700,1699,1594,1685,1625,1442,1442,1322,1322,-780,-973,-910,1279,1278,1277,1262,1276,1261,1275,1215,1260,1229,-959,974,974,989,989,-943,735,478,478,495,463,506,414,-1039,1003,958,1017,927,942,987,957,431,476,1272,1167,1228,-1183,1256,-1199,895,895,941,941,1242,1227,1212,1135,1014,1014,490,489,503,487,910,1013,985,925,863,894,970,955,1012,847,-1343,831,755,755,984,909,428,366,754,559,-1391,752,486,457,924,997,698,698,983,893,740,740,908,877,739,739,667,667,953,938,497,287,271,271,683,606,590,712,726,574,302,302,738,736,481,286,526,725,605,711,636,724,696,651,589,681,666,710,364,467,573,695,466,466,301,465,379,379,709,604,665,679,316,316,634,633,436,436,464,269,424,394,452,332,438,363,347,408,393,448,331,422,362,407,392,421,346,406,391,376,375,359,1441,1306,-2367,1290,-2383,1337,-2399,-2415,1426,1321,-2431,1411,1336,-2447,-2463,-2479,1169,1169,1049,1049,1424,1289,1412,1352,1319,-2495,1154,1154,1064,1064,1153,1153,416,390,360,404,403,389,344,374,373,343,358,372,327,357,342,311,356,326,1395,1394,1137,1137,1047,1047,1365,1392,1287,1379,1334,1364,1349,1378,1318,1363,792,792,792,792,1152,1152,1032,1032,1121,1121,1046,1046,1120,1120,1030,1030,-2895,1106,1061,1104,849,849,789,789,1091,1076,1029,1090,1060,1075,833,833,309,324,532,532,832,772,818,803,561,561,531,560,515,546,289,274,288,258,
        -250,-1179,-1579,-1836,-1996,-2124,-2253,-2333,-2413,-2477,-2542,-2574,-2607,-2622,-2655,1314,1313,1298,1312,1282,785,785,785,785,1040,1040,1025,1025,768,768,768,768,-766,-798,-830,-862,-895,-911,-927,-943,-959,-975,-991,-1007,-1023,-1039,-1055,-1070,1724,1647,-1103,-1119,1631,1767,1662,1738,1708,1723,-1135,1780,1615,1779,1599,1677,1646,1778,1583,-1151,1777,1567,1737,1692,1765,1722,1707,1630,1751,1661,1764,1614,1736,1676,1763,1750,1645,1598,1721,1691,1762,1706,1582,1761,1566,-1167,1749,1629,767,766,751,765,494,494,735,764,719,749,734,763,447,447,748,718,477,506,431,491,446,476,461,505,415,430,475,445,504,399,460,489,414,503,383,474,429,459,502,502,746,752,488,398,501,473,413,472,486,271,480,270,-1439,-1455,1357,-1471,-1487,-1503,1341,1325,-1519,1489,1463,1403,1309,-1535,1372,1448,1418,1476,1356,1462,1387,-1551,1475,1340,1447,1402,1386,-1567,1068,1068,1474,1461,455,380,468,440,395,425,410,454,364,467,466,464,453,269,409,448,268,432,1371,1473,1432,1417,1308,1460,1355,1446,1459,1431,1083,1083,1401,1416,1458,1445,1067,1067,1370,1457,1051,1051,1291,1430,1385,1444,1354,1415,1400,1443,1082,1082,1173,1113,1186,1066,1185,1050,-1967,1158,1128,1172,1097,1171,1081,-1983,1157,1112,416,266,375,400,1170,1142,1127,1065,793,793,1169,1033,1156,1096,1141,1111,1155,1080,1126,1140,898,898,808,808,897,897,792,792,1095,1152,1032,1125,1110,1139,1079,1124,882,807,838,881,853,791,-2319,867,368,263,822,852,837,866,806,865,-2399,851,352,262,534,534,821,836,594,594,549,549,593,593,533,533,848,773,579,579,564,578,548,563,276,276,577,576,306,291,516,560,305,305,275,259,
        -251,-892,-2058,-2620,-2828,-2957,-3023,-3039,1041,1041,1040,1040,769,769,769,769,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,256,-511,-527,-543,-559,1530,-575,-591,1528,1527,1407,1526,1391,1023,1023,1023,1023,1525,1375,1268,1268,1103,1103,1087,1087,1039,1039,1523,-604,815,815,815,815,510,495,509,479,508,463,507,447,431,505,415,399,-734,-782,1262,-815,1259,1244,-831,1258,1228,-847,-863,1196,-879,1253,987,987,748,-767,493,493,462,477,414,414,686,669,478,446,461,445,474,429,487,458,412,471,1266,1264,1009,1009,799,799,-1019,-1276,-1452,-1581,-1677,-1757,-1821,-1886,-1933,-1997,1257,1257,1483,1468,1512,1422,1497,1406,1467,1496,1421,1510,1134,1134,1225,1225,1466,1451,1374,1405,1252,1252,1358,1480,1164,1164,1251,1251,1238,1238,1389,1465,-1407,1054,1101,-1423,1207,-1439,830,830,1248,1038,1237,1117,1223,1148,1236,1208,411,426,395,410,379,269,1193,1222,1132,1235,1221,1116,976,976,1192,1162,1177,1220,1131,1191,963,963,-1647,961,780,-1663,558,558,994,993,437,408,393,407,829,978,813,797,947,-1743,721,721,377,392,844,950,828,890,706,706,812,859,796,960,948,843,934,874,571,571,-1919,690,555,689,421,346,539,539,944,779,918,873,932,842,903,888,570,570,931,917,674,674,-2575,1562,-2591,1609,-2607,1654,1322,1322,1441,1441,1696,1546,1683,1593,1669,1624,1426,1426,1321,1321,1639,1680,1425,1425,1305,1305,1545,1668,1608,1623,1667,1592,1638,1666,1320,1320,1652,1607,1409,1409,1304,1304,1288,1288,1664,1637,1395,1395,1335,1335,1622,1636,1394,1394,1319,1319,1606,1621,1392,1392,1137,1137,1137,1137,345,390,360,375,404,373,1047,-2751,-2767,-2783,1062,1121,1046,-2799,1077,-2815,1106,1061,789,789,1105,1104,263,355,310,340,325,354,352,262,339,324,1091,1076,1029,1090,1060,1075,833,833,788,788,1088,1028,818,818,803,803,561,561,531,531,816,771,546,546,289,274,288,258,
        -253,-317,-381,-446,-478,-509,1279,1279,-811,-1179,-1451,-1756,-1900,-2028,-2189,-2253,-2333,-2414,-2445,-2511,-2526,1313,1298,-2559,1041,1041,1040,1040,1025,1025,1024,1024,1022,1007,1021,991,1020,975,1019,959,687,687,1018,1017,671,671,655,655,1016,1015,639,639,758,758,623,623,757,607,756,591,755,575,754,559,543,543,1009,783,-575,-621,-685,-749,496,-590,750,749,734,748,974,989,1003,958,988,973,1002,942,987,957,972,1001,926,986,941,971,956,1000,910,985,925,999,894,970,-1071,-1087,-1102,1390,-1135,1436,1509,1451,1374,-1151,1405,1358,1480,1420,-1167,1507,1494,1389,1342,1465,1435,1450,1326,1505,1310,1493,1373,1479,1404,1492,1464,1419,428,443,472,397,736,526,464,464,486,457,442,471,484,482,1357,1449,1434,1478,1388,1491,1341,1490,1325,1489,1463,1403,1309,1477,1372,1448,1418,1433,1476,1356,1462,1387,-1439,1475,1340,1447,1402,1474,1324,1461,1371,1473,269,448,1432,1417,1308,1460,-1711,1459,-1727,1441,1099,1099,1446,1386,1431,1401,-1743,1289,1083,1083,1160,1160,1458,1445,1067,1067,1370,1457,1307,1430,1129,1129,1098,1098,268,432,267,416,266,400,-1887,1144,1187,1082,1173,1113,1186,1066,1050,1158,1128,1143,1172,1097,1171,1081,420,391,1157,1112,1170,1142,1127,1065,1169,1049,1156,1096,1141,1111,1155,1080,1126,1154,1064,1153,1140,1095,1048,-2159,1125,1110,1137,-2175,823,823,1139,1138,807,807,384,264,368,263,868,838,853,791,867,822,852,837,866,806,865,790,-2319,851,821,836,352,262,850,805,849,-2399,533,533,835,820,336,261,578,548,563,577,532,532,832,772,562,562,547,547,305,275,560,515,290,290,288,258 };
    static const uint8_t tab32[] = { 130,162,193,209,44,28,76,140,9,9,9,9,9,9,9,9,190,254,222,238,126,94,157,157,109,61,173,205 };
    static const uint8_t tab33[] = { 252,236,220,204,188,172,156,140,124,108,92,76,60,44,28,12 };
    static const int16_t tabindex[2*16] = { 0,32,64,98,0,132,180,218,292,364,426,538,648,746,0,1126,1460,1460,1460,1460,1460,1460,1460,1460,1842,1842,1842,1842,1842,1842,1842,1842 };
    static const uint8_t g_linbits[] =  { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,2,3,4,6,8,10,13,4,5,6,7,8,9,11,13 };

#define MINIMP3_PEEK_BITS(n)  (bs_cache >> (32 - n))
#define MINIMP3_FLUSH_BITS(n) { bs_cache <<= (n); bs_sh += (n); }
#define MINIMP3_CHECK_BITS    while (bs_sh >= 0) { bs_cache |= (uint32_t)*bs_next_ptr++ << bs_sh; bs_sh -= 8; }
#define MINIMP3_BSPOS         ((bs_next_ptr - bs->buf)*8 - 24 + bs_sh)

    float one = 0.0f;
    int ireg = 0, big_val_cnt = gr_info->big_values;
    const uint8_t *sfb = gr_info->sfbtab;
    const uint8_t *bs_next_ptr = bs->buf + bs->pos/8;
    uint32_t bs_cache = (((bs_next_ptr[0]*256u + bs_next_ptr[1])*256u + bs_next_ptr[2])*256u + bs_next_ptr[3]) << (bs->pos & 7);
    int pairs_to_decode, np, bs_sh = (bs->pos & 7) - 8;
    bs_next_ptr += 4;

    while (big_val_cnt > 0)
    {
        int tab_num = gr_info->table_select[ireg];
        int sfb_cnt = gr_info->region_count[ireg++];
        const int16_t *codebook = tabs + tabindex[tab_num];
        int linbits = g_linbits[tab_num];
        if (linbits)
        {
            do
            {
                np = *sfb++ / 2;
                pairs_to_decode = std::min (big_val_cnt, np);
                one = *scf++;
                do
                {
                    int j, w = 5;
                    int leaf = codebook[MINIMP3_PEEK_BITS(w)];
                    while (leaf < 0)
                    {
                        MINIMP3_FLUSH_BITS(w);
                        w = leaf & 7;
                        leaf = codebook[MINIMP3_PEEK_BITS(w) - (leaf >> 3)];
                    }
                    MINIMP3_FLUSH_BITS(leaf >> 8);

                    for (j = 0; j < 2; j++, dst++, leaf >>= 4)
                    {
                        int lsb = leaf & 0x0F;
                        if (lsb == 15)
                        {
                            lsb += MINIMP3_PEEK_BITS(linbits);
                            MINIMP3_FLUSH_BITS(linbits);
                            MINIMP3_CHECK_BITS;
                            *dst = one*L3_pow_43(lsb)*((int32_t)bs_cache < 0 ? -1: 1);
                        } else
                        {
                            *dst = g_pow43[16 + lsb - 16*(bs_cache >> 31)]*one;
                        }
                        MINIMP3_FLUSH_BITS(lsb ? 1 : 0);
                    }
                    MINIMP3_CHECK_BITS;
                } while (--pairs_to_decode);
            } while ((big_val_cnt -= np) > 0 && --sfb_cnt >= 0);
        } else
        {
            do
            {
                np = *sfb++ / 2;
                pairs_to_decode = std::min (big_val_cnt, np);
                one = *scf++;
                do
                {
                    int j, w = 5;
                    int leaf = codebook[MINIMP3_PEEK_BITS(w)];
                    while (leaf < 0)
                    {
                        MINIMP3_FLUSH_BITS(w);
                        w = leaf & 7;
                        leaf = codebook[MINIMP3_PEEK_BITS(w) - (leaf >> 3)];
                    }
                    MINIMP3_FLUSH_BITS(leaf >> 8);

                    for (j = 0; j < 2; j++, dst++, leaf >>= 4)
                    {
                        int lsb = leaf & 0x0F;
                        *dst = g_pow43[16 + lsb - 16*(bs_cache >> 31)]*one;
                        MINIMP3_FLUSH_BITS(lsb ? 1 : 0);
                    }
                    MINIMP3_CHECK_BITS;
                } while (--pairs_to_decode);
            } while ((big_val_cnt -= np) > 0 && --sfb_cnt >= 0);
        }
    }

    for (np = 1 - big_val_cnt;; dst += 4)
    {
        const uint8_t *codebook_count1 = (gr_info->count1_table) ? tab33 : tab32;
        int leaf = codebook_count1[MINIMP3_PEEK_BITS(4)];
        if (!(leaf & 8))
        {
            leaf = codebook_count1[(leaf >> 3) + (bs_cache << 4 >> (32 - (leaf & 3)))];
        }
        MINIMP3_FLUSH_BITS(leaf & 7);
        if (MINIMP3_BSPOS > layer3gr_limit)
        {
            break;
        }
#define MINIMP3_RELOAD_SCALEFACTOR  if (!--np) { np = *sfb++/2; if (!np) break; one = *scf++; }
#define MINIMP3_DEQ_COUNT1(s) if (leaf & (128 >> s)) { dst[s] = ((int32_t)bs_cache < 0) ? -one : one; MINIMP3_FLUSH_BITS(1) }
        MINIMP3_RELOAD_SCALEFACTOR;
        MINIMP3_DEQ_COUNT1(0);
        MINIMP3_DEQ_COUNT1(1);
        MINIMP3_RELOAD_SCALEFACTOR;
        MINIMP3_DEQ_COUNT1(2);
        MINIMP3_DEQ_COUNT1(3);
        MINIMP3_CHECK_BITS;
    }

    bs->pos = layer3gr_limit;
}

static void L3_midside_stereo(float *left, int n)
{
    int i = 0;
    float *right = left + 576;
#if MINIMP3_HAVE_SIMD
    if (have_simd())
    {
        for (; i < n - 3; i += 4)
        {
            f4 vl = MINIMP3_VLD(left + i);
            f4 vr = MINIMP3_VLD(right + i);
            MINIMP3_VSTORE(left + i, MINIMP3_VADD(vl, vr));
            MINIMP3_VSTORE(right + i, MINIMP3_VSUB(vl, vr));
        }
#ifdef __GNUC__
        /* Workaround for spurious -Waggressive-loop-optimizations warning from gcc.
         * For more info see: https://github.com/lieff/minimp3/issues/88
         */
        if (__builtin_constant_p(n % 4 == 0) && n % 4 == 0)
            return;
#endif
    }
#endif /* MINIMP3_HAVE_SIMD */
    for (; i < n; i++)
    {
        float a = left[i];
        float b = right[i];
        left[i] = a + b;
        right[i] = a - b;
    }
}

static void L3_intensity_stereo_band(float *left, int n, float kl, float kr)
{
    int i;
    for (i = 0; i < n; i++)
    {
        left[i + 576] = left[i]*kr;
        left[i] = left[i]*kl;
    }
}

static void L3_stereo_top_band(const float *right, const uint8_t *sfb, int nbands, int max_band[3])
{
    int i, k;

    max_band[0] = max_band[1] = max_band[2] = -1;

    for (i = 0; i < nbands; i++)
    {
        for (k = 0; k < sfb[i]; k += 2)
        {
            if (right[k] != 0 || right[k + 1] != 0)
            {
                max_band[i % 3] = i;
                break;
            }
        }
        right += sfb[i];
    }
}

static void L3_stereo_process(float *left, const uint8_t *ist_pos, const uint8_t *sfb, const uint8_t *hdr, int max_band[3], int mpeg2_sh)
{
    static const float g_pan[7*2] = { 0,1,0.21132487f,0.78867513f,0.36602540f,0.63397460f,0.5f,0.5f,0.63397460f,0.36602540f,0.78867513f,0.21132487f,1,0 };
    unsigned i, max_pos = MINIMP3_HDR_TEST_MPEG1(hdr) ? 7 : 64;

    for (i = 0; sfb[i]; i++)
    {
        unsigned ipos = ist_pos[i];
        if ((int)i > max_band[i % 3] && ipos < max_pos)
        {
            float kl, kr, s = MINIMP3_HDR_TEST_MS_STEREO(hdr) ? 1.41421356f : 1;
            if (MINIMP3_HDR_TEST_MPEG1(hdr))
            {
                kl = g_pan[2*ipos];
                kr = g_pan[2*ipos + 1];
            } else
            {
                kl = 1;
                kr = L3_ldexp_q2(1, (ipos + 1) >> 1 << mpeg2_sh);
                if (ipos & 1)
                {
                    kl = kr;
                    kr = 1;
                }
            }
            L3_intensity_stereo_band(left, sfb[i], kl*s, kr*s);
        } else if (MINIMP3_HDR_TEST_MS_STEREO(hdr))
        {
            L3_midside_stereo(left, sfb[i]);
        }
        left += sfb[i];
    }
}

static void L3_intensity_stereo(float *left, uint8_t *ist_pos, const L3_gr_info_t *gr, const uint8_t *hdr)
{
    int max_band[3], n_sfb = gr->n_long_sfb + gr->n_short_sfb;
    int i, max_blocks = gr->n_short_sfb ? 3 : 1;

    L3_stereo_top_band(left + 576, gr->sfbtab, n_sfb, max_band);
    if (gr->n_long_sfb)
    {
        max_band[0] = max_band[1] = max_band[2] = std::max (std::max (max_band[0], max_band[1]), max_band[2]);
    }
    for (i = 0; i < max_blocks; i++)
    {
        int default_pos = MINIMP3_HDR_TEST_MPEG1(hdr) ? 3 : 0;
        int itop = n_sfb - max_blocks + i;
        int prev = itop - max_blocks;
        ist_pos[itop] = max_band[i] >= prev ? default_pos : ist_pos[prev];
    }
    L3_stereo_process(left, ist_pos, gr->sfbtab, hdr, max_band, gr[1].scalefac_compress & 1);
}

static void L3_reorder(float *grbuf, float *scratch, const uint8_t *sfb)
{
    int i, len;
    float *src = grbuf, *dst = scratch;

    for (;0 != (len = *sfb); sfb += 3, src += 2*len)
    {
        for (i = 0; i < len; i++, src++)
        {
            *dst++ = src[0*len];
            *dst++ = src[1*len];
            *dst++ = src[2*len];
        }
    }
    memcpy(grbuf, scratch, (dst - scratch)*sizeof(float));
}

static void L3_antialias(float *grbuf, int nbands)
{
    static const float g_aa[2][8] = {
        {0.85749293f,0.88174200f,0.94962865f,0.98331459f,0.99551782f,0.99916056f,0.99989920f,0.99999316f},
        {0.51449576f,0.47173197f,0.31337745f,0.18191320f,0.09457419f,0.04096558f,0.01419856f,0.00369997f}
    };

    for (; nbands > 0; nbands--, grbuf += 18)
    {
        int i = 0;
#if MINIMP3_HAVE_SIMD
        if (have_simd()) for (; i < 8; i += 4)
        {
            f4 vu = MINIMP3_VLD(grbuf + 18 + i);
            f4 vd = MINIMP3_VLD(grbuf + 14 - i);
            f4 vc0 = MINIMP3_VLD(g_aa[0] + i);
            f4 vc1 = MINIMP3_VLD(g_aa[1] + i);
            vd = MINIMP3_VREV(vd);
            MINIMP3_VSTORE(grbuf + 18 + i, MINIMP3_VSUB(MINIMP3_VMUL(vu, vc0), MINIMP3_VMUL(vd, vc1)));
            vd = MINIMP3_VADD(MINIMP3_VMUL(vu, vc1), MINIMP3_VMUL(vd, vc0));
            MINIMP3_VSTORE(grbuf + 14 - i, MINIMP3_VREV(vd));
        }
#endif /* MINIMP3_HAVE_SIMD */
#ifndef MINIMP3_ONLY_SIMD
        for(; i < 8; i++)
        {
            float u = grbuf[18 + i];
            float d = grbuf[17 - i];
            grbuf[18 + i] = u*g_aa[0][i] - d*g_aa[1][i];
            grbuf[17 - i] = u*g_aa[1][i] + d*g_aa[0][i];
        }
#endif /* MINIMP3_ONLY_SIMD */
    }
}

static void L3_dct3_9(float *y)
{
    float s0, s1, s2, s3, s4, s5, s6, s7, s8, t0, t2, t4;

    s0 = y[0]; s2 = y[2]; s4 = y[4]; s6 = y[6]; s8 = y[8];
    t0 = s0 + s6*0.5f;
    s0 -= s6;
    t4 = (s4 + s2)*0.93969262f;
    t2 = (s8 + s2)*0.76604444f;
    s6 = (s4 - s8)*0.17364818f;
    s4 += s8 - s2;

    s2 = s0 - s4*0.5f;
    y[4] = s4 + s0;
    s8 = t0 - t2 + s6;
    s0 = t0 - t4 + t2;
    s4 = t0 + t4 - s6;

    s1 = y[1]; s3 = y[3]; s5 = y[5]; s7 = y[7];

    s3 *= 0.86602540f;
    t0 = (s5 + s1)*0.98480775f;
    t4 = (s5 - s7)*0.34202014f;
    t2 = (s1 + s7)*0.64278761f;
    s1 = (s1 - s5 - s7)*0.86602540f;

    s5 = t0 - s3 - t2;
    s7 = t4 - s3 - t0;
    s3 = t4 + s3 - t2;

    y[0] = s4 - s7;
    y[1] = s2 + s1;
    y[2] = s0 - s3;
    y[3] = s8 + s5;
    y[5] = s8 - s5;
    y[6] = s0 + s3;
    y[7] = s2 - s1;
    y[8] = s4 + s7;
}

static void L3_imdct36(float *grbuf, float *overlap, const float *window, int nbands)
{
    int i, j;
    static const float g_twid9[18] = {
        0.73727734f,0.79335334f,0.84339145f,0.88701083f,0.92387953f,0.95371695f,0.97629601f,0.99144486f,0.99904822f,
        0.67559021f,0.60876143f,0.53729961f,0.46174861f,0.38268343f,0.30070580f,0.21643961f,0.13052619f,0.04361938f
    };

    for (j = 0; j < nbands; j++, grbuf += 18, overlap += 9)
    {
        float co[9], si[9];
        co[0] = -grbuf[0];
        si[0] = grbuf[17];
        for (i = 0; i < 4; i++)
        {
            si[8 - 2*i] =   grbuf[4*i + 1] - grbuf[4*i + 2];
            co[1 + 2*i] =   grbuf[4*i + 1] + grbuf[4*i + 2];
            si[7 - 2*i] =   grbuf[4*i + 4] - grbuf[4*i + 3];
            co[2 + 2*i] = -(grbuf[4*i + 3] + grbuf[4*i + 4]);
        }
        L3_dct3_9(co);
        L3_dct3_9(si);

        si[1] = -si[1];
        si[3] = -si[3];
        si[5] = -si[5];
        si[7] = -si[7];

        i = 0;

#if MINIMP3_HAVE_SIMD
        if (have_simd()) for (; i < 8; i += 4)
        {
            f4 vovl = MINIMP3_VLD(overlap + i);
            f4 vc = MINIMP3_VLD(co + i);
            f4 vs = MINIMP3_VLD(si + i);
            f4 vr0 = MINIMP3_VLD(g_twid9 + i);
            f4 vr1 = MINIMP3_VLD(g_twid9 + 9 + i);
            f4 vw0 = MINIMP3_VLD(window + i);
            f4 vw1 = MINIMP3_VLD(window + 9 + i);
            f4 vsum = MINIMP3_VADD(MINIMP3_VMUL(vc, vr1), MINIMP3_VMUL(vs, vr0));
            MINIMP3_VSTORE(overlap + i, MINIMP3_VSUB(MINIMP3_VMUL(vc, vr0), MINIMP3_VMUL(vs, vr1)));
            MINIMP3_VSTORE(grbuf + i, MINIMP3_VSUB(MINIMP3_VMUL(vovl, vw0), MINIMP3_VMUL(vsum, vw1)));
            vsum = MINIMP3_VADD(MINIMP3_VMUL(vovl, vw1), MINIMP3_VMUL(vsum, vw0));
            MINIMP3_VSTORE(grbuf + 14 - i, MINIMP3_VREV(vsum));
        }
#endif /* MINIMP3_HAVE_SIMD */
        for (; i < 9; i++)
        {
            float ovl  = overlap[i];
            float sum  = co[i]*g_twid9[9 + i] + si[i]*g_twid9[0 + i];
            overlap[i] = co[i]*g_twid9[0 + i] - si[i]*g_twid9[9 + i];
            grbuf[i]      = ovl*window[0 + i] - sum*window[9 + i];
            grbuf[17 - i] = ovl*window[9 + i] + sum*window[0 + i];
        }
    }
}

static void L3_idct3(float x0, float x1, float x2, float *dst)
{
    float m1 = x1*0.86602540f;
    float a1 = x0 - x2*0.5f;
    dst[1] = x0 + x2;
    dst[0] = a1 + m1;
    dst[2] = a1 - m1;
}

static void L3_imdct12(float *x, float *dst, float *overlap)
{
    static const float g_twid3[6] = { 0.79335334f,0.92387953f,0.99144486f, 0.60876143f,0.38268343f,0.13052619f };
    float co[3], si[3];
    int i;

    L3_idct3(-x[0], x[6] + x[3], x[12] + x[9], co);
    L3_idct3(x[15], x[12] - x[9], x[6] - x[3], si);
    si[1] = -si[1];

    for (i = 0; i < 3; i++)
    {
        float ovl  = overlap[i];
        float sum  = co[i]*g_twid3[3 + i] + si[i]*g_twid3[0 + i];
        overlap[i] = co[i]*g_twid3[0 + i] - si[i]*g_twid3[3 + i];
        dst[i]     = ovl*g_twid3[2 - i] - sum*g_twid3[5 - i];
        dst[5 - i] = ovl*g_twid3[5 - i] + sum*g_twid3[2 - i];
    }
}

static void L3_imdct_short(float *grbuf, float *overlap, int nbands)
{
    for (;nbands > 0; nbands--, overlap += 9, grbuf += 18)
    {
        float tmp[18];
        memcpy(tmp, grbuf, sizeof(tmp));
        memcpy(grbuf, overlap, 6*sizeof(float));
        L3_imdct12(tmp, grbuf + 6, overlap + 6);
        L3_imdct12(tmp + 1, grbuf + 12, overlap + 6);
        L3_imdct12(tmp + 2, overlap, overlap + 6);
    }
}

static void L3_change_sign(float *grbuf)
{
    int b, i;
    for (b = 0, grbuf += 18; b < 32; b += 2, grbuf += 36)
        for (i = 1; i < 18; i += 2)
            grbuf[i] = -grbuf[i];
}

static void L3_imdct_gr(float *grbuf, float *overlap, unsigned block_type, unsigned n_long_bands)
{
    static const float g_mdct_window[2][18] = {
        { 0.99904822f,0.99144486f,0.97629601f,0.95371695f,0.92387953f,0.88701083f,0.84339145f,0.79335334f,0.73727734f,
          0.04361938f,0.13052619f,0.21643961f,0.30070580f,0.38268343f,0.46174861f,0.53729961f,0.60876143f,0.67559021f },
        { 1,1,1,1,1,1,0.99144486f,0.92387953f,0.79335334f,0,0,0,0,0,0,0.13052619f,0.38268343f,0.60876143f }
    };
    if (n_long_bands)
    {
        L3_imdct36(grbuf, overlap, g_mdct_window[0], n_long_bands);
        grbuf += 18*n_long_bands;
        overlap += 9*n_long_bands;
    }
    if (block_type == SHORT_BLOCK_TYPE)
        L3_imdct_short(grbuf, overlap, 32 - n_long_bands);
    else
        L3_imdct36(grbuf, overlap, g_mdct_window[block_type == STOP_BLOCK_TYPE], 32 - n_long_bands);
}

static void L3_save_reservoir(mp3dec_t *h, mp3dec_scratch_t *s)
{
    int pos = (s->bs.pos + 7)/8u;
    int remains = s->bs.limit/8u - pos;
    if (remains > MAX_BITRESERVOIR_BYTES)
    {
        pos += remains - MAX_BITRESERVOIR_BYTES;
        remains = MAX_BITRESERVOIR_BYTES;
    }
    if (remains > 0)
    {
        memmove(h->reserv_buf, s->maindata + pos, remains);
    }
    h->reserv = remains;
}

static int L3_restore_reservoir(mp3dec_t *h, bs_t *bs, mp3dec_scratch_t *s, int main_data_begin)
{
    int frame_bytes = (bs->limit - bs->pos)/8;
    int bytes_have = std::min (h->reserv, main_data_begin);
    memcpy(s->maindata, h->reserv_buf + std::max (0, h->reserv - main_data_begin), std::min (h->reserv, main_data_begin));
    memcpy(s->maindata + bytes_have, bs->buf + bs->pos/8, frame_bytes);
    bs_init(&s->bs, s->maindata, bytes_have + frame_bytes);
    return h->reserv >= main_data_begin;
}

static void L3_decode(mp3dec_t *h, mp3dec_scratch_t *s, L3_gr_info_t *gr_info, int nch)
{
    int ch;

    for (ch = 0; ch < nch; ch++)
    {
        int layer3gr_limit = s->bs.pos + gr_info[ch].part_23_length;
        L3_decode_scalefactors(h->header, s->ist_pos[ch], &s->bs, gr_info + ch, s->scf, ch);
        L3_huffman(s->grbuf[ch], &s->bs, gr_info + ch, s->scf, layer3gr_limit);
    }

    if (MINIMP3_HDR_TEST_I_STEREO(h->header))
    {
        L3_intensity_stereo(s->grbuf[0], s->ist_pos[1], gr_info, h->header);
    } else if (MINIMP3_HDR_IS_MS_STEREO(h->header))
    {
        L3_midside_stereo(s->grbuf[0], 576);
    }

    for (ch = 0; ch < nch; ch++, gr_info++)
    {
        int aa_bands = 31;
        int n_long_bands = (gr_info->mixed_block_flag ? 2 : 0) << (int)(MINIMP3_HDR_GET_MY_SAMPLE_RATE(h->header) == 2);

        if (gr_info->n_short_sfb)
        {
            aa_bands = n_long_bands - 1;
            L3_reorder(s->grbuf[ch] + n_long_bands*18, s->syn[0], gr_info->sfbtab + gr_info->n_long_sfb);
        }

        L3_antialias(s->grbuf[ch], aa_bands);
        L3_imdct_gr(s->grbuf[ch], h->mdct_overlap[ch], gr_info->block_type, n_long_bands);
        L3_change_sign(s->grbuf[ch]);
    }
}

static void mp3d_DCT_II(float *grbuf, int n)
{
    static const float g_sec[24] = {
        10.19000816f,0.50060302f,0.50241929f,3.40760851f,0.50547093f,0.52249861f,2.05778098f,0.51544732f,0.56694406f,1.48416460f,0.53104258f,0.64682180f,1.16943991f,0.55310392f,0.78815460f,0.97256821f,0.58293498f,1.06067765f,0.83934963f,0.62250412f,1.72244716f,0.74453628f,0.67480832f,5.10114861f
    };
    int i, k = 0;
#if MINIMP3_HAVE_SIMD
    if (have_simd()) for (; k < n; k += 4)
    {
        f4 t[4][8], *x;
        float *y = grbuf + k;

        for (x = t[0], i = 0; i < 8; i++, x++)
        {
            f4 x0 = MINIMP3_VLD(&y[i*18]);
            f4 x1 = MINIMP3_VLD(&y[(15 - i)*18]);
            f4 x2 = MINIMP3_VLD(&y[(16 + i)*18]);
            f4 x3 = MINIMP3_VLD(&y[(31 - i)*18]);
            f4 t0 = MINIMP3_VADD(x0, x3);
            f4 t1 = MINIMP3_VADD(x1, x2);
            f4 t2 = MINIMP3_VMUL_S(MINIMP3_VSUB(x1, x2), g_sec[3*i + 0]);
            f4 t3 = MINIMP3_VMUL_S(MINIMP3_VSUB(x0, x3), g_sec[3*i + 1]);
            x[0] = MINIMP3_VADD(t0, t1);
            x[8] = MINIMP3_VMUL_S(MINIMP3_VSUB(t0, t1), g_sec[3*i + 2]);
            x[16] = MINIMP3_VADD(t3, t2);
            x[24] = MINIMP3_VMUL_S(MINIMP3_VSUB(t3, t2), g_sec[3*i + 2]);
        }
        for (x = t[0], i = 0; i < 4; i++, x += 8)
        {
            f4 x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3], x4 = x[4], x5 = x[5], x6 = x[6], x7 = x[7], xt;
            xt = MINIMP3_VSUB(x0, x7); x0 = MINIMP3_VADD(x0, x7);
            x7 = MINIMP3_VSUB(x1, x6); x1 = MINIMP3_VADD(x1, x6);
            x6 = MINIMP3_VSUB(x2, x5); x2 = MINIMP3_VADD(x2, x5);
            x5 = MINIMP3_VSUB(x3, x4); x3 = MINIMP3_VADD(x3, x4);
            x4 = MINIMP3_VSUB(x0, x3); x0 = MINIMP3_VADD(x0, x3);
            x3 = MINIMP3_VSUB(x1, x2); x1 = MINIMP3_VADD(x1, x2);
            x[0] = MINIMP3_VADD(x0, x1);
            x[4] = MINIMP3_VMUL_S(MINIMP3_VSUB(x0, x1), 0.70710677f);
            x5 = MINIMP3_VADD(x5, x6);
            x6 = MINIMP3_VMUL_S(MINIMP3_VADD(x6, x7), 0.70710677f);
            x7 = MINIMP3_VADD(x7, xt);
            x3 = MINIMP3_VMUL_S(MINIMP3_VADD(x3, x4), 0.70710677f);
            x5 = MINIMP3_VSUB(x5, MINIMP3_VMUL_S(x7, 0.198912367f)); /* rotate by PI/8 */
            x7 = MINIMP3_VADD(x7, MINIMP3_VMUL_S(x5, 0.382683432f));
            x5 = MINIMP3_VSUB(x5, MINIMP3_VMUL_S(x7, 0.198912367f));
            x0 = MINIMP3_VSUB(xt, x6); xt = MINIMP3_VADD(xt, x6);
            x[1] = MINIMP3_VMUL_S(MINIMP3_VADD(xt, x7), 0.50979561f);
            x[2] = MINIMP3_VMUL_S(MINIMP3_VADD(x4, x3), 0.54119611f);
            x[3] = MINIMP3_VMUL_S(MINIMP3_VSUB(x0, x5), 0.60134488f);
            x[5] = MINIMP3_VMUL_S(MINIMP3_VADD(x0, x5), 0.89997619f);
            x[6] = MINIMP3_VMUL_S(MINIMP3_VSUB(x4, x3), 1.30656302f);
            x[7] = MINIMP3_VMUL_S(MINIMP3_VSUB(xt, x7), 2.56291556f);
        }

        if (k > n - 3)
        {
#if MINIMP3_HAVE_SSE
#define MINIMP3_VSAVE2(i, v) _mm_storel_pi((__m64 *)(void*)&y[i*18], v)
#else /* MINIMP3_HAVE_SSE */
#define MINIMP3_VSAVE2(i, v) vst1_f32((float32_t *)&y[i*18],  vget_low_f32(v))
#endif /* MINIMP3_HAVE_SSE */
            for (i = 0; i < 7; i++, y += 4*18)
            {
                f4 s = MINIMP3_VADD(t[3][i], t[3][i + 1]);
                MINIMP3_VSAVE2(0, t[0][i]);
                MINIMP3_VSAVE2(1, MINIMP3_VADD(t[2][i], s));
                MINIMP3_VSAVE2(2, MINIMP3_VADD(t[1][i], t[1][i + 1]));
                MINIMP3_VSAVE2(3, MINIMP3_VADD(t[2][1 + i], s));
            }
            MINIMP3_VSAVE2(0, t[0][7]);
            MINIMP3_VSAVE2(1, MINIMP3_VADD(t[2][7], t[3][7]));
            MINIMP3_VSAVE2(2, t[1][7]);
            MINIMP3_VSAVE2(3, t[3][7]);
        } else
        {
#define MINIMP3_VSAVE4(i, v) MINIMP3_VSTORE(&y[i*18], v)
            for (i = 0; i < 7; i++, y += 4*18)
            {
                f4 s = MINIMP3_VADD(t[3][i], t[3][i + 1]);
                MINIMP3_VSAVE4(0, t[0][i]);
                MINIMP3_VSAVE4(1, MINIMP3_VADD(t[2][i], s));
                MINIMP3_VSAVE4(2, MINIMP3_VADD(t[1][i], t[1][i + 1]));
                MINIMP3_VSAVE4(3, MINIMP3_VADD(t[2][1 + i], s));
            }
            MINIMP3_VSAVE4(0, t[0][7]);
            MINIMP3_VSAVE4(1, MINIMP3_VADD(t[2][7], t[3][7]));
            MINIMP3_VSAVE4(2, t[1][7]);
            MINIMP3_VSAVE4(3, t[3][7]);
        }
    } else
#endif /* MINIMP3_HAVE_SIMD */
#ifdef MINIMP3_ONLY_SIMD
    {} /* for MINIMP3_HAVE_SIMD=1, MINIMP3_ONLY_SIMD=1 case we do not need non-intrinsic "else" branch */
#else /* MINIMP3_ONLY_SIMD */
    for (; k < n; k++)
    {
        float t[4][8], *x, *y = grbuf + k;

        for (x = t[0], i = 0; i < 8; i++, x++)
        {
            float x0 = y[i*18];
            float x1 = y[(15 - i)*18];
            float x2 = y[(16 + i)*18];
            float x3 = y[(31 - i)*18];
            float t0 = x0 + x3;
            float t1 = x1 + x2;
            float t2 = (x1 - x2)*g_sec[3*i + 0];
            float t3 = (x0 - x3)*g_sec[3*i + 1];
            x[0] = t0 + t1;
            x[8] = (t0 - t1)*g_sec[3*i + 2];
            x[16] = t3 + t2;
            x[24] = (t3 - t2)*g_sec[3*i + 2];
        }
        for (x = t[0], i = 0; i < 4; i++, x += 8)
        {
            float x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3], x4 = x[4], x5 = x[5], x6 = x[6], x7 = x[7], xt;
            xt = x0 - x7; x0 += x7;
            x7 = x1 - x6; x1 += x6;
            x6 = x2 - x5; x2 += x5;
            x5 = x3 - x4; x3 += x4;
            x4 = x0 - x3; x0 += x3;
            x3 = x1 - x2; x1 += x2;
            x[0] = x0 + x1;
            x[4] = (x0 - x1)*0.70710677f;
            x5 =  x5 + x6;
            x6 = (x6 + x7)*0.70710677f;
            x7 =  x7 + xt;
            x3 = (x3 + x4)*0.70710677f;
            x5 -= x7*0.198912367f;  /* rotate by PI/8 */
            x7 += x5*0.382683432f;
            x5 -= x7*0.198912367f;
            x0 = xt - x6; xt += x6;
            x[1] = (xt + x7)*0.50979561f;
            x[2] = (x4 + x3)*0.54119611f;
            x[3] = (x0 - x5)*0.60134488f;
            x[5] = (x0 + x5)*0.89997619f;
            x[6] = (x4 - x3)*1.30656302f;
            x[7] = (xt - x7)*2.56291556f;

        }
        for (i = 0; i < 7; i++, y += 4*18)
        {
            y[0*18] = t[0][i];
            y[1*18] = t[2][i] + t[3][i] + t[3][i + 1];
            y[2*18] = t[1][i] + t[1][i + 1];
            y[3*18] = t[2][i + 1] + t[3][i] + t[3][i + 1];
        }
        y[0*18] = t[0][7];
        y[1*18] = t[2][7] + t[3][7];
        y[2*18] = t[1][7];
        y[3*18] = t[3][7];
    }
#endif /* MINIMP3_ONLY_SIMD */
}

static float mp3d_scale_pcm(float sample)
{
    return sample*(1.f/32768.f);
}

static void mp3d_synth_pair(mp3d_sample_t *pcm, int nch, const float *z)
{
    float a;
    a  = (z[14*64] - z[    0]) * 29;
    a += (z[ 1*64] + z[13*64]) * 213;
    a += (z[12*64] - z[ 2*64]) * 459;
    a += (z[ 3*64] + z[11*64]) * 2037;
    a += (z[10*64] - z[ 4*64]) * 5153;
    a += (z[ 5*64] + z[ 9*64]) * 6574;
    a += (z[ 8*64] - z[ 6*64]) * 37489;
    a +=  z[ 7*64]             * 75038;
    pcm[0] = mp3d_scale_pcm(a);

    z += 2;
    a  = z[14*64] * 104;
    a += z[12*64] * 1567;
    a += z[10*64] * 9727;
    a += z[ 8*64] * 64019;
    a += z[ 6*64] * -9975;
    a += z[ 4*64] * -45;
    a += z[ 2*64] * 146;
    a += z[ 0*64] * -5;
    pcm[16*nch] = mp3d_scale_pcm(a);
}

static void mp3d_synth(float *xl, mp3d_sample_t *dstl, int nch, float *lins)
{
    int i;
    float *xr = xl + 576*(nch - 1);
    mp3d_sample_t *dstr = dstl + (nch - 1);

    static const float g_win[] = {
        -1,26,-31,208,218,401,-519,2063,2000,4788,-5517,7134,5959,35640,-39336,74992,
        -1,24,-35,202,222,347,-581,2080,1952,4425,-5879,7640,5288,33791,-41176,74856,
        -1,21,-38,196,225,294,-645,2087,1893,4063,-6237,8092,4561,31947,-43006,74630,
        -1,19,-41,190,227,244,-711,2085,1822,3705,-6589,8492,3776,30112,-44821,74313,
        -1,17,-45,183,228,197,-779,2075,1739,3351,-6935,8840,2935,28289,-46617,73908,
        -1,16,-49,176,228,153,-848,2057,1644,3004,-7271,9139,2037,26482,-48390,73415,
        -2,14,-53,169,227,111,-919,2032,1535,2663,-7597,9389,1082,24694,-50137,72835,
        -2,13,-58,161,224,72,-991,2001,1414,2330,-7910,9592,70,22929,-51853,72169,
        -2,11,-63,154,221,36,-1064,1962,1280,2006,-8209,9750,-998,21189,-53534,71420,
        -2,10,-68,147,215,2,-1137,1919,1131,1692,-8491,9863,-2122,19478,-55178,70590,
        -3,9,-73,139,208,-29,-1210,1870,970,1388,-8755,9935,-3300,17799,-56778,69679,
        -3,8,-79,132,200,-57,-1283,1817,794,1095,-8998,9966,-4533,16155,-58333,68692,
        -4,7,-85,125,189,-83,-1356,1759,605,814,-9219,9959,-5818,14548,-59838,67629,
        -4,7,-91,117,177,-106,-1428,1698,402,545,-9416,9916,-7154,12980,-61289,66494,
        -5,6,-97,111,163,-127,-1498,1634,185,288,-9585,9838,-8540,11455,-62684,65290
    };
    float *zlin = lins + 15*64;
    const float *w = g_win;

    zlin[4*15]     = xl[18*16];
    zlin[4*15 + 1] = xr[18*16];
    zlin[4*15 + 2] = xl[0];
    zlin[4*15 + 3] = xr[0];

    zlin[4*31]     = xl[1 + 18*16];
    zlin[4*31 + 1] = xr[1 + 18*16];
    zlin[4*31 + 2] = xl[1];
    zlin[4*31 + 3] = xr[1];

    mp3d_synth_pair(dstr, nch, lins + 4*15 + 1);
    mp3d_synth_pair(dstr + 32*nch, nch, lins + 4*15 + 64 + 1);
    mp3d_synth_pair(dstl, nch, lins + 4*15);
    mp3d_synth_pair(dstl + 32*nch, nch, lins + 4*15 + 64);

#if MINIMP3_HAVE_SIMD
    if (have_simd()) for (i = 14; i >= 0; i--)
    {
#define MINIMP3_VLOAD(k) f4 w0 = MINIMP3_VSET(*w++); f4 w1 = MINIMP3_VSET(*w++); f4 vz = MINIMP3_VLD(&zlin[4*i - 64*k]); f4 vy = MINIMP3_VLD(&zlin[4*i - 64*(15 - k)]);
#define MINIMP3_V0(k) { MINIMP3_VLOAD(k) b =                 MINIMP3_VADD(MINIMP3_VMUL(vz, w1), MINIMP3_VMUL(vy, w0)) ; a =                 MINIMP3_VSUB(MINIMP3_VMUL(vz, w0), MINIMP3_VMUL(vy, w1));  }
#define MINIMP3_V1(k) { MINIMP3_VLOAD(k) b = MINIMP3_VADD(b, MINIMP3_VADD(MINIMP3_VMUL(vz, w1), MINIMP3_VMUL(vy, w0))); a = MINIMP3_VADD(a, MINIMP3_VSUB(MINIMP3_VMUL(vz, w0), MINIMP3_VMUL(vy, w1))); }
#define MINIMP3_V2(k) { MINIMP3_VLOAD(k) b = MINIMP3_VADD(b, MINIMP3_VADD(MINIMP3_VMUL(vz, w1), MINIMP3_VMUL(vy, w0))); a = MINIMP3_VADD(a, MINIMP3_VSUB(MINIMP3_VMUL(vy, w1), MINIMP3_VMUL(vz, w0))); }
        f4 a, b;
        zlin[4*i]     = xl[18*(31 - i)];
        zlin[4*i + 1] = xr[18*(31 - i)];
        zlin[4*i + 2] = xl[1 + 18*(31 - i)];
        zlin[4*i + 3] = xr[1 + 18*(31 - i)];
        zlin[4*i + 64] = xl[1 + 18*(1 + i)];
        zlin[4*i + 64 + 1] = xr[1 + 18*(1 + i)];
        zlin[4*i - 64 + 2] = xl[18*(1 + i)];
        zlin[4*i - 64 + 3] = xr[18*(1 + i)];

        MINIMP3_V0(0) MINIMP3_V2(1) MINIMP3_V1(2) MINIMP3_V2(3) MINIMP3_V1(4) MINIMP3_V2(5) MINIMP3_V1(6) MINIMP3_V2(7)

        {
           #if _MSC_VER && (defined(__ARM_NEON) || defined(__aarch64__) || defined(_M_ARM64))
            const float scale_init[4] = { 1.0f/32768.0f, 1.0f/32768.0f, 1.0f/32768.0f, 1.0f/32768.0f };
            static const f4 g_scale = vld1q_f32 (scale_init);
           #else
            static const f4 g_scale = { 1.0f/32768.0f, 1.0f/32768.0f, 1.0f/32768.0f, 1.0f/32768.0f };
           #endif

            a = MINIMP3_VMUL(a, g_scale);
            b = MINIMP3_VMUL(b, g_scale);
#if MINIMP3_HAVE_SSE
            _mm_store_ss(dstr + (15 - i)*nch, _mm_shuffle_ps(a, a, _MM_SHUFFLE(1, 1, 1, 1)));
            _mm_store_ss(dstr + (17 + i)*nch, _mm_shuffle_ps(b, b, _MM_SHUFFLE(1, 1, 1, 1)));
            _mm_store_ss(dstl + (15 - i)*nch, _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 0)));
            _mm_store_ss(dstl + (17 + i)*nch, _mm_shuffle_ps(b, b, _MM_SHUFFLE(0, 0, 0, 0)));
            _mm_store_ss(dstr + (47 - i)*nch, _mm_shuffle_ps(a, a, _MM_SHUFFLE(3, 3, 3, 3)));
            _mm_store_ss(dstr + (49 + i)*nch, _mm_shuffle_ps(b, b, _MM_SHUFFLE(3, 3, 3, 3)));
            _mm_store_ss(dstl + (47 - i)*nch, _mm_shuffle_ps(a, a, _MM_SHUFFLE(2, 2, 2, 2)));
            _mm_store_ss(dstl + (49 + i)*nch, _mm_shuffle_ps(b, b, _MM_SHUFFLE(2, 2, 2, 2)));
#else /* MINIMP3_HAVE_SSE */
            vst1q_lane_f32(dstr + (15 - i)*nch, a, 1);
            vst1q_lane_f32(dstr + (17 + i)*nch, b, 1);
            vst1q_lane_f32(dstl + (15 - i)*nch, a, 0);
            vst1q_lane_f32(dstl + (17 + i)*nch, b, 0);
            vst1q_lane_f32(dstr + (47 - i)*nch, a, 3);
            vst1q_lane_f32(dstr + (49 + i)*nch, b, 3);
            vst1q_lane_f32(dstl + (47 - i)*nch, a, 2);
            vst1q_lane_f32(dstl + (49 + i)*nch, b, 2);
#endif /* MINIMP3_HAVE_SSE */
        }
    } else
#endif /* HAVE_SIMD */
#ifdef MINIMP3_ONLY_SIMD
    {} /* for HAVE_SIMD=1, MINIMP3_ONLY_SIMD=1 case we do not need non-intrinsic "else" branch */
#else /* MINIMP3_ONLY_SIMD */
    for (i = 14; i >= 0; i--)
    {
#define MINIMP3_LOAD(k) float w0 = *w++; float w1 = *w++; float *vz = &zlin[4*i - k*64]; float *vy = &zlin[4*i - (15 - k)*64];
#define MINIMP3_S0(k) { int j; MINIMP3_LOAD(k); for (j = 0; j < 4; j++) b[j]  = vz[j]*w1 + vy[j]*w0, a[j]  = vz[j]*w0 - vy[j]*w1; }
#define MINIMP3_S1(k) { int j; MINIMP3_LOAD(k); for (j = 0; j < 4; j++) b[j] += vz[j]*w1 + vy[j]*w0, a[j] += vz[j]*w0 - vy[j]*w1; }
#define MINIMP3_S2(k) { int j; MINIMP3_LOAD(k); for (j = 0; j < 4; j++) b[j] += vz[j]*w1 + vy[j]*w0, a[j] += vy[j]*w1 - vz[j]*w0; }
        float a[4], b[4];

        zlin[4*i]     = xl[18*(31 - i)];
        zlin[4*i + 1] = xr[18*(31 - i)];
        zlin[4*i + 2] = xl[1 + 18*(31 - i)];
        zlin[4*i + 3] = xr[1 + 18*(31 - i)];
        zlin[4*(i + 16)]   = xl[1 + 18*(1 + i)];
        zlin[4*(i + 16) + 1] = xr[1 + 18*(1 + i)];
        zlin[4*(i - 16) + 2] = xl[18*(1 + i)];
        zlin[4*(i - 16) + 3] = xr[18*(1 + i)];

        MINIMP3_S0(0) MINIMP3_S2(1) MINIMP3_S1(2) MINIMP3_S2(3) MINIMP3_S1(4) MINIMP3_S2(5) MINIMP3_S1(6) MINIMP3_S2(7)

        dstr[(15 - i)*nch] = mp3d_scale_pcm(a[1]);
        dstr[(17 + i)*nch] = mp3d_scale_pcm(b[1]);
        dstl[(15 - i)*nch] = mp3d_scale_pcm(a[0]);
        dstl[(17 + i)*nch] = mp3d_scale_pcm(b[0]);
        dstr[(47 - i)*nch] = mp3d_scale_pcm(a[3]);
        dstr[(49 + i)*nch] = mp3d_scale_pcm(b[3]);
        dstl[(47 - i)*nch] = mp3d_scale_pcm(a[2]);
        dstl[(49 + i)*nch] = mp3d_scale_pcm(b[2]);
    }
#endif /* MINIMP3_ONLY_SIMD */
}

static void mp3d_synth_granule(float *qmf_state, float *grbuf, int nbands, int nch, mp3d_sample_t *pcm, float *lins)
{
    int i;
    for (i = 0; i < nch; i++)
    {
        mp3d_DCT_II(grbuf + 576*i, nbands);
    }

    memcpy(lins, qmf_state, sizeof(float)*15*64);

    for (i = 0; i < nbands; i += 2)
    {
        mp3d_synth(grbuf + i, pcm + 32*nch*i, nch, lins + i*64);
    }
#ifndef MINIMP3_NONSTANDARD_BUT_LOGICAL
    if (nch == 1)
    {
        for (i = 0; i < 15*64; i += 2)
        {
            qmf_state[i] = lins[nbands*64 + i];
        }
    } else
#endif /* MINIMP3_NONSTANDARD_BUT_LOGICAL */
    {
        memcpy(qmf_state, lins + nbands*64, sizeof(float)*15*64);
    }
}

static int mp3d_match_frame(const uint8_t *hdr, int mp3_bytes, int frame_bytes)
{
    int i, nmatch;
    for (i = 0, nmatch = 0; nmatch < MAX_FRAME_SYNC_MATCHES; nmatch++)
    {
        i += hdr_frame_bytes(hdr + i, frame_bytes) + hdr_padding(hdr + i);
        if (i + HDR_SIZE > mp3_bytes)
            return nmatch > 0;
        if (!hdr_compare(hdr, hdr + i))
            return 0;
    }
    return 1;
}

static int mp3d_find_frame(const uint8_t *mp3, int mp3_bytes, int *free_format_bytes, int *ptr_frame_bytes)
{
    int i, k;
    for (i = 0; i < mp3_bytes - HDR_SIZE; i++, mp3++)
    {
        if (hdr_valid(mp3))
        {
            int frame_bytes = hdr_frame_bytes(mp3, *free_format_bytes);
            int frame_and_padding = frame_bytes + hdr_padding(mp3);

            for (k = HDR_SIZE; !frame_bytes && k < MAX_FREE_FORMAT_FRAME_SIZE && i + 2*k < mp3_bytes - HDR_SIZE; k++)
            {
                if (hdr_compare(mp3, mp3 + k))
                {
                    int fb = k - hdr_padding(mp3);
                    int nextfb = fb + hdr_padding(mp3 + k);
                    if (i + k + nextfb + HDR_SIZE > mp3_bytes || !hdr_compare(mp3, mp3 + k + nextfb))
                        continue;
                    frame_and_padding = k;
                    frame_bytes = fb;
                    *free_format_bytes = fb;
                }
            }
            if ((frame_bytes && i + frame_and_padding <= mp3_bytes &&
                mp3d_match_frame(mp3, mp3_bytes - i, frame_bytes)) ||
                (!i && frame_and_padding == mp3_bytes))
            {
                *ptr_frame_bytes = frame_and_padding;
                return i;
            }
            *free_format_bytes = 0;
        }
    }
    *ptr_frame_bytes = 0;
    return mp3_bytes;
}

static void mp3dec_init(mp3dec_t *dec)
{
    dec->header[0] = 0;
}

static int mp3dec_decode_frame(mp3dec_t *dec, const uint8_t *mp3, int mp3_bytes, mp3d_sample_t *pcm, mp3dec_frame_info_t *info)
{
    int i = 0, igr, frame_size = 0, success = 1;
    const uint8_t *hdr;
    bs_t bs_frame[1];
    mp3dec_scratch_t scratch;

    if (mp3_bytes > 4 && dec->header[0] == 0xff && hdr_compare(dec->header, mp3))
    {
        frame_size = hdr_frame_bytes(mp3, dec->free_format_bytes) + hdr_padding(mp3);
        if (frame_size != mp3_bytes && (frame_size + HDR_SIZE > mp3_bytes || !hdr_compare(mp3, mp3 + frame_size)))
        {
            frame_size = 0;
        }
    }
    if (!frame_size)
    {
        memset(dec, 0, sizeof(mp3dec_t));
        i = mp3d_find_frame(mp3, mp3_bytes, &dec->free_format_bytes, &frame_size);
        if (!frame_size || i + frame_size > mp3_bytes)
        {
            info->frame_bytes = i;
            return 0;
        }
    }

    hdr = mp3 + i;
    memcpy(dec->header, hdr, HDR_SIZE);
    info->frame_bytes = i + frame_size;
    info->frame_offset = i;
    info->channels = MINIMP3_HDR_IS_MONO(hdr) ? 1 : 2;
    info->hz = hdr_sample_rate_hz(hdr);
    info->layer = 4 - MINIMP3_HDR_GET_LAYER(hdr);
    info->bitrate_kbps = hdr_bitrate_kbps(hdr);

    if (!pcm)
    {
        return hdr_frame_samples(hdr);
    }

    bs_init(bs_frame, hdr + HDR_SIZE, frame_size - HDR_SIZE);
    if (MINIMP3_HDR_IS_CRC(hdr))
    {
        get_bits(bs_frame, 16);
    }

    if (info->layer == 3)
    {
        int main_data_begin = L3_read_side_info(bs_frame, scratch.gr_info, hdr);
        if (main_data_begin < 0 || bs_frame->pos > bs_frame->limit)
        {
            mp3dec_init(dec);
            return 0;
        }
        success = L3_restore_reservoir(dec, bs_frame, &scratch, main_data_begin);
        if (success)
        {
            for (igr = 0; igr < (MINIMP3_HDR_TEST_MPEG1(hdr) ? 2 : 1); igr++, pcm += 576*info->channels)
            {
                memset(scratch.grbuf[0], 0, 576*2*sizeof(float));
                L3_decode(dec, &scratch, scratch.gr_info + igr*info->channels, info->channels);
                mp3d_synth_granule(dec->qmf_state, scratch.grbuf[0], 18, info->channels, pcm, scratch.syn[0]);
            }
        }
        L3_save_reservoir(dec, &scratch);
    } else
    {
#ifdef MINIMP3_ONLY_MP3
        return 0;
#else /* MINIMP3_ONLY_MP3 */
        L12_scale_info sci[1];
        L12_read_scale_info(hdr, bs_frame, sci);

        memset(scratch.grbuf[0], 0, 576*2*sizeof(float));
        for (i = 0, igr = 0; igr < 3; igr++)
        {
            if (12 == (i += L12_dequantize_granule(scratch.grbuf[0] + i, bs_frame, sci, info->layer | 1)))
            {
                i = 0;
                L12_apply_scf_384(sci, sci->scf + igr, scratch.grbuf[0]);
                mp3d_synth_granule(dec->qmf_state, scratch.grbuf[0], 12, info->channels, pcm, scratch.syn[0]);
                memset(scratch.grbuf[0], 0, 576*2*sizeof(float));
                pcm += 384*info->channels;
            }
            if (bs_frame->pos > bs_frame->limit)
            {
                mp3dec_init(dec);
                return 0;
            }
        }
#endif /* MINIMP3_ONLY_MP3 */
    }
    return success*hdr_frame_samples(dec->header);
}

//==============================================================================
//==============================================================================
// minimp3_ex.h
//==============================================================================

/* flags for mp3dec_ex_open_* functions */
enum
{
    MP3D_SEEK_TO_BYTE    = 0,      /* mp3dec_ex_seek seeks to byte in stream */
    MP3D_SEEK_TO_SAMPLE  = 1,      /* mp3dec_ex_seek precisely seeks to sample using index (created during duration calculation scan or when mp3dec_ex_seek called) */
    MP3D_DO_NOT_SCAN     = 2,      /* do not scan whole stream for duration if vbrtag not found, mp3dec_ex_t::samples will be filled only if mp3dec_ex_t::vbr_tag_found == 1 */
#ifdef MINIMP3_ALLOW_MONO_STEREO_TRANSITION
    MP3D_ALLOW_MONO_STEREO_TRANSITION  = 4,
    MP3D_FLAGS_MASK = 7
#else
    MP3D_FLAGS_MASK = 3
#endif
};

/* compile-time config */
static constexpr int MINIMP3_PREDECODE_FRAMES = 2; /* frames to pre-decode and skip after seek (to fill internal structures) */
static constexpr int MINIMP3_IO_SIZE = (128*1024); /* io buffer size for streaming functions, must be greater than MINIMP3_BUF_SIZE */
static constexpr int MINIMP3_BUF_SIZE = (16*1024); /* buffer which can hold minimum 10 consecutive mp3 frames (~16KB) worst case */

/* return error codes */
static constexpr int MP3D_E_PARAM   = -1;
static constexpr int MP3D_E_MEMORY  = -2;
static constexpr int MP3D_E_IOERROR = -3;
static constexpr int MP3D_E_USER    = -4;  /* can be used to stop processing from callbacks without indicating specific error */
static constexpr int MP3D_E_DECODE  = -5;  /* decode error which can't be safely skipped, such as sample rate, layer and channels change */

typedef struct
{
    mp3d_sample_t *buffer;
    size_t samples; /* channels included, byte size = samples*sizeof(mp3d_sample_t) */
    int channels, hz, layer, avg_bitrate_kbps;
} mp3dec_file_info_t;

typedef struct
{
    const uint8_t *buffer;
    size_t size;
} mp3dec_map_info_t;

typedef struct
{
    uint64_t sample;
    uint64_t offset;
} mp3dec_frame_t;

typedef struct
{
    mp3dec_frame_t *frames;
    size_t num_frames, capacity;
} mp3dec_index_t;

typedef size_t (*MP3D_READ_CB)(void *buf, size_t size, void *user_data);
typedef int (*MP3D_SEEK_CB)(uint64_t position, void *user_data);

typedef struct
{
    MP3D_READ_CB read;
    void *read_data;
    MP3D_SEEK_CB seek;
    void *seek_data;
} mp3dec_io_t;

typedef struct
{
    mp3dec_t mp3d;
    mp3dec_map_info_t file;
    mp3dec_io_t *io;
    mp3dec_index_t index;
    uint64_t offset, samples, detected_samples, cur_sample, start_offset, end_offset;
    mp3dec_frame_info_t info;
    mp3d_sample_t buffer[MINIMP3_MAX_SAMPLES_PER_FRAME];
    size_t input_consumed, input_filled;
    int is_file, flags, vbr_tag_found, indexes_built;
    int free_format_bytes;
    int buffer_samples, buffer_consumed, to_skip, start_delay;
    int last_error;
} mp3dec_ex_t;

typedef int (*MP3D_ITERATE_CB)(void *user_data, const uint8_t *frame, int frame_size, int free_format_bytes, size_t buf_size, uint64_t offset, mp3dec_frame_info_t *info);
typedef int (*MP3D_PROGRESS_CB)(void *user_data, size_t file_size, uint64_t offset, mp3dec_frame_info_t *info);

/* iterate through frames */
static int mp3dec_iterate_buf(const uint8_t *buf, size_t buf_size, MP3D_ITERATE_CB callback, void *user_data);
static int mp3dec_iterate_cb(mp3dec_io_t *io, uint8_t *buf, size_t buf_size, MP3D_ITERATE_CB callback, void *user_data);
/* streaming decoder with seeking capability */
static int mp3dec_ex_open_cb(mp3dec_ex_t *dec, mp3dec_io_t *io, int flags);

static void mp3dec_skip_id3v1(const uint8_t *buf, size_t *pbuf_size)
{
    size_t buf_size = *pbuf_size;
#ifndef MINIMP3_NOSKIP_ID3V1
    if (buf_size >= 128 && !memcmp(buf + buf_size - 128, "TAG", 3))
    {
        buf_size -= 128;
        if (buf_size >= 227 && !memcmp(buf + buf_size - 227, "TAG+", 4))
            buf_size -= 227;
    }
#endif
#ifndef MINIMP3_NOSKIP_APEV2
    if (buf_size > 32 && !memcmp(buf + buf_size - 32, "APETAGEX", 8))
    {
        buf_size -= 32;
        const uint8_t *tag = buf + buf_size + 8 + 4;
        uint32_t tag_size = (uint32_t)(tag[3] << 24) | (tag[2] << 16) | (tag[1] << 8) | tag[0];
        if (buf_size >= tag_size)
            buf_size -= tag_size;
    }
#endif
    *pbuf_size = buf_size;
}

static size_t mp3dec_skip_id3v2(const uint8_t *buf, size_t buf_size)
{
#define MINIMP3_ID3_DETECT_SIZE 10
#ifndef MINIMP3_NOSKIP_ID3V2
    if (buf_size >= MINIMP3_ID3_DETECT_SIZE && !memcmp(buf, "ID3", 3) && !((buf[5] & 15) || (buf[6] & 0x80) || (buf[7] & 0x80) || (buf[8] & 0x80) || (buf[9] & 0x80)))
    {
        size_t id3v2size = (((buf[6] & 0x7f) << 21) | ((buf[7] & 0x7f) << 14) | ((buf[8] & 0x7f) << 7) | (buf[9] & 0x7f)) + 10;
        if ((buf[5] & 16))
            id3v2size += 10; /* footer */
        return id3v2size;
    }
#endif
    return 0;
}

static void mp3dec_skip_id3(const uint8_t **pbuf, size_t *pbuf_size)
{
    uint8_t *buf = (uint8_t *)(*pbuf);
    size_t buf_size = *pbuf_size;
    size_t id3v2size = mp3dec_skip_id3v2(buf, buf_size);
    if (id3v2size)
    {
        if (id3v2size >= buf_size)
            id3v2size = buf_size;
        buf      += id3v2size;
        buf_size -= id3v2size;
    }
    mp3dec_skip_id3v1(buf, &buf_size);
    *pbuf = (const uint8_t *)buf;
    *pbuf_size = buf_size;
}

static int mp3dec_check_vbrtag(const uint8_t *frame, int frame_size, uint32_t *frames, int *delay, int *padding)
{
    static const char g_xing_tag[4] = { 'X', 'i', 'n', 'g' };
    static const char g_info_tag[4] = { 'I', 'n', 'f', 'o' };

    enum
    {
        FRAMES_FLAG     = 1,
        BYTES_FLAG      = 2,
        TOC_FLAG        = 4,
        VBR_SCALE_FLAG  = 8
    };

    /* Side info offsets after header:
    /                Mono  Stereo
    /  MPEG1          17     32
    /  MPEG2 & 2.5     9     17*/
    bs_t bs[1];
    L3_gr_info_t gr_info[4];
    bs_init(bs, frame + HDR_SIZE, frame_size - HDR_SIZE);
    if (MINIMP3_HDR_IS_CRC(frame))
        get_bits(bs, 16);
    if (L3_read_side_info(bs, gr_info, frame) < 0)
        return 0; /* side info corrupted */

    const uint8_t *tag = frame + HDR_SIZE + bs->pos/8;
    if (memcmp(g_xing_tag, tag, 4) && memcmp(g_info_tag, tag, 4))
        return 0;
    int flags = tag[7];
    if (!((flags & FRAMES_FLAG)))
        return -1;
    tag += 8;
    *frames = (uint32_t)(tag[0] << 24) | (tag[1] << 16) | (tag[2] << 8) | tag[3];
    tag += 4;
    if (flags & BYTES_FLAG)
        tag += 4;
    if (flags & TOC_FLAG)
        tag += 100;
    if (flags & VBR_SCALE_FLAG)
        tag += 4;
    *delay = *padding = 0;
    if (*tag)
    {   /* extension, LAME, Lavc, etc. Should be the same structure. */
        tag += 21;
        if (tag - frame + 14 >= frame_size)
            return 0;
        *delay   = ((tag[0] << 4) | (tag[1] >> 4)) + (528 + 1);
        *padding = (((tag[1] & 0xF) << 8) | tag[2]) - (528 + 1);
    }
    return 1;
}

int mp3dec_iterate_buf(const uint8_t *buf, size_t buf_size, MP3D_ITERATE_CB callback, void *user_data)
{
    const uint8_t *orig_buf = buf;
    if (!buf || (size_t)-1 == buf_size || !callback)
        return MP3D_E_PARAM;
    /* skip id3 */
    mp3dec_skip_id3(&buf, &buf_size);
    if (!buf_size)
        return 0;
    mp3dec_frame_info_t frame_info;
    memset(&frame_info, 0, sizeof(frame_info));
    do
    {
        int free_format_bytes = 0, frame_size = 0, ret;
        int i = mp3d_find_frame(buf, buf_size, &free_format_bytes, &frame_size);
        buf      += i;
        buf_size -= i;
        if (i && !frame_size)
            continue;
        if (!frame_size)
            break;
        const uint8_t *hdr = buf;
        frame_info.channels = MINIMP3_HDR_IS_MONO(hdr) ? 1 : 2;
        frame_info.hz = hdr_sample_rate_hz(hdr);
        frame_info.layer = 4 - MINIMP3_HDR_GET_LAYER(hdr);
        frame_info.bitrate_kbps = hdr_bitrate_kbps(hdr);
        frame_info.frame_bytes = frame_size;

        if (callback)
        {
            if ((ret = callback(user_data, hdr, frame_size, free_format_bytes, buf_size, hdr - orig_buf, &frame_info)))
                return ret;
        }
        buf      += frame_size;
        buf_size -= frame_size;
    } while (1);
    return 0;
}

int mp3dec_iterate_cb(mp3dec_io_t *io, uint8_t *buf, size_t buf_size, MP3D_ITERATE_CB callback, void *user_data)
{
    if (!io || !buf || (size_t)-1 == buf_size || buf_size < MINIMP3_BUF_SIZE || !callback)
        return MP3D_E_PARAM;
    size_t filled = io->read(buf, MINIMP3_ID3_DETECT_SIZE, io->read_data), consumed = 0;
    uint64_t readed = 0;
    mp3dec_frame_info_t frame_info;
    int eof = 0;
    memset(&frame_info, 0, sizeof(frame_info));
    if (filled > MINIMP3_ID3_DETECT_SIZE)
        return MP3D_E_IOERROR;
    if (MINIMP3_ID3_DETECT_SIZE != filled)
        return 0;
    size_t id3v2size = mp3dec_skip_id3v2(buf, filled);
    if (id3v2size)
    {
        if (io->seek(id3v2size, io->seek_data))
            return MP3D_E_IOERROR;
        filled = io->read(buf, buf_size, io->read_data);
        if (filled > buf_size)
            return MP3D_E_IOERROR;
        readed += id3v2size;
    } else
    {
        size_t readed = io->read(buf + MINIMP3_ID3_DETECT_SIZE, buf_size - MINIMP3_ID3_DETECT_SIZE, io->read_data);
        if (readed > (buf_size - MINIMP3_ID3_DETECT_SIZE))
            return MP3D_E_IOERROR;
        filled += readed;
    }
    if (filled < MINIMP3_BUF_SIZE)
        mp3dec_skip_id3v1(buf, &filled);
    do
    {
        int free_format_bytes = 0, frame_size = 0, ret;
        int i = mp3d_find_frame(buf + consumed, filled - consumed, &free_format_bytes, &frame_size);
        if (i && !frame_size)
        {
            consumed += i;
            continue;
        }
        if (!frame_size)
            break;
        const uint8_t *hdr = buf + consumed + i;
        frame_info.channels = MINIMP3_HDR_IS_MONO(hdr) ? 1 : 2;
        frame_info.hz = hdr_sample_rate_hz(hdr);
        frame_info.layer = 4 - MINIMP3_HDR_GET_LAYER(hdr);
        frame_info.bitrate_kbps = hdr_bitrate_kbps(hdr);
        frame_info.frame_bytes = frame_size;

        readed += i;
        if (callback)
        {
            if ((ret = callback(user_data, hdr, frame_size, free_format_bytes, filled - consumed, readed, &frame_info)))
                return ret;
        }
        readed += frame_size;
        consumed += i + frame_size;
        if (!eof && filled - consumed < MINIMP3_BUF_SIZE)
        {   /* keep minimum 10 consecutive mp3 frames (~16KB) worst case */
            memmove(buf, buf + consumed, filled - consumed);
            filled -= consumed;
            consumed = 0;
            size_t readed = io->read(buf + filled, buf_size - filled, io->read_data);
            if (readed > (buf_size - filled))
                return MP3D_E_IOERROR;
            if (readed != (buf_size - filled))
                eof = 1;
            filled += readed;
            if (eof)
                mp3dec_skip_id3v1(buf, &filled);
        }
    } while (1);
    return 0;
}

static int mp3dec_load_index(void *user_data, const uint8_t *frame, int frame_size, int free_format_bytes, size_t buf_size, uint64_t offset, mp3dec_frame_info_t *info)
{
    mp3dec_frame_t *idx_frame;
    mp3dec_ex_t *dec = (mp3dec_ex_t *)user_data;
    if (!dec->index.frames && !dec->start_offset)
    {   /* detect VBR tag and try to avoid full scan */
        uint32_t frames;
        int delay, padding;
        dec->info = *info;
        dec->start_offset = dec->offset = offset;
        dec->end_offset   = offset + buf_size;
        dec->free_format_bytes = free_format_bytes; /* should not change */
        if (3 == dec->info.layer)
        {
            int ret = mp3dec_check_vbrtag(frame, frame_size, &frames, &delay, &padding);
            if (ret)
                dec->start_offset = dec->offset = offset + frame_size;
            if (ret > 0)
            {
                padding *= info->channels;
                dec->start_delay = dec->to_skip = delay*info->channels;
                dec->samples = hdr_frame_samples(frame)*info->channels*(uint64_t)frames;
                if (dec->samples >= (uint64_t)dec->start_delay)
                    dec->samples -= dec->start_delay;
                if (padding > 0 && dec->samples >= (uint64_t)padding)
                    dec->samples -= padding;
                dec->detected_samples = dec->samples;
                dec->vbr_tag_found = 1;
                return MP3D_E_USER;
            } else if (ret < 0)
                return 0;
        }
    }
    if (dec->flags & MP3D_DO_NOT_SCAN)
        return MP3D_E_USER;
    if (dec->index.num_frames + 1 > dec->index.capacity)
    {
        if (!dec->index.capacity)
            dec->index.capacity = 4096;
        else
            dec->index.capacity *= 2;
        mp3dec_frame_t *alloc_buf = (mp3dec_frame_t *)realloc((void*)dec->index.frames, sizeof(mp3dec_frame_t)*dec->index.capacity);
        if (!alloc_buf)
            return MP3D_E_MEMORY;
        dec->index.frames = alloc_buf;
    }
    idx_frame = &dec->index.frames[dec->index.num_frames++];
    idx_frame->offset = offset;
    idx_frame->sample = dec->samples;
    if (!dec->buffer_samples && dec->index.num_frames < 256)
    {   /* for some cutted mp3 frames, bit-reservoir not filled and decoding can't be started from first frames */
        /* try to decode up to 255 first frames till samples starts to decode */
        dec->buffer_samples = mp3dec_decode_frame(&dec->mp3d, frame, std::min (buf_size, (size_t) 0x7fffffff), dec->buffer, info);
        dec->samples += dec->buffer_samples*info->channels;
    } else
        dec->samples += hdr_frame_samples(frame)*info->channels;
    return 0;
}

#ifndef MINIMP3_SEEK_IDX_LINEAR_SEARCH
static size_t mp3dec_idx_binary_search(mp3dec_index_t *idx, uint64_t position)
{
    size_t end = idx->num_frames, start = 0, index = 0;
    while (start <= end)
    {
        size_t mid = (start + end) / 2;
        if (idx->frames[mid].sample >= position)
        {   /* move left side. */
            if (idx->frames[mid].sample == position)
                return mid;
            end = mid - 1;
        }  else
        {   /* move to right side */
            index = mid;
            start = mid + 1;
            if (start == idx->num_frames)
                break;
        }
    }
    return index;
}
#endif

static int mp3dec_ex_seek(mp3dec_ex_t *dec, uint64_t position)
{
    size_t i;
    if (!dec)
        return MP3D_E_PARAM;
    if (!(dec->flags & MP3D_SEEK_TO_SAMPLE))
    {
        if (dec->io)
        {
            dec->offset = position;
        } else
        {
            dec->offset = std::min (position, (uint64_t) dec->file.size);
        }
        dec->cur_sample = 0;
        goto do_exit;
    }
    dec->cur_sample = position;
    position += dec->start_delay;
    if (0 == position)
    {   /* optimize seek to zero, no index needed */
seek_zero:
        dec->offset  = dec->start_offset;
        dec->to_skip = 0;
        goto do_exit;
    }
    if (!dec->indexes_built)
    {   /* no index created yet (vbr tag used to calculate track length or MP3D_DO_NOT_SCAN open flag used) */
        dec->indexes_built = 1;
        dec->samples = 0;
        dec->buffer_samples = 0;
        if (dec->io)
        {
            if (dec->io->seek(dec->start_offset, dec->io->seek_data))
                return MP3D_E_IOERROR;
            int ret = mp3dec_iterate_cb(dec->io, (uint8_t *)dec->file.buffer, dec->file.size, mp3dec_load_index, dec);
            if (ret && MP3D_E_USER != ret)
                return ret;
        } else
        {
            int ret = mp3dec_iterate_buf(dec->file.buffer + dec->start_offset, dec->file.size - dec->start_offset, mp3dec_load_index, dec);
            if (ret && MP3D_E_USER != ret)
                return ret;
        }
        for (i = 0; i < dec->index.num_frames; i++)
            dec->index.frames[i].offset += dec->start_offset;
        dec->samples = dec->detected_samples;
    }
    if (!dec->index.frames)
        goto seek_zero; /* no frames in file - seek to zero */
#ifdef MINIMP3_SEEK_IDX_LINEAR_SEARCH
    for (i = 0; i < dec->index.num_frames; i++)
    {
        if (dec->index.frames[i].sample >= position)
            break;
    }
#else
    i = mp3dec_idx_binary_search(&dec->index, position);
#endif
    if (i)
    {
        int to_fill_bytes = 511;
        int skip_frames = MINIMP3_PREDECODE_FRAMES
#ifdef MINIMP3_SEEK_IDX_LINEAR_SEARCH
         + ((dec->index.frames[i].sample == position) ? 0 : 1)
#endif
        ;
        i -= std::min (i, (size_t)skip_frames);
        if (3 == dec->info.layer)
        {
            while (i && to_fill_bytes)
            {   /* make sure bit-reservoir is filled when we start decoding */
                bs_t bs[1];
                L3_gr_info_t gr_info[4];
                int frame_bytes, frame_size;
                const uint8_t *hdr;
                if (dec->io)
                {
                    hdr = dec->file.buffer;
                    if (dec->io->seek(dec->index.frames[i - 1].offset, dec->io->seek_data))
                        return MP3D_E_IOERROR;
                    size_t readed = dec->io->read((uint8_t *)hdr, HDR_SIZE, dec->io->read_data);
                    if (readed != HDR_SIZE)
                        return MP3D_E_IOERROR;
                    frame_size = hdr_frame_bytes(hdr, dec->free_format_bytes) + hdr_padding(hdr);
                    readed = dec->io->read((uint8_t *)hdr + HDR_SIZE, frame_size - HDR_SIZE, dec->io->read_data);
                    if (readed != (size_t)(frame_size - HDR_SIZE))
                        return MP3D_E_IOERROR;
                    bs_init(bs, hdr + HDR_SIZE, frame_size - HDR_SIZE);
                } else
                {
                    hdr = dec->file.buffer + dec->index.frames[i - 1].offset;
                    frame_size = hdr_frame_bytes(hdr, dec->free_format_bytes) + hdr_padding(hdr);
                    bs_init(bs, hdr + HDR_SIZE, frame_size - HDR_SIZE);
                }
                if (MINIMP3_HDR_IS_CRC(hdr))
                    get_bits(bs, 16);
                i--;
                if (L3_read_side_info(bs, gr_info, hdr) < 0)
                    break; /* frame not decodable, we can start from here */
                frame_bytes = (bs->limit - bs->pos)/8;
                to_fill_bytes -= std::min (to_fill_bytes, frame_bytes);
            }
        }
    }
    dec->offset = dec->index.frames[i].offset;
    dec->to_skip = position - dec->index.frames[i].sample;
    while ((i + 1) < dec->index.num_frames && !dec->index.frames[i].sample && !dec->index.frames[i + 1].sample)
    {   /* skip not decodable first frames */
        const uint8_t *hdr;
        if (dec->io)
        {
            hdr = dec->file.buffer;
            if (dec->io->seek(dec->index.frames[i].offset, dec->io->seek_data))
                return MP3D_E_IOERROR;
            size_t readed = dec->io->read((uint8_t *)hdr, HDR_SIZE, dec->io->read_data);
            if (readed != HDR_SIZE)
                return MP3D_E_IOERROR;
        } else
            hdr = dec->file.buffer + dec->index.frames[i].offset;
        dec->to_skip += hdr_frame_samples(hdr)*dec->info.channels;
        i++;
    }
do_exit:
    if (dec->io)
    {
        if (dec->io->seek(dec->offset, dec->io->seek_data))
            return MP3D_E_IOERROR;
    }
    dec->buffer_samples  = 0;
    dec->buffer_consumed = 0;
    dec->input_consumed  = 0;
    dec->input_filled    = 0;
    dec->last_error      = 0;
    mp3dec_init(&dec->mp3d);
    return 0;
}

static size_t mp3dec_ex_read_frame(mp3dec_ex_t *dec, mp3d_sample_t **buf, mp3dec_frame_info_t *frame_info, size_t max_samples)
{
    if (!dec || !buf || !frame_info)
    {
        if (dec)
            dec->last_error = MP3D_E_PARAM;
        return 0;
    }
    if (dec->detected_samples && dec->cur_sample >= dec->detected_samples)
        return 0; /* at end of stream */
    if (dec->last_error)
        return 0; /* error eof state, seek can reset it */
    *buf = NULL;
    uint64_t end_offset = dec->end_offset ? dec->end_offset : dec->file.size;
    int eof = 0;
    while (dec->buffer_consumed == dec->buffer_samples)
    {
        const uint8_t *dec_buf;
        if (dec->io)
        {
            if (!eof && (dec->input_filled - dec->input_consumed) < MINIMP3_BUF_SIZE)
            {   /* keep minimum 10 consecutive mp3 frames (~16KB) worst case */
                memmove((uint8_t*)dec->file.buffer, (uint8_t*)dec->file.buffer + dec->input_consumed, dec->input_filled - dec->input_consumed);
                dec->input_filled -= dec->input_consumed;
                dec->input_consumed = 0;
                size_t readed = dec->io->read((uint8_t*)dec->file.buffer + dec->input_filled, dec->file.size - dec->input_filled, dec->io->read_data);
                if (readed > (dec->file.size - dec->input_filled))
                {
                    dec->last_error = MP3D_E_IOERROR;
                    readed = 0;
                }
                if (readed != (dec->file.size - dec->input_filled))
                    eof = 1;
                dec->input_filled += readed;
                if (eof)
                    mp3dec_skip_id3v1((uint8_t*)dec->file.buffer, &dec->input_filled);
            }
            dec_buf = dec->file.buffer + dec->input_consumed;
            if (!(dec->input_filled - dec->input_consumed))
                return 0;
            dec->buffer_samples = mp3dec_decode_frame(&dec->mp3d, dec_buf, dec->input_filled - dec->input_consumed, dec->buffer, frame_info);
            dec->input_consumed += frame_info->frame_bytes;
        } else
        {
            dec_buf = dec->file.buffer + dec->offset;
            uint64_t buf_size = end_offset - dec->offset;
            if (!buf_size)
                return 0;
            dec->buffer_samples = mp3dec_decode_frame(&dec->mp3d, dec_buf, std::min (buf_size, (uint64_t) 0x7fffffff), dec->buffer, frame_info);
        }
        dec->buffer_consumed = 0;
        if (dec->info.hz != frame_info->hz || dec->info.layer != frame_info->layer)
        {
return_e_decode:
            dec->last_error = MP3D_E_DECODE;
            return 0;
        }
        if (dec->buffer_samples)
        {
            dec->buffer_samples *= frame_info->channels;
            if (dec->to_skip)
            {
                size_t skip = std::min (dec->buffer_samples, dec->to_skip);
                dec->buffer_consumed += skip;
                dec->to_skip -= skip;
            }
            if (
#ifdef MINIMP3_ALLOW_MONO_STEREO_TRANSITION
                !(dec->flags & MP3D_ALLOW_MONO_STEREO_TRANSITION) &&
#endif
                dec->buffer_consumed != dec->buffer_samples && dec->info.channels != frame_info->channels)
            {
                goto return_e_decode;
            }
        } else if (dec->to_skip)
        {   /* In mp3 decoding not always can start decode from any frame because of bit reservoir,
               count skip samples for such frames */
            int frame_samples = hdr_frame_samples(dec_buf)*frame_info->channels;
            dec->to_skip -= std::min (frame_samples, dec->to_skip);
        }
        dec->offset += frame_info->frame_bytes;
    }
    size_t out_samples = std::min ((size_t)(dec->buffer_samples - dec->buffer_consumed), max_samples);
    if (dec->detected_samples)
    {   /* count decoded samples to properly cut padding */
        if (dec->cur_sample + out_samples >= dec->detected_samples)
            out_samples = dec->detected_samples - dec->cur_sample;
    }
    dec->cur_sample += out_samples;
    *buf = dec->buffer + dec->buffer_consumed;
    dec->buffer_consumed += out_samples;
    return out_samples;
}

int mp3dec_ex_open_cb(mp3dec_ex_t *dec, mp3dec_io_t *io, int flags)
{
    if (!dec || !io || (flags & (~MP3D_FLAGS_MASK)))
        return MP3D_E_PARAM;
    memset(dec, 0, sizeof(*dec));

    dec->file.size = MINIMP3_IO_SIZE;
    dec->file.buffer = (const uint8_t*)malloc(dec->file.size);
    if (!dec->file.buffer)
        return MP3D_E_MEMORY;

    dec->flags = flags;
    dec->io = io;
    mp3dec_init(&dec->mp3d);
    if (io->seek(0, io->seek_data))
        return MP3D_E_IOERROR;
    int ret = mp3dec_iterate_cb(io, (uint8_t *)dec->file.buffer, dec->file.size, mp3dec_load_index, dec);
    if (ret && MP3D_E_USER != ret)
        return ret;
    if (dec->io->seek(dec->start_offset, dec->io->seek_data))
        return MP3D_E_IOERROR;
    mp3dec_init(&dec->mp3d);
    dec->buffer_samples = 0;
    dec->indexes_built = !(dec->vbr_tag_found || (flags & MP3D_DO_NOT_SCAN));
    dec->flags &= (~MP3D_DO_NOT_SCAN);
    return 0;
}

static void mp3dec_ex_close(mp3dec_ex_t *dec)
{
    if (dec->io && dec->file.buffer)
        free((void*)dec->file.buffer);

    if (dec->index.frames)
        free(dec->index.frames);
    memset(dec, 0, sizeof(*dec));
}


} // namespace minimp3

#include "../platform/choc_ReenableAllWarnings.h"

//==============================================================================
//==============================================================================
//
// Finally back to choc code now....
//
//==============================================================================
//==============================================================================

struct MP3AudioFileFormat::Implementation
{
    struct FormatException {};
    [[noreturn]] static void throwFormatException()    { throw FormatException(); }

    //==============================================================================
    struct MP3Reader : public AudioFileReader
    {
        MP3Reader (std::shared_ptr<std::istream> s)  : stream (std::move (s))
        {
            io.read = readCallback;
            io.seek = seekCallback;
            io.read_data = stream.get();
            io.seek_data = stream.get();
        }

        ~MP3Reader() override
        {
            minimp3::mp3dec_ex_close (std::addressof (decoder));
        }

        bool initialise()
        {
            stream->exceptions (std::istream::failbit);

            if (minimp3::mp3dec_ex_open_cb (std::addressof (decoder), std::addressof (io),
                                            minimp3::MP3D_SEEK_TO_SAMPLE) != 0)
                return false;

            if (! fillCache (0))
                return false;

            if (frame.bitrate_kbps == 0 || frame.channels == 0 || frame.hz == 0)
                return false;

            properties.formatName  = "MP3";
            properties.numChannels = static_cast<uint32_t> (frame.channels);
            properties.numFrames   = decoder.samples / properties.numChannels;
            properties.sampleRate  = static_cast<double> (frame.hz);
            properties.bitDepth    = BitDepth::int16;
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
            if (frameIndex >= cacheStart && frameIndex < cacheStart + cacheFrames.getNumFrames())
            {
                auto startInCache = static_cast<uint32_t> (frameIndex - cacheStart);
                auto numFrames = std::min (destBuffer.getNumFrames(), static_cast<uint32_t> (cacheStart + cacheFrames.getNumFrames() - frameIndex));

                copyRemappingChannels (destBuffer.getStart (numFrames),
                                       cacheFrames.getFrameRange ({ startInCache, startInCache + numFrames }));

                return numFrames;
            }

            return 0;
        }

        bool fillCache (uint64_t frameIndex)
        {
            if (nextReadPosition != frameIndex)
                if (minimp3::mp3dec_ex_seek (std::addressof (decoder), frameIndex * properties.numChannels) != 0)
                    return false;

            float* framesOut = nullptr;
            auto numSamples = minimp3::mp3dec_ex_read_frame (std::addressof (decoder),
                                                             std::addressof (framesOut),
                                                             std::addressof (frame),
                                                             1024);
            if (numSamples == 0 || frame.channels <= 0)
                return false;

            auto numFrames = numSamples / static_cast<uint32_t> (frame.channels);
            cacheFrames = choc::buffer::createInterleavedView (framesOut, frame.channels, numFrames);
            cacheStart = frameIndex;
            nextReadPosition = frameIndex + numFrames;
            return true;
        }

        static size_t readCallback (void* buf, size_t size, void* user_data)
        {
            auto& s = *static_cast<std::istream*> (user_data);

            try
            {
                s.read (static_cast<char*> (buf), static_cast<std::streamsize> (size));
                return size;
            }
            catch (...) {}

            if (s.eof())
            {
                s.clear();
                return (size_t) s.gcount();
            }

            return 0;
        }

        static int seekCallback (uint64_t position, void* user_data)
        {
            auto& s = *static_cast<std::istream*> (user_data);

            try
            {
                s.seekg (static_cast<std::istream::off_type> (position), std::ios_base::beg);
                return 0;
            }
            catch (...) {}

            s.clear();
            return -1;
        }

        std::shared_ptr<std::istream> stream;
        AudioFileProperties properties;

        minimp3::mp3dec_ex_t decoder;
        minimp3::mp3dec_io_t io;
        minimp3::mp3dec_frame_info_t frame;

        uint64_t cacheStart = 0, nextReadPosition = 0;
        choc::buffer::InterleavedView<float> cacheFrames;
    };
};

//==============================================================================
inline std::string MP3AudioFileFormat::getFileSuffixes()                   { return "mp3"; }

inline std::vector<double> MP3AudioFileFormat::getSupportedSampleRates()   { return {}; }
inline uint32_t MP3AudioFileFormat::getMaximumNumChannels()                { return 2; }
inline std::vector<BitDepth> MP3AudioFileFormat::getSupportedBitDepths()   { return { BitDepth::int16 }; }
inline std::vector<std::string> MP3AudioFileFormat::getQualityLevels()     { return {}; }
inline bool MP3AudioFileFormat::supportsWriting() const                    { return false; }

inline std::unique_ptr<AudioFileReader> MP3AudioFileFormat::createReader (std::shared_ptr<std::istream> s)
{
    if (s == nullptr || s->fail())
        return {};

    auto r = std::make_unique<typename Implementation::MP3Reader> (std::move (s));

    if (r->initialise())
        return r;

    return {};
}

inline std::unique_ptr<AudioFileWriter> MP3AudioFileFormat::createWriter (std::shared_ptr<std::ostream>, AudioFileProperties)
{
    return {};
}

} // namespace choc::audio

#endif // CHOC_AUDIOFILEFORMAT_MP3_HEADER_INCLUDED
