# 04 - Distortion Effect

In this tutorial, we're going to analyze the structure of an audio effect application, by applying distortion to a looping audio file.

## Main Tracktion Engine Classes
#### Engine
Every tracktion Engine app starts with an 'Engine' instance. This performs the initialisation and shutdown of the engine.

#### Edit
The 'Edit' is the container for a playable arrangement. it holds tracks, tempo sequences, Racks etc.

#### Track
'Edit' contains a list of 'Track's. This project makes use of the 'AudioTrack' which outputs audio.

#### Clip
'Clip' is the base class for clips that live in 'Track's. 'Clip's can be MIDI, audio, step or the edit-in-an-edit 'editClip.'


## Main JUCE Classes
#### Plugin

## DistortionEffectDemo.h


## DistortionPlugin Class

### DistortionPlugin() Constructor

- In the constructor, we initialise the undo manager to enable users to undo plugin value changes, and refer each plugin value to the undo manager.
```
auto um = getUndoManager();
gainValue.referTo (state, IDs::gain, um, 1.0f);
```

- We then initialise each plugin parameter and attach them to the values they represent.
```
gainParam = addParam ("gain", TRANS("Gain"), { 0.1f, 20.0f });
gainParam->attachToCurrentValue (gainValue);
```

### DistortionPlugin() Destructor

- In the Destructor, we notify listeners that the object has been deleted and detach all parameters from their current values.
```
notifyListenersOfDeletion();
gainParam->detachFromCurrentValue();
```

### ApplyToBuffer (const PluginRenderContext& fc) Function

- The `applyToBuffer` function is where plugin processing takes place. The destination buffer can be obtained from the `PluginRenderContext` in order to apply digital signal processing.

- First, if the plugin is disabled, we return the function early to prevent processing.
```
if (! isEnabled())
    return;
```

- We iterate through each sample of each channel and apply the tanh waveshaping function to the output audio.
```
for (int channel = 0; channel < fc.destBuffer->getNumChannels(); ++channel)
{
    auto dest = fc.destBuffer->getWritePointer (channel);
                
    for (int i = 0; i < fc.bufferNumSamples; ++i)
        dest[i] = std::tanh (gainValue * dest[i]);
}
```

## DistortionEffectDemo Class

The DistortionEffectDemo is defined similarly to the PitchAndTimeComponent class

```
class DistortionEffectDemo : public Component,
                             private ChangeListener
```

## Private Members
- If you look in the private member section of the `DistortionEffectDemo` again, you'll see similarities to the PitchAndTimeComponent and PlayBackDemo. There is an `Engine` to set up Tracktion Engine, and an `Edit` for editing.

```
te::Edit edit { engine, te::createEmptyEdit(), te::Edit::forEditing, nullptr, 0 };
te::TransportControl& transport { edit.getTransport() };
```
- We've added a slider called `gainSlider` to represent distortion gain.
```
TextButton settingsButton { "Settings" }, playPauseButton { "Play" }, loadFileButton { "Load file" };
Thumbnail thumbnail { transport };
Slider gainSlider;
```

### DistortionEffectDemo() Constructor

- In the constructor, we'll start by registering the `DistortionPlugin` class with Tracktion Engine.
```
engine.getPluginManager().createBuiltInType<DistortionPlugin>();
```

- We'll add the sliders and buttons to the GUI.
```
Helpers::addAndMakeVisible (*this, { &gainSlider, &settingsButton, &playPauseButton });
```

- Next, we'll load the PlayBackDemo audio example into file f.
```
oggTempFile = std::make_unique<TemporaryFile> (".ogg");
auto f = oggTempFile->getFile();
f.replaceWithData (PlaybackDemoAudio::BITs_Export_2_ogg, PlaybackDemoAudio::BITs_Export_2_oggSize);
```

- We'll create a clip from the audio file. Then we'll create a track on the edit. Finally, we'll load the clip onto the track.
```
auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0);
jassert (track != nullptr);

te::AudioFile audioFile (edit.engine, f);
auto clip = track->insertWaveClip (f.getFileNameWithoutExtension(), f,
                                  { { 0.0, audioFile.getLength() }, 0.0 }, false);
jassert (clip != nullptr);
```

- Now that the track has been created and the clip is on the track, we need to create a new instance of DistortionPlugin and load it onto the track.
```
auto gainPlugin = edit.getPluginCache().createNewPlugin (DistortionPlugin::xmlTypeName, {});
track->pluginList.insertPlugin (gainPlugin, 0, nullptr);
```
- Finally, in order to hear the effect being played, we must set the transport loop range to the start/end of the clip, set it to loop, and initialise play.
```
edit.getTransport().addChangeListener (this);
EngineHelpers::loopAroundClip (*clip);
```

- In order to control the gain parameter with the slider, we need to bind them together.
```
auto gainParam = gainPlugin->getAutomatableParameterByID ("gain");
bindSliderToParameter (gainSlider, *gainParam);
```