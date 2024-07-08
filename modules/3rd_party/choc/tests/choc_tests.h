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

#ifndef CHOC_TESTS_HEADER_INCLUDED
#define CHOC_TESTS_HEADER_INCLUDED

#include "../containers/choc_ZipFile.h"
#include "../platform/choc_FileWatcher.h"
#include "../threading/choc_ThreadSafeFunctor.h"
#include "../threading/choc_TaskThread.h"
#include "../gui/choc_MessageLoop.h"
#include "../gui/choc_WebView.h"
#include "../text/choc_OpenSourceLicenseList.h"
#include "../audio/choc_AudioFileFormat_MP3.h"
#include "../audio/choc_AudioFileFormat_FLAC.h"
#include "../audio/choc_AudioFileFormat_Ogg.h"
#include "../audio/choc_AudioFileFormat_WAV.h"
#include "../memory/choc_VariableLengthEncoding.h"
#include "../containers/choc_NonAllocatingStableSort.h"
#include "../platform/choc_DetectDebugger.h"
#include "../platform/choc_Platform.h"
#include "../threading/choc_SpinLock.h"
#include "../platform/choc_DynamicLibrary.h"
#include "../memory/choc_Endianness.h"
#include "../text/choc_CodePrinter.h"
#include "../text/choc_FloatToString.h"
#include "../text/choc_HTML.h"
#include "../text/choc_JSON.h"
#include "../text/choc_StringUtilities.h"
#include "../text/choc_UTF8.h"
#include "../text/choc_TextTable.h"
#include "../text/choc_Files.h"
#include "../text/choc_Wildcard.h"
#include "../network/choc_MIMETypes.h"
#include "../memory/choc_Base64.h"
#include "../memory/choc_xxHash.h"
#include "../math/choc_MathHelpers.h"
#include "../containers/choc_COM.h"
#include "../containers/choc_DirtyList.h"
#include "../containers/choc_Span.h"
#include "../containers/choc_Value.h"
#include "../containers/choc_MultipleReaderMultipleWriterFIFO.h"
#include "../containers/choc_SingleReaderMultipleWriterFIFO.h"
#include "../containers/choc_SingleReaderSingleWriterFIFO.h"
#include "../containers/choc_VariableSizeFIFO.h"
#include "../containers/choc_SmallVector.h"
#include "../memory/choc_PoolAllocator.h"
#include "../memory/choc_ObjectPointer.h"
#include "../memory/choc_ObjectReference.h"
#include "../memory/choc_AlignedMemoryBlock.h"
#include "../audio/choc_MIDI.h"
#include "../audio/choc_MIDIFile.h"
#include "../audio/choc_Oscillators.h"
#include "../audio/choc_SampleBuffers.h"
#include "../audio/choc_AudioSampleData.h"
#include "../audio/choc_SincInterpolator.h"
#include "../audio/choc_SampleBufferUtilities.h"
#include "../audio/choc_AudioMIDIBlockDispatcher.h"
#include "../javascript/choc_javascript.h"
#include "../javascript/choc_javascript_Timer.h"
#include "../javascript/choc_javascript_Console.h"

#if CHOC_ENABLE_HTTP_SERVER_TEST
 #include "../network/choc_HTTPServer.h"
#endif

#include "choc_UnitTest.h"
#include <future>

