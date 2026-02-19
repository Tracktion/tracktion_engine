# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is the CHOC library - "Classy Header-Only Classes" - a collection of header-only, dependency-free C++ classes designed to fill gaps in the standard library. The library is organized into multiple modules covering audio, containers, GUI, JavaScript, memory management, networking, platform utilities, text processing, and threading.

## Architecture

- **Header-only library**: All functionality is contained in header files, no compilation required
- **Modular design**: Each header is as self-contained as possible
- **Modern C++**: Requires C++17 minimum, with C++20 features used where available
- **Cross-platform**: Supports Windows, macOS, and Linux
- **Dependency-free**: Core functionality requires no external libraries
- **ISC Licensed**: Very permissive open-source license

### Key Directories

- `choc/` - Main library source code directory containing all header files
  - `choc/audio/` - Audio processing, MIDI handling, file formats, sample buffers
    - `choc/audio/io/` - Audio/MIDI device abstraction layer (RtAudio-based)
  - `choc/containers/` - Data structures like Value, SmallVector, FIFOs, Span
  - `choc/gui/` - WebView, message loops, desktop window management
  - `choc/javascript/` - JavaScript engine integration (V8, QuickJS, Duktape)
  - `choc/math/` - Mathematical helpers and utilities
  - `choc/memory/` - Memory utilities, allocators, encoding/decoding
  - `choc/network/` - HTTP server, WebSocket support
  - `choc/platform/` - Platform detection, dynamic libraries, file watching
  - `choc/text/` - String utilities, JSON, HTML generation, file operations
  - `choc/threading/` - Thread-safe containers, task management
- `tests/` - Unit test framework and all test implementations

## Build System

Uses CMake for building tests and examples. Main build configuration is in `tests/CMakeLists.txt`.

### Common Commands

```bash
# Build tests (from project root)
cd tests
mkdir build && cd build
cmake ..
make

# Run all tests
./choc_tests

# Run WebView demo
./choc_tests webview

# Run WebServer demo
./choc_tests webserver

# Run tests with multithreading
./choc_tests --multithread

# Clean whitespace (run from project root)
python3 tidy_whitespace.py
```

### Build Requirements

- CMake 3.16+
- C++17 compiler (C++20 preferred)
- Platform-specific dependencies:
  - **macOS**: WebKit, CoreServices, CoreAudio, CoreMIDI frameworks
  - **Linux**: gtk+-3.0, webkit2gtk-4.1, ALSA, JACK
  - **Windows**: No additional dependencies for basic functionality

### Optional Dependencies

- **V8**: Set `V8_LOCATION` and `V8_BUILD_NAME` in CMake for V8 JavaScript engine support
  - Enables `CHOC_V8_AVAILABLE=1` preprocessor flag
  - Example: `V8_LOCATION="/path/to/v8"` and `V8_BUILD_NAME="arm64.release"`
- **Boost**: Set `BOOST_LOCATION` in CMake for HTTP server functionality
  - Enables `CHOC_ENABLE_HTTP_SERVER_TEST=1` preprocessor flag
  - Required for WebServer demo and HTTP server tests

## Development Workflow

### Testing

The project uses its own minimal unit test framework defined in `choc/platform/choc_UnitTest.h`. All tests are defined in `tests/choc_tests.h` and run via `main.cpp`.

#### Test Macros
- `CHOC_CATEGORY(name)` - Start a new test category
- `CHOC_TEST(name)` - Define a test within a category
- `CHOC_EXPECT_TRUE(condition)` - Assert condition is true
- `CHOC_EXPECT_FALSE(condition)` - Assert condition is false
- `CHOC_EXPECT_EQ(a, b)` - Assert equality with detailed error messages
- `CHOC_EXPECT_NE(a, b)` - Assert inequality
- `CHOC_EXPECT_NEAR(a, b, tolerance)` - Assert floating-point near-equality
- `CHOC_FAIL(message)` - Explicitly fail a test with message

### Code Style

- Strict compiler warnings enabled (`-Werror -Wall -Wextra` and many others)
- Uses sanitizers when available (undefined behavior sanitizer with Clang)
- Code formatting enforced by `tidy_whitespace.py` (converts tabs to 4 spaces, removes trailing whitespace)
- No external dependencies for core functionality

### Platform-Specific Notes

- WebView functionality uses Edge WebView2 on Windows (embedded as header data)
- JavaScript engines are optionally included as single-header implementations
- Audio I/O uses platform-native APIs (CoreAudio, ALSA, JACK, DirectSound)

## Coding Style Guidelines

The CHOC codebase follows consistent C++ coding conventions. When contributing or modifying code, adhere to these patterns:

### Naming Conventions

- **Classes/Structs**: PascalCase (e.g., `WebView`, `SampleBuffer`, `StringUtilities`)
- **Functions/Methods**: camelCase (e.g., `getDescription()`, `trimStart()`, `getSample()`)
- **Variables**: camelCase (e.g., `numChannels`, `frameCount`, `textToTrim`)
- **Template Parameters**: PascalCase (e.g., `SampleType`, `LayoutType`)
- **Constants/Enums**: camelCase or PascalCase (e.g., `maxNumVectorElements`, `MainType`)
- **Namespaces**: lowercase with `::` separators (e.g., `choc::value`, `choc::text`, `choc::buffer`)

