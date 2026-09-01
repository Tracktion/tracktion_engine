# Tracktion Engine breaking changes

___

### Change
`toBitSet (const juce::Array<Track*>&)` now returns a bitset of only the tracks passed in.

#### Possible Issues
Previously it ignored its argument and set a bit for every track in the Edit, so anything built from it addressed the whole Edit. Code that passed a subset of tracks - most visibly `Renderer::Parameters::tracksToDo` and `Renderer::measureStatistics()` - will now render or measure just that subset instead of everything.

#### Workaround
Pass `getAllTracks (edit)` where the whole Edit really is wanted, or leave `Renderer::Parameters::tracksToDo` empty, which already means "all tracks".

#### Rationale
The function used its argument only to reach the Edit and then looped over every track, which contradicted its documented behaviour and silently broke subset rendering. See issue #399.

___

### Change
`Plugin` has a new pure virtual method `getBusses()` which every subclass must implement.

#### Possible Issues
Custom `Plugin` subclasses will no longer compile until they implement `getBusses()`. The default implementation of `takesAudioInput()` has also changed from `! isSynth()` to `! isSynth() && ! getBusses().inputs.empty()`.

#### Workaround
Implement `getBusses()` returning a `Plugin::BusLayout` describing the plugin's inputs and outputs. Helpers cover the common cases: `BusLayout::singleStereoInOut()` for a typical stereo effect, `BusLayout::singleInOut (in, out)` for other fixed layouts, `BusLayout::singlePassThrough()` for plugins that pass through whatever they're given, and `{}` for MIDI-only plugins.

#### Rationale
Multichannel support requires every plugin to explicitly declare its channel requirements rather than the engine assuming stereo.

___

### Change
`ExternalPlugin::getNumInputs()` and `getNumOutputs()` have been removed.

#### Possible Issues
Code calling these methods will no longer compile.

#### Workaround
Use `getBusses()` for main-bus channel counts, or `getAudioPluginInstance()->getTotalNumInputChannels()` / `getTotalNumOutputChannels()` for totals. A deprecated `getTotalNumInputChannels()` shim exists on `ExternalPlugin` for the input side.

#### Rationale
Part of the multichannel work: single input/output counts can't describe multi-bus plugins.

___

### Change
The `DeviceManager` per-channel stereo-pair API has been removed. This includes `setWaveOutChannelsEnabled`, `setWaveInChannelsEnabled`, `setDeviceOutChannelStereo`, `setDeviceInChannelStereo`, `isDeviceOutChannelStereo`, `isDeviceInChannelStereo`, `isDeviceOutEnabled`, `isDeviceInEnabled`, `enableAllWaveInputs`, `enableAllWaveOutputs`, `setAllWaveInputsToStereoPair` and `setAllWaveOutputsToStereoPair`.

#### Possible Issues
Code managing device channels through these methods will no longer compile.

#### Workaround
Use the new device-centric API: `setDeviceEnabled (WaveInputDevice&/WaveOutputDevice&, bool)`, `setDeviceNumChannels (device, numChannels)`, `setAllWaveInputsToNumChannels()`, `setAllWaveOutputsToNumChannels()` and `getPossibleChannelGroupsForDevice()`. Whole configurations can be saved/restored with the new wave device layout preset API (`getCurrentWaveDeviceLayout()`, `applyWaveDeviceLayout()` etc.).

#### Rationale
Channel groupings are no longer limited to mono or stereo pairs; devices can now use groups of any size up to `maxNumChannelsPerDevice`.

___

### Change
`WaveDeviceDescription` has been restructured around the new `ChannelConfiguration` class. Its `std::vector<ChannelIndex> channels` member is now a `ChannelConfiguration`, and the constructors taking left/right channel indices or a `ChannelIndex*` array have been removed. `ChannelIndex` has moved to `utilities/tracktion_ChannelConfiguration.h`, and the free functions `createDescriptionOfChannels()` and `createChannelSet()` have been removed.