/**
    To keep things simpole for users, I've just shoved all the tests for everything into this
    one dependency-free header, and provided one function call (`choc::test::runAllTests) that
    tests everything.

    The idea is that you can then simply include this header and call runAllTests() somewhere in
    your own test build, to make sure that everything is working as expected in your project.

    At some point the library will probably grow to a size where this needs to be refactored into
    smaller modules and done in a more sophisticated way, but we're not there yet!
*/
namespace choc_unit_tests
{

/// Just create a TestProgress and pass it to this function to run all the
/// tests. The TestProgress object contains a callback that will be used
/// to log its progress.
bool runAllTests (choc::test::TestProgress&);

static void runTestOnMessageThread (std::function<void(const std::function<void()>&)> setup,
                                    std::function<void()> handleResult = {})
{
    std::atomic_bool finished { false };
    std::function<void()> setFinished = [&] { finished = true; };

    choc::messageloop::postMessage ([&]
    {
        setup (setFinished);
    });

    while (! finished)
        std::this_thread::yield();

    if (handleResult)
    {
        finished = false;

        choc::messageloop::postMessage ([&]
        {
            handleResult();
            finished = true;
        });

        while (! finished)
            std::this_thread::yield();
    }
}


inline void testPlatform (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (Platform);

    {
        CHOC_TEST (DetectDebugger)
        CHOC_EXPECT_FALSE (choc::isDebuggerActive());
    }

    {
        CHOC_TEST (Endianness)

        {
            union { uint32_t i; char c[4]; } n;
            n.i = 0x01020304;

           if constexpr (CHOC_LITTLE_ENDIAN)
            CHOC_EXPECT_EQ (n.c[0], 4);

           if constexpr (CHOC_BIG_ENDIAN)
            CHOC_EXPECT_EQ (n.c[0], 1);
        }

        auto a = 0x0102030405060708ull;
        uint8_t buffer[16];

        CHOC_EXPECT_EQ (choc::memory::swapByteOrder ((uint16_t) 0x1122u), 0x2211u);
        CHOC_EXPECT_EQ (choc::memory::swapByteOrder ((uint32_t) 0x11223344u), 0x44332211u);
        CHOC_EXPECT_EQ (choc::memory::swapByteOrder ((uint64_t) 0x1122334455667788ull), 0x8877665544332211ull);

        choc::memory::writeNativeEndian (buffer, a);
        CHOC_EXPECT_EQ (a, choc::memory::readNativeEndian<decltype(a)> (buffer));

        choc::memory::writeLittleEndian (buffer, a);
        CHOC_EXPECT_EQ (buffer[0], 8);
        CHOC_EXPECT_EQ (buffer[1], 7);
        CHOC_EXPECT_EQ (buffer[6], 2);
        CHOC_EXPECT_EQ (buffer[7], 1);
        CHOC_EXPECT_EQ (choc::memory::readLittleEndian<decltype(a)> (buffer), a);

        choc::memory::writeBigEndian (buffer, a);
        CHOC_EXPECT_EQ (buffer[0], 1);
        CHOC_EXPECT_EQ (buffer[1], 2);
        CHOC_EXPECT_EQ (buffer[6], 7);
        CHOC_EXPECT_EQ (buffer[7], 8);
        CHOC_EXPECT_EQ (choc::memory::readBigEndian<decltype(a)> (buffer), a);

        CHOC_EXPECT_EQ (choc::memory::bit_cast<uint64_t> (1.0), 0x3ff0000000000000ull);
        CHOC_EXPECT_EQ (choc::memory::bit_cast<double> (0x3ff0000000000000ull), 1.0);
        CHOC_EXPECT_EQ (choc::memory::bit_cast<uint32_t> (1.0f), 0x3f800000u);
        CHOC_EXPECT_EQ (choc::memory::bit_cast<float> (0x3f800000u), 1.0f);
    }

    {
        CHOC_TEST (ClearBitCount)

        CHOC_EXPECT_EQ (choc::math::countUpperClearBits ((uint32_t) 1), 31u);
        CHOC_EXPECT_EQ (choc::math::countUpperClearBits ((uint64_t) 1), 63u);
        CHOC_EXPECT_EQ (choc::math::countUpperClearBits ((uint32_t) 0x700000), 9u);
        CHOC_EXPECT_EQ (choc::math::countUpperClearBits ((uint32_t) 0xffffffff), 0u);
        CHOC_EXPECT_EQ (choc::math::countUpperClearBits ((uint64_t) 0x700000), 32u + 9u);
        CHOC_EXPECT_EQ (choc::math::countUpperClearBits ((uint64_t) 0xffffffffull), 32u);
        CHOC_EXPECT_EQ (choc::math::countUpperClearBits ((uint64_t) 0x70000000000000ull), 9u);
        CHOC_EXPECT_EQ (choc::math::countUpperClearBits ((uint64_t) 0xffffffff00000000ull), 0u);
    }

    {
        CHOC_TEST (VariableLengthIntEncoding)
        using namespace choc::integer_encoding;
        CHOC_EXPECT_EQ (3, zigzagEncode (-2));

        for (int64_t i : { (int64_t) 0, (int64_t) 1, (int64_t) -1, (int64_t) 3, (int64_t) -3, (int64_t) 65535, (int64_t) -65535, (int64_t) (1ll << 31), (int64_t) 0x8000000000000000ull })
            CHOC_EXPECT_EQ (i, zigzagDecode (zigzagEncode (i));)

        for (int32_t i : { (int32_t) 0, (int32_t) 1, (int32_t) -1, (int32_t) 3, (int32_t) -3, (int32_t) 65535, (int32_t) -65535, (int32_t) (1u << 31), (int32_t) 0x80000000u })
            CHOC_EXPECT_EQ (i, zigzagDecode (zigzagEncode (i));)

        {
            char buffer[16];
            CHOC_EXPECT_EQ (1u, encodeVariableLengthInt (buffer, zigzagEncode (-1)));
        }

        for (int64_t i : { (int64_t) 0, (int64_t) 1, (int64_t) -1, (int64_t) 3, (int64_t) -3, (int64_t) 65535, (int64_t) -65535, (int64_t) (1ll << 31), (int64_t) 0x8000000000000000ull })
        {
            char buffer[16];

            {
                auto encodedSize = encodeVariableLengthInt (buffer, i);
                size_t bytesUsed;
                CHOC_EXPECT_EQ (i, decodeVariableLengthInt<decltype(i)> (buffer, encodedSize, bytesUsed))
                CHOC_EXPECT_EQ (bytesUsed, encodedSize);
            }

            {
                auto encodedSize = encodeVariableLengthInt (buffer, zigzagEncode (i));
                size_t bytesUsed;
                CHOC_EXPECT_EQ (i, zigzagDecode (decodeVariableLengthInt<decltype(i)> (buffer, encodedSize, bytesUsed)))
                CHOC_EXPECT_EQ (bytesUsed, encodedSize);
            }
        }
    }

    {
        CHOC_TEST (DynamicLibrary)

       #if CHOC_WINDOWS
        if (auto kernel = choc::file::DynamicLibrary ("Kernel32.dll"))
        {
            CHOC_EXPECT_TRUE (kernel);
            auto k2 = std::move (kernel);
            CHOC_EXPECT_TRUE (k2.findFunction ("GetSystemTime") != nullptr);
            CHOC_EXPECT_TRUE (k2.findFunction ("XYZ") == nullptr);
        }
        else
        {
            CHOC_FAIL ("Failed to load kernel32");
        }
       #endif

       #if ! CHOC_LINUX // (can't seem to get this to link in github actions)
        choc::file::DynamicLibrary nope ("foo123487654");
        CHOC_EXPECT_TRUE (! nope);
        CHOC_EXPECT_TRUE (nope.findFunction ("xyz") == nullptr);
       #endif
    }
}

//==============================================================================
inline void testContainerUtils (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (Containers);

    {
        CHOC_TEST (Span)

        std::vector<int>  v { 1, 2, 3 };
        int a[] = { 1, 2, 3 };

        CHOC_EXPECT_TRUE (choc::span<int>().empty());
        CHOC_EXPECT_FALSE (choc::span<int> (a).empty());
        CHOC_EXPECT_TRUE (choc::span<int> (v).size() == 3);
        CHOC_EXPECT_TRUE (choc::span<int> (v).tail().size() == 2);
        CHOC_EXPECT_TRUE (choc::span<int> (v).createVector().size() == 3);
        CHOC_EXPECT_TRUE (choc::span<int> (v) == choc::span<int> (a));
    }

    {
        CHOC_TEST (SmartPointers)

        struct Foo { int x; };
        using P = choc::ObjectPointer<Foo>;
        Foo f1, f2;
        P a;
        CHOC_EXPECT_TRUE (a == nullptr && ! a);
        P b = f1;
        CHOC_EXPECT_TRUE (b != nullptr && !! b);
        CHOC_EXPECT_TRUE (a != b);
        CHOC_EXPECT_TRUE (b == f1);
        CHOC_EXPECT_TRUE (std::addressof (*b) == std::addressof (f1));
        b = f2;
        CHOC_EXPECT_TRUE (std::addressof (*b) == std::addressof (f2));

        using R = choc::ObjectReference<Foo>;
        R c (f1);
        CHOC_EXPECT_TRUE (c == f1);
        c = f2;
        CHOC_EXPECT_TRUE (c == f2);
    }

    {
        CHOC_TEST (AlignedMemoryBlock)

        choc::AlignedMemoryBlock<32> m (345);
        CHOC_EXPECT_TRUE (m.data() != nullptr);
        CHOC_EXPECT_TRUE ((reinterpret_cast<size_t> (m.data()) & 31u) == 0);
        CHOC_EXPECT_EQ (m.size(), 345u);
        m.resize (1);
        CHOC_EXPECT_TRUE ((reinterpret_cast<size_t> (m.data()) & 31u) == 0);
        CHOC_EXPECT_EQ (m.size(), 1u);
        m.reset();
        CHOC_EXPECT_TRUE (m.size() == 0 && m.data() == nullptr);
    }
}

inline void testStringUtilities (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (Strings);

    {
        CHOC_TEST (FloatToString)

        CHOC_EXPECT_EQ ("1.0",                      choc::text::floatToString (1.0));
        CHOC_EXPECT_EQ ("1.0",                      choc::text::floatToString (1.0));
        CHOC_EXPECT_EQ ("1.1",                      choc::text::floatToString (1.1));
        CHOC_EXPECT_EQ ("1.1",                      choc::text::floatToString (1.1));
        CHOC_EXPECT_EQ ("0.0",                      choc::text::floatToString (1.123e-6, 4));
        CHOC_EXPECT_EQ ("1.0",                      choc::text::floatToString (1.0, 1));
        CHOC_EXPECT_EQ ("1.0",                      choc::text::floatToString (1.0, 2));
        CHOC_EXPECT_EQ ("1.1",                      choc::text::floatToString (1.1, 2));
        CHOC_EXPECT_EQ ("1.12",                     choc::text::floatToString (1.126, 2));
        CHOC_EXPECT_EQ ("0.0012",                   choc::text::floatToString (1.23e-3, 4));
        CHOC_EXPECT_EQ ("0.0",                      choc::text::floatToString (0.0f));
        CHOC_EXPECT_EQ ("0.0",                      choc::text::floatToString (0.0f));
        CHOC_EXPECT_EQ ("-0.0",                     choc::text::floatToString (-1.0f / std::numeric_limits<float>::infinity()));
        CHOC_EXPECT_EQ ("-0.0",                     choc::text::floatToString (-1.0 / std::numeric_limits<double>::infinity()));
        CHOC_EXPECT_EQ ("inf",                      choc::text::floatToString (std::numeric_limits<float>::infinity()));
        CHOC_EXPECT_EQ ("-inf",                     choc::text::floatToString (-std::numeric_limits<float>::infinity()));
        CHOC_EXPECT_EQ ("inf",                      choc::text::floatToString (std::numeric_limits<double>::infinity()));
        CHOC_EXPECT_EQ ("-inf",                     choc::text::floatToString (-std::numeric_limits<double>::infinity()));
        CHOC_EXPECT_EQ ("nan",                      choc::text::floatToString (std::numeric_limits<float>::quiet_NaN()));
        CHOC_EXPECT_EQ ("-nan",                     choc::text::floatToString (-std::numeric_limits<float>::quiet_NaN()));
        CHOC_EXPECT_EQ ("nan",                      choc::text::floatToString (std::numeric_limits<double>::quiet_NaN()));
        CHOC_EXPECT_EQ ("-nan",                     choc::text::floatToString (-std::numeric_limits<double>::quiet_NaN()));
        CHOC_EXPECT_EQ ("3.4028235e38",             choc::text::floatToString (std::numeric_limits<float>::max()));
        CHOC_EXPECT_EQ ("1.1754944e-38",            choc::text::floatToString (std::numeric_limits<float>::min()));
        CHOC_EXPECT_EQ ("-3.4028235e38",            choc::text::floatToString (std::numeric_limits<float>::lowest()));
        CHOC_EXPECT_EQ ("1.7976931348623157e308",   choc::text::floatToString (std::numeric_limits<double>::max()));
        CHOC_EXPECT_EQ ("2.2250738585072014e-308",  choc::text::floatToString (std::numeric_limits<double>::min()));
        CHOC_EXPECT_EQ ("-1.7976931348623157e308",  choc::text::floatToString (std::numeric_limits<double>::lowest()));
    }

    {
        CHOC_TEST (HexConversion)

        CHOC_EXPECT_EQ ("1",                choc::text::createHexString (1));
        CHOC_EXPECT_EQ ("100",              choc::text::createHexString (256));
        CHOC_EXPECT_EQ ("ffff",             choc::text::createHexString (65535));
        CHOC_EXPECT_EQ ("fffffffffffffffe", choc::text::createHexString (-2ll));
        CHOC_EXPECT_EQ ("00000001",         choc::text::createHexString (1, 8));
        CHOC_EXPECT_EQ ("00000100",         choc::text::createHexString (256, 8));
        CHOC_EXPECT_EQ ("0000ffff",         choc::text::createHexString (65535, 8));
        CHOC_EXPECT_EQ ("fffffffffffffffe", choc::text::createHexString (-2ll, 8));
    }

    {
        CHOC_TEST (Trimming)

        CHOC_EXPECT_EQ ("test", choc::text::trim ("test"));
        CHOC_EXPECT_EQ ("test", choc::text::trim (" test"));
        CHOC_EXPECT_EQ ("test", choc::text::trim ("  test"));
        CHOC_EXPECT_EQ ("test", choc::text::trim ("test  "));
        CHOC_EXPECT_EQ ("test", choc::text::trim ("test "));
        CHOC_EXPECT_EQ ("test", choc::text::trim ("  test  "));
        CHOC_EXPECT_EQ ("",     choc::text::trim ("  "));
        CHOC_EXPECT_EQ ("",     choc::text::trim (" "));
        CHOC_EXPECT_EQ ("",     choc::text::trim (""));

        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view ("test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view (" test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view ("  test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view ("test  ")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view ("test ")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string_view ("  test  ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string_view ("  ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string_view (" ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string_view ("")));

        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string ("test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string (" test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string ("  test")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string ("test  ")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string ("test ")));
        CHOC_EXPECT_EQ ("test", choc::text::trim (std::string ("  test  ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string ("  ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string (" ")));
        CHOC_EXPECT_EQ ("",     choc::text::trim (std::string ("")));

        CHOC_EXPECT_EQ ("test",   choc::text::trimStart ("test"));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (" test"));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart ("  test"));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart ("test  "));
        CHOC_EXPECT_EQ ("test ",  choc::text::trimStart ("test "));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart ("  test  "));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart ("  "));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (" "));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (""));

        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string_view ("test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string_view (" test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string_view ("  test")));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart (std::string_view ("test  ")));
        CHOC_EXPECT_EQ ("test ",  choc::text::trimStart (std::string_view ("test ")));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart (std::string_view ("  test  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string_view ("  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string_view (" ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string_view ("")));

        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string ("test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string (" test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimStart (std::string ("  test")));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart (std::string ("test  ")));
        CHOC_EXPECT_EQ ("test ",  choc::text::trimStart (std::string ("test ")));
        CHOC_EXPECT_EQ ("test  ", choc::text::trimStart (std::string ("  test  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string ("  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string (" ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimStart (std::string ("")));

        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd ("test"));
        CHOC_EXPECT_EQ (" test",  choc::text::trimEnd (" test"));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd ("  test"));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd ("test  "));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd ("test "));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd ("  test  "));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd ("  "));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (" "));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (""));

        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string_view ("test")));
        CHOC_EXPECT_EQ (" test",  choc::text::trimEnd (std::string_view (" test")));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd (std::string_view ("  test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string_view ("test  ")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string_view ("test ")));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd (std::string_view ("  test  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string_view ("  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string_view (" ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string_view ("")));

        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string ("test")));
        CHOC_EXPECT_EQ (" test",  choc::text::trimEnd (std::string (" test")));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd (std::string ("  test")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string ("test  ")));
        CHOC_EXPECT_EQ ("test",   choc::text::trimEnd (std::string ("test ")));
        CHOC_EXPECT_EQ ("  test", choc::text::trimEnd (std::string ("  test  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string ("  ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string (" ")));
        CHOC_EXPECT_EQ ("",       choc::text::trimEnd (std::string ("")));
    }

    {
        CHOC_TEST (EndsWith)

        CHOC_EXPECT_TRUE  (choc::text::endsWith ("test", "t"));
        CHOC_EXPECT_TRUE  (choc::text::endsWith ("test", "st"));
        CHOC_EXPECT_TRUE  (choc::text::endsWith ("test", "est"));
        CHOC_EXPECT_TRUE  (choc::text::endsWith ("test", "test"));
        CHOC_EXPECT_FALSE (choc::text::endsWith ("test", "x"));
        CHOC_EXPECT_FALSE (choc::text::endsWith ("test", "ttest"));
        CHOC_EXPECT_TRUE  (choc::text::endsWith ("test", ""));
    }

    {
        CHOC_TEST (Durations)

        CHOC_EXPECT_EQ ("0 sec", choc::text::getDurationDescription (std::chrono::milliseconds (0)));
        CHOC_EXPECT_EQ ("999 microseconds", choc::text::getDurationDescription (std::chrono::microseconds (999)));
        CHOC_EXPECT_EQ ("1 microsecond", choc::text::getDurationDescription (std::chrono::microseconds (1)));
        CHOC_EXPECT_EQ ("-1 microsecond", choc::text::getDurationDescription (std::chrono::microseconds (-1)));
        CHOC_EXPECT_EQ ("1 ms", choc::text::getDurationDescription (std::chrono::milliseconds (1)));
        CHOC_EXPECT_EQ ("-1 ms", choc::text::getDurationDescription (std::chrono::milliseconds (-1)));
        CHOC_EXPECT_EQ ("2 ms", choc::text::getDurationDescription (std::chrono::milliseconds (2)));
        CHOC_EXPECT_EQ ("1.5 ms", choc::text::getDurationDescription (std::chrono::microseconds (1495)));
        CHOC_EXPECT_EQ ("2 ms", choc::text::getDurationDescription (std::chrono::microseconds (1995)));
        CHOC_EXPECT_EQ ("1 sec", choc::text::getDurationDescription (std::chrono::seconds (1)));
        CHOC_EXPECT_EQ ("2 sec", choc::text::getDurationDescription (std::chrono::seconds (2)));
        CHOC_EXPECT_EQ ("2.3 sec", choc::text::getDurationDescription (std::chrono::milliseconds (2300)));
        CHOC_EXPECT_EQ ("2.31 sec", choc::text::getDurationDescription (std::chrono::milliseconds (2310)));
        CHOC_EXPECT_EQ ("2.31 sec", choc::text::getDurationDescription (std::chrono::milliseconds (2314)));
        CHOC_EXPECT_EQ ("2.31 sec", choc::text::getDurationDescription (std::chrono::milliseconds (2305)));
        CHOC_EXPECT_EQ ("1 min 3 sec", choc::text::getDurationDescription (std::chrono::milliseconds (63100)));
        CHOC_EXPECT_EQ ("2 min 3 sec", choc::text::getDurationDescription (std::chrono::milliseconds (123100)));
        CHOC_EXPECT_EQ ("1 hour 2 min", choc::text::getDurationDescription (std::chrono::seconds (3726)));
        CHOC_EXPECT_EQ ("-1 hour 2 min", choc::text::getDurationDescription (std::chrono::seconds (-3726)));
    }

    {
        CHOC_TEST (BytesSizes)

        CHOC_EXPECT_EQ ("0 bytes", choc::text::getByteSizeDescription (0));
        CHOC_EXPECT_EQ ("1 byte", choc::text::getByteSizeDescription (1));
        CHOC_EXPECT_EQ ("2 bytes", choc::text::getByteSizeDescription (2));
        CHOC_EXPECT_EQ ("1 KB", choc::text::getByteSizeDescription (1024));
        CHOC_EXPECT_EQ ("1.1 KB", choc::text::getByteSizeDescription (1024 + 100));
        CHOC_EXPECT_EQ ("1 MB", choc::text::getByteSizeDescription (1024 * 1024));
        CHOC_EXPECT_EQ ("1.2 MB", choc::text::getByteSizeDescription ((1024 + 200) * 1024));
        CHOC_EXPECT_EQ ("1 GB", choc::text::getByteSizeDescription (1024 * 1024 * 1024));
        CHOC_EXPECT_EQ ("1.3 GB", choc::text::getByteSizeDescription ((1024 + 300) * 1024 * 1024));
    }

    {
        CHOC_TEST (UTF8)

        auto text = "line1\xd7\x90\n\xcf\x88line2\nli\xe1\xb4\x81ne3\nline4\xe1\xb4\xa8";
        choc::text::UTF8Pointer p (text);

        CHOC_EXPECT_TRUE (choc::text::findInvalidUTF8Data (text, std::string_view (text).length()) == nullptr);
        CHOC_EXPECT_EQ (2u, choc::text::findLineAndColumn (p, p.find ("ine2")).line);
        CHOC_EXPECT_EQ (3u, choc::text::findLineAndColumn (p, p.find ("ine2")).column);
        CHOC_EXPECT_TRUE (p.find ("ine4").findStartOfLine (p).startsWith ("line4"));

        CHOC_EXPECT_EQ (0x12345u, choc::text::createUnicodeFromHighAndLowSurrogates (choc::text::splitCodePointIntoSurrogatePair (0x12345u)));
    }

    {
        CHOC_TEST (TextTable)

        choc::text::TextTable table;
        table << "1" << "234" << "5";
        table.newRow();
        table << "" << "2345" << "x" << "y";
        table.newRow();
        table << "2345";

        CHOC_EXPECT_EQ (table.getNumRows(), 3u);
        CHOC_EXPECT_EQ (table.getNumColumns(), 4u);
        CHOC_EXPECT_EQ (table.toString ("<", ";", ">"), std::string ("<1   ;234 ;5; ><    ;2345;x;y><2345;    ; ; >"));
    }

    {
        CHOC_TEST (Base64)

        CHOC_EXPECT_EQ (choc::base64::encodeToString (std::string_view ("")), "");
        CHOC_EXPECT_EQ (choc::base64::encodeToString (std::string_view ("f")), "Zg==");
        CHOC_EXPECT_EQ (choc::base64::encodeToString (std::string_view ("fo")), "Zm8=");
        CHOC_EXPECT_EQ (choc::base64::encodeToString (std::string_view ("foo")), "Zm9v");
        CHOC_EXPECT_EQ (choc::base64::encodeToString (std::string_view ("foob")), "Zm9vYg==");
        CHOC_EXPECT_EQ (choc::base64::encodeToString (std::string_view ("fooba")), "Zm9vYmE=");
        CHOC_EXPECT_EQ (choc::base64::encodeToString (std::string_view ("foobar")), "Zm9vYmFy");

        std::vector<uint8_t> data;

        auto testRoundTrip = [&]
        {
            auto base64 = choc::base64::encodeToString (data);
            std::vector<uint8_t> decoded;
            choc::base64::decodeToContainer (decoded, base64);
            return decoded == data;
        };

        CHOC_EXPECT_TRUE (testRoundTrip());

        for (uint32_t start = 0; start < 256; ++start)
        {
            data.clear();
            uint32_t byte = start;

            for (int i = 0; i < 80; ++i)
            {
                data.push_back ((uint8_t) byte);
                CHOC_EXPECT_TRUE (testRoundTrip());
                byte = (byte * 7 + 3);
            }
        }
    }

    {
        CHOC_TEST (URIEncoding)
        CHOC_EXPECT_EQ (choc::text::percentEncodeURI ("ABC://``\\123-abc~-xyz"), "ABC%3a%2f%2f%60%60%5c123-abc~-xyz");
    }

    {
        CHOC_TEST (OpenSourceLicenseList)
        CHOC_EXPECT_TRUE (choc::text::OpenSourceLicenseList::getAllLicenseText().length() > 1000);
    }

    {
        CHOC_TEST (xxHash)

        struct Test
        {
            std::string_view input;
            uint32_t seed;
            uint32_t hash32;
            uint64_t hash64;
        };

        constexpr Test tests[] = {
            { "", 0, 0x2cc5d05u, 0xef46db3751d8e999u },
            { "C", 1, 0xdfbce743u, 0xbb9cff53b7445da8u },
            { "EK", 2, 0x956c6231u, 0x2057a2db0cbfa023u },
            { "KIO", 3, 0x8269d336u, 0x878204a3ab2b0cbdu },
            { "IKUW", 4, 0x43388fd5u, 0x99f454dbf4f8d5e9u },
            { "KUWQS", 5, 0x5c2d65bu, 0xbdc87b641d37787cu },
            { "USQ_][", 6, 0x3b5a98eeu, 0xa47f857228b0fc74u },
            { "SQ_][Yg", 7, 0xf68b9027u, 0xb99cd3405c695045u },
            { "QSUWikmo", 8, 0x6da56924u, 0x85fa50a6adfb0378u },
            { "SUWikmoac", 9, 0xf79c62f5u, 0x19b8a3ab91ee1344u },
            { "UkiomcageI", 10, 0xd498b3eau, 0xd6ebe7ba5c03e1d8u },
            { "kiomcageIGM", 11, 0x87739234u, 0xb58219f701ed8e8au },
            { "ikegacKMGICE", 12, 0xc74af22fu, 0xc16a0d06b5705ad0u },
            { "kegacKMGICEqA", 13, 0x31a02c0cu, 0xe334f0db41934ce9u },
            { "ecaMKIGECAq][Y", 14, 0xf8ccea89u, 0x724918b9b637bf89u },
            { "caMKIGECAq][YWU", 15, 0x5443abd7u, 0xdd6386ed9cb516du },
            { "acegikmo_acegikm", 16, 0x84ffaa2du, 0x9a67ab4327b988e7u },
            { "ekioma_ecigmkQOUSY", 18, 0x10d40da9u, 0x50224b0a592395f0u },
            { "kioma_ecigmkQOUSYW]", 19, 0x34494b81u, 0x85e378fea54379b7u },
            { "kce_akmgiSUOQ[]WYQSMO", 21, 0xd33b77a2u, 0x28fb408b710db96eu },
            { "ca_mkigUSQO][YWSQOM[YW", 22, 0x588fe96au, 0x6318fc8a76594d7bu },
            { "_aceWY[]OQSUUWY[MOQSEGIK", 24, 0x4eb5e87u, 0x670d63f8b5a60384u },
            { "aceWY[]OQSUUWY[MOQSEGIKoq", 25, 0x727bfaeau, 0xe1f5af9d6511b6dfu },
            { "YW][QOUSWU[YOMSQGEKIqoCAECI", 27, 0x7abed4e7u, 0x6664317746e019fau },
            { "WYSUOQY[UWQSMOIKEGACoqGICEqA", 28, 0xa4c7f508u, 0xdb11256faac2467cu },
            { "QO[YWUSQOMKIGECAqoIGECAqomkigec", 31, 0x5d5e2778u, 0x41424284e2253e86u },
            { "OQSUWY[]_acegikmKMOQSUWY[]_acegi", 32, 0x8cf29158u, 0x1f9a864d4c2ef30eu },
            { "QSUWY[]_acegikmKMOQSUWY[]_acegi]_", 33, 0x8dfda7e5u, 0x5fd725e54d9d5608u },
            { "YW][a_ecigmkMKQOUSYW][a_ecig_]cagek", 35, 0x842c29f5u, 0x2929a36df1365d1fu },
            { "WYce_akmgiOQKMWYSU_a[]giceac]_ikegqA", 36, 0x4b3d4fbau, 0x975dcc8958c35dd9u },
            { "ca_mkigQOMKYWUSa_][igecca_]kigeAqomIGE", 38, 0xac870e96u, 0xd6893eeacf4cf3edu },
            { "a_mkigQOMKYWUSa_][igecca_]kigeAqomIGEC_", 39, 0xec9bab74u, 0xd5f77f507395788u },
            { "aceSUWYKMOQcegi[]_aegik]_acCEGImoqAacegY[", 41, 0xd07660fau, 0x9ecdfb63c489228u },
            { "cUSYWMKQOecig][a_geki_]caECIGomAqcage[Y_]A", 42, 0xeab316f2u, 0xd38549656406504du },
            { "USYWMKQOecig][a_geki_]caECIGomAqcage[Y_]AqE", 43, 0x9c95496du, 0xaa7aa3068f97dab8u },
            { "MKigeca_][kigeca_]IGECAqomgeca_][YECAqomkiGECAq", 47, 0xf7176037u, 0x1cbd705f76c815e9u },
            { "KMOQSUWYmoqACEGI]_acegikikmoqACEY[]_acegIKMOQSUW", 48, 0x4bb111deu, 0xdb5ad15a8c64851eu },
            { "MOQSUWYmoqACEGI]_acegikikmoqACEY[]_acegIKMOQSUWkm", 49, 0x354d9f5u, 0x778add70ef71bd44u },
            { "OUSYWomAqECIG_]cagekikiomAqEC[Y_]cageKIOMSQWUmkqoC", 50, 0x2588e2d2u, 0xf9341494a5e0e8dcu },
            { "UqAmoGICEac]_ikegmoikCEqA]_Y[egacMOIKUWQSoqkmEGACIKEG", 53, 0x4a2a2ec3u, 0xa4fc2925334eb991u },
            { "omIGECca_]kigeomkiECAq_][YgecaOMKIWUSQqomkGECAKIGESQOMm", 55, 0x15e3bc0cu, 0x1983a0fd074d4547u },
            { "qgeki_]caAqECkiomcage[Y_]SQWUKIOMCAGEmkqoOMSQGEKIqoCAigmka", 58, 0x8fe33c30u, 0x226cac49fa9e3291u },
            { "gac]_CEqAmoikegac]_Y[UWQSMOIKEGACoqkmQSMOIKEGACoqkmgice_a[]WY", 61, 0xf36eac3du, 0x9f39b71c967657f5u },
            { "]_acegikmoqACEGIKMOQSUWY[]_acegiUWY[]_acegikmoqACEGIKMOQSUWY[]_a", 64, 0xb1cca1fu, 0x6a1b54e459c4f910u },
            { "_acegikmoqACEGIKMOQSUWY[]_acegiUWY[]_acegikmoqACEGIKMOQSUWY[]_aGI", 65, 0x1954b8d2u, 0xa7bc3d9bb019ff88u },
            { "gekiomAqECIGMKQOUSYW][a_ecigWU[Y_]cagekiomAqECIGMKQOUSYW][a_IGMKQOU", 67, 0x1b6c90c9u, 0xe4e33001a3a5ce1au },
            { "qomIGECQOMKYWUSa_][igec[YWUca_]kigeAqomIGECQOMKYWUSa_][MKIGUSQO][YWeca", 70, 0xba086ea7u, 0x8826868d435e6f08u },
            { "omIGECQOMKYWUSa_][igec[YWUca_]kigeAqomIGECQOMKYWUSa_][MKIGUSQO][YWeca_m", 71, 0xbaba8050u, 0x4a74507e42327b09u },
            { "qUSYWMKQOecig][a__]caWU[YomAqgekiMKQOECIG][a_USYWQOUSIGMKa_ecYW][qoCAigmkO", 74, 0x3334060bu, 0xef9a38af81f74e2du },
        };

        for (auto t : tests)
        {
            choc::hash::xxHash32 h32 (t.seed);
            h32.addInput (t.input.data(), t.input.length());
            CHOC_EXPECT_EQ (t.hash32, h32.getHash());

            choc::hash::xxHash64 h64 (t.seed);
            h64.addInput (t.input.data(), t.input.length());
            CHOC_EXPECT_EQ (t.hash64, h64.getHash());
        }
    }

    {
        CHOC_TEST (MIMETypes)
        CHOC_EXPECT_EQ (choc::network::getMIMETypeFromFilename ("dfsdfsg/sdfgds.txt"), "text/plain");
        CHOC_EXPECT_EQ (choc::network::getMIMETypeFromFilename (".ogg"), "audio/ogg");
        CHOC_EXPECT_EQ (choc::network::getMIMETypeFromFilename (".."), "application/text");
        CHOC_EXPECT_EQ (choc::network::getMIMETypeFromFilename ({}, "blah"), "blah");
        CHOC_EXPECT_EQ (choc::network::getMIMETypeFromFilename (".ogg?foo...x"), "audio/ogg");
    }

    {
        CHOC_TEST (SafeFilename)
        CHOC_EXPECT_EQ (choc::file::makeSafeFilename (""), "_");
        CHOC_EXPECT_EQ (choc::file::makeSafeFilename ("//"), "_");
        CHOC_EXPECT_EQ (choc::file::makeSafeFilename ("::sadf/sdfds123 sdf.sdfs."), "sadfsdfds123 sdf.sdfs.");
        CHOC_EXPECT_EQ (choc::file::makeSafeFilename ("::,;sadf/sdfds123 sdfsd.xyz", 10), "sadfsd.xyz");
        CHOC_EXPECT_EQ (choc::file::makeSafeFilename ("\\sa'df/sdfds123 sdfsd.xyzdfgdfgdfg", 10), "sa.xyzdfgdfgdfg");
    }
}

