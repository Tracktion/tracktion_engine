# Transitioning to Tracktion Engine 2.0

## New Module Division

Tracktion Engine has been split up in to three modules. The idea behind this is that developers can use the primitive base types (**core**) in the framework or the audio playback engine (**graph**) without having to use the engine model. Most applications will still use `tracktion::engine` but if you’re making a lower level app the other modules may be of use to you. N.B. a Tracktion Engine license is required for use of **any** of these modules.

Although these are separate modules, they are contained within nested namespaces so you can access them all with the `tracktion::` specifier. This is the recommended method incase classes/functions move between modules.

### tracktion::core

Contains primitive types that do low level operations like define a time or beat position/duration and convert between them. This library aims to be header only so is simple to include. It is used extensively throughout the rest of the framework.

### tracktion::graph

Contains the low level audio processing library which powers playback inside tracktion::engine. This provides base classes for processing nodes and graph construction, then playing these back using multiple threads in a lock-free way.

### tracktion::engine

Contains the bulk of the framework and the higher level classes required to build an application quickly. This defines the Edit model and the various classes used to interact with the model.

## New Time/Beat Position/Duration primitives

These are used extensively throughout the framework to disambiguate between _beats_ and _time_ (in seconds). There is also a separation between _positions_ and _durations_. The classes contain operator overloads for combining the two but it should now be much clearer when a position or a duration is being used and the type system should highlight misuses.

- There are literals that can be used to construct these. `_tp, _td, _bp, _bd`
- The `Position` classes have a `DurationType` alias member to get the corresponding duration type
- `TimePosition/Duration` can be constructed from a `std::chrono` time or a number of samples (with a sample rate). This enables you to use `std::chrono::literals` to write code like `setDelay (1ms)`
- Converting between the two requires a `TempoSequence` (`toTime`/`toBeats`)
- There is also a variant type, `EditTime` that can hold either type and requires a TempoSequence to get one of them out. This is mainly used to simplify APIs that can accept either time format

## New Time/Beat Range Types

Similarly to the new `Position`/`Duration` primitives, there are also now distinct range types. This makes it a lot easier to determine when classes are using time or beats. Again you can convert between the two using a `TempoSequence` with `toTime`/`toBeats`.

There is also a variant type `EditTimeRange` that can hold either type of time range.

## Real-time Time Stretching

Audio clips no longer have to generate WAV proxies to be played back from compressed formats or to pitch/time-stretch. They can do this during the audio callback. This makes it much quicker to play back and edit files, especially long ones.

To enable this feature for clips, simply call `setUsesProxy (false)` on it.

This feature also allows temporary adjustment of the tempo, speeding up or slowing down playback without changing the pitch of audio clip playback. This can be accessed by setting `EditPlaybackContext::setTempoAdjustment` but is usually handled internally by syncing to MIDI timecode or AbletonLink.

## High Quality Sample Rate Conversion using libsamplerate

If you use the `AudioClipBase::setUsesProxy (false)` mode to enable real-time time-stretching for audio clips, you can use the `AudioClipBase::setResamplingQuality` function to use higher quality sinc-based sample rate conversion. This produces less aliasing artefacts at the cost of some CPU cycles.

The are several options: *lagrange* (the old and now default mode), *sincFast*, *sincMedium* and *sincBest*.

## New MIDI playback mode

There's a new MIDI playback mode which is enabled by calling `MidiClip::setUsesProxy (false)`. This generates looped MIDI sequences during playback which can speed up graph creation of long sequences and is generally better for situations where MIDI changes a lot. It does come at the expense of some bursty CPU use though when the playhead traverses a loop boundry.

## TempoSequencePosition has Been Removed

This class has been replaced by the more optimised and `tempo::Sequence::Position`. You can still create one from a `TempoSequence` using the `createPosition (tempoSquence)` function.

## TempoSequence API changes
The API of TempoSequence has changed slightly to make better use of the new time primitives. Updating to the new API should be simple.
