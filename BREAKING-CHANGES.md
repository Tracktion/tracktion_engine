# Tracktion Engine breaking changes

## Develop


### Change
Removed the TRACKTION_ENABLE_REALTIME_TIMESTRETCHING option.

#### Possible Issues
Most code should be unaffected as it was enabled by default anyway. Along with this are the `setUsesTimestretchedPreview`/`usesTimestretchedPreview` `AudioClipBase` functions. Use `setUsesProxy` instead now for the same effect.

#### Workaround
None.

#### Rationale
This was always a temporary flag used to transition to this new feature.

---

### Change
Added a new ContainerClip type. To facilitate this, a new ClipOwner class has been created.

#### Possible Issues
You may need to fix code which passes `Track`s to functions, passing in a ClipOwner subclass like a ClipTrack instead.

#### Workaround
None.

#### Rationale
The changes required should be small but enable this new feature.

---

### Change
Removed the fixed buffer size requirement in HostedAudioDeviceInterface for using the Engine inside a plugin.

#### Possible Issues
You may need to fix code which set the value of this member.

#### Workaround
None.

#### Rationale
With the audio playback rewite it's no longer required to have a fixed block size so we don't need to add a block of latency when using the Engine inside a plugin.

---

### Change
Removed the TracktionThumbnail class.

#### Possible Issues
If you were using the default thumbnails of Tracktion Engine, these will now be juce::AudioThumbnails and so won't be anti-aliased and appear more jagged.

#### Workaround
Take a copy of the TracktionThumbnail class from the history and add it to your own project.
Override the new `UIBehaviour::createAudioThumbnail` function to return instances of it to get back the old behaviour.

#### Rationale
TracktionThumbnail never really should have been included in the Engine. We needed a way to support multiple thumbnail types in Waveform and in doing so broke all the dependancies on TracktionThumbnail so it seemed cleaner to remove it completely. It's simple to get back the old behaviour but also means it's now a lot easier to use your own thumbnail classes if desired.

---

### Change
Changed the minimum version of JUCE supported to 7 on commit of October 22.

#### Possible Issues
If your project uses an eariler version of JUCE it may fail to compile.

#### Workaround
Use the commit of JUCE pointed at in the modules/juce Git submodule. This is guaranteed to work.

#### Rationale
There have been many breaking changes in JUCE recently and it is no longer feasible to support multiple versions.
We aim to always be compatible with the tip of juce/develop in order to take advantage of the latest fixes and features.

---

### Change
Removed EngineBehaviour::getMaxNumMasterPlugins and static Edit constants.

#### Possible Issues
Code relying on the Edit members or implementing the EngineBehaviour method will no longer compile.

#### Workaround
Update to implement EngineBehaviour::getEditLimits() and call this instead of the static methods.

#### Rationale
This enables a single customisation point for these properties and avoids them having static values.

---
### Change
The old AudioNode based engine has been replaced by a new tracktion_graph based engine.

#### Possible Issues
Some classes will no longer be publicly available such as PlayHead, AudioNode nodes etc.

#### Workaround
You'll have to update your code to use the new APIs. In most cases this shouldn't be a
problem as this is a move to remove the public nature of the engine, the higher level APIs
that you are using should stay the same. Some arguments to classes such as Plugin have
changed to remove dependancy on playback specific classed.
If you really need to use the old engine, see the archive/old_engine branch.

#### Rationale
The move to the new engine greatly improves CPU performance, especially with multiple
threads and fixes PDC in a lot of obscure cases such as track to track routing and bussing.

---
### Change
The PluginWindowConnection class has been removed to simplify the process of
creating plugin windows and connecting them to a PluginWindowState.

#### Possible Issues
PluginWindowState now takes an Edit in its constructor rather than an Engine.
The UIBehaviour method createPluginWindowConnection has been changed to
createPluginWindow which now simply needs to create and return a Component
which will be used to show your plugin UI.

#### Workaround
You'll have to update your code to use the new APIs.

#### Rationale
The old PluginWindowConnection class was an attempt at communicating via IPC
for sandboxed plugins. However, this is no longer necessary as it's possible
to encapsulate a sandboxed plugin completely within the juice::AudioProcessor
classes.
The new API is much simpler for the standard use cases.