inline void testFileUtilities (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (Files);

    {
        CHOC_TEST (WildcardPattern)

        choc::text::WildcardPattern p1 ("*.xyz;*.foo", false), p2 ("*", false), p3, p4 ("abc?.x", true);

        CHOC_EXPECT_TRUE (p1.matches ("sdf.xyz") && p1.matches ("sdf.XyZ") && p1.matches (".xyz") && p1.matches ("dfg.foo"));
        CHOC_EXPECT_FALSE (p1.matches ("sdf.xxyz") || p1.matches ("") || p1.matches ("abc.xy") || p1.matches (".xyzz"));
        CHOC_EXPECT_TRUE (p2.matches ("") && p2.matches ("abcd"));
        CHOC_EXPECT_TRUE (p3.matches ("") && p3.matches ("dfgdfg"));
        CHOC_EXPECT_TRUE (p4.matches ("abcd.x"));
        CHOC_EXPECT_FALSE (p4.matches ("abcd.X") || p4.matches ("abcdd.x") || p4.matches ("abc.x"));
    }
}

//==============================================================================
inline void testValues (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (Values);

    {
        CHOC_TEST (Primitives)

        auto v = choc::value::createPrimitive (101);
        CHOC_EXPECT_TRUE (v.isInt32());
        CHOC_EXPECT_EQ (sizeof(int), v.getRawDataSize());
        CHOC_EXPECT_EQ (101, v.get<int>());
    }

    {
        CHOC_TEST (Defaults)

        choc::value::Value v;
        CHOC_EXPECT_TRUE (v.isVoid());
        CHOC_EXPECT_EQ (0ul, v.getRawDataSize());

        try
        {
            v.getObjectMemberAt (2);
            CHOC_FAIL ("Failed to fail");
        }
        catch (choc::value::Error& e)
        {
            CHOC_EXPECT_EQ (e.description, std::string ("This type is not an object"));
        }
    }

    {
        CHOC_TEST (ObjectCreation)

        auto v = choc::value::createObject ("test",
                                            "int32Field", (int32_t) 1,
                                            "boolField", true);
        CHOC_EXPECT_TRUE (v.isObject());
        CHOC_EXPECT_EQ (4ul + sizeof (choc::value::BoolStorageType), v.getRawDataSize());
        CHOC_EXPECT_EQ (2ul, v.size());

        auto member0 = v.getObjectMemberAt (0);
        auto member1 = v.getObjectMemberAt (1);

        CHOC_EXPECT_EQ (member0.name, std::string ("int32Field"));
        CHOC_EXPECT_TRUE (member0.value.isInt32());
        CHOC_EXPECT_EQ (1, member0.value.getInt32());
        CHOC_EXPECT_EQ (member1.name, std::string ("boolField"));
        CHOC_EXPECT_TRUE (member1.value.isBool());
        CHOC_EXPECT_TRUE (member1.value.getBool());

        try
        {
            v.getObjectMemberAt (2);
            CHOC_FAIL ("Failed to fail");
        }
        catch (choc::value::Error& e)
        {
            CHOC_EXPECT_EQ (e.description, std::string ("Index out of range"));
        }
    }

    {
        CHOC_TEST (Vectors)

        float values[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
        auto v1 = choc::value::createVector (values, 6);
        auto v2 = choc::value::createVector (6, [](uint32_t index) -> float { return (float) index; });

        CHOC_EXPECT_TRUE (v1.isVector());
        CHOC_EXPECT_EQ (6ul, v1.size());
        CHOC_EXPECT_EQ (6 * sizeof(float), v1.getRawDataSize());
        CHOC_EXPECT_TRUE (v2.isVector());
        CHOC_EXPECT_EQ (6ul, v2.size());
        CHOC_EXPECT_EQ (6 * sizeof(float), v2.getRawDataSize());
    }

    {
        CHOC_TEST (UniformArray)

        auto v = choc::value::createEmptyArray();
        v.addArrayElement (1);
        v.addArrayElement (2);
        v.addArrayElement (3);
        CHOC_EXPECT_TRUE (v.getType().isUniformArray());
    }

    {
        CHOC_TEST (ComplexArray)

        auto v = choc::value::createEmptyArray();
        v.addArrayElement (1);
        v.addArrayElement (2.0);
        v.addArrayElement (3);
        v.addArrayElement (false);
        CHOC_EXPECT_FALSE (v.getType().isUniformArray());
    }

    {
        CHOC_TEST (CreatingArrays)

        std::vector<std::string> items1 = { "a", "b", "c" };
        auto v1 = choc::value::createArray (items1);
        CHOC_EXPECT_EQ (v1.size(), items1.size());
        CHOC_EXPECT_EQ (v1[0].toString(), items1[0]);
        CHOC_EXPECT_EQ (v1[1].toString(), items1[1]);

        std::vector<int32_t> items2 = { 1, 2, 3, 4 };
        auto v2 = choc::value::createArray (items2);
        CHOC_EXPECT_EQ (v2.size(), items2.size());
        CHOC_EXPECT_EQ (v2[0].get<int32_t>(), items2[0]);
        CHOC_EXPECT_EQ (v2[1].get<int32_t>(), items2[1]);

        std::vector<choc::value::Value> items3 = { choc::value::createString ("xx"), choc::value::createPrimitive (1234)};
        auto v3 = choc::value::createArray (items3);
        CHOC_EXPECT_EQ (v3.size(), items3.size());
        CHOC_EXPECT_EQ (choc::json::toString (v3[0]), choc::json::toString (items3[0]));
        CHOC_EXPECT_EQ (choc::json::toString (v3[1]), choc::json::toString (items3[1]));
    }

    {
        CHOC_TEST (Alignment)

        {
            auto v1 = choc::value::createEmptyArray();
            v1.addArrayElement (false);
            v1.addArrayElement (2.0);
            CHOC_EXPECT_EQ (sizeof (choc::value::BoolStorageType), ((size_t) v1[1].getRawData()) - (size_t) v1.getRawData());
        }

        auto v2 = choc::value::createObject ("foo",
                                             "x", choc::value::createVector (3, [] (uint32_t) { return true; }),
                                             "y", choc::value::createVector (3, [] (uint32_t) { return true; }),
                                             "z", choc::value::createVector (3, [] (uint32_t) { return 1.0; }));

        CHOC_EXPECT_EQ (3 * sizeof (choc::value::BoolStorageType), ((size_t) v2["y"].getRawData()) - (size_t) v2.getRawData());
        CHOC_EXPECT_EQ (6 * sizeof (choc::value::BoolStorageType), ((size_t) v2["z"].getRawData()) - (size_t) v2.getRawData());
    }

    {
        auto v = choc::value::createObject ("testObject",
                                            "int32", (int32_t) 1,
                                            "int64", (int64_t) 2,
                                            "float32", 3.0f,
                                            "float64", 4.0,
                                            "boolean", false,
                                            "string1", "string value1",
                                            "string2", std::string_view ("string value2"),
                                            "string3", std::string ("string value3"));

        {
            float floatVector[] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
            auto vector = choc::value::createVector (floatVector, 6);
            v.addMember ("vector", vector);
        }

        {
            auto array = choc::value::createEmptyArray();

            array.addArrayElement (1);
            array.addArrayElement (2);
            array.addArrayElement (3);
            v.addMember ("primitiveArray", array);
        }

        {
            auto array = choc::value::createEmptyArray();

            array.addArrayElement (1);
            array.addArrayElement (2.0);
            array.addArrayElement (true);
            v.addMember ("complexArray", array);
        }

        v.setMember ("object", choc::value::createObject ("object",
                                                          "int32", choc::value::createPrimitive (1)));

        CHOC_EXPECT_EQ (88ul + 2 * sizeof (choc::value::BoolStorageType), v.getRawDataSize());

        CHOC_EXPECT_EQ (v.getType().getSignature (false), "o12_i32_i64_f32_f64_b_s_s_s_V6_f32_a3_i32_A3_1xi32_1xf64_1xb_o1_i32");
        CHOC_EXPECT_EQ (v.getType().getSignature (true), "o12_testObject_int32_i32_int64_i64_float32_f32_float64_f64_boolean_b_string1_s_string2_s_string3_s_vector_V6_f32_primitiveArray_a3_i32_complexArray_A3_1xi32_1xf64_1xb_object_o1_object_int32_i32");
        CHOC_EXPECT_EQ (v.getType().getDescription(), "object \"testObject\" { int32: int32, int64: int64, float32: float32, float64: float64, boolean: bool, string1: string, string2: string, string3: string, vector: vector 6 x float32, primitiveArray: array 3 x int32, complexArray: array (1 x int32, 1 x float64, 1 x bool), object: object \"object\" { int32: int32 } }");

        {
            CHOC_TEST (TypeRoundTrip)

            auto typeAsValue = v.getType().toValue();
            auto roundTripped = choc::value::Type::fromValue (typeAsValue);
            CHOC_EXPECT_EQ (v.getType().getSignature (true), roundTripped.getSignature (true));
        }

        {
            CHOC_TEST (SetMembers)

            try
            {
                v.addMember ("vector", 1234);
                CHOC_FAIL ("Failed to fail");
            }
            catch (choc::value::Error& e)
            {
                CHOC_EXPECT_EQ (e.description, std::string ("This object already contains a member with the given name"));
            }

            auto v1 = v;
            float floatVector[] = { 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 8.0f };
            auto vector = choc::value::createVector (floatVector, 6);
            v1.setMember ("vector", vector);
            CHOC_EXPECT_EQ (v1["vector"][5].get<float>(), 8.0f);

            {
                auto v2 = v1;
                v2.setMember ("vector", 12345);
                CHOC_EXPECT_EQ (v2["vector"].getInt32(), 12345);
            }

            {
                auto v2 = v1;
                v2.setMember ("vector", "modified");
                CHOC_EXPECT_EQ (v2["vector"].getString(), "modified");
            }
        }

        {
            CHOC_TEST (Casting)
            CHOC_EXPECT_EQ (choc::value::createString ("1.2").get<float>(), 1.2f);
            CHOC_EXPECT_EQ (choc::value::createString ("1.2").get<double>(), 1.2);
            CHOC_EXPECT_EQ (choc::value::createString ("1234567").get<int32_t>(), 1234567);
            CHOC_EXPECT_EQ (choc::value::createString ("1234567").get<uint32_t>(), 1234567u);
            CHOC_EXPECT_EQ (choc::value::createString ("123456789012345").get<int64_t>(), 123456789012345ll);
            CHOC_EXPECT_EQ (choc::value::createString ("123456789012345").get<uint64_t>(), 123456789012345ull);
            CHOC_EXPECT_EQ (choc::value::createString ("1").get<bool>(), true);
            CHOC_EXPECT_EQ (choc::value::createString ("0").get<bool>(), false);
            CHOC_EXPECT_EQ (choc::value::createString ("true").get<bool>(), true);

            CHOC_EXPECT_EQ (choc::value::createString ("x").getWithDefault<float> (1.2f), 1.2f);
            CHOC_EXPECT_EQ (choc::value::createString ("x").getWithDefault<double> (1.2), 1.2);
            CHOC_EXPECT_EQ (choc::value::createString ("x").getWithDefault<int32_t> (123), 123);
            CHOC_EXPECT_EQ (choc::value::createString ("x").getWithDefault<uint32_t> (123u), 123u);
            CHOC_EXPECT_EQ (choc::value::createString ("x").getWithDefault<int64_t> (123ll), 123ll);
            CHOC_EXPECT_EQ (choc::value::createString ("x").getWithDefault<uint64_t> (123ull), 123ull);
            CHOC_EXPECT_EQ (choc::value::createString ("x").getWithDefault<bool> (true), true);
            CHOC_EXPECT_EQ (choc::value::createString ("x").getWithDefault<bool> (false), false);
        }

        {
            CHOC_TEST (Serialisation)

            auto compare = [&] (const choc::value::ValueView& original, const choc::value::ValueView& deserialised)
            {
                auto s1 = choc::json::toString (original, false);
                auto s2 = choc::json::toString (deserialised, false);
                CHOC_EXPECT_EQ (s1, s2);
                auto s3 = choc::json::toString (original, true);
                auto s4 = choc::json::toString (deserialised, true);
                CHOC_EXPECT_EQ (s3, s4);
            };

            {
                auto serialised = v.serialise();
                auto deserialised = serialised.deserialise();
                compare (v, deserialised);
            }

            {
                auto serialised = v.getView().serialise();
                auto deserialised = serialised.deserialise();
                compare (v, deserialised);
            }

            {
                auto serialised = v.serialise();
                auto data = serialised.getInputData();

                choc::value::ValueView::deserialise (data, [&] (const choc::value::ValueView& deserialised)
                {
                    compare (v, deserialised);
                });
            }

            {
                auto serialised = v.getView().serialise();
                auto data = serialised.getInputData();

                choc::value::ValueView::deserialise (data, [&] (const choc::value::ValueView& deserialised)
                {
                    compare (v, deserialised);
                });
            }
        }
    }
}

//==============================================================================
inline void testJSON (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (JSON);

    {
        CHOC_TEST (ConvertDoubles)

        CHOC_EXPECT_EQ ("2.5",            choc::json::doubleToString (2.5));
        CHOC_EXPECT_EQ ("\"NaN\"",        choc::json::doubleToString (std::numeric_limits<double>::quiet_NaN()));
        CHOC_EXPECT_EQ ("\"Infinity\"",   choc::json::doubleToString (std::numeric_limits<double>::infinity()));
        CHOC_EXPECT_EQ ("\"-Infinity\"",  choc::json::doubleToString (-std::numeric_limits<double>::infinity()));
        CHOC_EXPECT_EQ (1.28,             choc::json::parseValue ("1.28").getFloat64());
        CHOC_EXPECT_EQ (-4,               choc::json::parseValue ("-4.0").getFloat64());
        CHOC_EXPECT_EQ (10.0,             choc::json::parseValue ("1.0e1").getFloat64());
        CHOC_EXPECT_EQ (0.1,              choc::json::parseValue ("1.0e-1").getFloat64());
        CHOC_EXPECT_EQ (10.0,             choc::json::parseValue ("1E1").getFloat64());
        CHOC_EXPECT_EQ ("1234",           std::string (choc::json::parseValue ("\"1234\"").getString()));
    }

    auto checkError = [&] (const std::string& json, const std::string& message, size_t line, size_t column)
    {
        try
        {
            auto v = choc::json::parse (json);
            CHOC_FAIL ("Should have thrown");
        }
        catch (choc::json::ParseError& e)
        {
            CHOC_EXPECT_EQ (e.what(), message);
            CHOC_EXPECT_EQ (e.lineAndColumn.line, line);
            CHOC_EXPECT_EQ (e.lineAndColumn.column, column);
        }
    };

    {
        CHOC_TEST (InvalidTopLevel)

        auto json = R"(
"invalidTopLevel": 5,
)";

        checkError (json, "Expected an object or array", 2, 1);
    }

    {
        CHOC_TEST (InvalidTrailingComma)

        auto json = R"(
{
"hasTrailingComma": 5,
}
)";
        checkError (json, "Expected a name", 4, 1);
    }

    {
        CHOC_TEST (InvalidMissingValue)

        auto json = R"(
{
"hasTrailingComma": 5,
"hasMissingValue": ,
}
)";

        checkError (json, "Syntax error", 4, 20);
    }

    {
        CHOC_TEST (InvalidWrongQuotes)

        auto json = R"(
{ "field": 'value' }
)";

        checkError (json, "Syntax error", 2, 12);
    }

    {
        CHOC_TEST (ValidLongNumber)

        auto json = R"(
{
  "negativeInt64": -1234,
  "largestInt64Possible": 9223372036854775806,
  "largestInt64": 9223372036854775807,
  "veryLarge": 12345678901234567890123456789012345678901234567890,
  "veryVeryLarge": 12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890,
  "scientificNotation": 1e5,
  "booleanTrue": true,
  "booleanFalse": false
}
)";

        auto holder = choc::json::parse (json);
        auto v = holder.getView();

        CHOC_EXPECT_TRUE (v["negativeInt64"].isInt64());
        CHOC_EXPECT_EQ (-1234, v["negativeInt64"].get<int64_t>());
        CHOC_EXPECT_TRUE (v["largestInt64Possible"].isInt64());
        CHOC_EXPECT_EQ (9223372036854775806, v["largestInt64Possible"].get<int64_t>());
        CHOC_EXPECT_TRUE (v["largestInt64"].isFloat64());
        CHOC_EXPECT_NEAR (9223372036854775807.0, v["largestInt64"].get<double>(), 0.0001);
        CHOC_EXPECT_TRUE (v["veryLarge"].isFloat64());
        CHOC_EXPECT_NEAR (1.2345678901234567e+49, v["veryLarge"].get<double>(), 0.0001);
        CHOC_EXPECT_TRUE (v["veryVeryLarge"].isFloat64());
        CHOC_EXPECT_EQ (INFINITY, v["veryVeryLarge"].get<double>());
        CHOC_EXPECT_TRUE (v["scientificNotation"].isFloat64());
        CHOC_EXPECT_NEAR (1e5, v["scientificNotation"].get<double>(), 0.0001);
        CHOC_EXPECT_TRUE (v["booleanTrue"].isBool());
        CHOC_EXPECT_TRUE (v["booleanTrue"].get<bool>());
        CHOC_EXPECT_TRUE (v["booleanFalse"].isBool());
        CHOC_EXPECT_FALSE (v["booleanFalse"].get<bool>());
    }

    {
        CHOC_TEST (ValidJSON)

        auto validJSON = R"(
{
    "tests": [
        {
            "name": "test1",
            "actions": [
                {
                    "action": "standardTestSteps",
                    "deviceType": "llvm",
                    "deviceName": "llvm",
                    "codeName": "adsr",
                    "sampleRate": 44100.0,
                    "blockSize": 32,
                    "requiredSamples": 1000
                }
            ]
        },
        {
            "name": "test2",
            "actions": [
                {
                    "action": "standardTestSteps",
                    "deviceType": "cpp",
                    "deviceName": "cpp",
                    "codeName": "\u12aB",
                    "sampleRate": 44100.0,
                    "blockSize": 32,
                    "requiredSamples": 1000
                }
            ]
        }
    ]
}
)";

        auto holder = choc::json::parse (validJSON);
        auto v = holder.getView();

        // Test some aspects of the parsed JSON
        CHOC_EXPECT_TRUE (v.getType().isObject());

        CHOC_EXPECT_EQ ("test1",   v["tests"][0]["name"].get<std::string>());
        CHOC_EXPECT_NEAR (44100.0, v["tests"][0]["actions"][0]["sampleRate"].get<double>(), 0.0001);
        CHOC_EXPECT_EQ (32,        v["tests"][0]["actions"][0]["blockSize"].get<int>());

        CHOC_EXPECT_EQ ("test2", v["tests"][1]["name"].get<std::string>());
        CHOC_EXPECT_TRUE (strcmp ("test2", v["tests"][1]["name"].get<const char*>()) == 0);

        auto s = choc::value::createString ({});
        CHOC_EXPECT_TRUE (s.get<const char*>()[0] == 0);
    }

    {
        CHOC_TEST (RoundTrip)
        auto json = R"({"tests": [{"name": "\"\\\n\r\t\a\b\f\u12ab", "actions": [{"action": "standardTestSteps", "deviceType": "llvm", "deviceName": "llvm", "codeName": "adsr", "sampleRate": 44100, "blockSize": 32, "requiredSamples": 1000}]}, {"name": "test2", "actions": [{"action": "standardTestSteps", "deviceType": "cpp", "deviceName": "cpp", "codeName": "adsr", "sampleRate": 44100, "array": [1, 2, 3, 4, 5], "emptyArray": [], "requiredSamples": 1000}]}]})";
        auto holder = choc::json::parse (json);
        auto output = choc::json::toString (holder.getView());
        CHOC_EXPECT_EQ (json, output);
    }
}

