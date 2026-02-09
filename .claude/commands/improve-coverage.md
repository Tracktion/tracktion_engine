Improve test coverage for the tracktion_engine project. If the user provides a directory argument, scope the analysis to that directory; otherwise analyse the entire engine.

**User-specified scope:** `$ARGUMENTS` (optional — relative path under `modules/`, e.g. `tracktion_engine/model/clips`)

## Steps

### 1. Build the coverage target

```bash
cmake -B tests/coverage/build -S tests/coverage
cmake --build tests/coverage/build --target coverage -j $(sysctl -n hw.ncpu)
```

If the build fails, read the error output and fix the issue before continuing.

### 2. Run the test binary to generate profiling data

```bash
tests/coverage/build/coverage_artefacts/coverage
```

The binary will produce `.gcda` files alongside the `.gcno` files already in the build tree.

### 3. Capture coverage with lcov

```bash
/opt/homebrew/bin/lcov \
  --capture \
  --directory tests/coverage/build \
  --output-file tests/coverage/build/coverage.info \
  --ignore-errors inconsistent,inconsistent
```

### 4. Filter the coverage data

Always remove third-party and JUCE code:

```bash
/opt/homebrew/bin/lcov \
  --remove tests/coverage/build/coverage.info \
  '*/3rd_party/*' '*/juce/*' '*/doctest/*' '/usr/*' '*/tests/*' \
  --output-file tests/coverage/build/coverage_filtered.info \
  --ignore-errors inconsistent,inconsistent,unused,unused
```

If `$ARGUMENTS` is provided (non-empty), further extract only that directory:

```bash
/opt/homebrew/bin/lcov \
  --extract tests/coverage/build/coverage_filtered.info \
  "*/$ARGUMENTS/*" \
  --output-file tests/coverage/build/coverage_filtered.info \
  --ignore-errors inconsistent,inconsistent,unused,unused
```

### 5. Generate the HTML report

```bash
/opt/homebrew/bin/genhtml \
  tests/coverage/build/coverage_filtered.info \
  --output-directory tests/coverage/build/html_report \
  --ignore-errors inconsistent,inconsistent
```

Tell the user: "Coverage report generated at `tests/coverage/build/html_report/index.html`"

### 6. Identify coverage gaps

Parse the lcov summary to find the **5 source files with the lowest line-coverage percentage**. To do this, run:

```bash
/opt/homebrew/bin/lcov --summary tests/coverage/build/coverage_filtered.info --ignore-errors inconsistent,inconsistent
```

Also run this to list per-file coverage, then sort by coverage percentage ascending:

```bash
/opt/homebrew/bin/lcov --list tests/coverage/build/coverage_filtered.info --ignore-errors inconsistent,inconsistent
```

From the per-file list, pick the **3–5 implementation files** (`.cpp`, not `.h`) with the lowest coverage that are **not** test files and **not** trivially small. Prefer files that have meaningful, testable logic (not just boilerplate or forwarding).

### 7. Analyse uncovered code

For each of the selected files, read the source file and understand:
- What functions/methods are untested
- What the code does
- What test scenarios would exercise the uncovered paths

Use the `gcov` annotated output or read the HTML report to see exact uncovered lines if helpful:

```bash
# Optional: get per-line annotation for a specific file
gcov -o tests/coverage/build/CMakeFiles/coverage.dir/path/to/file.cpp.gcno path/to/file.cpp
```

### 8. Plan tests

For each gap, propose specific test cases. Follow these **project conventions**:

#### Test framework choice
- **Prefer doctest** for all new test files. Use `TEST_SUITE("tracktion_engine") { TEST_CASE("...") { ... } }`.
- **Only use `juce::UnitTest`** if you are adding tests to an _existing_ file that already uses that style, to maintain consistency.

#### Test file naming and location
- Name: `tracktion_ClassName.test.cpp`, co-located next to the source file being tested.

#### Preprocessor guard
- Every test file must be wrapped in:
  ```cpp
  #if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLASSNAME
  // tests here
  #endif
  ```

#### doctest template (preferred for new files)

```cpp
/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if TRACKTION_UNIT_TESTS && ENGINE_UNIT_TESTS_CLASSNAME

#include <tracktion_engine/../3rd_party/doctest/tracktion_doctest.hpp>

namespace tracktion::inline engine
{

TEST_SUITE ("tracktion_engine")
{
    TEST_CASE ("ClassName: description of test")
    {
        auto& engine = *Engine::getEngines()[0];
        auto edit = test_utilities::createTestEdit (engine);

        // test logic here
        CHECK (condition);
        CHECK_EQ (actual, expected);
    }
}

} // namespace tracktion::inline engine

#endif
```

#### juce::UnitTest template (only for extending existing juce::UnitTest files)

```cpp
class ClassNameTests : public juce::UnitTest
{
public:
    ClassNameTests() : juce::UnitTest ("ClassName", "tracktion_engine") {}

    void runTest() override
    {
        beginTest ("Description");
        {
            // test logic
            expect (condition);
            expectEquals (actual, expected);
        }
    }
};

static ClassNameTests classNameTests;
```

#### Test utilities
Use helpers from `tracktion_TestUtilities.h`:
- `test_utilities::createTestEdit (engine)` — creates a blank Edit for testing
- `test_utilities::expectPeak (ut, edit, range, tracks, expectedPeak)` — checks audio output
- `test_utilities::renderToAudioBuffer (edit)` — renders Edit to buffer

#### Test config registration
Add a new `#define ENGINE_UNIT_TESTS_CLASSNAME 1` to `modules/tracktion_core/tracktion_TestConfig.h`, in the `// Defined in tracktion_engine` section, keeping alphabetical order.

#### Compilation unit registration
The new `.test.cpp` file must be `#include`-ed from the correct module compilation unit:

| Source directory             | Compilation unit file                                |
|------------------------------|------------------------------------------------------|
| `model/` (clips, edit, etc.) | `tracktion_engine_model_1.cpp` or `_model_2.cpp`     |
| `playback/`                  | `tracktion_engine_playback.cpp`                       |
| `plugins/`                   | `tracktion_engine_plugins.cpp`                        |
| `audio_files/`               | `tracktion_engine_audio_files.cpp`                    |
| `selection/`, `testing/`     | `tracktion_engine_utils.cpp`                          |
| `midi/`                      | `tracktion_engine_midi.cpp`                           |
| `timestretch/`               | `tracktion_engine_timestretch.cpp`                    |
| `utilities/`                 | `tracktion_engine_utils.cpp`                          |
| `project/`                   | `tracktion_engine_model_2.cpp`                        |

Look at the existing compilation unit to find the right place to add the `#include`.

### 9. Present the plan

Show the user:
1. A **coverage summary table** — file name, current line coverage %, number of uncovered lines
2. For each file, the **proposed test cases** with a one-line description of what each test verifies
3. Ask the user to confirm before writing any code

### 10. Write the tests

After user approval, for each new test:

1. **Create the test file** — `tracktion_ClassName.test.cpp` next to the source
2. **Add the `#define`** — `ENGINE_UNIT_TESTS_CLASSNAME 1` in `tracktion_TestConfig.h`
3. **Add the `#include`** — in the appropriate compilation unit file

### 11. Verify

Rebuild and re-run to confirm:

```bash
cmake --build tests/coverage/build --target coverage -j $(sysctl -n hw.ncpu)
tests/coverage/build/coverage_artefacts/coverage
```

Check that:
- The build succeeds with no errors
- All new tests pass (exit code 0)
- Optionally re-run lcov to show improved coverage numbers

If any tests fail, fix them before finishing. Report the results to the user.