### Function Declaration Style

**Single-line parameters** (when arguments fit comfortably):
```cpp
bool contains (ChannelCount channel, FrameCount frame) const
static void check (bool condition, const char* errorMessage)
std::string trim (std::string textToTrim)
```

**Multi-line parameters** (for longer function signatures):
```cpp
template <typename StringType, typename... OtherReplacements>
[[nodiscard]] std::string replace (StringType textToSearch,
                                   std::string_view firstSubstringToReplace,
                                   std::string_view firstReplacement,
                                   OtherReplacements&&... otherPairsOfStringsToReplace);
```

**Parameter alignment**: When breaking function parameters across multiple lines, subsequent parameters must be aligned with the first parameter (not just indented). All parameters after the first line should start at the same column as the first parameter.

**Parentheses spacing**:
- **With parameters**: ALWAYS include a space between function name and opening parenthesis
- **No parameters**: NO space between function name and empty parentheses
- Space after commas in parameter lists

Examples:
```cpp
// Functions with parameters - space before opening paren
bool contains (ChannelCount channel, FrameCount frame) const
static void check (bool condition, const char* errorMessage)
std::string trim (std::string textToTrim)

// Functions with no parameters - no space before opening paren
size_t getElementSize() const
Value deserialise() const
void freeAllocatedData()
```

### Control Structure Spacing

**Control structures** (`if`, `for`, `while`, `switch`, `catch`) ALWAYS require a space before the opening parenthesis:

```cpp
// Correct - space before opening parenthesis
if (condition)
for (int i = 0; i < count; ++i)
while (isRunning)
switch (value)
catch (const std::exception& e)

// Incorrect - no space before opening parenthesis
if(condition)          // WRONG
for(int i = 0; i < count; ++i)  // WRONG
while(isRunning)       // WRONG
switch(value)          // WRONG
catch(const std::exception& e)  // WRONG
```

**Logical NOT operator** (`!`) ALWAYS requires a space after it:

```cpp
// Correct - space after logical NOT
if (! condition)
while (! isRunning)
if (! array || ! array->isValid())

// Incorrect - no space after logical NOT
if (!condition)        // WRONG
while (!isRunning)     // WRONG
if (!array || !array->isValid())  // WRONG
```

**Multi-line control structure spacing**: When a multi-line `if`, `for`, `while`, or `switch` statement follows a line with text (not just an open brace), add a blank line before it. Similarly, add a blank line after closing braces when followed by text (not just another close brace). **Exception:** Do not add blank lines before `catch` or `else` statements:

```cpp
// Correct - blank line before multi-line control structure
auto result = calculateSomething();

if (condition)
{
    // multi-line block
    doSomething();
    doSomethingElse();
}

processResult(result);

// Correct - blank line after block when followed by text
if (condition)
{
    doSomething();
}

auto nextValue = getNext();

// Correct - no blank line when followed by another close brace
if (outerCondition)
{
    if (innerCondition)
    {
        doSomething();
    }
}

// Correct - no blank line before catch or else
try
{
    doSomething();
}
catch (const std::exception& e)
{
    handleError();
}

if (condition)
{
    doSomething();
}
else
{
    doSomethingElse();
}

// Incorrect - missing blank line before control structure
auto result = calculateSomething();
if (condition)           // WRONG - needs blank line above
{
    doSomething();
}

// Incorrect - missing blank line after block
if (condition)
{
    doSomething();
}
processResult(result);   // WRONG - needs blank line above
```

### Formatting Patterns

**Braces**: Opening brace on same line for functions, control structures:
```cpp
inline std::string trimStart (std::string text)
{
    auto i = text.begin();
    // implementation
}

if (condition)
{
    // code
}
```

**Indentation**: 4 spaces (no tabs)

**Line length**: Generally keep lines under ~120 characters, break long parameter lists across multiple lines

**const correctness**: Extensive use of `const` methods and parameters:
```cpp
constexpr FrameCount size() const { return end - start; }
bool contains (ChannelCount index) const { return index >= start && index < end; }
```

**Template style**: Template parameters on separate line for complex declarations:
```cpp
template <typename SampleType, template<typename> typename LayoutType>
struct AllocatedBuffer;
```

### Code Organization

- Use `[[nodiscard]]` attribute for functions returning values that shouldn't be ignored
- Prefer `constexpr` for compile-time evaluable functions
- Use `noexcept` where appropriate
- Static assertions for template parameter validation
- Extensive use of `inline` for header-only implementation

### Documentation Style

- Use `///` for public API documentation
- Brief, descriptive comments explaining purpose
- Document template parameters and complex algorithms
- Include usage examples in class headers where helpful

### Note on Third-Party Code

Some files contain embedded third-party libraries (like RtAudio, JavaScript engines). These sections may not follow the above conventions and should be left as-is to maintain compatibility with upstream sources.