//==============================================================================
inline void testMIDI (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (MIDI);

    {
        CHOC_TEST (FrequencyUtils)

        CHOC_EXPECT_NEAR (440.0f, choc::midi::noteNumberToFrequency (69), 0.001f);
        CHOC_EXPECT_NEAR (440.0f, choc::midi::noteNumberToFrequency (69.0f), 0.001f);
        CHOC_EXPECT_NEAR (880.0f, choc::midi::noteNumberToFrequency (69 + 12), 0.001f);
        CHOC_EXPECT_NEAR (880.0f, choc::midi::noteNumberToFrequency (69.0f + 12.0f), 0.001f);
        CHOC_EXPECT_NEAR (69.0f + 12.0f, choc::midi::frequencyToNoteNumber (880.0f), 0.001f);
    }

    {
        CHOC_TEST (ControllerNames)

        CHOC_EXPECT_EQ ("Bank Select",               choc::midi::getControllerName (0));
        CHOC_EXPECT_EQ ("Modulation Wheel (coarse)", choc::midi::getControllerName (1));
        CHOC_EXPECT_EQ ("Sound Variation",           choc::midi::getControllerName (70));
        CHOC_EXPECT_EQ ("255",                       choc::midi::getControllerName (255));
    }

    {
        CHOC_TEST (NoteNumbers)

        {
            choc::midi::NoteNumber note { 60 };

            CHOC_EXPECT_EQ (60,          note);
            CHOC_EXPECT_EQ (0,           note.getChromaticScaleIndex());
            CHOC_EXPECT_EQ (3,           note.getOctaveNumber());
            CHOC_EXPECT_NEAR (261.625f,  note.getFrequency(), 0.001f);
            CHOC_EXPECT_EQ ("C",         note.getName());
            CHOC_EXPECT_EQ ("C",         note.getNameWithSharps());
            CHOC_EXPECT_EQ ("C",         note.getNameWithFlats());
            CHOC_EXPECT_TRUE (note.isNatural());
            CHOC_EXPECT_FALSE (note.isAccidental());
            CHOC_EXPECT_EQ ("C3",        note.getNameWithOctaveNumber());
        }

        {
            choc::midi::NoteNumber note { 61 + 12 };

            CHOC_EXPECT_EQ (73,          note);
            CHOC_EXPECT_EQ (1,           note.getChromaticScaleIndex());
            CHOC_EXPECT_EQ (4,           note.getOctaveNumber());
            CHOC_EXPECT_NEAR (554.365f,  note.getFrequency(), 0.001f);
            CHOC_EXPECT_EQ ("C#",        note.getName());
            CHOC_EXPECT_EQ ("C#",        note.getNameWithSharps());
            CHOC_EXPECT_EQ ("Db",        note.getNameWithFlats());
            CHOC_EXPECT_FALSE (note.isNatural());
            CHOC_EXPECT_TRUE (note.isAccidental());
            CHOC_EXPECT_EQ ("C#4",       note.getNameWithOctaveNumber());
        }
    }

    {
        CHOC_TEST (ShortMessages)

        choc::midi::ShortMessage msg;
        CHOC_EXPECT_TRUE (msg.isNull());
    }
}

