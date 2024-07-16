# Changelist

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