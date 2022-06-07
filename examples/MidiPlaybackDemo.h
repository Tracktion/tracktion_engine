/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             MidiPlaybackDemo
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      This example shows how to play back MIDI at various tempos.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine, tracktion_graph
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_VST3=1, JUCE_PLUGINHOST_AU=1
  defines:          TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1, JUCE_MODAL_LOOPS_PERMITTED=1

  type:             Component
  mainClass:        MidiPlaybackComponent

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "common/Utilities.h"
#include "common/Components.h"

static auto pluckPatch = R"(
    <PLUGIN type="4osc" id="1065" enabled="1" filterType="1" ampVelocity="41.63125228881836"
          filterAttack="0.0" filterDecay="0.07040189951658249" filterSustain="25.0"
          filterAmount="0.3224061727523804" waveShape1="2" ampAttack="0.001000000047497451"
          ampRelease="0.3397806882858276" filterRelease="0.08646350353956223"
          ampDecay="0.883297860622406" ampSustain="0.0" filterFreq="87.23107147216797"
          masterLevel="-5.751228332519531" filterResonance="43.2890625"
          filterVelocity="50.0" voiceMode="2" filterSlope="12" delayOn="0"
          tune1="-12.0" fineTune1="0.0" pulseWidth1="0.3752184212207794"
          detune1="0.2413906306028366" voices1="2" tune2="12.0" level2="-100.0"
          waveShape2="4" tune3="28.0" level3="-100.0" pulseWidth3="0.8984618782997131"
          waveShape3="2" level4="-100.0" pulseWidth4="0.4288274943828583"
          waveShape4="2" modAttack1="0.0" modDecay1="0.2390823066234589"
          modSustain1="0.0" modRelease1="0.1669068783521652" voices2="1"
          spread1="21.76250457763672" windowX="457" windowY="61" windowLocked="0">
    <MACROPARAMETERS id="1066"/>
    <MODIFIERASSIGNMENTS/>
    <FACEPLATE width="8" height="4" autoSize="1">
      <RESOURCES/>
    </FACEPLATE>
    <MODMATRIX>
      <MODMATRIXITEM modParam="filterFreq" modItem="cc1" modDepth="0.1999531388282776"/>
      <MODMATRIXITEM modParam="level3" modItem="env1" modDepth="0.5507969260215759"/>
      <MODMATRIXITEM modParam="level4" modItem="cc1" modDepth="1.0"/>
      <MODMATRIXITEM modParam="level2" modItem="env1" modDepth="0.8862656950950623"/>
    </MODMATRIX>
    </PLUGIN>)";

static constexpr unsigned char midi_file_data_local[] =
{ 77,84,104,100,0,0,0,6,0,1,0,2,0,96,77,84,114,107,0,0,0,19,0,255,81,3,6,138,27,0,255,88,4,4,2,24,8,0,255,47,0,77,84,114,107,0,0,1,92,0,255,3,12,114,101,70,88,32,78,101,120,117,115,32,49,0,144,48,100,0,144,60,100,48,128,60,64,0,144,63,100,48,128,63,64,
0,144,67,100,48,128,48,64,0,128,67,64,0,144,72,100,48,128,72,64,0,144,48,100,0,144,60,100,48,128,60,64,0,144,63,100,48,128,63,64,0,144,67,100,48,128,48,64,0,128,67,64,0,144,51,100,0,144,72,100,48,128,51,64,0,128,72,64,0,144,44,100,0,144,56,100,48,128,
56,64,0,144,60,100,48,128,60,64,0,144,63,100,48,128,44,64,0,128,63,64,0,144,68,100,48,128,68,64,0,144,44,100,0,144,56,100,48,128,56,64,0,144,60,100,48,128,60,64,0,144,63,100,48,128,44,64,0,128,63,64,0,144,68,100,48,128,68,64,0,144,39,100,0,144,51,100,
48,128,51,64,0,144,58,100,48,128,58,64,0,144,63,100,48,128,39,64,0,128,63,64,0,144,67,100,48,128,67,64,0,144,39,100,0,144,51,100,48,128,51,64,0,144,58,100,48,128,58,64,0,144,63,100,48,128,39,64,0,128,63,64,0,144,67,100,48,128,67,64,0,144,39,100,0,144,
51,100,48,128,51,64,0,144,58,100,48,128,58,64,0,144,63,100,48,128,39,64,0,128,63,64,0,144,67,100,48,128,67,64,0,144,38,100,0,144,50,100,48,128,50,64,0,144,58,100,48,128,58,64,0,144,67,100,48,128,38,64,0,128,67,64,0,144,65,100,48,128,65,64,0,255,47,0,
0,0 };