//==============================================================================
inline void testAudioBuffers (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (AudioBuffers);

    auto findMaxDiff = [] (auto buffer1, auto buffer2)
    {
        float maxDiff = 0;

        for (choc::buffer::FrameCount frame = 0; frame < buffer1.getNumFrames(); ++frame)
            for (choc::buffer::ChannelCount chan = 0; chan < buffer1.getNumChannels(); ++chan)
                maxDiff = std::max (maxDiff, std::abs (buffer1.getSample (chan, frame) - buffer2.getSample (chan, frame)));

        return maxDiff;
    };

    {
        CHOC_TEST (InterleavedApplyClear)

        choc::buffer::InterleavedBuffer<float> channels (2, 20);
        CHOC_ASSERT (channels.getNumChannels() == 2);
        CHOC_ASSERT (channels.getNumFrames() == 20);
        CHOC_ASSERT (channels.getIterator (0).stride == 2);

        for (int i = 0 ; i < 20; i ++)
        {
            channels.getSample (0, (uint32_t) i) = (float) i;
            channels.getSample (1, (uint32_t) i) = (float) -i;
        }

        setAllSamples (channels, [] (float f) { return f + 10.0f; });

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            CHOC_EXPECT_EQ (channels.getSample (0, i), float (i) + 10.0f);
            CHOC_EXPECT_EQ (channels.getSample (1, i), 10.0f - float (i));
        }

        channels.clear();

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            CHOC_EXPECT_EQ (channels.getSample (0, i), 0.0f);
            CHOC_EXPECT_EQ (channels.getSample (1, i), 0.0f);
        }
    }

    {
        CHOC_TEST (InterleavedFrame)

        choc::buffer::InterleavedBuffer<uint32_t> channels (3, 10);
        CHOC_ASSERT (channels.getNumChannels() == 3);
        CHOC_ASSERT (channels.getNumFrames() == 10);
        CHOC_ASSERT (channels.getIterator (0).stride == 3);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
            channels.getSample (2, i) = i + 200;
        }

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            uint32_t frame[3];
            channels.getSamplesInFrame (i, frame);

            CHOC_EXPECT_EQ (i, frame[0]);
            CHOC_EXPECT_EQ (i + 100, frame[1]);
            CHOC_EXPECT_EQ (i + 200, frame[2]);
        }
    }

    {
        CHOC_TEST (InterleavedSlice)

        choc::buffer::InterleavedBuffer<double> channels (2, 20);
        CHOC_ASSERT (channels.getNumChannels() == 2);
        CHOC_ASSERT (channels.getNumFrames() == 20);
        CHOC_ASSERT (channels.getIterator(0).stride == 2);

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
        }

        CHOC_EXPECT_EQ (channels.getSample (0, 0), 0);
        CHOC_EXPECT_EQ (channels.getSample (1, 0), 100);

        auto slice = channels.getFrameRange ({ 2, 7 });

        CHOC_ASSERT (slice.getNumChannels() == 2);
        CHOC_ASSERT (slice.getNumFrames() == 5);
        CHOC_ASSERT (slice.data.stride == 2);

        for (uint32_t i = 0 ; i < slice.getNumFrames(); i ++)
        {
            CHOC_EXPECT_EQ (slice.getSample (0, i), 2 + i);
            CHOC_EXPECT_EQ (slice.getSample (1, i), 2 + i + 100);
        }
    }

    {
        CHOC_TEST (InterleavedRange)

        choc::buffer::InterleavedBuffer<uint32_t> channels (5, 10);
        CHOC_ASSERT (channels.getNumChannels() == 5);
        CHOC_ASSERT (channels.getNumFrames() == 10);
        CHOC_ASSERT (channels.getIterator (0).stride == 5);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
            channels.getSample (2, i) = i + 200;
            channels.getSample (3, i) = i + 300;
            channels.getSample (4, i) = i + 400;
        }

        auto set = channels.getChannelRange ({ 1, 3 });

        CHOC_ASSERT (set.getNumChannels() == 2);
        CHOC_ASSERT (set.getNumFrames() == 10);
        CHOC_ASSERT (set.data.stride == 5);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            CHOC_EXPECT_EQ (set.getSample (0, i), i + 100);
            CHOC_EXPECT_EQ (set.getSample (1, i), i + 200);
        }

        auto slice = set.getFrameRange ({ 2, 7 });

        CHOC_ASSERT (slice.getNumChannels() == 2);
        CHOC_ASSERT (slice.getNumFrames() == 5);
        CHOC_ASSERT (slice.data.stride == 5);

        for (uint32_t i = 0 ; i < slice.getNumFrames(); i ++)
        {
            CHOC_EXPECT_EQ (slice.getSample (0, i), 2 + i + 100);
            CHOC_EXPECT_EQ (slice.getSample (1, i), 2 + i + 200);
        }
    }

    {
        CHOC_TEST (InterleavedPacked)

        choc::buffer::InterleavedBuffer<uint32_t> channels (3, 10);
        CHOC_ASSERT (channels.getNumChannels() == 3);
        CHOC_ASSERT (channels.getNumFrames() == 10);
        CHOC_ASSERT (channels.getIterator(0).stride == 3);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
            channels.getSample (2, i) = i + 200;
        }

        auto iter0 = channels.getIterator (0);
        auto iter1 = channels.getIterator (1);
        auto iter2 = channels.getIterator (2);

        for (uint32_t i = 0; i < 10; ++i)
        {
            CHOC_EXPECT_EQ (*iter0++, i);
            CHOC_EXPECT_EQ (*iter1++, i + 100);
            CHOC_EXPECT_EQ (*iter2++, i + 200);
        }
    }

    {
        CHOC_TEST (DiscreteApplyClear)

        choc::buffer::ChannelArrayBuffer<float> channels (2, 20);
        CHOC_ASSERT (channels.getNumChannels() == 2);
        CHOC_ASSERT (channels.getNumFrames() == 20);
        CHOC_ASSERT (channels.getIterator(0).stride == 1);
        CHOC_ASSERT (channels.getView().data.offset == 0);

        for (int i = 0 ; i < 20; i ++)
        {
            channels.getSample (0, (uint32_t) i) = (float) i;
            channels.getSample (1, (uint32_t) i) = (float) -i;
        }

        setAllSamples (channels, [] (float f) { return f + 10.0f; });

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            CHOC_EXPECT_EQ (channels.getSample (0, i), float (i) + 10.0f);
            CHOC_EXPECT_EQ (channels.getSample (1, i), 10.0f - float (i));
        }

        channels.clear();

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            CHOC_EXPECT_EQ (channels.getSample (0, i), 0.0f);
            CHOC_EXPECT_EQ (channels.getSample (1, i), 0.0f);
        }
    }

    {
        CHOC_TEST (DiscreteFrame)

        choc::buffer::ChannelArrayBuffer<uint32_t> channels (3, 10);
        CHOC_EXPECT_EQ (channels.getNumChannels(), 3UL);
        CHOC_EXPECT_EQ (channels.getNumFrames(), 10UL);
        CHOC_EXPECT_EQ (channels.getIterator(0).stride, 1UL);
        CHOC_EXPECT_EQ (channels.getView().data.offset, 0UL);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
            channels.getSample (2, i) = i + 200;
        }

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            uint32_t frame[3];

            channels.getSamplesInFrame (i, frame);

            CHOC_EXPECT_EQ (i, frame[0]);
            CHOC_EXPECT_EQ (i + 100, frame[1]);
            CHOC_EXPECT_EQ (i + 200, frame[2]);
        }
    }

    {
        CHOC_TEST (DiscreteSlice)

        choc::buffer::ChannelArrayBuffer<double> channels (2, 20);
        CHOC_ASSERT (channels.getNumChannels() == 2);
        CHOC_ASSERT (channels.getNumFrames() == 20);
        CHOC_ASSERT (channels.getIterator(0).stride == 1);
        CHOC_ASSERT (channels.getView().data.offset == 0);

        for (uint32_t i = 0 ; i < 20; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
        }

        CHOC_EXPECT_EQ (channels.getSample (0, 0), 0);
        CHOC_EXPECT_EQ (channels.getSample (1, 0), 100);

        auto slice = channels.getFrameRange ({ 2, 7 });

        CHOC_ASSERT (slice.getNumChannels() == 2);
        CHOC_ASSERT (slice.getNumFrames() == 5);
        CHOC_ASSERT (slice.getIterator(0).stride == 1);
        CHOC_ASSERT (slice.data.offset == 2);

        for (uint32_t i = 0 ; i < slice.getNumFrames(); i ++)
        {
            CHOC_EXPECT_EQ (slice.getSample (0, i), 2 + i);
            CHOC_EXPECT_EQ (slice.getSample (1, i), 2 + i + 100);
        }
    }

    {
        CHOC_TEST (DiscreteRange)

        choc::buffer::ChannelArrayBuffer<uint32_t> channels (5, 10);
        CHOC_ASSERT (channels.getNumChannels() == 5);
        CHOC_ASSERT (channels.getNumFrames() == 10);
        CHOC_ASSERT (channels.getIterator(0).stride == 1);
        CHOC_ASSERT (channels.getView().data.offset == 0);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            channels.getSample (0, i) = i;
            channels.getSample (1, i) = i + 100;
            channels.getSample (2, i) = i + 200;
            channels.getSample (3, i) = i + 300;
            channels.getSample (4, i) = i + 400;
        }

        auto set = channels.getChannelRange ({ 1, 3 });

        CHOC_ASSERT (set.getNumChannels() == 2);
        CHOC_ASSERT (set.getNumFrames() == 10);
        CHOC_ASSERT (set.getIterator(0).stride == 1);
        CHOC_ASSERT (set.data.offset == 0);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            CHOC_EXPECT_EQ (set.getSample (0, i), i + 100);
            CHOC_EXPECT_EQ (set.getSample (1, i), i + 200);
        }

        auto slice = set.getFrameRange ({ 2, 7 });

        CHOC_ASSERT (slice.getNumChannels() == 2);
        CHOC_ASSERT (slice.getNumFrames() == 5);
        CHOC_ASSERT (slice.getIterator(0).stride == 1);
        CHOC_ASSERT (slice.data.offset == 2);

        for (uint32_t i = 0 ; i < slice.getNumFrames(); i ++)
        {
            CHOC_EXPECT_EQ (slice.getSample (0, i), 2 + i + 100);
            CHOC_EXPECT_EQ (slice.getSample (1, i), 2 + i + 200);
        }
    }

    {
        CHOC_TEST (SetsAreSameSize)

        choc::buffer::ChannelArrayBuffer<int>   set1 (5, 10);
        choc::buffer::ChannelArrayBuffer<int>   set2 (5, 11);
        choc::buffer::ChannelArrayBuffer<int>   set3 (6, 10);
        choc::buffer::ChannelArrayBuffer<float> set4 (5, 10);
        choc::buffer::InterleavedBuffer<double> set5 (5, 10);

        CHOC_EXPECT_EQ (true,  set1.getSize() == set1.getSize());
        CHOC_EXPECT_EQ (false, set1.getSize() == set2.getSize());
        CHOC_EXPECT_EQ (false, set1.getSize() == set3.getSize());
        CHOC_EXPECT_EQ (true, set1.getSize() == set4.getSize());
        CHOC_EXPECT_EQ (true, set1.getSize() == set5.getSize());
    }

    {
        CHOC_TEST (Copy)

        choc::buffer::ChannelArrayBuffer<float> source (5, 10);

        for (uint32_t i = 0; i < 10; i ++)
        {
            source.getSample (0, i) = (float) i;
            source.getSample (1, i) = (float) i + 100;
            source.getSample (2, i) = (float) i + 200;
            source.getSample (3, i) = (float) i + 300;
            source.getSample (4, i) = (float) i + 400;
        }

        auto slice = source.getChannelRange ({ 1, 3 }).getFrameRange ({ 2, 7 });

        CHOC_ASSERT (slice.getNumChannels() == 2);
        CHOC_ASSERT (slice.getNumFrames() == 5);
        CHOC_ASSERT (slice.getIterator(0).stride == 1);
        CHOC_ASSERT (slice.data.offset == 2);

        choc::buffer::InterleavedBuffer<double> dest (2, 5);

        copy (dest, slice);

        for (uint32_t i = 0 ; i < slice.getNumFrames(); i ++)
        {
            CHOC_EXPECT_EQ (dest.getSample (0, i), 2 + i + 100);
            CHOC_EXPECT_EQ (dest.getSample (1, i), 2 + i + 200);
        }
    }

    {
        CHOC_TEST (CopyToFit)

        choc::buffer::ChannelArrayBuffer<float> source1 (1, 10),
                                                source2 (2, 10);
        source1.clear();
        source2.clear();

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            source1.getSample (0, i) = (float) i;
            source2.getSample (0, i) = (float) i;
            source2.getSample (1, i) = (float) i + 100;
        }

        choc::buffer::InterleavedBuffer<double> dest1 (1, 10);
        choc::buffer::InterleavedBuffer<double> dest2 (2, 10);
        choc::buffer::InterleavedBuffer<double> dest3 (3, 10);

        // 1 -> 1, 1-> 2, 1 -> 3
        copyRemappingChannels (dest1, source1);
        copyRemappingChannels (dest2, source1);
        copyRemappingChannels (dest3, source1);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            CHOC_EXPECT_EQ (dest1.getSample (0, i), i);
            CHOC_EXPECT_EQ (dest2.getSample (0, i), i);
            CHOC_EXPECT_EQ (dest2.getSample (0, i), i);
            CHOC_EXPECT_EQ (dest3.getSample (0, i), i);
            CHOC_EXPECT_EQ (dest3.getSample (0, i), i);
            CHOC_EXPECT_EQ (dest3.getSample (0, i), i);
        }

        // 2 -> 1, 2-> 2, 2 -> 3
        dest1.clear();
        dest2.clear();
        dest3.clear();

        copyRemappingChannels (dest1, source2);
        copyRemappingChannels (dest2, source2);
        copyRemappingChannels (dest3, source2);

        for (uint32_t i = 0 ; i < 10; i ++)
        {
            CHOC_EXPECT_EQ (dest1.getSample (0, i), i);       // Channel 0
            CHOC_EXPECT_EQ (dest2.getSample (0, i), i);       // Channel 0
            CHOC_EXPECT_EQ (dest2.getSample (1, i), i + 100); // Channel 1
            CHOC_EXPECT_EQ (dest3.getSample (0, i), i);       // Channel 0
            CHOC_EXPECT_EQ (dest3.getSample (1, i), i + 100); // Channel 1
            CHOC_EXPECT_EQ (dest3.getSample (2, i), 0);       // blank
        }
    }

    {
        CHOC_TEST (CopyAllZero)

        choc::buffer::ChannelArrayBuffer<float> source (5, 10);
        source.clear();
        CHOC_EXPECT_EQ (true, isAllZero (source));
        source.getSample (2, 6) = 1.0f;
        CHOC_EXPECT_EQ (false, isAllZero (source));
    }

    {
        CHOC_TEST (ContentIsIdentical)

        choc::buffer::ChannelArrayBuffer<float> source (2, 10);

        for (uint32_t i = 0; i < 10; ++i)
        {
            source.getSample (0, i) = (float) i;
            source.getSample (1, i) = (float) i + 100;
        }

        choc::buffer::ChannelArrayBuffer<float> dest (2, 10);
        copy (dest, source);
        CHOC_EXPECT_EQ (true, contentMatches (source, dest));
    }

    {
        CHOC_TEST (SincInterpolator)

        auto sourceBuffer = choc::buffer::createChannelArrayBuffer (2, 2200, [] (choc::buffer::ChannelCount,
                                                                                 choc::buffer::FrameCount frame) -> float
        {
            return -0.75f + std::fmod (static_cast<float> (frame) * 0.025f, 1.5f);
        });

        for (auto ratio : { 1.0, 1.0181, 1.1013, 1.2417, 1.97004, 2.0, 2.0628, 3.77391 })
        {
            auto resampledNumFrames = static_cast<choc::buffer::FrameCount> (ratio * sourceBuffer.getNumFrames());

            choc::buffer::ChannelArrayBuffer<float> resampledBuffer (sourceBuffer.getNumChannels(), resampledNumFrames);
            choc::interpolation::sincInterpolate (resampledBuffer, sourceBuffer);

            choc::buffer::ChannelArrayBuffer<float> roundTripResult (sourceBuffer.getSize());
            choc::interpolation::sincInterpolate (roundTripResult, resampledBuffer);

            choc::buffer::FrameCount margin = 50;
            choc::buffer::FrameRange range { margin, sourceBuffer.getNumFrames() - margin };

            auto maxDiff = findMaxDiff (sourceBuffer.getFrameRange (range),
                                        roundTripResult.getFrameRange (range));
            CHOC_EXPECT_TRUE (maxDiff < 0.01f);
        }
    }

    {
        CHOC_TEST (InterleavingScratchBufferRoundTrip)

        choc::buffer::InterleavingScratchBuffer<float> ib;
        choc::buffer::DeinterleavingScratchBuffer<float> db;

        auto test = [&] (choc::buffer::ChannelCount numChans, choc::buffer::FrameCount numFrames)
        {
            auto source = choc::buffer::createChannelArrayBuffer (numChans, numFrames,
                                                                  [] (choc::buffer::ChannelCount chan,
                                                                      choc::buffer::FrameCount frame) -> float
            {
                return (float) chan + (float) frame * 0.01f;
            });

            auto interleaved = ib.interleave (source);
            auto roundTripped = db.deinterleave (interleaved);

            auto maxDiff = findMaxDiff (source, roundTripped);
            CHOC_EXPECT_TRUE (maxDiff < 0.01f);
        };

        test (1, 50);
        test (2, 100);
        test (3, 50);
        test (5, 120);
    }

    {
        CHOC_TEST (InterleavingScratchBufferReusedWithDifferentChannelCounts)

        choc::buffer::InterleavingScratchBuffer<float> scratch;

        // start by copying a stereo channel array, then check the underlying storage
        {
            constexpr const uint32_t frameCount = 4;
            const std::array<float, frameCount> left {{ -0.1f, -0.2f, -0.3f, -0.4f }};
            const std::array<float, frameCount> right {{ 0.1f, 0.2f, 0.3f, 0.4f }};
            const std::array<const float*, 2> channels {{ left.data(), right.data() }};

            const auto channelCount = static_cast<uint32_t> (channels.size());
            const auto view = choc::buffer::createChannelArrayView (channels.data(), channelCount, frameCount);

            auto interleaved = scratch.interleave (view);

            CHOC_EXPECT_NEAR (interleaved.data.data[0], -0.1f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[1],  0.1f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[2], -0.2f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[3],  0.2f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[4], -0.3f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[5],  0.3f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[6], -0.4f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[7],  0.4f, 0.0001f);
        }

        // re-use scratch to copy from a smaller channel view. data is stored according to latest channel count.
        {
            constexpr const uint32_t frameCount = 4;
            const std::array<float, frameCount> left {{ -0.1f, -0.2f, -0.3f, -0.4f }};
            const std::array<const float*, 1> channels {{ left.data() }};

            const auto channelCount = static_cast<uint32_t> (channels.size());
            const auto view = choc::buffer::createChannelArrayView (channels.data(), channelCount, frameCount);

            const auto interleaved = scratch.interleave (view);

            CHOC_EXPECT_NEAR (interleaved.data.data[0], -0.1f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[1], -0.2f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[2], -0.3f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[3], -0.4f, 0.0001f);
        }

        // re-use scratch to copy from a larger channel view. data is stored according to latest channel count.
        {
            constexpr const uint32_t frameCount = 4;
            const std::array<float, frameCount> one {{ -0.1f, -0.2f, -0.3f, -0.4f }};
            const std::array<float, frameCount> two {{ 0.1f, 0.2f, 0.3f, 0.4f }};
            const std::array<float, frameCount> three {{ 0.2f, 0.4f, 0.6f, 0.8f }};
            const std::array<const float*, 3> channels {{ one.data(), two.data(), three.data() }};

            const auto channelCount = static_cast<uint32_t> (channels.size());
            const auto view = choc::buffer::createChannelArrayView (channels.data(), channelCount, frameCount);

            const auto interleaved = scratch.interleave (view);

            CHOC_EXPECT_NEAR (interleaved.data.data[0], -0.1f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[1],  0.1f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[2],  0.2f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[3], -0.2f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[4],  0.2f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[5],  0.4f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[6], -0.3f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[7],  0.3f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[8],  0.6f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[9], -0.4f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[10], 0.4f, 0.0001f);
            CHOC_EXPECT_NEAR (interleaved.data.data[11], 0.8f, 0.0001f);
        }
    }

    {
        CHOC_TEST (Oscillators)

        auto b1 = choc::oscillator::createChannelArraySine<float> ({ 2, 100 }, 10, 1000);
        auto b2 = choc::oscillator::createInterleavedSine<float> ({ 2, 100 }, 10, 1000);

        CHOC_EXPECT_TRUE (b1.getSize() == b2.getSize());
        CHOC_EXPECT_TRUE (b1.getSample (0, 20) != 0);
        CHOC_EXPECT_TRUE (b2.getSample (1, 30) != 0);
    }
}


//==============================================================================
template <typename Format, typename Buffer>
inline void testIntToFloatBuffer (choc::test::TestProgress& progress, Buffer& buffer, uint32_t sampleStride)
{
    std::vector<uint8_t> data;
    data.resize (buffer.getNumChannels() * buffer.getNumFrames() * sampleStride);
    auto b2 = buffer;
    auto b3 = buffer;

    choc::audio::sampledata::copyToInterleavedIntData<Format> (data.data(), sampleStride, buffer);
    choc::audio::sampledata::copyFromInterleavedIntData<Format> (b2, data.data(), sampleStride);
    choc::audio::sampledata::copyToInterleavedIntData<Format> (data.data(), sampleStride, b2);
    choc::audio::sampledata::copyFromInterleavedIntData<Format> (b3, data.data(), sampleStride);

    CHOC_EXPECT_EQ (true, contentMatches (b2, b3));
}

