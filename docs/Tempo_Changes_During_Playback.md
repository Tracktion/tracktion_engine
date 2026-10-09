# Tempo Changes During Playback

Changing the tempo whilst playing, e.g. dragging a tempo curve point, keeps playback on the same beat. MIDI and beat-based audio carry on from exactly where they were, looped or not, in the arrangement and the launcher, without waiting for the playback graph to be rebuilt. Clips that don't follow the beat (time-based audio, clips playing a pre-rendered proxy, clips with absolute sync) jump, as they must.

## How a change flows through

1. **The model** (message thread). `TempoSetting::set` with `remapEditPositions` takes an `EditTimecodeRemapperSnapshot` of the Edit's beat positions, changes the tempo, then `remapEdit` swaps in the new `tempo::Sequence` (`TempoSequence::updateTempoData`, under the audio callback lock) and moves every clip, automation point, the start position and the transport's loop back on to their beats. Every move is applied, however small, so a drag made of many small steps doesn't leave clips off their beats.
2. **The playhead** (audio thread, start of the next block). `EditPlaybackContext::NodePlaybackContext::checkForTempoSequenceChanges` sees the sequence's hash has changed and puts the playhead back on the beat the last block ended on. The playhead is in whole samples, so the difference between that beat and the nearest sample's is given to the `ProcessState` as a beat offset (`ProcessState::setBeatOffset`), which it adds to the beats it works out until the playhead next jumps or loops. The beats the nodes see therefore carry on exactly.
3. **The loop** is kept on its beats on the audio thread too, before the position (which is clipped to it). Its beats come from the exact loop times the transport last gave the playhead (`EditPlaybackContext::setExactLoopTimes`), as a range rounded to samples at one tempo can be several samples out at a slower one. They're kept between changes so rounding doesn't add up. When the transport later pushes the Edit's loop times again, `PlayHead::setLoopRange` keeps the playhead where it is and only counts it as a jump if it's now outside the loop.
4. **The nodes** play in beats, so they carry on from the same beat with no rebuild:
   - Arrangement MIDI clips play their sequence in beats, whether it's pre-looped (`MidiNode` with `MidiList::TimeBase::beats`) or generated as it plays (`LoopingMidiNode`), as do step clips.
   - Beat-based audio clips read their source by beats (`WaveNodeRealTime` with a `BeatRangeReader`), and their fades (`FadeInOutNode`) are positioned by the clip's beats and worked out with the current tempo each block.
   - `CombiningNode` groups its inputs by beats, so the playhead's new time can't land in a group that doesn't list a clip it's in.
   - Launched clips run on the monotonic beat, which carries on through tempo changes and the arrangement loop.
5. **Splitting blocks.** `TracktionNodePlayer` splits each block at every tempo change in it, so each section has one tempo and events are placed exactly.
6. **The rebuild** that follows (clips moved, tempo properties changed) is seamless for all of these, as each node takes over its predecessor's state: wave readers with the same beat-domain state hash are reused, and MIDI nodes keep their active notes.

## Things that must stay true

- A node mustn't hold edit *times* for things that move with the beat. Store beats and convert with the current tempo each block.
- MIDI nodes work out the events in a section from where the last section stopped, not from where this one starts: `LoopingMidiNode` plays events in a window offset by half a sample, and half a sample in beats changes with the tempo. They also only chase notes at a clip's start if the last section didn't already play it.
- Events must fall inside their section. `PluginNode` drops any after a section's end, so notes struck just after a jump are clamped in to it (`MidiNodeHelpers::clampTimeStamps`), however short the section is.
- `tempo::Sequence::Position` can be left over a Sequence that's since been replaced, so `set` walks either way to the right section.

## Tests

- `playback/graph/tracktion_TempoChangePlayback.test.cpp`
  - Regression tests for each of the above.
  - A fuzz test: random tempo curves, arrangement and launched MIDI and audio clips, a random schedule of tempo changes, drags and moves, with and without the arrangement loop and graph rebuilds. A model of the playhead (on the same beat after every change, looping on the same beats) is checked against what each track's plugins get: every section's position, every note-on and note-off to a couple of samples, each beat-based clip's source position and its output's continuity. Set `TE_TEMPO_CHANGE_FUZZ_SEEDS` to run more seeds and `TE_TEMPO_CHANGE_FUZZ_SEED` to run one; `TE_TEMPO_CHANGE_FUZZ_DUMP` prints the sections around the first problem.
- `playback/tracktion_TransportControl.test.cpp` and `tracktion_core/utilities/tracktion_Tempo.test.cpp` (`Position` following a replaced Sequence).

## Known limitations

- A time-stretched beat-based clip is reset where it loops, so its output isn't seamless there whatever the tempo, and it takes the stretcher's latency (longer the more it's stretched) to start a clip. Neither is to do with tempo changes.