#### Possible Issues
Custom `EngineBehaviour::describeWaveDevices()` implementations will no longer compile (the virtual's signature is unchanged, so the failure is in the body, not the override). Code using the removed free functions will also fail to compile.

#### Workaround
Construct descriptions with `WaveDeviceDescription (name, ChannelConfiguration, enabled)` or `WaveDeviceDescription::withNumChannels (name, firstChannelIndexInDevice, numChannels, enabled)`. Replace `createDescriptionOfChannels()` with `ChannelConfiguration::getDescription()` and `createChannelSet()` with `ChannelConfiguration::toChannelSet()`.

#### Rationale
Wave devices can now describe arbitrary channel layouts, not just mono/stereo.

___

### Change
`InputDevice` and `OutputDevice` now share a common `IODevice` base class. Their constructors no longer take a `type` string, `getType()` has been removed in favour of a new pure virtual `getDeviceTypeDescription()`, `getAlias()` has been replaced by `getAliasOrName()` / `getAliasIfSet()`, and `OutputDevice::getName()` is no longer virtual.

#### Possible Issues
Custom device subclasses will no longer compile (constructor and `getDeviceTypeDescription()` requirements). Code overriding `OutputDevice::getName()` will silently stop being called.

#### Workaround
Update constructors to the new `(Engine&, name, deviceID)` signatures and implement `getDeviceTypeDescription()`. Replace `getAlias()` calls with `getAliasOrName()` (falls back to the name) or `getAliasIfSet()` (may be empty).

#### Rationale
The two device hierarchies duplicated a lot of state and behaviour; a common base simplifies the multichannel refactor.

___

### Change
`WaveInputDevice::getChannels()` and `WaveOutputDevice::getChannels()` now return `const ChannelConfiguration&` instead of `const std::vector<ChannelIndex>&`. `isStereoPair()`, `setStereoPair()`, `WaveOutputDevice::getLeftChannel()` and `getRightChannel()` are deprecated.

#### Possible Issues
Code binding the old return type or using the deprecated methods will fail to compile or emit warnings.

#### Workaround
Use `getChannels().getNumChannels()`, `getChannels()[i].indexInDevice` and `setChannelConfiguration()` instead.

#### Rationale
Wave devices are no longer restricted to one or two channels.

___

### Change
`RackInstance`'s two-channel API has been removed: the `enum Channel { left, right }`, the `Channel`-taking name/level setters and getters, and the `leftInputGoesTo` / `rightInputGoesTo` / `leftOutputComesFrom` / `rightOutputComesFrom` cached values and their associated parameters.

#### Possible Issues
Code using the left/right rack routing API will no longer compile.

#### Workaround
Use the new dynamic channel-mapping API: `get/setNumInputChannels`, `get/setNumOutputChannels`, `getNumChannelMappings()`, `get/setInputMapping`, `get/setOutputMapping`, `getInput/OutputGainParam (int)` etc. Existing saved edits are migrated automatically on load, so only code needs updating.

#### Rationale
Racks now support arbitrary channel counts rather than fixed stereo routing.

___

### Change
`AudioClipBase`'s left/right channel API has been removed: `setLeftChannelActive`, `isLeftChannelActive`, `setRightChannelActive`, `isRightChannelActive` and the protected `activeChannels` member. `getActiveChannels()` remains but is deprecated.

#### Possible Issues
Code using these methods will no longer compile.

#### Workaround
Use `getSourceChannelConfiguration()`, `getActiveChannelConfiguration()`, `setActiveChannelConfiguration()` and `getOutputChannelConfiguration()`.

#### Rationale
Clips can now enable/disable any subset of an arbitrary number of source channels.

___

### Change
`LevelMeasurer::Client::maxNumChannels` has been removed (channel storage is now dynamic), along with `setLevelCache()` and `getLevelCache()`.

#### Possible Issues
Code referencing the constant or the cache methods will no longer compile.

#### Workaround
Use the per-channel access APIs, which are unchanged; storage grows to fit however many channels are fed to the meter.

#### Rationale
Meters previously crashed when given more than the fixed number of channels; they now handle any channel count.

___

### Change
`PluginWindowState::windowLocked` has been removed, along with its saved property.

#### Possible Issues
Code reading or writing `windowLocked` will no longer compile.

#### Workaround
None - window locking has been removed as a concept. Implement it at the application level if needed.

#### Rationale
The flag didn't belong in the engine and was barely used in practice.

___

### Change
`RenderOptions::getStereo()` and the `stereo` cached value have been removed.

#### Possible Issues
Code querying or setting stereo/mono rendering through these will no longer compile.

#### Workaround
Use `getChannelConfiguration()` / `setChannelConfiguration()`.

#### Rationale
Renders can now target any channel layout, not just mono or stereo.

___

### Change
`UIBehaviour::showOkCancelAlertBox()` and `showYesNoCancelAlertBox()` have been replaced by `showOkCancelAlertBoxAsync()` and `showYesNoCancelAlertBoxAsync()`, which take a completion callback instead of returning a result. `showMenuAndCreatePlugin()` is deprecated in favour of `showMenuAndCreatePluginAsync()`.

#### Possible Issues
Overrides of the old methods will no longer compile, and callers can no longer get a synchronous answer.

#### Workaround
Override the `Async` versions and restructure calling code into continuation callbacks. The result semantics are unchanged (`true` = OK; `1` = yes, `2` = no, `0` = cancel).

#### Rationale
The engine no longer runs modal loops, which block the message thread and can re-enter the engine in unexpected ways.

___

### Change
Several save/load and project APIs have become asynchronous or lost their built-in UI as part of removing modal loops from the engine:
- `EditFileOperations::save()` and `saveAs()` now return `void` and take a completion callback (as do `AppFunctions::saveEdit()` / `saveEditAs()`)
- `Renderer::checkTargetFile()` now takes a completion callback instead of returning `bool`
- `ProjectManager::createNewProjectInteractively()` now returns `void` and takes a callback
- `ProjectManager::unpackArchiveAndAddToList()` now requires the caller to supply the destination directory (the engine no longer shows a file chooser)
- `Project::askAboutTempoDetect()` has been removed

#### Possible Issues
Code using the old synchronous signatures will no longer compile.

#### Workaround
Pass completion callbacks and move any dependent logic into them. For tests, `test_utilities::saveEditSync()` provides a synchronous save. UI questions such as tempo detection are now the application's responsibility.

#### Rationale
Modal loops inside the engine blocked the message thread and made embedding the engine in other applications fragile.

___

### Change
The free function `yieldGUIThread()` has been removed.

#### Possible Issues
Code calling it will no longer compile.

#### Workaround
None - delete the call. It was a Windows-only `juce::Thread::yield()` used from within modal loops, which no longer exist in the engine.

#### Rationale
Antiquated API that made no sense once the modal loops were removed.

___

### Change
`FallbackReader` has been renamed to `AudioFormatReaderWithTimeout`, and `AudioFileCache::createFallbackReader()` to `createAudioFormatReaderWithTimeout()`. `AudioFileCache::Reader::readSamples()` now takes `ChannelConfiguration` arguments instead of `juce::AudioChannelSet`.

#### Possible Issues
Code using the old names or signature will no longer compile. No compatibility alias is provided.

#### Workaround
Rename the symbols and update `readSamples()` call sites to pass `ChannelConfiguration` (see `ChannelConfiguration::fromChannelSet()`).

#### Rationale
The old name didn't describe what the class did, and the cache is now channel-layout aware.

___

### Change
`SourceFileReference::setToDirectFileReference()` and the file-taking `setToProjectFileReference()` overload have been replaced by a single `setToFile (const juce::File&, PathStyle, bool allowProjectItems)` method. `getSourceProjectItemID()` is now `getSourceProjectItemRef()`, returning a `ProjectItemRef`.

#### Possible Issues
Code using the removed setters or the old getter will no longer compile.

#### Workaround
Use `setToFile()` with `PathStyle::chooseBest`, `alwaysRelative` or `alwaysAbsolute`. Direct-file callers should pass `allowProjectItems = false`; project-item callers `chooseBest, true`.

#### Rationale
A single entry point with an explicit path policy replaces several overlapping setters, and supports the smarter relative/absolute path selection needed for folder-based projects.

___

### Change
`Exportable::ReferencedItem::itemID` (a `ProjectItemID`) is now `itemRef` (a `ProjectItemRef`), and the pure virtual `reassignReferencedItem()` now takes a `ProjectItemRef` instead of a `ProjectItemID`. The `EngineBehaviour::reassignReferencedItem()` overloads changed the same way.

#### Possible Issues
Implementations of these virtuals in custom `Clip` or `Plugin` subclasses will no longer compile (or, if not marked `override`, will silently stop being called).

#### Workaround
Update the signatures to take `ProjectItemRef`. `ProjectItemRef` converts implicitly from `ProjectItemID`, and `getProjectItemID()` returns a `std::optional<ProjectItemID>` when the underlying ID is needed.

#### Rationale
Referenced items must support path-based references for folder-based projects.

___

### Change
The engine's unit tests have been converted from `juce::UnitTest` to doctest.

#### Possible Issues
Clients that ran the engine's self-tests through a `juce::UnitTestRunner` (with `TRACKTION_UNIT_TESTS=1`) will find them gone.

#### Workaround
Run the tests with a doctest runner instead - see `examples/TestRunner`. The helpers in `tracktion_TestUtilities.h` retain their `juce::UnitTest`-taking overloads.

#### Rationale
doctest allows test filtering, better reporting and running the engine tests without a JUCE test harness.

___

### Change
`TracktionArchiveFile` and `ExportJob` moved to `legacy::` namespace.

#### Possible Issues
Code using `TracktionArchiveFile` or `ExportJob` directly will no longer compile.

#### Workaround
Qualify with `legacy::` (e.g. `legacy::TracktionArchiveFile`). For new code, use `ArchiveJob` instead, which produces standard zip files. Use `isArchive()` and `isLegacyArchive()` to detect archive formats.

#### Rationale
The legacy `.trkarch` format is replaced by standard zip archives. The old classes are preserved in a `legacy::` namespace for reading existing archives.

___

### Change
`ProjectSearchIndex`, `SearchOperation`, and related APIs have been removed.

#### Possible Issues
Code using `ProjectSearchIndex`, `SearchOperation`, `createSearchForKeywords()`, or `Project::searchFor()` will no longer compile. The header `tracktion_ProjectSearchIndex.h` has been deleted.

#### Workaround
Use plain string matching or Levenshtein distance on project item names/descriptions.

#### Rationale
The search infrastructure was tightly coupled to file-based project internals and not applicable to folder-based projects. Simpler alternatives are sufficient.

___

### Change
`Edit::createEditForExamining(Engine&, ValueTree, EditRole, LoadContext*)` overload is now `[[deprecated]]`.

#### Possible Issues
Deprecation warnings when calling this overload.

#### Workaround
Use `loadEditForExamining(ProjectManager&, ProjectItemRef)` instead.

#### Rationale
The new function works with both file-based and folder-based projects and handles source resolution correctly.

___

### Change
`Project::getAllItemIDs()` has been removed.

#### Possible Issues
Code that calls `getAllItemIDs()` will no longer compile.

#### Workaround
Use `getAllProjectItemRefs()` instead to iterate over project items.

#### Rationale
The method was unused and redundant with `getAllProjectItemRefs()`.

___

### Change
`ProjectItemRef::getProjectItemID()` now returns `std::optional<ProjectItemID>` instead of a bare `ProjectItemID`.

#### Possible Issues
Code that chains method calls on the return value (e.g. `.isValid()`, `.getProjectID()`, `.getRawID()`) will no longer compile.

#### Workaround
Use `has_value()` or bool conversion instead of `.isValid()`. Use `->` to access members when the optional is known to contain a value (e.g. `pid->getProjectID()`). Use `*` to extract the value when passing to functions that expect `ProjectItemID`.

#### Rationale
Returning `std::optional` makes the fallibility explicit at the type level, preventing accidental use of an invalid `ProjectItemID`.

___

### Change
`Project::getProjectID()` now returns `ProjectID` instead of `int`. `ProjectItemID` constructors and `createNewID` take `ProjectID` instead of `int`. `ProjectManager::getProject()` takes `ProjectID` instead of `int`.

#### Possible Issues
Code that stores or passes project IDs as raw `int` values will no longer compile.

#### Workaround
Wrap raw ints with `ProjectID(myInt)` and extract with `.toInt()`. For example: `ProjectID pid = project.getProjectID();` instead of `int pid = project.getProjectID();`.

#### Rationale
A strong type prevents accidental misuse of raw integers as project identifiers and makes the API self-documenting.

___

### Change
`ProjectItemRef` replaces raw `ProjectItemID` in many APIs. `Project::getProjectItemRef()`, `getProjectItemFor()`, `removeProjectItem()`, and `getIndexOf()` now use `ProjectItemRef`. `ProjectItem::getProjectItemRef()` returns `const ProjectItemRef&`. `ProjectManager::getProjectItem()` and `findSourceFile()` have new `ProjectItemRef` overloads.

#### Possible Issues
Code that explicitly types return values as `ProjectItemID` from these methods will no longer compile.

#### Workaround
`ProjectItemRef` has an implicit constructor from `ProjectItemID`, so most call sites compile unchanged. For return values, use `auto` or `ProjectItemRef`. If you need the old type, call `.getProjectItemID()` on the ref after checking `isProjectItemID()`.

#### Rationale
`ProjectItemRef` supports both ID-based references (file-based projects) and path-based references (folder-based projects) in a single type.

___

### Change
`ProjectManager::createNewProject`, `createNewProjectInteractively`, `createNewProjectFromTemplate`, and the `TempProject` constructor now accept a `ProjectType` parameter (`ProjectType::fileBased` or `ProjectType::folderBased`).

#### Possible Issues
None for existing code — the parameter defaults to `ProjectType::fileBased`.

#### Workaround
No changes required. Pass `ProjectType::folderBased` to create folder-based projects.

#### Rationale
Enables creation of folder-based projects through the existing project creation APIs.

___

### Change
`Project::findOrphanItems()` has been deprecated in favour of `findOrphanItemRefs()`.

#### Possible Issues
Deprecation warnings when calling `findOrphanItems()`.

#### Workaround
Replace `findOrphanItems()` with `findOrphanItemRefs()`, which returns `juce::Array<ProjectItemRef>` instead of `juce::Array<ProjectItem::Ptr>`.

#### Rationale
The new method returns `ProjectItemRef` values that work with both file-based and folder-based project backends.

___

### Change
`AudioClipBase::melodyneProxy` public member has been removed. Use the new `getARAProxy()` accessor instead.

#### Possible Issues
Code directly accessing `clip.melodyneProxy` will no longer compile.

#### Workaround
Replace `clip.melodyneProxy` with `clip.getARAProxy()`. The deprecated `getMelodyneProxy()` accessor is also available temporarily.

#### Rationale
The direct public member has been replaced with an accessor to support generic ARA plugins beyond Melodyne.

___

### Change
`AudioClipBase::showMelodyneWindow()`, `hideMelodyneWindow()`, and `melodyneConvertToMIDI()` have been moved from member functions to free functions: `showARAWindow(AudioClipBase&)`, `hideARAWindow(AudioClipBase&)`, and `araConvertToMIDI(AudioClipBase&)`.

#### Possible Issues
Code calling `clip.showMelodyneWindow()` etc. as member functions will no longer compile.

#### Workaround
Replace member calls with free function calls:
- `clip.showMelodyneWindow()` → `showARAWindow(clip)`
- `clip.hideMelodyneWindow()` → `hideARAWindow(clip)`
- `clip.melodyneConvertToMIDI()` → `araConvertToMIDI(clip)`

Deprecated free-function wrappers `showMelodyneWindow(clip)`, `hideMelodyneWindow(clip)`, and `melodyneConvertToMIDI(clip)` are also available temporarily.

#### Rationale
Moving these to free functions supports the generalised ARA plugin architecture.

___

### Change
`MelodyneFileReader` has been renamed to `ARAFileReader`. The header file has been renamed from `tracktion_MelodyneFileReader.h` to `tracktion_ARAFileReader.h`.

#### Possible Issues
Direct `#include` of the old header path will fail. Code using the class name directly will get deprecation warnings.

#### Workaround
A `using MelodyneFileReader = ARAFileReader` alias is provided for source compatibility. Update includes and type names when convenient.

#### Rationale
The reader now supports any ARA-compatible plugin, not just Melodyne.

___

### Change
`MelodyneNode` has been renamed to `ARANode`. The header file has been renamed from `tracktion_MelodyneNode.h` to `tracktion_ARANode.h`.

#### Possible Issues
Direct `#include` of the old header path will fail. Code referencing the class name directly will get deprecation warnings.

#### Workaround
A `using MelodyneNode = ARANode` alias is provided for source compatibility. Update includes and type names when convenient.

#### Rationale
The node now handles any ARA plugin playback, not just Melodyne.

___

### Change
`AudioClipBase::isUsingMelodyne()` has been renamed to `isUsingARA()`.

#### Possible Issues
Deprecation warnings. Code will still compile via the deprecated wrapper.

#### Workaround
Replace `isUsingMelodyne()` with `isUsingARA()`.

#### Rationale
The method now reflects generic ARA plugin support.

___

### Change
`TimeStretcher::Mode::melodyne` has been renamed to `TimeStretcher::Mode::ara`. `TimeStretcher::isMelodyne()` has been renamed to `isARA()`. The `getPossibleModes()` parameter has been renamed from `excludeMelodyne` to `excludeARA`.

#### Possible Issues
Code using `TimeStretcher::Mode::melodyne` will get deprecation warnings but will still compile. Code passing a named parameter `excludeMelodyne` will need updating.

#### Workaround
Replace `TimeStretcher::Mode::melodyne` with `TimeStretcher::Mode::ara`, `isMelodyne()` with `isARA()`, and `excludeMelodyne` with `excludeARA`.

#### Rationale
The timestretcher mode now supports any ARA plugin, not just Melodyne.

___

### Change
New `TRACKTION_ENABLE_ARA` config flag replaces the implicit Melodyne-only behaviour. ARA now requires explicit JUCE and SDK configuration.

#### Possible Issues
ARA support is now opt-in via the `TRACKTION_ENABLE_ARA` preprocessor flag (defaults to `0`/disabled). Projects that previously relied on implicit Melodyne support will need to update their build configuration.

#### Workaround
To enable ARA support:
1. Define `TRACKTION_ENABLE_ARA=1` in your build
2. Enable `JUCE_PLUGINHOST_ARA=1` in your JUCE project settings
3. Call `juce_set_ara_sdk_path()` in your CMake configuration, pointing to ARA SDK version 2.3 or later

#### Rationale
ARA support is now a generic, configurable feature rather than being tied to a single plugin. The explicit SDK dependency ensures correct versioning and compatibility.

___

### Change
`AutomationCurve` has been restructured. It now only stores the parameter as a string and not a reference.

#### Possible Issues
Many functions now need a `AutomatableParameter&`, `TempoSequence&` or `juce::UndoManager*` as additional arguments. 

#### Workaround
These extra parameters are usually within easy reach at the call site. You can use the new `getTempoSequence (Type&)` and `getUndoManager_p (Type&)` to easily get these from a bunch of objects.  

#### Rationale
This decoupling enables `AutomationCurve` to be used in more places and supports clip automation and `AutomationClip`s.

___

### Change
The `Edit` constructor can now throw exceptions in rare cases. E.g. if it's being constructed on the message thread which is blocked.

#### Possible Issues
You may need to catch this exception.

#### Workaround
It's generally safer to use Edit::createEdit as this will catch the exception
and just return a nullptr.


#### Rationale
Previously if the above scenario happened, the Edit would just be left in an invalid (likely to crash) state. Ths stops that happening.

___
### Change
The APIs around `InputDevice` and `InputDeviceInstance` have been simplified to work more with `EditItemID`s.

#### Possible Issues
Some existing code might not compile.

#### Workaround
Update to new APIs in `InputDeviceInstance`: `prepareToRecord`, `startRecording` and `stopRecording` along with other property setters/getters.
There are also some non-member functions at the bottom of `tracktion_InputDevice.h` which may replicate the old APIs (but with non-member arguments). 

#### Rationale
This change was required to implement `ClipSlot` recording and async-record-stopping.  

___
### Change
`InputDevice::setEndToEnd` has been replaced by `MonitorMode`.

#### Possible Issues
Code won't compile.

#### Workaround
Use the more explicit an clear `get/setMonitorMode`.

#### Rationale
`MonitorMode` provides a way to only enable audible input whilst record is enabled.  

___
### Change
An `Edit` constructor has been removed.

#### Possible Issues
Code using that constructor will fail to compile.

#### Workaround
Use the new constructor that takes an `Edit::Options` or an `EditRole`.

#### Rationale
The behaviour old constructor was ambiguous and this cleans up the API.

___

### Change
Removed the `float newValue` parameter to `AutomatableParameter::currentValueChanged (AutomatableParameter&)`.

#### Possible Issues
Existing code overriding the function will no longer compile.

#### Workaround
Remove the `float newValue` argument and use `AutomatoableParameter::getCurrentValue()` instead.

#### Rationale
This simplifies the API a bit as with Modifiers, the current value gets remapped a lot. Always getting the value via `getCurrentValue, getCurrentExplicitValue, getCurrentBaseValue, getCurrentModifierValue` avoids ambiguity about what `newValue` means.

___

### Change
Removed `DeviceManager::CPUUsageListener`

#### Possible Issues
Existing code using it will no longer compile.

#### Workaround
Use `getCPUStatistics()/restCPUStatistics()` instead.

#### Rationale
The old listener code wasn't thread safe. The new funtion returns more information and is wait-free from the audio side. 

---

### Change
`CurveEditor::getCurrentLineColour()` is no longer `const`

#### Possible Issues
Code implementing `CurveEditor::getCurrentLineColour()` will fail to compile.

#### Workaround
Remove `const` qualifier in sub classes.

#### Rationale
Better API design.

---

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