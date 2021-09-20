# 04 - Distortion Effect

In this tutorial, we're going to analyze the structure of an audio effect application, by applying distortion to a looping audio file.

## Key Topics Covered

- Adding custom plugin classes
- Customizing an Edit using plugin objects.
- Registering plugins with the Engine so that they can be added to an Edit's PluginList
- Adding plugins to a Track.
- Writing a custom ValueSource to bind a Slider/Value to an AutomatableParameter.

We'll show you how to register plugins with the engine in order to add them to an Edit's pluginList. We'll also cover adding plugins to a track

## Main Tracktion Engine Classes

#### Plugin
`Plugin` is the base class for plugins that can be loaded onto a `Track`.

#### PluginManager
`PluginManager` is a class for the management, registration and loading of plugins by the `Engine`.

## DistortionEffectDemo.h
## ParameterValueSource Class

We need to construct a class that wraps a `te::AutomatableParameter` as a `juce::ValueSource` so that it can be used as a `Value` in a `Slider`.

- `param` is a pointer to an `AutomatableParameter`


In the `ParameterValueSource` constructor, we attach a listener to
the `AutomatableParameter`

```
ParameterValueSource (AutomatableParameter::Ptr p)
    : param (p)
{
    param->addListener (this);
}
```



`setValue()` is automatically called whenever the value changes. We need to override the function to update `param` to reflect the new value.

```
void setValue (const var& newValue) override
{
    param->setParameter (static_cast<float> (newValue), juce::sendNotification);
}
```


We need to override the function `getValue()` to return the value from `param`.

```
var getValue() const override
{
    return param->getCurrentValue();
}
```

## bindSliderToParameter() Function

Now that we have created a class to wrap an AutomatableParameter as a ValueSource, we need to bind the
AutomatableParameter to a Slider so that changes in either are reflected across each other.

- We can do this by getting the valueRange from the AutomatableParameter and using it to construct a NormalisableRange object `range`. We then set the NormalisableRange of the Slider to `range`.
```
const auto v = p.valueRange;
const auto range = NormalisableRange<double> (static_cast<double> (v.start),
                                              static_cast<double> (v.end),
                                              static_cast<double> (v.interval),
                                              static_cast<double> (v.skew),
                                              v.symmetricSkew);
s.setNormalisableRange (range);
```
- We can then create a new ParameterValueSource object from AutomatableParameter `p`, cast it to the
`Value` type and refer the slider's `Value` object to it.
```
s.getValueObject().referTo (juce::Value (new ParameterValueSource (p)));
```


## DistortionPlugin Class

### DistortionPlugin() Constructor

- In the constructor, we refer the gainValue CachedValue member to the plugin's `state` gain property, by using the Edit's undomanager and providing a default value of 1.
```
auto um = getUndoManager();
gainValue.referTo (state, IDs::gain, um, 1.0f);
```

- We create an AutomatableParameter and assign it to our gainParam member. `addParam` registers the parameter with the `Plugin`. Next, `attachToCurrentValue` attaches the `gainValue` `CachedValue` member to the gain parameter.
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

- If the plugin is disabled, we return the function early to prevent processing.
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

- In order to control the gain parameter with the slider, we need to bind them together. This ensures that parameter value changes are reflected by the slider. When the slider is moved, the plugin parameter values are updated. Similarly, when the plugin parameter values are updated, the slider position changes.
```
auto gainParam = gainPlugin->getAutomatableParameterByID ("gain");
bindSliderToParameter (gainSlider, *gainParam);
```