const char* midi_file_data = (const char*) midi_file_data_local;
const size_t midi_file_dataSize = 397;


//==============================================================================
//==============================================================================
class ClipEditorComponent : public juce::Component,
                            private juce::ValueTree::Listener
{
public:
    ClipEditorComponent (te::MidiClip::Ptr);
    ClipEditorComponent (te::WaveAudioClip::Ptr);

    void paint (Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;

    void mouseMove (const MouseEvent&) override;
    void mouseDown (const MouseEvent&) override;
    void mouseDrag (const MouseEvent&) override;
    void mouseUp (const MouseEvent&) override;

private:
    te::MidiClip::Ptr midiClip;
    te::WaveAudioClip::Ptr audioClip;
    te::Clip::Ptr clip;
    juce::ValueTree state { clip->state };
    te::SelectionManager selectionManager { clip->edit.engine };
    EditViewState editViewState { clip->edit, selectionManager };
    std::unique_ptr<te::SmartThumbnail> thumbnail;

    enum class DragType
    {
        none, loopStart, loopEnd
    };

    DragType dragType;
    te::TimePosition dragStartTime;
    te::TimeRange loopRangeOnDragStart;

    void init();

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
};


//==============================================================================
ClipEditorComponent::ClipEditorComponent (te::MidiClip::Ptr c)
    : midiClip (c), clip (c)
{
    init();
}

ClipEditorComponent::ClipEditorComponent (te::WaveAudioClip::Ptr c)
    : audioClip (c), clip (c)
{
    thumbnail = std::make_unique<te::SmartThumbnail> (clip->edit.engine, te::AudioFile (clip->edit.engine, audioClip->getOriginalFile()), *this, &clip->edit);

    init();
}

void ClipEditorComponent::paint (Graphics& g)
{
    const auto r = getLocalBounds();

    g.setColour (Colours::black.withAlpha (0.15f));
    g.fillRect (r.reduced (1));

    g.setColour (Colours::white.withAlpha (0.5f));
    g.drawRect (r);

    if (midiClip)
        drawMidiClip (g, *midiClip, r, clip->getEditTimeRange());
    else if (thumbnail)
        thumbnail->drawChannels (g, r, true,
                                 clip->getEditTimeRange(), 1.0f);
}

void ClipEditorComponent::paintOverChildren (juce::Graphics& g)
{
    const te::TimeRange loopRange (clip->getLoopStart(), clip->getLoopLength());
    const auto loopStartX   = editViewState.timeToX (loopRange.getStart(), getWidth());
    const auto loopEndX     = editViewState.timeToX (loopRange.getEnd(), getWidth());

    const auto r = getLocalBounds();
    g.setColour (Colours::white.withAlpha (0.8f));
    g.fillRect (r.withX (loopStartX).withWidth (2));
    g.fillRect (r.withX (loopEndX - 2).withWidth (2));

    g.setColour (Colours::black.withAlpha (0.25f));
    g.fillRect (r.withRight (loopStartX));
    g.fillRect (r.withX (loopEndX));
}

void ClipEditorComponent::mouseMove (const MouseEvent& e)
{
    const te::TimeRange loopRange (clip->getLoopStart(), clip->getLoopLength());
    const te::TimeRange targetRange (editViewState.xToTime (e.x - 2, getWidth()),
                                     editViewState.xToTime (e.x + 2, getWidth()));

    if (targetRange.contains (loopRange.getStart()))
        setMouseCursor (MouseCursor::LeftRightResizeCursor);
    else if (targetRange.contains (loopRange.getEnd()))
        setMouseCursor (MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor (MouseCursor::ParentCursor);
}

void ClipEditorComponent::mouseDown (const MouseEvent& e)
{
    const te::TimeRange loopRange (clip->getLoopStart(), clip->getLoopLength());
    loopRangeOnDragStart = { loopRange.getStart(), loopRange.getEnd() };
    dragStartTime = editViewState.xToTime (e.x, getWidth());
    const te::TimeRange targetRange (editViewState.xToTime (e.x - 2, getWidth()),
                                     editViewState.xToTime (e.x + 2, getWidth()));

    if (targetRange.contains (loopRange.getStart()))
        dragType = DragType::loopStart;
    else if (targetRange.contains (loopRange.getEnd()))
        dragType = DragType::loopEnd;
    else
        dragType = DragType::none;
}

void ClipEditorComponent::mouseDrag (const MouseEvent& e)
{
    if (dragType == DragType::none)
        return;

    const auto dragDelta = toDuration (editViewState.xToTime (e.getDistanceFromDragStartX(), getWidth()));
    const auto newPosition = clip->getEditTimeRange().clipPosition (dragStartTime + dragDelta);

    if (dragType == DragType::loopStart)
        clip->setLoopRange ({ std::min (newPosition, loopRangeOnDragStart.getEnd()), loopRangeOnDragStart.getEnd() });
    if (dragType == DragType::loopEnd)
        clip->setLoopRange ({ loopRangeOnDragStart.getStart(), std::max (loopRangeOnDragStart.getStart(), newPosition) });
}

void ClipEditorComponent::mouseUp (const MouseEvent& e)
{
    mouseDrag (e);
    dragType = DragType::none;
    setMouseCursor (MouseCursor::ParentCursor);
}

void ClipEditorComponent::init()
{
    editViewState.viewX2 = clip->getPosition().getEnd();
    state.addListener (this);
}

void ClipEditorComponent::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& i)
{
    if (i == te::IDs::offset
        || i == te::IDs::loopStart || i == te::IDs::loopLength
        || i == te::IDs::loopStartBeats || i == te::IDs::loopLengthBeats)
       repaint();
}


//==============================================================================
class MidiPlaybackComponent : public Component,
                              private FilenameComponentListener,
                              private ChangeListener,
                              private Timer
{
public:
    //==============================================================================
    MidiPlaybackComponent()
    {
//ddd        edit.tempoSequence.insertTempo (BeatPosition::fromBeats (2), 120.0, 0.0);
//        edit.tempoSequence.insertTempo (BeatPosition::fromBeats (8), 60.0, 0.0);
//        edit.tempoSequence.insertTimeSig (BeatPosition::fromBeats (4))->denominator = 8;
//        edit.pitchSequence.insertPitch (te::BeatPosition::fromBeats (4), 66);

        create4OSCPlugin();
        addUIComponents();

//ddd        {
//            TemporaryFile tempMidiFile ("mid");
//            tempMidiFile.getFile().replaceWithData (midi_file_data, midi_file_dataSize);
//            auto mc = loadMidiFile (tempMidiFile.getFile());
//            addAndMakeVisible (*(clipEditorComponent = std::make_unique<ClipEditorComponent> (mc)));
//        }
        if (auto wc = loadAudioFile (juce::String ("/Users/dave/Music/Samples/Amen Breaks/412149__leonsptvx__amen-break-materjal.wav")))
//        if (auto wc = loadAudioFile (juce::String ("/Users/dave/Music/Samples/Test Samples/Click 120bpm.wav")))
//        if (auto wc = loadAudioFile (juce::String ("/Users/dave/Music/Samples/Tones/sin clip.wav")))
            addAndMakeVisible (*(clipEditorComponent = std::make_unique<ClipEditorComponent> (wc)));

        startTimerHz (2);
        
        setSize (600, 200);
    }

    ~MidiPlaybackComponent() override
    {
        edit.getTempDirectory (false).deleteRecursively();
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds();

        {
            auto topR = r.removeFromTop (30);
            int w = topR.getWidth() / 3;
            settingsButton.setBounds (topR.removeFromLeft (w).reduced (2));
            playPauseButton.setBounds (topR.removeFromLeft (w).reduced (2));
            metronomeButton.setBounds (topR.reduced (2));

            r.removeFromTop (8);
            topR = r.removeFromTop (30);
            fileComp.setBounds (topR.reduced (2));

            topR = r.removeFromTop (30);
            cpuUsage.setBounds (topR.removeFromLeft (w).reduced (2));
        }

        {
            auto bottomR = r.removeFromBottom (60).withTrimmedLeft (60);
            nudgeSlider.setBounds (bottomR.removeFromBottom (30).reduced (2));
            tempoSlider.setBounds (bottomR.reduced (2));
        }

        if (clipEditorComponent)
            clipEditorComponent->setBounds (r.reduced (5, 0));
    }
    
    //==============================================================================
    void timerCallback() override
    {
        auto& dm = engine.getDeviceManager();
        cpuUsage.setText (String::formatted ("%d%% CPU", int (dm.getCpuUsage() * 100)), dontSendNotification);
    }

private:
    void addUIComponents()
    {
        transport.addChangeListener (this);
        
        Helpers::addAndMakeVisible (*this, { &settingsButton, &playPauseButton, &metronomeButton, &fileComp, &cpuUsage,
                                             &tempoSlider, &nudgeSlider, &tempoLabel, &nudgeLabel });
        
        settingsButton.onClick  = [this] { EngineHelpers::showAudioDeviceSettings (engine); };
        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (edit); };
        metronomeButton.getToggleStateValue().referTo (edit.clickTrackEnabled.getPropertyAsValue());

        fileComp.addListener (this);

        tempoLabel.attachToComponent (&tempoSlider, true);
        tempoSlider.setTextBoxStyle (Slider::TextBoxRight, false, 80, 20);
        tempoSlider.setRange (30.0, 220.0, 0.1);
        tempoSlider.setValue (edit.tempoSequence.getTempos()[0]->getBpm(), dontSendNotification);
        tempoSlider.onValueChange = [this] { edit.tempoSequence.getTempos()[0]->setBpm (tempoSlider.getValue()); };

        nudgeLabel.attachToComponent (&nudgeSlider, true);
        nudgeSlider.setTextBoxStyle (Slider::TextBoxRight, false, 80, 20);
        nudgeSlider.setTextValueSuffix ("%");
        nudgeSlider.setRange (-50.0, 50.0, 0.1);
        nudgeSlider.setValue (0.0, dontSendNotification);
        nudgeSlider.onDragEnd = [this] { nudgeSlider.setValue (0.0); };
        nudgeSlider.onValueChange = [this]
                                    {
                                        if (auto epc = edit.getCurrentPlaybackContext())
                                            epc->setTempoAdjustment (nudgeSlider.getValue() * 0.01);
                                    };
    }
    
    void create4OSCPlugin()
    {
        if (auto synth = dynamic_cast<te::FourOscPlugin*> (edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}).get()))
        {
            if (auto e = parseXML (pluckPatch))
            {
                auto vt = ValueTree::fromXml (*e);

                if (vt.isValid())
                    synth->restorePluginStateFromValueTree (vt);
            }
            
            if (auto t = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
                t->pluginList.insertPlugin (*synth, 0, nullptr);
        }
    }
    
    te::MidiClip::Ptr loadMidiFile (juce::File f)
    {
        // Load MIDI files on to track 0 which has 4OSC
        if (auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
        {
            juce::OwnedArray<te::MidiList> lists;
            juce::Array<te::BeatPosition> tempoChangeBeatNumbers;
            juce::Array<double> bpms;
            juce::Array<int> numerators;
            juce::Array<int> denominators;
            te::BeatDuration songLength;
            te::MidiList::readSeparateTracksFromFile (f, lists, tempoChangeBeatNumbers,
                                                      bpms, numerators, denominators, songLength, false);

            if (auto list = lists.getFirst())
            {
                // Remove clips from all tracks so this new one is the only clip
                for (auto at : te::getAudioTracks (edit))
                    for (auto c : at->getClips())
                        c->removeFromParentTrack();

                auto clip = track->insertMIDIClip ({ te::TimePosition(), edit.tempoSequence.beatsToTime (list->getLastBeatNumber()) }, nullptr);

                if (auto midiClip = clip.get())
                {
                    midiClip->setLoopRange (clip->getEditTimeRange());
                    midiClip->getSequence().copyFrom (*list, nullptr);
                    return EngineHelpers::loopAroundClip (*midiClip);
                }
            }
        }
        
        return {};
    }

    te::WaveAudioClip::Ptr loadAudioFile (juce::File f)
    {
        // Load audio files on to track 1 to avoid being blocked by 4OSC on track 0
        if (auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 1))
        {
            // Remove clips from all tracks so this new one is the only clip
            for (auto at : te::getAudioTracks (edit))
                for (auto c : at->getClips())
                    c->removeFromParentTrack();

            auto clip = track->insertWaveClip (f.getFileNameWithoutExtension(), f,
                                               {{ 0s, te::TimeDuration::fromSeconds (te::AudioFile (engine, f).getLength()) }, {}},
                                               false);

            if (auto audioClip = clip.get())
            {
                audioClip->setUsesProxy (false);
                audioClip->setLoopRange (clip->getEditTimeRange().withStart (0s));
                audioClip->setAutoPitch (true);

                return EngineHelpers::loopAroundClip (*audioClip);
            }
        }

        return {};
    }
    
    //==============================================================================
    te::Engine engine { ProjectInfo::projectName };
    te::Edit edit { engine, te::createEmptyEdit (engine), te::Edit::forEditing, nullptr, 0 };
    te::TransportControl& transport { edit.getTransport() };

    juce::TextButton settingsButton { "Settings" }, playPauseButton { "Play" };
    juce::ToggleButton metronomeButton { "Metronome" };
    FilenameComponent fileComp { "Audio or MIDI File", {}, true, false, false,
                                 te::soundFileAndMidiWildCard, {}, "Browse for an audio or MIDI file to load..." };
    std::unique_ptr<ClipEditorComponent> clipEditorComponent;
    Slider tempoSlider, nudgeSlider;
    Label tempoLabel { {}, "Tempo" }, nudgeLabel { {}, "Nudge" }, cpuUsage;

    void filenameComponentChanged (FilenameComponent*) override
    {
        if (auto f = fileComp.getCurrentFile(); te::isMidiFile (f))
        {
            if (auto mc = loadMidiFile (f))
                addAndMakeVisible (*(clipEditorComponent = std::make_unique<ClipEditorComponent> (mc)));
        }
        else
        {
            if (auto wc = loadAudioFile (f))
                addAndMakeVisible (*(clipEditorComponent = std::make_unique<ClipEditorComponent> (wc)));
        }

        resized();
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }
    
    void updatePlayButtonText()
    {
        playPauseButton.setButtonText (transport.isPlaying() ? "Pause" : "Play");
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiPlaybackComponent)
};
