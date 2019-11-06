# 01 - Playback

In this tutorial, we're going to take a quick look at the basic structure of a Tracktion Engine app by simply loading and playing back an audio file.


## Main Tracktion Engine Classes
##### Engine
Every Tracktion Engine app starts with an `Engine` instance. This performs the initialisation and shutdown of the engine.

##### Edit
The `Edit` is the container for a playable arrangement. It holds tracks, tempo sequences, Racks etc.

##### Track
`Edit` contains a list of `Track`s. There are several different types of `Track` such as `AudioTrack`s which output audio, `TempoTrack`s, `MarkerTrack`s etc. We'll come on to all of them in time but at the moment you'll probably only need to be familiar with `AudioTrack`.

##### Clip
`Clip` is the base class for clips that live in `Track`s. `Clip`s can be MIDI, audio, step or the edit-in-an-edit `EditClip`.


## PlaybackDemo.h
If you open the PlaybackDemo.h file you'll see it's a JUCE PIP. You can use the Projucer to create a project to build the PIP or use the script in `/tests` to generate one automatically.

### PlaybackDemo Class
You'll notice that the `PlaybackDemo` class inherits from `Component` so it can be displayed on the screen, and also `ChangeListener`. This is so we can be notified of changes to the playback state.
```
class PlaybackDemo  : public Component,
                      private ChangeListener
```

### Private Members
- If you look in the private members section of the `PlaybackDemo` class you'll see an instance of `Engine` which we pass our app name to as an argument
```
te::Engine engine { ProjectInfo::projectName };
```

- Next you'll see a pointer to the `Edit` which we will load from a command line argument.
```
std::unique_ptr<te::Edit> edit;
```

- Finally here you'll see two buttons and a label. One button to show audio settings (`settingsButton`) and one to start/stop playback (`playPauseButton`). We'll connect these in the constructor.
```
TextButton settingsButton { "Settings" }, playPauseButton { "Play" }; //[3]
Label editNameLabel { "No Edit Loaded" };
```

### PlaybackDemo() Constructor
- The first thing we do here is get the command line arguments and try and read the first argument as a `File`.
```
const auto editFilePath = JUCEApplication::getCommandLineParameterArray()[0];
jassert (editFilePath.isNotEmpty());
const File editFile (editFilePath);
```
If the Edit File exists, we'll try and load it.
```
edit = std::make_unique<te::Edit> (engine, te::loadEditFromFile (editFile, {}), te::Edit::forEditing, nullptr, 0);
```
What we want to end up with is an `Edit` instance which has been loaded from our file path argument. We use the `loadEditFromFile` helper method to do this which simply takes the Edit file as it's first argument. The second argument here is a `ProjectItemID` to use if the Edit file is empty but we're not interested in that at the moment so we can pass a default constructed `ProjectItemID` with `{}`.

- Once we've got back the Edit file's `ValueTree` state, we can pass this on to our `Edit` constructor to load it.
The Edit constructor is declared like this:
```
Edit (Engine&, juce::ValueTree editState, EditRole, LoadContext*, int numUndoLevelsToStore);
```
so we have to make sure to pass in our `engine` instance, the Edit state we've just loaded from the file and `Edit::forEditing` to make sure we can play it back (as opposed to just being used to examine or render). We can optionally specify an `Edit::LoadContext` here to be notified of load progress and a number of undo levels.

If the Edit File does not exist, we will create a sample Edit by taking an .ogg file and adding it to Track 1 as a clip and then setting the loop markers around the clip.
```
auto f = File::createTempFile (".ogg");
f.replaceWithData (PlaybackDemoAudio::BITs_Export_2_ogg, PlaybackDemoAudio::BITs_Export_2_oggSize);

edit = std::make_unique<te::Edit> (engine, te::createEmptyEdit(), te::Edit::forEditing, nullptr, 0);
auto clip = EngineHelpers::loadAudioFileAsClip (*edit, f);
EngineHelpers::loopAroundClip (*clip);
```

- Once we've got our Edit instance and stored it in our member unique_ptr, we can set up the transport to play the whole thing back.
First we get the `TransportControl` instance from the `Edit` which is used to control playback.
Then we set its loop range based on the length of the `Edit`.
Then make sure the transport is looping, play it and add ourselves as a change listener to be notified of play state changes.
```
auto& transport = edit->getTransport();
transport.setLoopRange ({ 0.0, edit->getLength() });
transport.looping = true;
transport.play (false);
transport.addChangeListener (this);
```

- Finally we set the name of our label to the file name passed in, and assign a callback to our button.
All we'll do here is capture our `this` pointer so we have access to our `edit` member, then use the helper method to toggle the play state.
```
editNameLabel.setText (editFile.getFileNameWithoutExtension(), dontSendNotification);
playPauseButton.onClick = [this] { EngineHelpers::togglePlay (*edit); };
```

- And that's it! The `Edit` should now be playing back looping around.
There's a little more house-keeping to update the play button text to reflect the play state and add all our components as children:
```
settingsButton.onClick  = [this] { EngineHelpers::showAudioDeviceSettings (engine); };
updatePlayButtonText();
editNameLabel.setJustificationType (Justification::centred);
Helpers::addAndMakeVisible (*this, { &settingsButton, &playPauseButton, &editNameLabel });
```
```
void updatePlayButtonText()
{
    playPauseButton.setButtonText (edit->getTransport().isPlaying() ? "Pause" : "Play");
}
```
