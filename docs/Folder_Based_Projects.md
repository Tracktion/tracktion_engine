# Folder-Based Projects

## Overview

**Folder-based projects are the recommended default for new users.** File-based projects remain supported for legacy compatibility.

Tracktion Engine supports two project backends:

- **Folder-based projects** (recommended) — a plain directory on disk where media files live directly, with no binary project file.
- **File-based projects** (legacy) — a single `.tracktion` binary file that stores all project metadata, item references, and properties.

Folder-based projects are designed for workflows where:
- Human-readable, browsable project structure is preferred
- Git-friendly storage is desired (no opaque binary files)
- A simpler, lightweight project model is sufficient
- Media files should be accessible directly on disk without extraction

## How It Works

`Project` uses a pimpl pattern with an abstract `ProjectBase` interface. At construction time, the project file path is checked with `isDirectory()`:

- If it's a **file** → a `FileBasedProject` backend is created (reads/writes the `.tracktion` binary format)
- If it's a **directory** → a `FolderBasedProject` backend is created (lazily scans the folder for media)

All public `Project` methods delegate to the active backend. You can check which backend is in use via `Project::isFolderBased()`.

### FolderBasedProject internals

`FolderBasedProject` lazily scans its folder to discover items. On first access to the item list, it walks the directory tree and creates `ProjectItem` objects for each discovered file. The item type and category are inferred from file extensions and subfolder names. The scan covers edits (`.tracktionedit`/`.trkedit`), audio (`.wav`, `.aiff`, `.aif`, `.mp3`, `.ogg`, `.flac`), MIDI (`.mid`, `.midi`) and the video formats reported by `VideoEngine::getVideoFileExtensions()`.

Project-level properties (description, custom properties) are stored in a `project_info.json` file inside the folder. Per-item descriptions are also persisted there, keyed by the item's relative path (`"itemDesc:<path>"`), since the items themselves are recreated on every scan. The project name is derived from the folder name itself.

`Project::reload()` clears the scan cache and rescans the folder (lazily or immediately depending on the `ReloadMode`). The rescan preserves items whose source files live outside the project folder, e.g. renders sent to a custom directory.

## Key New Types

### `ProjectID`
A strong-typed wrapper around `int` that replaces raw `int` for project identification. Defined in `tracktion_ProjectItemID.h`.

```cpp
ProjectID id (42);          // construct from int
int raw = id.toInt();       // extract raw value
bool ok  = id.isValid();   // true if non-zero
```

For folder-based projects, the `ProjectID` is computed from the folder path's hash (`folder.hashCode()`), so it changes if the folder moves. For this reason, it should not be saved.

### `ProjectItemRef`
A unified reference that can hold either a `ProjectItemID` (for file-based projects) or a file path string (for folder-based projects). Defined in `tracktion_ProjectItemRef.h`.

```cpp
// From a ProjectItemID (implicit conversion)
ProjectItemRef ref = myProjectItemID;

// From a path
auto ref = ProjectItemRef::fromPath ("audio/vocals.wav");
auto ref = ProjectItemRef::fromAbsolutePath (someFile);

// Inspection
ref.isProjectItemID();   // true if backed by a ProjectItemID
ref.isRelativePath();    // true if backed by a relative path
ref.isAbsolutePath();    // true if backed by an absolute path
ref.isValid();           // true if non-empty (any type)

// Extraction
if (auto id = ref.getProjectItemID())       // std::optional<ProjectItemID>,
    use (*id);                              // present only if isProjectItemID()

ProjectID pid = ref.getProjectID();         // project portion of the ID
```

### `ProjectType`
An enum used when creating new projects:

```cpp
enum class ProjectType { fileBased, folderBased };
```

Passed to `ProjectManager::createNewProject`, `createNewProjectInteractively`, `createNewProjectFromTemplate`, and the `TempProject` helper.

## File-Based vs Folder-Based Comparison

