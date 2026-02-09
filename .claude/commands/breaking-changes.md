Analyse the current branch for breaking API changes and add entries to `BREAKING-CHANGES.md`.

## Steps

1. **Determine the base branch.** Run `git merge-base HEAD master` to find the common ancestor. If the user provides a different base branch, use that instead.

2. **Gather the diff.** Run `git diff <merge-base>...HEAD` scoped to the tracktion_engine module directory. Focus on public header files (`.h`).

3. **Identify breaking changes.** Look through the diff for:
   - Removed or renamed public classes, structs, enums, or type aliases
   - Removed or renamed public member functions or free functions
   - Removed or renamed public member variables
   - Changed function signatures (added/removed/reordered parameters, changed types)
   - Renamed header files (broken `#include` paths)
   - Removed or renamed preprocessor macros / config flags
   - Member functions moved to free functions (or vice versa)
   - Changed enum values or constants
   - Any other change that would cause existing user code to fail to compile

   Ignore: private/protected members, implementation-only files (`.cpp`), internal namespaces, comment-only changes, and additions that don't modify existing API.

4. **Read the existing `BREAKING-CHANGES.md`** to understand the current format and avoid duplicating entries that already exist.

5. **Present findings to the user.** Before writing anything, list the breaking changes you found and ask the user to confirm which ones should be documented. The user may want to adjust wording, skip some, or add context.

6. **Write entries.** For each confirmed breaking change, prepend an entry to `BREAKING-CHANGES.md` (after line 1, before existing entries) using this exact format:

```
___

### Change
<Brief description of what changed.>

#### Possible Issues
<What will break for users and how it will manifest (compile error, deprecation warning, runtime change, etc.).>

#### Workaround
<How to migrate. Include before/after code snippets where helpful.>

#### Rationale
<Why the change was made.>
```

Each entry is separated by `___`. Keep descriptions concise and precise. Use backtick-quoted code references for class names, functions, and symbols.
