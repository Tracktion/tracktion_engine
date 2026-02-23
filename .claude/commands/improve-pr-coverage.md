Improve test coverage for this PR's changed code.

Follow these steps in order:

1. **Identify PR scope:** Run `git diff main...HEAD --name-only` to find all files changed in this branch. These are the only files you should care about. Exclude anything in the `legacy` namespace entirely.

2. **Understand what changed:** Read the changed files and build a mental model of the new/modified features, public APIs, branching logic, and edge cases introduced by this PR.

3. **Run coverage baseline:** Build and run the coverage tool in `tests/coverage` to generate a report. Parse the output to extract per-file and per-function coverage percentages for the changed files only.

4. **Prioritize gaps:** From the changed files, rank uncovered or under-covered areas by size and importance:
   - Prefer covering entire uncovered functions/methods over chasing individual branch misses
   - Prefer public API surface over internal helpers
   - Prefer complex branching logic and error handling over straight-line code
   - Skip trivial getters/setters and boilerplate

5. **Write tests:** Add or extend tests targeting the largest coverage gaps first. For each test:
   - Place it in the appropriate existing test file/directory following the project's conventions
   - Name it descriptively for what behavior it verifies
   - Cover the happy path, then key error/edge cases
   - Stop working on a file once it reaches ~85% coverage — do not chase 100%

6. **Verify:** Re-run the coverage tool after writing tests. Confirm that the changed files are at or near 85%. If any file is still significantly below, do another pass on the biggest remaining gap only.

**Constraints:**
- Do NOT modify any source code — only add/edit test files
- Do NOT touch or add tests for anything in the `legacy` namespace
- Do NOT add tests for code outside the PR's changed files
- Target 85% coverage on changed files, not the whole repo