| Aspect | File-based | Folder-based |
|---|---|---|
| Storage | Single `.tracktion` binary file | Plain directory on disk |
| Item IDs | Stable `ProjectItemID` (numeric) | Invalid `ProjectItemID`; items referenced by path |
| Source references in edits | ID strings (e.g. `"1234_5678"`) | Relative or absolute file paths |
| `ProjectID` stability | Stored in the file; stable across moves | Hash of folder path; **changes if folder moves** |
| Merging projects | Supported | No-op |
| Reordering items | Supported | No-op |
| Project name | Stored in binary file | Derived from folder name |
| Custom properties | Stored in binary file | Stored in `project_info.json` |
| `getAllProjectItemRefs()` | Returns all refs | Returns all refs (triggers lazy scan) |
| Item discovery | Read from binary file on load | Lazy folder scan on first access |

## Creating Projects

### File-based
```cpp
auto project = projectManager.createNewProject (projectFile,
                                                folderTree,
                                                ProjectType::fileBased);
```

### Folder-based
```cpp
projectFolder.createDirectory(); // the folder must exist before the call

auto project = projectManager.createNewProject (projectFolder,
                                                folderTree,
                                                ProjectType::folderBased);
```

N.B. the backend is chosen by whether the path is an existing directory on disk,
so the caller must create the folder first (`createNewProjectInteractively` does
this for you). The file is asserted to match the requested `ProjectType`.

### Temporary projects
```cpp
ProjectManager::TempProject temp (projectManager, file, true,
                                  ProjectType::folderBased);
```

### Converting an existing file-based project
```cpp
auto folderProject = convertToFolderBasedProject (existingProject);
```

## Source References

Edits reference their media files differently depending on the project type:

- **File-based**: edits store a `ProjectItemID` string (e.g. `"1234_5678"`). The engine resolves this to a file via `ProjectManager::findSourceFile(ProjectItemID)`.
- **Folder-based**: edits store a relative or absolute file path. The engine resolves this via `ProjectManager::findSourceFile(ProjectItemRef)`.

For folder-based projects, use `Project::getSourcePathForFile(file)` to obtain the reference string for a given file. It returns a path relative to the project folder if the file is inside it, otherwise the absolute path. (File-based projects should not use this method - they reference sources by `ProjectItemID`.)

## Things to Look Out For

1. **No-op operations** — `mergeOtherProjectIntoThis()`, `mergeArchiveContents()`, and `moveProjectItem()` are no-ops for folder-based projects. Check `isFolderBased()` if your code depends on these. Note that `get/setDescription()` and `get/setProjectProperty()` work for folder-based projects (stored in `project_info.json`), and `removeProjectItem()` works too (removing the item from the cache and optionally deleting its source file).

2. **Lazy scanning cost** — the first call to item-listing methods triggers a directory scan. For large folders this may have noticeable latency.

3. **Unstable `ProjectID`** — folder-based project IDs are computed from the folder path hash. If the folder is moved or renamed, the `ProjectID` changes. Note that calling `setName()` on a folder-based project renames (moves) the folder on disk, so it too changes the `ProjectID`. Do not persist folder-based `ProjectID` values for long-term identification.

4. **Invalid `ProjectItemID`** — items in folder-based projects have invalid (zero) `ProjectItemID` values. Code that checks `projectItemID.isValid()` will return `false` for these items. Use `ProjectItem::getProjectItemRef()` instead.

5. **`isValid()` on `ProjectItemRef`** — `isValid()` returns `true` for any non-empty ref, including path-based refs. To check specifically for a `ProjectItemID`-backed ref, use `isProjectItemID()`.

---

## Migrating User Code

### `int` → `ProjectID`

`Project::getProjectID()` now returns `ProjectID` instead of `int`. Related changes:

| Before | After |
|---|---|
| `int pid = project.getProjectID();` | `ProjectID pid = project.getProjectID();` |
| `ProjectItemID (itemId, projectId)` | `ProjectItemID (itemId, ProjectID(projectId))` |
| `ProjectItemID::createNewID (intId)` | `ProjectItemID::createNewID (ProjectID(intId))` |
| `projectManager.getProject (intId)` | `projectManager.getProject (ProjectID(intId))` |

To convert between `ProjectID` and raw `int`:
```cpp
ProjectID pid (42);        // wrap
int raw = pid.toInt();     // unwrap
```

### `ProjectItemID` → `ProjectItemRef`

Many APIs that previously accepted or returned `ProjectItemID` now use `ProjectItemRef`:

- `Project::getProjectItemRef()`, `getProjectItemFor()`, `removeProjectItem()`, `getIndexOf()`
- `ProjectItem::getProjectItemRef()` returns `const ProjectItemRef&`
- `ProjectManager::getProjectItem()` and `findSourceFile()` have `ProjectItemRef` overloads

**In most cases, no code changes are needed** — `ProjectItemRef` has an implicit constructor from `ProjectItemID`, so existing call sites that pass a `ProjectItemID` will compile unchanged.

If you need the old `ProjectItemID` from a `ProjectItemRef`, note that
`getProjectItemID()` returns a `std::optional<ProjectItemID>`:
```cpp
ProjectItemRef ref = item->getProjectItemRef();

if (auto id = ref.getProjectItemID())
    use (*id);
```

### New `ProjectType` parameter

`ProjectManager::createNewProject` has a single-argument overload (taking just a `juce::File`) that picks the backend from the path: an existing directory gives a folder-based project, anything else file-based. The three-argument `createNewProject` overload, `createNewProjectInteractively`, and `createNewProjectFromTemplate` take an explicit `ProjectType` parameter; for `createNewProject` the path must match the requested type (see "Creating Projects" above). The `TempProject` helper defaults to `ProjectType::fileBased` if not specified.

### `findOrphanItems()` → `findOrphanItemRefs()`

`Project::findOrphanItems()` is deprecated. Use `findOrphanItemRefs()` instead, which returns `juce::Array<ProjectItemRef>`.

---

## Consolidation (`ProjectUtilities`)

The `ProjectUtilities` namespace in `tracktion_ProjectUtilities.h` provides functions for consolidating projects and edits — copying external media files into the project folder so everything is self-contained.

| Function | Description |
|---|---|
| `consolidateEdit(Edit&)` | Copies external files referenced by an edit into its project folder and updates references. Returns `{count, errorMsg}`. |
| `consolidateProject(Project&)` | Consolidates all edits in a project. Returns `{count, errorMsg}`. |
| `canConsolidateEdit(Edit&)` | Returns `true` if the edit references files outside its project folder. |
| `canConsolidateProject(Project&)` | Returns `true` if any edit in the project has external references. |
| `isConsolidated(Edit&)` / `isConsolidated(Project&)` | Returns `true` if fully consolidated (no external file refs). |
| `getEditsInProject(Project&)` | Returns an `EditReferences` container holding all edits in a project (reuses already-open edits, loads closed ones for examining). |
| `importExternalFiles(Project&, Array<ProjectItemRef>)` | Lower-level: copies source files from outside the project folder into it. |
| `importExternalReferences(Project&, Array<ProjectItemRef>)` | Lower-level: creates new project items for refs pointing at other projects, updating edit references. |
| `consolidateEditInteractive(Edit&, callback)` | Shows confirmation/completion dialogs around `consolidateEdit`. |
| `consolidateProjectInteractive(Project&, callback)` | Shows confirmation/completion dialogs around `consolidateProject`. |

---

## Archiving

The `tracktion_Archive.h` header provides a new archive system that replaces the legacy `.trkarch` format with standard zip files.

### `ArchiveJob`

`ArchiveJob` is a `ThreadPoolJobWithProgress` that archives a `Project` or single `ProjectItem` into a zip file.

- **Folder-based projects**: copies the project folder to a temp directory, consolidates the copy, then zips it.
- **File-based projects**: converts to folder-based in a temp directory first, then consolidates and zips as above.

In both cases the work happens on a temporary copy - the user's project folder is never modified. Applications can bundle extra resources into archives by overriding `EngineBehaviour::getExtraFilesToArchive (Edit&)`.

```cpp
ArchiveJob::Source source = &myProject;
ArchiveJob job (source, destZipFile, ArchiveJob::CompressionLevel::normal);
job.runSynchronously();  // or submit to a thread pool
```

### Detection helpers

| Function | Description |
|---|---|
| `isArchive(Engine&, File&)` | Returns `true` if the file is any supported archive (legacy or zip). |
| `isLegacyArchive(Engine&, File&)` | Returns `true` if the file is a legacy `.trkarch` archive. |

### Legacy classes

`TracktionArchiveFile` and `ExportJob` have been moved to the `legacy::` namespace. They remain available for reading old `.trkarch` files but should not be used for new archives.
