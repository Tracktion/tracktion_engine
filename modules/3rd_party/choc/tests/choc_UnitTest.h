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

#ifndef CHOC_UNITTEST_HEADER_INCLUDED
#define CHOC_UNITTEST_HEADER_INCLUDED

#include <iostream>
#include <vector>
#include <string>
#include <functional>

/**
    Maybe the world's smallest, simplest unit-test framework :)

    This was written simply as a very lightweight, dependency-free framework to run choc's
    own internal set of unit-tests. Feel free to use it for your own testing needs, but
    many other more feature-rich testing frameworks are available!

    For examples of how to structure a test and use the macros, have a look at the
    choc_tests.h file.
*/

namespace choc::test
{

/// Keeps track of the number of passes/fails for a test-run.
struct TestProgress
{
    void startCategory (std::string category);
    void startTest (std::string_view testName);
    void endTest();
    void fail (const char* filename, int lineNumber, std::string_view message);
    void check (bool, const char* filename, int lineNumber, std::string_view failureMessage);
    void print (std::string_view);

    /// Call this at the end to print out the number of passes and fails
    void printReport();

    /// This is used by the print() function to emit progress messages as the tests run.
    /// If you don't supply your own funciton here, the default will write to std::cout
    std::function<void(std::string_view)> printMessage;

    std::string currentCategory, currentTest;
    int numPasses = 0, numFails = 0;
    std::vector<std::string> failedTests;

private:
    bool currentTestFailed = false;
};

// Some macros to use to perform tests. If you want to use this to write your own tests,
// have a look at the tests for choc functions, and it should be pretty obvious how the
// macros are supposed to be used.

#define CHOC_CATEGORY(category)          progress.startCategory (#category);
#define CHOC_TEST(name)                  choc::test::ScopedTest scopedTest_ ## __LINE__ (progress, #name);
#define CHOC_FAIL(message)               progress.fail (__FILE__, __LINE__, message);
#define CHOC_EXPECT_TRUE(b)              progress.check (b, __FILE__, __LINE__, "Expected " #b);
#define CHOC_EXPECT_FALSE(b)             progress.check (! (b), __FILE__, __LINE__, "Expected ! " #b);
#define CHOC_EXPECT_EQ(a, b)             { auto x = a; auto y = b; progress.check (x == y, __FILE__, __LINE__, "Expected " #a " (" + choc::test::convertToString (x) + ") == " #b " (" + choc::test::convertToString (y) + ")"); }
#define CHOC_EXPECT_NE(a, b)             { auto x = a; auto y = b; progress.check (x != y, __FILE__, __LINE__, "Expected " #a " (" + choc::test::convertToString (x) + ") != " #b); }
#define CHOC_EXPECT_NEAR(a, b, diff)     { auto x = a; auto y = b; auto d = diff; progress.check (std::abs (x - y) <= d, __FILE__, __LINE__, #a " (" + choc::test::convertToString (x) + ") and " #b " (" + choc::test::convertToString (y) + ") differ by more than " + choc::test::convertToString (d)); }
#define CHOC_CATCH_UNEXPECTED_EXCEPTION  catch (...) { CHOC_FAIL ("Unexpected exception thrown"); }



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

inline void TestProgress::print (std::string_view message)
{
    if (printMessage != nullptr)
        printMessage (message);
    else
        std::cout << message << std::endl;
}

inline void TestProgress::startCategory (std::string category)
{
    currentCategory = std::move (category);
}

inline void TestProgress::startTest (std::string_view testName)
{
    CHOC_ASSERT (! currentCategory.empty());
    currentTest = currentCategory + "/" + std::string (testName);
    currentTestFailed = false;
    print ("[ RUN      ] " + currentTest);
}

inline void TestProgress::endTest()
{
    if (currentTestFailed)
    {
        print ("[     FAIL ] " + currentTest);
        ++numFails;
        failedTests.push_back (currentTest);
    }
    else
    {
        print ("[       OK ] " + currentTest);
        ++numPasses;
    }

    currentTest = {};
}

inline void TestProgress::fail (const char* filename, int lineNumber, std::string_view message)
{
    currentTestFailed = true;
    CHOC_ASSERT (! currentTest.empty());
    print ("FAILED: " + std::string (filename) + ":" + std::to_string (lineNumber));
    print (message);
}

inline void TestProgress::check (bool condition, const char* filename, int lineNumber, std::string_view message)
{
    if (! condition)
        fail (filename, lineNumber, message);
}

inline void TestProgress::printReport()
{
    print ("========================================================");
    print (" Passed:      " + std::to_string (numPasses));
    print (" Failed:      " + std::to_string (numFails));

    for (auto& failed : failedTests)
        print ("  Failed test: " + failed);

    print ("========================================================");
}

struct ScopedTest
{
    ScopedTest (TestProgress& p, std::string name) : progress (p)   { progress.startTest (std::move (name)); }
    ~ScopedTest()                                                   { progress.endTest(); }

    TestProgress& progress;
};

template <typename Type>
std::string convertToString (Type n)
{
    if constexpr (std::is_same<const Type, const char* const>::value)           return std::string (n);
    else if constexpr (std::is_same<const Type, const std::string>::value)      return n;
    else if constexpr (std::is_same<const Type, const std::string_view>::value) return std::string (n);
    else                                                                        return std::to_string (n);
}

} // namespace choc::test

#endif // CHOC_UNITTEST_HEADER_INCLUDED