template <typename Format>
inline void testIntToFloatFormat (choc::test::TestProgress& progress)
{
    std::array testValues { 10.0f, 1.1f, 1.0f, 0.99f, 0.8f, 0.6f, 0.5f, 0.3f, 0.2f, 0.01f, 0.0f };
    char data[8];

    for (auto f1 : testValues)
    {
        {
            Format::write (data, f1);
            auto f2 = Format::template read<float> (data);
            Format::write (data, f2);
            auto f3 = Format::template read<float> (data);
            CHOC_EXPECT_EQ (f2, f3);
        }

        {
            Format::write (data, -f1);
            auto f2 = Format::template read<float> (data);
            Format::write (data, f2);
            auto f3 = Format::template read<float> (data);
            CHOC_EXPECT_EQ (f2, f3);
        }
    }

    for (uint32_t numChans = 1; numChans < 4; ++numChans)
    {
        const uint32_t numFrames = 32;
        size_t testIndex = 0;

        auto source = choc::buffer::createChannelArrayBuffer (numChans, numFrames, [&]
        {
            ++testIndex;
            return testValues[testIndex % testValues.size()] * ((testIndex & 1) ? -1.0f : 1.0f);
        });

        testIntToFloatBuffer<Format> (progress, source, Format::sizeInBytes);
    }
}

inline void testIntToFloat (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (AudioSampleConversion);

    { CHOC_TEST(Int8);               testIntToFloatFormat<choc::audio::sampledata::Int8> (progress); }
    { CHOC_TEST(UInt8);              testIntToFloatFormat<choc::audio::sampledata::UInt8> (progress); }
    { CHOC_TEST(Int16LittleEndian);  testIntToFloatFormat<choc::audio::sampledata::Int16LittleEndian> (progress); }
    { CHOC_TEST(Int16BigEndian);     testIntToFloatFormat<choc::audio::sampledata::Int16BigEndian> (progress); }
    { CHOC_TEST(Int24LittleEndian);  testIntToFloatFormat<choc::audio::sampledata::Int24LittleEndian> (progress); }
    { CHOC_TEST(Int24BigEndian);     testIntToFloatFormat<choc::audio::sampledata::Int24BigEndian> (progress); }
    { CHOC_TEST(Int32LittleEndian);  testIntToFloatFormat<choc::audio::sampledata::Int32LittleEndian> (progress); }
    { CHOC_TEST(Int32BigEndian);     testIntToFloatFormat<choc::audio::sampledata::Int32BigEndian> (progress); }
}

//==============================================================================
inline void testFIFOs (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (FIFOs);

    {
        CHOC_TEST (Valid)

        choc::fifo::VariableSizeFIFO queue;
        queue.reset (10000);

        CHOC_EXPECT_EQ (false, queue.push (nullptr, 0));

        for (int i = 0; i < 100; ++i)
        {
            CHOC_EXPECT_TRUE (queue.push (&i, sizeof (i)));
        }

        int msgCount = 0;

        queue.popAllAvailable ([&] (void* data, size_t size)
                               {
                                   CHOC_EXPECT_EQ (sizeof (int), size);
                                   auto i = choc::memory::readNativeEndian<int> (data);

                                   if (i == 20)
                                       return false;

                                   CHOC_EXPECT_EQ (msgCount++, i);
                                   return true;
                               });

        CHOC_EXPECT_EQ (msgCount, 20);

        while (queue.pop ([&] (void* data, size_t size)
                          {
                              CHOC_EXPECT_EQ (sizeof (int), size);
                              auto i = choc::memory::readNativeEndian<int> (data);
                              CHOC_EXPECT_EQ (msgCount, i);
                          }))
        {
            ++msgCount;
        }

        CHOC_EXPECT_EQ (100, msgCount);
    }

    {
        CHOC_TEST (Overflow)

        choc::fifo::VariableSizeFIFO queue;
        queue.reset (1000);

        std::vector<uint8_t> buffer;
        buffer.resize (1000);

        // The total available space includes the message headers, so although it looks like we've got space for 1000 bytes,
        // we actually only have space for 1000 - sizeof (MessageHeader)
        CHOC_EXPECT_TRUE (queue.push (&buffer[0], 200));

        CHOC_EXPECT_TRUE (queue.push (400, [&] (void* dest)
        {
            memcpy (dest, buffer.data(), 200);
            memcpy (static_cast<char*> (dest) + 200, buffer.data(), 200);
        }));

        CHOC_EXPECT_TRUE (queue.push (&buffer[0], 200));
        CHOC_EXPECT_FALSE (queue.push (&buffer[0], 1001 - 4 * 4));

        queue.reset (200);
        CHOC_EXPECT_TRUE (queue.push (&buffer[0], 195));
        CHOC_EXPECT_TRUE (queue.pop([&] (void*, size_t) {}));
        CHOC_EXPECT_FALSE (queue.push (&buffer[0], 196));
        CHOC_EXPECT_FALSE (queue.pop([&] (void*, size_t) {}));
        CHOC_EXPECT_FALSE (queue.push (&buffer[0], 197));
        CHOC_EXPECT_FALSE (queue.pop([&] (void*, size_t) {}));
        CHOC_EXPECT_FALSE (queue.push (&buffer[0], 201));
        CHOC_EXPECT_FALSE (queue.pop([&] (void*, size_t) {}));
    }

    {
        CHOC_TEST (Wrapping)

        choc::fifo::VariableSizeFIFO queue;
        queue.reset (1000);

        std::vector<uint8_t> buffer;

        for (uint8_t i = 0; i < 200; ++i)
            buffer.push_back (i);

        for (int i = 1; i <= 200; i += 7)
        {
            for (int j = 0; j < 100; j++)
            {
                auto messageSize = static_cast<uint32_t> (i);
                size_t retrievedBytes = 0;

                CHOC_EXPECT_TRUE (queue.push (&buffer[0], messageSize));
                CHOC_EXPECT_TRUE (queue.push (&buffer[0], messageSize));
                CHOC_EXPECT_TRUE (queue.push (&buffer[0], messageSize));

                int msgCount = 0;

                while (queue.pop ([&] (void* data, size_t size)
                                  {
                                      CHOC_EXPECT_EQ (0, memcmp (&buffer[0], data, size));
                                      retrievedBytes += size;
                                  }))
                {
                    ++msgCount;
                }

                CHOC_EXPECT_EQ (3, msgCount);
                CHOC_EXPECT_EQ (retrievedBytes, messageSize * 3);
            }
        }
    }
}

//==============================================================================
inline void testMIDIFiles (choc::test::TestProgress& progress)
{
    auto simpleHash = [] (const std::string& s)
    {
        uint64_t n = 123;

        for (auto c : s)
            n = (n * 127) + (uint8_t) c;

        return (uint64_t) n;
    };

    CHOC_CATEGORY (MIDIFile);

    {
        CHOC_TEST (SimpleFile)

        uint8_t testData[] =
             {77,84,104,100,0,0,0,6,0,1,0,2,1,0,77,84,114,107,0,0,0,25,0,255,88,4,3,3,36,8,0,255,89,2,255,1,0,255,81,3,
              12,53,0,1,255,47,0,77,84,114,107,0,0,1,40,0,192,0,0,176,121,0,0,176,64,0,0,176,91,48,0,176,10,51,0,176,7,100,0,176,
              121,0,0,176,64,0,0,176,91,48,0,176,10,51,0,176,7,100,0,255,3,5,80,105,97,110,111,0,144,62,74,64,128,62,0,0,144,64,83,64,
              128,64,0,0,144,65,86,64,128,65,0,0,144,67,92,64,128,67,0,0,144,69,93,64,128,69,0,0,144,70,89,64,128,70,0,0,144,61,69,64,
              128,61,0,0,144,70,98,64,128,70,0,0,144,69,83,64,128,69,0,0,144,67,83,64,128,67,0,0,144,65,78,64,128,65,0,0,144,64,73,64,
              128,64,0,0,144,65,86,0,144,50,76,64,128,50,0,0,144,52,82,64,128,65,0,0,128,52,0,0,144,69,95,0,144,53,84,64,128,53,0,0,
              144,55,91,64,128,69,0,0,128,55,0,0,144,74,98,0,144,57,87,64,128,57,0,0,144,58,90,64,128,74,0,0,128,58,0,0,144,67,69,0,
              144,49,73,64,128,49,0,0,144,58,87,64,128,67,0,0,128,58,0,0,144,73,98,0,144,57,81,64,128,57,0,0,144,55,83,64,128,73,0,0,
              128,55,0,0,144,76,90,0,144,53,81,64,128,53,0,0,144,52,81,64,128,76,0,0,128,52,0,1,255,47,0,0,0};

        choc::midi::File mf;

        try
        {
            mf.load (testData, sizeof (testData));
            CHOC_EXPECT_EQ (2u, mf.tracks.size());

            std::string output1, output2;

            mf.iterateEvents ([&] (const choc::midi::LongMessage& m, double time)
                              {
                                  output1 += choc::text::floatToString (time, 3) + " " + m.toHexString() + "\n";
                              });

            for (auto& e : mf.toSequence())
                output2 += choc::text::floatToString (e.timeStamp, 3) + " " + e.message.toHexString() + "\n";

            // This is just a simple regression test to see whether anything changes. Update the hash number if it does.
            CHOC_EXPECT_EQ (5294939095423848520ull, simpleHash (output1));
            CHOC_EXPECT_EQ (output1, output2);
        }
        CHOC_CATCH_UNEXPECTED_EXCEPTION

        testData[51] = 0x90;

        try
        {
            mf.load (testData, sizeof (testData));
            CHOC_FAIL ("Expected a failure")
        }
        catch (...) {}
    }
}

//==============================================================================
static bool areValuesEqual (const choc::value::ValueView& v1, const choc::value::ValueView& v2)
{
    if (choc::json::toString (v1) == choc::json::toString (v2))
        return true;

    if (((v1.isObject() && v2.isObject()) || (v1.isArray() && v2.isArray()))
          && v1.getType().getNumElements() == v2.getType().getNumElements())
    {
        if (v1.isArray())
        {
            for (uint32_t i = 0; i < v1.size(); ++i)
                if (! areValuesEqual (v1[i], v2[i]))
                    return false;
        }
        else
        {
            for (uint32_t i = 0; i < v1.getType().getNumElements(); ++i)
            {
                auto m = v1.getType().getObjectMember (i);

                if (! areValuesEqual (v1[m.name], v2[m.name]))
                    return false;
            }
        }

        return true;
    }

    return false;
}

inline void testJavascriptPlatform (choc::test::TestProgress& progress, std::function<choc::javascript::Context()> createContext, bool isDuktape)
{
    {
        CHOC_TEST (Basics)

        try
        {
            auto context = createContext();

            CHOC_EXPECT_EQ (3, context.evaluateExpression ("1 + 2").get<int>());
            CHOC_EXPECT_EQ (3.5, context.evaluateExpression ("1 + 2.5").get<double>());
            CHOC_EXPECT_EQ ("hello", context.evaluateExpression ("\"hello\"").get<std::string>());

            context.run ("const x = 100; function foo() { return 200; }");
            CHOC_EXPECT_EQ (300, context.evaluateExpression ("x + foo()").get<int>());

            context.run ("const a = [1, 2, 3, [4, 5]]");
            CHOC_EXPECT_EQ ("[1, 2, 3, [4, 5]]", choc::json::toString (context.evaluateExpression ("a")));

            context.run ("const b = [1, 2, 3, { x: 123, y: 4.3, z: [2, 3], s: \"abc\" }, [4, 5], {}]");
            CHOC_EXPECT_TRUE (areValuesEqual (choc::json::parseValue (R"([1, 2, 3, { "x": 123, "y": 4.3, "z": [2, 3], "s": "abc" }, [4, 5], {}])"),
                                              context.evaluateExpression ("b")));

            auto namedChocObj = choc::value::createObject ("foo", "a", 123);
            context.run ("var c = {}; function setValue (n) { c = n; } ");
            context.invoke ("setValue", namedChocObj);
            CHOC_EXPECT_TRUE (areValuesEqual (context.evaluateExpression ("c"), namedChocObj));
            CHOC_EXPECT_EQ (std::string (context.evaluateExpression ("c").getObjectClassName()),
                            std::string (namedChocObj.getObjectClassName()));
        }
        CHOC_CATCH_UNEXPECTED_EXCEPTION
    }

    {
        CHOC_TEST (Errors)

        try
        {
            auto context = createContext();
            context.evaluateExpression ("dfgdfsg>}");
            CHOC_FAIL ("Expected an error");
        }
        catch (const choc::javascript::Error& e)
        {
            CHOC_EXPECT_TRUE (choc::text::contains (e.what(), "unexpected token in expression:")
                                || choc::text::contains (e.what(), "SyntaxError: parse error")
                                || choc::text::contains (e.what(), "Unexpected token '}'"));
        }
    }

    {
        CHOC_TEST (NativeBindings)

        try
        {
            auto context = createContext();

            context.registerFunction ("addUp", [] (choc::javascript::ArgumentList args) -> choc::value::Value
                                                {
                                                    int total = 0;
                                                    for (size_t i = 0; i < args.numArgs; ++i)
                                                        total += args.get<int>(i);

                                                    return choc::value::createInt32 (total);
                                                });

            context.registerFunction ("concat", [] (choc::javascript::ArgumentList args) -> choc::value::Value
                                                {
                                                    std::string s;

                                                    for (auto& arg : args)
                                                        s += arg.get<std::string>();

                                                    return choc::value::createString (s);
                                                });

            CHOC_EXPECT_EQ (50, context.evaluateExpression ("addUp (11, 12, 13, 14)").get<int>());
            CHOC_EXPECT_EQ (45, context.evaluateExpression ("addUp (11, 12, addUp (1, 1)) + addUp (5, 15)").get<int>());
            CHOC_EXPECT_EQ ("abcdef", context.evaluateExpression ("concat (\"abc\", \"def\")").get<std::string>());
            CHOC_EXPECT_TRUE (context.evaluateExpression ("const xx = concat (\"abc\", \"def\")").isVoid());

            CHOC_EXPECT_EQ (0,   context.invoke ("addUp").get<int>());
            CHOC_EXPECT_EQ (123, context.invoke ("addUp", 123).get<int>());
            CHOC_EXPECT_EQ (100, context.invoke ("addUp", 50, 50).get<int>());
            CHOC_EXPECT_EQ (300, context.invoke ("addUp", 100, choc::value::createInt32 (200)).get<int>());
            CHOC_EXPECT_EQ (16,  context.invoke ("addUp", 2.0, choc::value::createFloat64 (4.0), 8, 2).get<int>());

            std::vector<int> args = { 100, 1, 10 };
            CHOC_EXPECT_EQ (111, context.invokeWithArgList ("addUp", args).get<int>());

            context.run ("function appendStuff (n) { return n + \"xx\"; }");

            CHOC_EXPECT_EQ ("abcxx",  std::string (context.invoke ("appendStuff", std::string_view ("abc")).getString()));
            CHOC_EXPECT_EQ ("abcxx",  std::string (context.invoke ("appendStuff", std::string ("abc")).getString()));
            CHOC_EXPECT_EQ ("abcxx",  std::string (context.invoke ("appendStuff", "abc").getString()));
            CHOC_EXPECT_EQ ("truexx", std::string (context.invoke ("appendStuff", true).getString()));
        }
        CHOC_CATCH_UNEXPECTED_EXCEPTION
    }

    {
        CHOC_TEST (Async)

        choc::value::Value result;
        std::string error;
        choc::javascript::Context context;

        runTestOnMessageThread ([&] (const std::function<void()>& finished)
        {
            try
            {
                context = createContext();

                context.run ("var x = 1234;");

                choc::messageloop::postMessage ([&]
                {
                    context.run ("dfjdfghj.dfgdfsg()",
                        [&] (const std::string& e, const choc::value::ValueView&)
                        {
                            error = e;
                        });

                    context.run ("x + 1",
                        [&] (const std::string&, const choc::value::ValueView& r)
                        {
                            result = r;
                            finished();
                        });
                });
            }
            CHOC_CATCH_UNEXPECTED_EXCEPTION
        });

        CHOC_EXPECT_EQ ("1235", choc::json::toString (result));
        CHOC_EXPECT_TRUE (! error.empty());
    }

    if (! isDuktape)
    {
        CHOC_TEST (CustomModules)

        bool worked = false;
        choc::javascript::Context context;

        runTestOnMessageThread ([&] (const std::function<void()>& finished)
        {
            try
            {
                context = createContext();

                context.registerFunction ("success", [&] (choc::javascript::ArgumentList) -> choc::value::Value
                                                    {
                                                        worked = true;
                                                        return {};
                                                    });

                choc::messageloop::postMessage ([&]
                {
                    context.runModule (R"(
                        import * as XX from "test_module";
                        if (XX.wasOK()) success();
                        )",
                        [] (std::string_view name) -> std::optional<std::string>
                        {
                            if (name == "test_module")
                                return "export function wasOK() { return true; }";

                            return {};
                        },
                        [&] (const std::string&, const choc::value::ValueView&)
                        {
                            finished();
                        });
                });
            }
            CHOC_CATCH_UNEXPECTED_EXCEPTION
        });

        CHOC_EXPECT_TRUE (worked);
    }

    {
        CHOC_TEST (Timers)

        int result = 0;
        choc::javascript::Context context;
        choc::messageloop::Timer timer;

        runTestOnMessageThread ([&] (const std::function<void()>& finished)
        {
            try
            {
                context = createContext();
                registerTimerFunctions (context);
                context.registerFunction ("testDone", [&] (choc::javascript::ArgumentList args) -> choc::value::Value
                {
                    result = args.get<int> (0);
                    finished();
                    return {};
                });

                timer = choc::messageloop::Timer (100, [&]
                {
                    context.run (R"(
                        var result = 0;
                        var intID;

                        function i()
                        {
                            if (result == 5)
                                clearInterval (intID);
                            else
                                ++result;
                        }

                        function stop() { testDone (result); }

                        function t1() {}

                        function t2()
                        {
                            testDone (result);
                            // clearInterval (intID);
                            // setTimeout (stop, 0);
                        }

                        setTimeout (t2, 600.1);
                        setTimeout (t1, 100);
                        intID = setInterval (i, 60.2);
                    )");

                    return false;
                });
            }
            CHOC_CATCH_UNEXPECTED_EXCEPTION
        },
        [&] { timer = {}; context = {}; });

        CHOC_EXPECT_TRUE (result == 4 || result == 5);
    }

    if (! isDuktape)
    {
        CHOC_TEST (Console)

        try
        {
            auto context = createContext();

            std::string output;

            registerConsoleFunctions (context, [&] (std::string_view text, auto level)
            {
                output += text;
                output += std::to_string (static_cast<int> (level));
            });

            context.run (R"(
                console.log ("log");
                console.info ("infoa", "infob");
                console.warn ("warn");
                console.error ("error");
                console.debug ("debug");
            )");

            CHOC_EXPECT_EQ ("log0infoa1infob1warn2error3debug4", output);
        }
        CHOC_CATCH_UNEXPECTED_EXCEPTION
    }
}

