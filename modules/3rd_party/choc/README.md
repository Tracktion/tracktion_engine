## CHOC: _"Classy Header Only Classes"_ [^1]

A random grab-bag of header-only, dependency-free C++ classes.

When you start trying to write a C++ project of any size, you'll soon start finding gaping holes in what the C++ standard library provides. To fill them, you'll end up either writing lots of helper functions yourself, or trawling the web for awful 3rd-party libraries which come with all kinds of baggage like messy build requirements, scads of stupid compiler warnings, awkward licenses, etc.

I got tired of re-implementing many of the same little helper classes and functions in different projects, so CHOC is an attempt at preventing future wheel-reinvention, by providing some commonly-needed things in a format that makes it as frictionless as possible for anyone to use this code in any kind of project.

So with that goal in mind, the rules for everything in CHOC are:

 - It's strictly header-only. Just `#include` the file you want, and you're done. Nothing needs adding to your build system to use any of this stuff.
 - It's all permissively ISC licensed. It should pass silently under the noses of even the most zealous legal teams.
 - Each file is as self-contained as possible. If you only need a couple of these classes, you should be able to cherry-pick at most a few CHOC headers into your project and not need to drag the whole repo along.
 - Clean, consistent, concise, modern C++. Not too simple. Not too over-generic or fancy. Easy to skim-read and find what you're looking for. Self-documenting where possible, with decent comments.

Basically CHOC is aimed at people (like me) who just want to use some decent library code without also spending their afternoon fighting CMake or looking up the right compiler flags to make the dratted thing work.

-----------------------------------------------------------------------

### Highlights

The library is getting quite big now. Some of its many delights include:

##### Miscellaneous

- A tiny [platform-detection](./platform/choc_Platform.h) header.
- A fast, round-trip-accurate [float/double to string converter](./text/choc_FloatToString.h).
- Helpers for reading writing data with different [endianness](./platform/choc_Endianness.h).
- Some headers which will [disable and reenable warnings](./platform/choc_DisableAllWarnings.h) for times when you have to include messy 3rd-party code in your otherwise faultless codebase.
- The world's simplest [unit test framework](./tests/choc_UnitTest.h). I mainly wrote this so that CHOC can self-host its own unit-tests without any external dependencies, but have found it surprisingly useful for something that's about 100 lines of code.
- Cross-platform [dynamic library loading](./platform/choc_DynamicLibrary.h).
- Various maths and bit-twiddling [bits and bobs](./math/choc_MathHelpers.h).

##### Text and Files

- [Utterly basic string stuff](./text/choc_StringUtilities.h) like trimming, splitting, joining, comparisons, etc. For god's sake, I shouldn't need to write my own library just to trim a string...
- Some [more esoteric](./text/choc_StringUtilities.h) string utilities like pretty-printing durations and sizes, URI encoding, etc.
- Some [UTF8](./text/choc_UTF8.h) validation and iteration classes.
- Some [file utilities](./text/choc_Files.h) to provide single-function-call ways to do obvious things like loading a file's content, or saving a string into a file, creating self-deleting temp files, etc.
- A [CodePrinter](./text/CodePrinter.h) class to help creating indented code listings.
- A [HTML generator](./text/choc_HTML.h) for creating a tree of DOM objects and generating HTML text for it
- A [text table generator](./text/choc_TextTable.h), which can take an array of strings and tabulate it to align the columns nicely.
- A [file wildcard](./text/choc_Wildcard.h) matcher. I claim this is the cleanest possible implementation of this algorithm - I challenge you to prove me wrong!
- A [base64](./text/choc_Base64.h) encoder/decoder.
- An implementation of the [xxHash](./text/choc_xxHash.h) very-fast-but-pretty-secure hash algorithm.

##### Containers

- A [span](./containers/choc_Span.h) class to fill the gap until we can finally use `std::span`.
- Some [type and value](./containers/choc_Value.h) classes which can represent typed values, but also build them dynamically, serialise them to a compact binary format (or as JSON).
- A handy [SmallVector](./containers/choc_SmallVector.h) class which offers a std::vector interface but has pre-allocated internal storage.
- One of those [aligned memory block](./containers/choc_AlignedMemoryBlock.h) classes that you always end up needing for some reason.
- Everyone hates COM, but sometimes you need some [COM helper classes](./containers/choc_COM.h) to hide the ugliness.
- A range of atomic FIFOs, and a handy [variable size object FIFO](./containers/choc_VariableSizeFIFO.h) for handling queues of heterogenous objects without locking.
- A lock-free [dirty list](./containers/choc_DirtyList.h) for efficiently queueing up objects that need some attention.

##### Scripting and JSON

- A [JSON](./text/choc_JSON.h) parser that uses choc::value::Value objects.
- A [Javascript](./javascript/choc_javascript.h) interpreter (which is a header-only encapsulation of the duktape library).

##### Audio

- Some [audio buffer classes](./audio/choc_SampleBuffers.h) for managing blocks of multi-channel sample data. These can flexibly handle both owned buffers and non-owned views in either packed/interleaved or separate-channel formats.
- Utility classes for handling [MIDI messages](./audio/choc_MIDI.h), [MIDI sequences](./audio/choc_MIDISequence.h) and [MIDI files](./.audio/choc_MIDIFile.h).
- Some basic audio utilities like simple [oscillators](./audio/choc_Oscillators.h).
- A [sinc interpolator](./audio/choc_SincInterpolator.h)
- A [MIDI/audio block sync](./audio/choc_AudioMIDIBlockDispatcher.h) mechanism.
- Functions for packing/unpacking [integer sample data](./audio/choc_AudioSampleData.h) to floats.

-----------------------------------------------------------------------

Hopefully some of you will find some of these things useful! If you like it, please tell your friends! If you think you're up to contributing, that's great, but be aware that anything other than an utterly immaculate pull request will be given short shrift :)

-- Jules



[^1]: ...or maybe "*Clean* Header-Only Classes" ...or "*Cuddly* Header-Only Classes"... It's just a backronym, so feel free to pick whatever C-word feels right to you. I may change the word occasionally on a whim, just to cause annoying diffs in the codebase.
