# Features

We often get asked what Tracktion Engine can do (and what it can't do) so this document aims to provide a rough summary of the features available. If you want more details, check out the source code or [documentation](https://tracktion.github.io/tracktion_engine/modules.html).

## Overview
As a very high level overview, Tracktion Engine provides a framework to create timelime based DAW-like apps. For the best example, take a look at [Tracktion Waveform Free](https://www.tracktion.com/products/waveform-free), which is built on top of Tracktion Engine. Most of the features are present in Tracktion Engine.

What Tracktion Engine doesn't provide is any kind of UI. You'll have to write code to display arrangements, tracks, clips, mixers, audio thumbnails, piano rolls etc. yourself. This may sound limiting but it's actually very useful as user-interfaces are what set apps apart and you want yours to be unique to your product and user base. For a starting point, take a look at some of the [examples](examples)

## Details
- Playback engine
    - High performance multi-CPU utilising audio engine
    - Minimal latency with full plugin bypass
    - Different threading algorithms for different platforms/use cases
    - Perfect plugin delay compensation
    - Full transport control including scrubbing
- Audio
    - WAV, AIFF, Flac, OGG, MP3, CAF and Rex file formats
    - Audio clips supporting the above file formats and loop start/end/offset
    - Time and pitch-stretching (provided via Elastique*, Rubber-band* or SoundTouch), conforming to dynamic tempo/key changes
    - High quality sample rate conversion utilising libsamplerate
    - Clip time warping (Warp Time)
    - Speed ramp fade in/out
    - ACID and Apple Loop metadata
    - Audio file caching and thread-safe reading
    - Lock-free audio file writing/disk recording
    - Recording audio thumbnails
    - Click track with customisable beat samples
    - Multi-point tempo curves, time signatures and key changes
    - Folder and submix tracks
    - Utilities:
        - Scratch buffers
        - Audio fade curves
        - Lock-free audio FIFOs
        - ADSR envelope generator
        - Ditherer
        - Band-limited square, saw and triangle oscillators
- MIDI
    - MIDI clips supporting note, controller and sysex events
    - Step clips supporting per-note gate and velocity
    - Groove template and quantisation support
    - Chord and scale utilities
    - MIDI pattern generation (bass, melody, chord and arp progressions)
    - MIDI input and output
    - Virtual MIDI inputs (combine multiple MIDI inputs)
    - Sync to incoming MIDI Time Code (MTC)
    - Send outgoing MIDI Clock and MTC
    - MPE (exported from note-expression data)    
- Plugins
    - Utility plugins: aux send/return, freeze point, inserts, level meter, ReWire, text, volume &pan
    - Basic effects: chorus, compressor, delay, 4-band EQ, impulse response, low pass filter, patch bay, phaser, pitch shifter, tone generator
    - Basic synths: 4OSC (4 oscillator subtractive synth), sampler (simple sample playback)
    - MIDI effects: MIDI modifier, MIDI patch bay
    - Rack patching environment for multi-track plugin buses
    - External plugin formats supported by JUCE
    - An API to register your own plugins to be used as entry points to the audio graph
    - The ability to wrap the engine inside a plugin and sync playback to the host
- Automation
    - Automation curves for plugin parameters using bezier curves
    - Automation recording (including simplification) and playback
    - Modifiers for plugin parameters: LFO, envelope follower, breakpoint, step, random, MIDI tracker
    - Macro parameter creation
    - Mappings for MIDI control of plugin parameters
    - MIDI learn
- Controllers
    - External control surface support
    - Built-in support for: MCU, Mackie C4, Alpha Track, Novation Automap, Remote SL Compact, Tranzport
- Rendering
    - Background thread rendering of Edits
    - Render specific tracks/clips/MIDI notes
    - Silence trimming
    - Normalise based on peak/RMS
    - MIDI file exporting
- Utilities
    - Selection
    - Undo/redo
    - Clipboard
    - Basic app shortcut functions
    - Crash tracing
    - Engine settings/behaviour
    - Spline interpolator
    - Temporary file management
    - Save/load compatibility with Tracktion Waveform projects

*\* Requires external licence and libraries from relevant companies*

## Supported Platforms
- macOS
- Windows
- Linux
- Raspberry PI
- iOS
- Android