inline void testJavascript (choc::test::TestProgress& progress)
{
   #if CHOC_V8_AVAILABLE
    CHOC_CATEGORY (Javascript_V8);
    testJavascriptPlatform (progress, [] { return choc::javascript::createV8Context(); }, false);
   #endif

    CHOC_CATEGORY (Javascript_Duktape);
    testJavascriptPlatform (progress, [] { return choc::javascript::createDuktapeContext(); }, true);

    CHOC_CATEGORY (Javascript_QuickJS);
    testJavascriptPlatform (progress, [] { return choc::javascript::createQuickJSContext(); }, false);
}

//==============================================================================
inline void testWebview (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (WebView);

    {
        CHOC_TEST (Javascript)

        std::string result, error1 = "x", error2, error3 = "x";
        choc::value::Value value1, value2, value3;
        std::unique_ptr<choc::ui::WebView> webview;
        choc::messageloop::Timer timer;
        bool unavailable = false;

        runTestOnMessageThread ([&] (const std::function<void()>& finished)
        {
            choc::ui::WebView::Options opts;
            opts.enableDebugMode = true;
            webview = std::make_unique<choc::ui::WebView> (opts);

            if (! webview->loadedOK())
            {
                std::cout << "WebView was unavailable" << std::endl;
                unavailable = true;
                finished();
                return;
            }

            webview->bind ("succeeded", [&] (const choc::value::ValueView& args)
            {
                result = choc::json::toString (args);
                finished();
                return choc::value::Value();
            });

            webview->evaluateJavascript ("let a = { x: [1, 2, 3], y: 987.0, z: true }; a", [&] (const std::string& error, const choc::value::ValueView& value)
            {
                error1 = error; value1 = value;
            });

            webview->evaluateJavascript ("return 1234;", [&] (const std::string& error, const choc::value::ValueView& value)
            {
                error2 = error; value2 = value;
            });

            webview->evaluateJavascript ("", [&] (const std::string& error, const choc::value::ValueView& value)
            {
                error3 = error; value3 = value;
            });

            timer = choc::messageloop::Timer (200, [&]
            {
                webview->evaluateJavascript ("succeeded (1234, 5678);");
                return false;
            });
        },
        [&] { webview.reset(); timer = {}; });

        if (unavailable)
            return;

        CHOC_EXPECT_EQ (result, "[1234, 5678]");
        CHOC_EXPECT_TRUE (error1.empty());
        CHOC_EXPECT_EQ (choc::json::toString (value1), R"({"x": [1, 2, 3], "y": 987, "z": true})");
        CHOC_EXPECT_TRUE (! error2.empty());
        CHOC_EXPECT_TRUE (value2.isVoid());
        CHOC_EXPECT_TRUE (error3.empty());
        CHOC_EXPECT_TRUE (value3.isVoid());
    }

    {
        CHOC_TEST (CustomResource);

        choc::ui::WebView::Options opts;
        opts.enableDebugMode = true;

        opts.fetchResource = [&] (const std::string& path) -> choc::ui::WebView::Options::Resource
        {
            if (path == "/")
            {
                return { R"(<!DOCTYPE html> <html>
<script>
fetch (new Request("./hello.txt"))
   .then (response => response.text())
   .then (text => succeeded (text));
</script>
</html>)",
                         "text/html" };
            }

            return { path, "text/plain" };
        };

        std::unique_ptr<choc::ui::WebView> webview;
        std::string result;

        runTestOnMessageThread ([&] (const std::function<void()>& finished)
        {
            webview = std::make_unique<choc::ui::WebView> (opts);

            if (! webview->loadedOK())
            {
                std::cout << "WebView was unavailable" << std::endl;
                finished();
                return;
            }

            webview->bind ("succeeded", [&] (const choc::value::ValueView& args)
            {
                result = choc::json::toString (args);
                finished();
                return choc::value::Value();
            });
        },
        [&] { webview.reset(); });

        CHOC_EXPECT_TRUE (choc::text::contains (result, "hello.txt"));
    }
}

//==============================================================================
inline void testCOM (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (COM);

    {
        CHOC_TEST (RefCounting)

        static int numObjs = 0;

        struct TestObj final  : public choc::com::ObjectWithAtomicRefCount<choc::com::Object, TestObj>
        {
            TestObj() { ++numObjs; }
            ~TestObj() { --numObjs; }
        };

        {
            auto t1 = choc::com::create<TestObj>();
            CHOC_EXPECT_EQ (numObjs, 1);
            auto t2 = choc::com::create<TestObj>();
            CHOC_EXPECT_EQ (numObjs, 2);
            auto t3 = t2;
            t2.reset();
            CHOC_EXPECT_EQ (numObjs, 2);
            t3 = t1;
            CHOC_EXPECT_EQ (numObjs, 1);
            auto t5 = t1;
        }

        CHOC_EXPECT_EQ (numObjs, 0);
    }

    {
        CHOC_TEST (String)

        auto s1 = choc::com::createString ("abc");
        auto s2 = choc::com::createString ("def");
        auto s3 = s1;
        CHOC_EXPECT_EQ (toString (s1), "abc");
        s1 = s2;
        CHOC_EXPECT_EQ (toString (s1), "def");
        CHOC_EXPECT_EQ (toString (s2), "def");
        CHOC_EXPECT_EQ (toString (s3), "abc");
        s1 = choc::com::createString ("zzz");
        CHOC_EXPECT_EQ (toString (s1), "zzz");
        s1 = {};
        CHOC_EXPECT_EQ (toString (s1), "");
    }
}

inline void testStableSort (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (StableSort);

    {
        CHOC_TEST (StableSort)

        for (int len = 0; len < 500; ++len)
        {
            std::vector<int> v;

            for (int i = 0; i < len; ++i)
                v.push_back (rand() & 63);

            {
                auto comp = [] (int a, int b) { return a / 2 < b / 2; };

                auto v2 = v;
                std::stable_sort (v2.begin(), v2.end(), comp);

                auto v3 = v;
                choc::sorting::stable_sort (v3.begin(), v3.end(), comp);
                CHOC_EXPECT_TRUE (v2 == v3);

                auto v4 = v;
                choc::sorting::stable_sort (v4.data(), v4.data() + v4.size(), comp);
                CHOC_EXPECT_TRUE (v2 == v4);
            }

            auto v2 = v;
            std::stable_sort (v2.begin(), v2.end());

            auto v3 = v;
            choc::sorting::stable_sort (v3.begin(), v3.end());
            CHOC_EXPECT_TRUE (v2 == v3);

            auto v4 = v;
            choc::sorting::stable_sort (v4.data(), v4.data() + v4.size());
            CHOC_EXPECT_TRUE (v2 == v4);
        }
    }
}

//==============================================================================
template <typename FileFormat, typename BufferSampleType>
inline void testAudioFileRoundTrip (choc::test::TestProgress& progress, choc::audio::BitDepth bitDepth,
                                    double sampleRate, uint32_t numChannels, uint32_t length,
                                    std::string quality, BufferSampleType maxDiff)
{
    auto compareBuffers = [&] (const auto& buffer1, const auto& buffer2)
    {
        if (maxDiff == 0)
        {
            maxDiff = 1.0f / 1000000.0f;
            if (bitDepth == choc::audio::BitDepth::int8)  maxDiff = 1.0f / 127.0f;
            if (bitDepth == choc::audio::BitDepth::int16) maxDiff = 1.0f / 32767.0f;
        }

        auto size = buffer1.getSize();
        CHOC_EXPECT_EQ (size.numFrames, buffer2.getSize().numFrames);
        CHOC_EXPECT_EQ (size.numChannels, buffer2.getSize().numChannels);

        if (size.numFrames != 0)
        {
            BufferSampleType biggestDiff = 0;

            for (decltype (size.numChannels) chan = 0; chan < size.numChannels; ++chan)
            {
                auto d1 = buffer1.getIterator (chan);
                auto d2 = buffer2.getIterator (chan);

                for (decltype (size.numFrames) i = 0; i < size.numFrames; ++i)
                    biggestDiff = std::max (biggestDiff, std::abs (*d1++ - *d2++));
            }

            CHOC_EXPECT_TRUE (biggestDiff < maxDiff);
        }
    };

    auto source = choc::buffer::ChannelArrayBuffer<BufferSampleType> (numChannels, length);

    for (uint32_t i = 0; i < numChannels; ++i)
        choc::oscillator::render<choc::oscillator::Sine<BufferSampleType>> (source.getChannel (i), 4000.0 + 1000.0 * i, sampleRate);

    FileFormat format;
    std::string file1;

    try
    {
        auto out = std::make_shared<std::ostringstream> (std::ios::binary);
        CHOC_EXPECT_FALSE (out->fail());

        choc::audio::AudioFileProperties props;
        props.bitDepth = bitDepth;
        props.sampleRate = sampleRate;
        props.numChannels = numChannels;
        props.quality = quality;

        auto writer = format.createWriter (out, props);
        CHOC_EXPECT_TRUE (writer != nullptr);

        auto s = source;

        for (;;)
        {
            auto numToDo = std::min (301u, s.getNumFrames());
            CHOC_EXPECT_TRUE (writer->appendFrames (s.getStart (numToDo)));

            if (numToDo == s.getNumFrames())
                break;

            s = s.fromFrame (numToDo);
            CHOC_EXPECT_TRUE (writer->flush());
        }

        writer.reset();
        file1 = out->str();
    }
    CHOC_CATCH_UNEXPECTED_EXCEPTION

    try
    {
        std::string padding = "1234567";
        auto in = std::make_shared<std::istringstream> (padding + file1);
        in->seekg (static_cast<std::streamoff> (padding.length()));

        choc::audio::AudioFileFormatList formats;
        formats.addFormat<choc::audio::OggAudioFileFormat<false>>();
        formats.addFormat<choc::audio::MP3AudioFileFormat>();
        formats.addFormat<choc::audio::FLACAudioFileFormat<false>>();
        formats.addFormat<choc::audio::WAVAudioFileFormat<false>>();

        auto reader = formats.createReader (in);
        CHOC_EXPECT_TRUE (reader != nullptr);

        auto& p = reader->getProperties();
        CHOC_EXPECT_EQ (p.sampleRate, sampleRate);
        CHOC_EXPECT_EQ (p.numFrames, length);
        CHOC_EXPECT_EQ (p.numChannels, numChannels);
        CHOC_EXPECT_EQ ((int) p.bitDepth, (int) bitDepth);

        auto reloaded = reader->template readEntireStream<BufferSampleType>();

        compareBuffers (reloaded, source);
    }
    CHOC_CATCH_UNEXPECTED_EXCEPTION
}

inline void testAudioFileFormat (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (AudioFileFormat);

    {
        CHOC_TEST (WAV)

        bool useDouble = false;

        for (auto bitDepth : choc::audio::FLACAudioFileFormat<true>().getSupportedBitDepths())
        {
            for (double sampleRate : { 22050.0, 44100.0, 48000.0 })
            {
                for (auto length : { 1, 2, 3, 4, 7, 15, 127, 256, 1024, 2049 })
                {
                    for (auto channels : { 1, 2, 3, 4, 6, 8 })
                    {
                        if (useDouble)
                            testAudioFileRoundTrip<choc::audio::WAVAudioFileFormat<true>, double> (progress, bitDepth, sampleRate, (uint32_t) channels, (uint32_t) length, {}, 0);
                        else
                            testAudioFileRoundTrip<choc::audio::WAVAudioFileFormat<true>, float> (progress, bitDepth, sampleRate, (uint32_t) channels, (uint32_t) length, {}, 0);

                        useDouble = ! useDouble;
                    }
                }
            }
        }
    }

    {
        CHOC_TEST (OggVorbis)

        bool useDouble = false;
        auto bitDepth = choc::audio::OggAudioFileFormat<true>().getSupportedBitDepths().front();

        for (double sampleRate : { 22050.0, 44100.0, 48000.0 })
        {
            for (auto length : { 1, 2, 3, 4, 7, 15, 127, 256, 1024, 2049 })
            {
                for (auto channels : { 1, 2, 3, 4 })
                {
                    if (useDouble)
                        testAudioFileRoundTrip<choc::audio::OggAudioFileFormat<true>, double> (progress, bitDepth, sampleRate, (uint32_t) channels, (uint32_t) length, "8", 0.07);
                    else
                        testAudioFileRoundTrip<choc::audio::OggAudioFileFormat<true>, float> (progress, bitDepth, sampleRate, (uint32_t) channels, (uint32_t) length, "10", 0.05f);

                    useDouble = ! useDouble;
                }
            }
        }
    }

    {
        CHOC_TEST (FLAC)

        bool useDouble = false;
        auto bitDepth = choc::audio::OggAudioFileFormat<true>().getSupportedBitDepths().front();

        for (double sampleRate : { 22050.0, 44100.0, 48000.0 })
        {
            for (auto length : { 1, 2, 3, 4, 7, 15, 127, 256, 1024, 2049 })
            {
                for (auto channels : { 1, 2, 3, 4 })
                {
                    if (useDouble)
                        testAudioFileRoundTrip<choc::audio::FLACAudioFileFormat<true>, double> (progress, bitDepth, sampleRate, (uint32_t) channels, (uint32_t) length, "", 0);
                    else
                        testAudioFileRoundTrip<choc::audio::FLACAudioFileFormat<true>, float> (progress, bitDepth, sampleRate, (uint32_t) channels, (uint32_t) length, "8", 0);

                    useDouble = ! useDouble;
                }
            }
        }
    }
}

inline void testTimers (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (MessageLoop);

    {
        CHOC_TEST (Timers)

        int count = 0, messageCount = 0;
        bool messageThread1 = false, messageThread2 = false;
        choc::messageloop::Timer t1, t2;

        runTestOnMessageThread ([&] (const std::function<void()>& finished)
        {
            t1 = choc::messageloop::Timer (100, [&]
            {
                return ++count != 13;
            });

            t2 = choc::messageloop::Timer (1500, [&]
            {
                if (count < 13)
                    return true;

                choc::messageloop::postMessage ([&finished, &messageCount, count]
                {
                    messageCount = count;
                    finished();
                });

                return false;
            });

            choc::messageloop::postMessage ([&] { messageThread1 = choc::messageloop::callerIsOnMessageThread(); });
            auto t = std::thread ([&] { messageThread2 = ! choc::messageloop::callerIsOnMessageThread(); });
            t.join();
        },
        [&]
        {
            t1 = {}; t2 = {};
        });

        CHOC_EXPECT_EQ (messageCount, 13);
        CHOC_EXPECT_TRUE (messageThread1);
        CHOC_EXPECT_TRUE (messageThread2);
    }
}

