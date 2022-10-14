# Roadmap

## Intro
This is a rough guide to plans we have for the Engine so you can get an idea for how it will progress. This isn't a definitive list nor necessarily in chronological order.

## Ongoing
- **Add documentation**
  - We'll we adding documentation over time especially as we create examples or refactor parts of the code.
- **Add examples**
  - We'll be adding new examples as we
- **Improve test coverage**
  - A lot of the Engine gets tested as part of our Waveform build process. However, as we continue, we'll be adding more unit tests and tooling coverage directly to the Engine. This will also serve as a source of documentation and examples.

## Features
### ~1) Complete Engine-as-plugin support~ [complete, March 2020]
Top of our priority list is to get finish up being able to use the Engine in a plugin. It's nearly there and can be used in limited form at the moment but there are a couple of tasks which need to be completed:
- Remove the last singletons
- Ensure all MIDI is passed on the the virtual MIDI input
- Ensure MIDI output is working
- Ensure host timeline syncs to Engine playhead

### ~2) Refactor Rendering Graph~ [complete, August 2021]
In order to improve PDC support for complex cross-track routing plugins (such as having multiple Racks on a track or receives going to sends) we need to change the way the audio graph is created and played back.

Currently a graph is built for each output device and then these are processed in turn. Cross-track sends are handled by double buffering which works in the single case but when complex routing is configured there is no way to know what latency to apply.
Multi-threaded processing of the graph is currently handled at track-level which generally works well but is not optimal.

There are several things that need to change here:
- There needs to be a single audio graph built to process all of the audio and MIDI channels in a single go. This means exposing all the devices to the graph at the top level and then narrowing them down once they reach the track level.
- The audio graph needs to be built using a dependancy walk rather than a straight track recursion. This isn't very different to how things are currently done but means that things like aux sends can be traversed to build a complete set of dependancies.
- This will result in a collection of blocks that can be processed in parallel with synchronisation points where dependancies are. This should enable us to better handle complex PDC and routing configurations.

### ~3) Allow Dynamic Time-stretched Playback~ [complete, June 2022]
For optimal audio file playback, time-stretched WAV proxy files are created for each audio clip that needs to be played back. This works very well when the content is largely static. However, editing a large clip in this way, or having a more dynamic tempo track can lead to lengthly proxy rebuilds. Being able to read from the original source file and time-stretch it at playback time would greatly improve flexibility in this regard.

### ~4) Add Beat-based `AudioNode`s~ [complete, June 2022]
Currently, all processing in the render callbacks is specified by a start and end time. This allows constant repositioning of the transport without any artefacts. However, this does mean that all content needs to be converted to a time-based format for playback, such as MIDI sequences. This is fine for standard, linear playback, but in order to support more dynamic playback modes, we need to add a way to play back a section of an Edit via beat start/end times.

In theory this isn't too difficult, it's more a case of efficiently being able to iterate data structures on the real-time audio thread and ensure any notes created are positioned at the correct time for the block. This probably means doing a linear interpolation between beat and seconds time for each block.

### 5) Refactor `Project`
Currently projects are stored as a binary format. This was originally devised to store huge sample libraries but has never fully be used in this way. Moving to a XML back ValueTree format will greatly improve readability and extensibility in the same way Edit files can be extended.

It should be noted that most users of the Engine won't need to use the `Project` class. A simpler approach is to use an Edit with an appropriate `Edit::Options::filePathResolver` set. This means you can keep files relative to your Edit file without having to maintain a `Project` as well.

### 6) Refactor Archiving
This won't be a huge change but at the moment, archives are saved as compressed binary blobs. A more straight-forward approach would be to split the archiving process in to several individual steps: consolidate, optimise, zip.
Doing this not only give the intermediate stage benefits but also allow archives to be manually unzipped and examined without having to go via the Engine as a tool.
