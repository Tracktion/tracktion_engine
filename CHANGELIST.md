# Changelist

## v3.5
- Folder-based projects
  - New folder-based project backend: a plain directory on disk, no binary project file (see docs/Folder_Based_Projects.md)
  - New `ProjectItemRef` type unifying ID-based and path-based source references
  - New strong `ProjectID` type replacing raw ints
  - New zip-based archive format replacing `.trkarch` (legacy classes preserved in a `legacy::` namespace for reading old archives)
  - New `ProjectUtilities` consolidation functions for making projects/edits self-contained
  - Smarter relative/absolute source path handling in clips
- Multichannel
  - Arbitrary channel counts throughout: devices, plugins, racks, clips, recording and rendering
  - New `ChannelConfiguration` class describing channel layouts
  - Device channel groupings of any size, with savable device I/O layout presets
  - Plugins explicitly declare their bus layouts via `Plugin::getBusses()`
- ARA
  - Generalised ARA hosting (no longer Melodyne-specific), enabled with `TRACKTION_ENABLE_ARA`
  - Overhauled hosting lifecycle, region management and notifications
  - Support for non-clip based ARA plugins, chord track progression and view selection sync
- Async engine
  - Removed all modal loops from the engine: alert/dialogue APIs are now async with completion callbacks
  - Added `Engine::prepareForShutdown` for non-blocking plugin teardown
- Time-stretching
  - Added support for the Signalsmith stretch library
  - Stretchers now report their latency
- Rendering
  - New `RenderSpecification` describing a render as data, with JSON round-tripping and per-track/stem expansion
  - New `RenderQueue` running specifications sequentially, with progress, cancellation, appending whilst running and partial-file cleanup
  - Stem rendering: muted tracks can be rendered, and source tracks keep processing whilst muted so sidechains and aux buses still feed them
  - Loudness normalisation to a LUFS target with a true-peak ceiling, and a wrap-remainder mode folding a render's tail back onto its start
- Analysis
  - New real-time-safe `LoudnessMeter` measuring EBU R128 momentary/short-term/gated-integrated loudness, EBU Tech 3342 loudness range and oversampled true peak
  - New `AudioFileAnalyser` reporting loudness, spectral and dynamics statistics for an audio file
- Import/Export
  - DAWproject: scene/launch-clip support, nested path fixes, default volume/pan/meter plugins and correct pan range on import
- Misc
  - Level meters for audio/MIDI output devices and a test tone generator on `WaveOutputDevice`
  - `AudioFileManager` support for registering in-memory audio data as clip sources
  - New `TRACKTION_SANITISE_PLUGIN_OUTPUT` and `TRACKTION_ENABLE_PLUGIN_CPU_MEASUREMENT` flags
  - Engine unit tests converted from juce::UnitTest to doctest
  - Tons of bug fixes, optimisations and API improvements

## v3.2
- Automation modes (read/write/touch/latch)
- AutomationCurve bypass

## v3
- Clip launcher for non-linear composition
  - Launch clips quantised to timeline
  - Launch scenes for triggering multiple clips at once
  - Record to clip slots
  - Comprehensive follow actions
- MIDI
  - MIDI recording thumbnails
  - Automatic MIDI device detection
  - Real-time MIDI Nodes for dynamic tempo changes
- Recording/playback 
  - Improved recording punch capabilities
  - New return-to-start/resume playback mode
  - Real-time audio time-stretching (with backgroud thread read-ahead for reduced CPU load)
  - New InputDevice MonitorModes for auto/on/off
  - Improved audio file reading (reducing CPU load)
- Plugins 
  - Cmajor support
  - Improved automation resolution
- Performance/optimisation 
  - Optimised playback graph
  - macOS Audio Workgroup support
  - Improved CPU use metrics
  - Tons of bug fixes, optimisations and API improvements
  - JUCE 8 support

## v2.2
- Added a new ContainerClip type which can contain other audio/MIDI clips and play them back looped
- Lots of small internal performance improvements

## v2.1
- Added support for user supplied thumbnails (and removed the TracktionThubmail class)
- Added a "quantised jump" so you can delay a transport position change to a time in the future

## v2.0
- Added improved sample rate conversion using libresample
- Added real-time time-stretching for audio and MIDI clips by setting `setUSesProxy (false)` on them

## v1.1
- Added an ImpulseResponsePlugin class
- Added support for RubberBand time stretch algoirthm (licensed separately)
- Added a DistortionEffectDemo to show how to create a custom plugin class and use it in an app (along with a tutorial for)
- Changed the underlying playback engine for better multi-thread CPU utilisation and improved PDC handling (using the new tracktion_graph module)

## v1.0
- Initial release