inline void testThreading (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (Threading);

    {
        CHOC_TEST (TaskThread)

        choc::threading::TaskThread tt1, tt2;
        std::atomic<int> numCallbacks1 { 0 }, numCallbacks2 { 0 };

        tt1.start (100, [&] { ++numCallbacks1; });
        tt2.start (0,   [&] { ++numCallbacks2; });

        std::this_thread::sleep_for (std::chrono::milliseconds (50));
        CHOC_EXPECT_EQ (0, numCallbacks2.load());
        tt2.trigger();

        for (int i = 0;; ++i)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (5));

            if (numCallbacks1 == 3)
                break;

            if (i > 100)
                CHOC_FAIL ("Expected some callbacks");
        }

        CHOC_EXPECT_EQ (1, numCallbacks2.load());
    }

    {
        CHOC_TEST (ThreadSafeFunctor)

        {
            choc::threading::ThreadSafeFunctor<std::function<void(int)>> tsf;

            int result = 0;
            tsf = [&] (int x) { result = x; };
            CHOC_EXPECT_TRUE (tsf (2));
            tsf.reset();
            CHOC_EXPECT_FALSE (tsf (3));
            CHOC_EXPECT_EQ (result, 2);
        }

        {
            choc::threading::ThreadSafeFunctor<std::function<void(size_t, size_t&)>> factorial;

            factorial = [&] (size_t n, size_t& out)
            {
                if (n == 1)
                {
                    out = 1;
                    return;
                }

                factorial (n - 1, out);

                out = n * out;
            };

            size_t result;

            CHOC_EXPECT_TRUE (factorial (4ul, result));

            CHOC_EXPECT_EQ (result, 24ul);
        }
    }
}

inline void testFileWatcher (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (FileWatcher);

    try
    {
        CHOC_TEST (Watch)

        choc::file::TempFile tempFolder ("choc_test_tmp");
        auto folder = tempFolder.file / choc::file::TempFile::createRandomFilename ("choc_test", {});
        auto testFile = folder / "blah" / "test1.txt";
        choc::file::replaceFileWithContent (testFile, "blah");
        std::filesystem::remove_all (folder / "blah");
        std::this_thread::sleep_for (std::chrono::milliseconds (100));

        std::mutex lock;
        std::string lastEvent;

        choc::file::Watcher watcher (folder, [&] (const choc::file::Watcher::Event& e)
        {
            std::scoped_lock lg (lock);

            switch (e.eventType)
            {
                case choc::file::Watcher::EventType::modified:      lastEvent += " modified"; break;
                case choc::file::Watcher::EventType::created:       lastEvent += " created"; break;
                case choc::file::Watcher::EventType::destroyed:     lastEvent += " destroyed"; break;
                case choc::file::Watcher::EventType::renamed:       lastEvent += " renamed"; break;
                case choc::file::Watcher::EventType::ownerChanged:  lastEvent += " ownerChanged"; break;
                case choc::file::Watcher::EventType::other:
                default:                                            lastEvent += " other"; break;
            }

            if (e.fileType == choc::file::Watcher::FileType::file)      lastEvent += " file ";
            if (e.fileType == choc::file::Watcher::FileType::folder)    lastEvent += " folder ";

            lastEvent += e.file.filename().string();
        });

        auto waitFor = [&] (std::string_view contentNeeded)
        {
            for (int i = 0; i < 400; ++i)
            {
                std::this_thread::sleep_for (std::chrono::milliseconds (10));
                std::scoped_lock lg (lock);

                if (choc::text::contains (lastEvent, contentNeeded))
                    return;
            }

            std::scoped_lock lg (lock);
            CHOC_FAIL ("Expected '" + std::string (contentNeeded) + "' in '" + lastEvent + "'");
        };

        choc::file::replaceFileWithContent (testFile, "blah");
        waitFor ("created folder blah");
       #ifdef __MINGW32__  // for some reason the mingw runner seems to have low-res timestamps
        std::this_thread::sleep_for (std::chrono::milliseconds (1000));
       #endif
        choc::file::replaceFileWithContent (testFile, "blah2");
        waitFor ("modified file test1.txt");
        std::filesystem::remove_all (testFile);
        waitFor ("destroyed file test1.txt");
    }
    CHOC_CATCH_UNEXPECTED_EXCEPTION
}

static std::string createRandomData (size_t length)
{
    std::ostringstream out (std::ios::binary);
    size_t total = 0;

    while (total < length)
    {
        size_t reps = 1;

        if ((rand() & 7) > 4)
            reps += (size_t) ((rand() & 7) < 5 ? (rand() & 7) : (rand() & 1023));

        auto c = rand() & 255;

        for (size_t i = 0; i < reps; ++i)
            out << (char) c;

        total += reps;
    }

    return out.str().substr (0, length);
}

static void testZLIB (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (zlib);

    {
        CHOC_TEST (Streams)

        auto performTestWithBits = [&] (size_t dataSize, choc::zlib::DeflaterStream::CompressionLevel compression,
                                        int windowBits,
                                        choc::zlib::InflaterStream::FormatType format)
        {
            const auto original = createRandomData (dataSize);

            std::string compressed;

            {
                auto out = std::make_shared<std::ostringstream> (std::ios::binary);

                {
                    choc::zlib::DeflaterStream deflater (out, compression, windowBits);
                    deflater.write (original.data(), static_cast<std::streamsize> (original.size()));
                }

                compressed = out->str();
            }

            CHOC_ASSERT (! compressed.empty());

            choc::zlib::InflaterStream in (std::make_shared<std::istringstream> (compressed, std::ios::binary), format);

            auto compareSections = [&] (size_t start, size_t len)
            {
                std::vector<char> buffer;
                buffer.resize (len);
                in.seekg (static_cast<std::istream::off_type> (start), std::ios_base::beg);
                CHOC_EXPECT_EQ (in.tellg(), static_cast<std::istream::off_type> (start));
                in.read (buffer.data(), static_cast<std::streamsize> (len));
                CHOC_EXPECT_EQ (in.gcount(), static_cast<std::istream::off_type> (len));
                CHOC_EXPECT_EQ (in.tellg(), static_cast<std::istream::off_type> (start + len));
                CHOC_EXPECT_TRUE (std::string (buffer.data(), len) == original.substr (start, len));
            };

            for (size_t i = 0; i < 50; ++i)
            {
                auto p1 = i == 0 ? 0u : ((size_t) rand() % (original.length() + 1u));
                auto p2 = i == 10 ? original.length() : ((size_t) rand() % (original.length() + 1));

                compareSections (std::min (p1, p2), std::max (p1, p2) - std::min (p1, p2));
            }
        };

        auto performTest = [&] (size_t dataSize, choc::zlib::DeflaterStream::CompressionLevel compression)
        {
            performTestWithBits (dataSize, compression, 0, choc::zlib::InflaterStream::FormatType::zlib);
            performTestWithBits (dataSize, compression, -15, choc::zlib::InflaterStream::FormatType::zlib);
        };

        for (auto compression = choc::zlib::DeflaterStream::CompressionLevel::none;
             compression <= choc::zlib::DeflaterStream::CompressionLevel::best;
             compression = (choc::zlib::DeflaterStream::CompressionLevel) (compression + 1))
        {
            performTest (0, compression);
            performTest (1, compression);
            performTest (2, compression);
            performTest (3, compression);
            performTest (128 * 1024, compression);

            for (size_t i = 0; i < 10; ++i)
                performTest (1 + (size_t) ((rand() & 8191) + (rand() & 7) * 1233), compression);
        }
    }
}

static void testZipFile (choc::test::TestProgress& progress)
{
    CHOC_CATEGORY (ZipFile);

    {
        CHOC_TEST (Decompress)

        try
        {
            struct File
            {
                std::string name, data;
            };

            auto generateFile = [] (uint32_t seed)
            {
                File file;
                auto len = std::max (1u, std::min (8192u, seed * 13));

                for (uint32_t i = 0; i < len; ++i)
                {
                    seed = ((seed * 3 + seed * 17) ^ seed) + 1;
                    file.data += (char) seed;

                    if ((seed & 4) == 0)
                        for (uint32_t n = 0; n < 1 + (seed / 10) % 30; ++n)
                            file.data += (char) seed;
                }

                file.name = "zip/folder" + std::to_string (seed % 3) + "/file" + std::to_string (seed);
                return file;
            };

            std::vector<File> files;

            for (uint32_t i = 0; i < 10; ++i)
                files.push_back (generateFile ((1 + (i * 13)) ^ ((i * 17) + i)));

            // for (auto& f : files)
            //     choc::file::replaceFileWithContent ("/Users/jules/Desktop/" + file.name, file.data);

            auto zipped = choc::file::loadFileAsString ((std::filesystem::path (__FILE__).parent_path() / "test.zip").string());
            auto stream = std::make_shared<std::istringstream> (zipped, std::ios::binary);

            auto check = [&] (const File& f)
            {
                for (auto& original : files)
                {
                    if (original.name == f.name)
                    {
                        CHOC_EXPECT_EQ (original.data, f.data);
                        return;
                    }
                }

                CHOC_FAIL ("File not found");
            };

            choc::zip::ZipFile z (stream);

            for (auto& item : z.items)
            {
                if (item.isFolder())
                    continue;

                File f;
                f.name = item.filename;

                auto reader = item.createReader();

                f.data.resize (item.uncompressedSize);

                size_t numRead = 0;

                while (numRead < item.uncompressedSize)
                {
                    std::istream::char_type c;
                    CHOC_EXPECT_TRUE (static_cast<bool> (reader->get (c)));
                    f.data[numRead++] = c;
                }

                check (f);
            }
        }
        CHOC_CATCH_UNEXPECTED_EXCEPTION
    }
}

//==============================================================================
static void testHTTPServer (choc::test::TestProgress& progress)
{
    (void) progress;

   #if CHOC_ENABLE_HTTP_SERVER_TEST
    struct TestClient
    {
        TestClient (std::string host, uint16_t port, std::function<void (std::string_view)> onMessage)
            : onMessageRead (std::move (onMessage))
        {
            // Look up the domain name
            auto results = resolver.resolve (host, std::to_string (port));

            // Make the connection on the IP address we get from a lookup
            boost::asio::connect (ws.next_layer(), results.begin(), results.end());

            // Set a decorator to change the User-Agent of the handshake
            ws.set_option (boost::beast::websocket::stream_base::decorator(
                [] (boost::beast::websocket::request_type& req)
                {
                    req.set (boost::beast::http::field::user_agent,
                        std::string (BOOST_BEAST_VERSION_STRING) + " websocket-client-coro");
                }));

            // Perform the websocket handshake
            ws.handshake (host, "/test123");
            ws.async_read (destBuffer, [this] (auto code, auto bytes) { readMessage (code, bytes); });

            std::atomic<bool> threadStarted { false };

            connection = std::thread ([this, &threadStarted]
            {
                try
                {
                    threadStarted = true;

                    while (! threadShouldExit)
                        ioc.run();
                }
                catch (std::exception const&) {}
            });

            // Wait for thread to be waiting for events
            while (! threadStarted)
                std::this_thread::sleep_for (std::chrono::milliseconds (1));
        }

        ~TestClient()
        {
            try
            {
                threadShouldExit = true;
                ioc.stop();
                ws.close (boost::beast::websocket::close_code::normal);
            }
            catch (std::exception const& e)
            {
                std::cerr << "Error: " << e.what() << std::endl;
            }

            connection.join();
        }

        void send (std::string text)
        {
            ws.write (boost::asio::buffer (text));
        }

        boost::asio::io_context ioc;
        boost::asio::ip::tcp::resolver resolver { ioc };
        boost::beast::websocket::stream<boost::asio::ip::tcp::socket> ws { ioc };
        boost::beast::flat_buffer destBuffer;

        std::function<void (std::string_view)> onMessageRead;
        std::thread connection;
        std::atomic<bool> threadShouldExit { false };

        void readMessage (boost::beast::error_code ec, std::size_t numBytes)
        {
            if (numBytes > 0 && ! ec)
            {
                onMessageRead (boost::beast::buffers_to_string (destBuffer.data()));
                destBuffer.clear();

                // Prepare another read callback to be processed by the thread
                ws.async_read (destBuffer,
                            [this] (auto code, auto bytes) { readMessage (code, bytes); });
            }
        }
    };

    using namespace std::chrono_literals;

    struct Test
    {
        std::string serverString, clientString, handshake;
        std::atomic<bool> clientConnected { false };
        std::atomic<int> messagecount { 0 };
    };

    Test testStatus;

    {
        struct TestClientInstance  : public choc::network::HTTPServer::ClientInstance
        {
            TestClientInstance (Test& t) : test (t)
            {
                test.clientConnected = true;
            }

            void handleWebSocketMessage (std::string_view m) override
            {
                test.serverString += m;
                sendWebSocketMessage (std::string (m));
            }

            void upgradedToWebSocket (std::string_view path) override
            {
                test.handshake = path;
            }

            choc::network::HTTPContent getHTTPContent (std::string_view) override  { return {}; }
            Test& test;
        };

        choc::network::HTTPServer server;

        if (server.open ("127.0.0.1", 8080, 0,
                         [&] { return std::make_unique<TestClientInstance> (testStatus); }, {}))
        {
            TestClient client (server.getHost(),
                               server.getPort(),
                               [&] (std::string_view m)
                               {
                                   testStatus.clientString += m;
                                   ++testStatus.messagecount;
                               });

            {
                auto start = std::chrono::system_clock::now();

                while (! testStatus.clientConnected)
                {
                    if ((std::chrono::system_clock::now() - start) >= 30s)
                        break;

                    std::this_thread::sleep_for (1ms);
                }

                client.send ("Hello ");
                client.send ("world!");
            }

            {
                auto start = std::chrono::system_clock::now();

                while (testStatus.messagecount < 2)
                {
                    if ((std::chrono::system_clock::now() - start) >= 30s)
                        break;

                    std::this_thread::sleep_for (1ms);
                }
            }
        }
    }

    {
        CHOC_CATEGORY (HTTPServer)
        CHOC_TEST (Websocket)
        CHOC_EXPECT_TRUE (testStatus.clientConnected)
        CHOC_EXPECT_EQ (testStatus.clientString, "Hello world!")
        CHOC_EXPECT_EQ (testStatus.serverString, "Hello world!")
        CHOC_EXPECT_EQ (testStatus.handshake, "/test123")
    }
   #endif
}

//==============================================================================
inline bool runAllTests (choc::test::TestProgress& progress, bool multithread)
{
    choc::threading::TaskThread emergencyKillThread;
    int secondsElapsed = 0;

    emergencyKillThread.start (1000, [&]
    {
         if (++secondsElapsed > 300)
         {
            std::cerr << "FAIL!! Tests timed out and were killed!" << std::endl;
            std::terminate();
         }

         return true;
    });

    choc::messageloop::initialise();

    std::function<void(choc::test::TestProgress&)> testFunctions[] =
    {
        testHTTPServer,
        testZLIB,
        testZipFile,
        testFileWatcher,
        testPlatform,
        testContainerUtils,
        testStringUtilities,
        testFileUtilities,
        testValues,
        testJSON,
        testMIDI,
        testAudioBuffers,
        testIntToFloat,
        testFIFOs,
        testMIDIFiles,
        testJavascript,
        testWebview,
        testCOM,
        testStableSort,
        testAudioFileFormat,
        testThreading,
        testTimers
    };

    auto t = std::thread ([&]
    {
        if (multithread)
        {
            std::vector<std::future<void>> futures;
            std::mutex progressLock;

            for (auto& fn : testFunctions)
            {
                futures.emplace_back (std::async (std::launch::async, [fn, &progress, &progressLock]
                {
                    std::ostringstream testOutput;
                    choc::test::TestProgress p;
                    p.printMessage = [&] (std::string_view m) { testOutput << m << "\n"; };
                    fn (p);

                    std::scoped_lock lock (progressLock);

                    progress.print (choc::text::trimEnd (testOutput.str()));
                    progress.numPasses += p.numPasses;
                    progress.numFails += p.numFails;

                    for (auto& failed : p.failedTests)
                        progress.failedTests.push_back (failed);
                }));
            }

            for (auto& f : futures)
                f.wait();
        }
        else
        {
            for (auto& fn : testFunctions)
                fn (progress);

        }

        choc::messageloop::stop();
    });

    choc::messageloop::run();
    t.join();

    progress.printReport();
    return progress.numFails == 0;
}

}

#endif
