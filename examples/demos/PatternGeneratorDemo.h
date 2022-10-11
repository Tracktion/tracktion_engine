/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "../common/Utilities.h"

static String organPatch = "<PLUGIN type=\"4osc\" windowLocked=\"1\" id=\"1069\" enabled=\"1\" filterType=\"1\" presetDirty=\"0\" presetName=\"4OSC: Organ\" filterFreq=\"127.00000000000000000000\" ampAttack=\"0.60000002384185791016\" ampDecay=\"10.00000000000000000000\" ampSustain=\"100.00000000000000000000\" ampRelease=\"0.40000000596046447754\" waveShape1=\"4\" tune2=\"-24.00000000000000000000\" waveShape2=\"4\"> <MACROPARAMETERS id=\"1069\"/> <MODIFIERASSIGNMENTS/> <MODMATRIX/> </PLUGIN>";

static String leadPatch = "<PLUGIN type=\"4osc\" windowLocked=\"1\" id=\"1069\" enabled=\"1\" filterType=\"1\" waveShape1=\"3\" filterFreq=\"100\"><MACROPARAMETERS id=\"1069\"/><MODIFIERASSIGNMENTS/><MODMATRIX/></PLUGIN>";

//==============================================================================
class PatternGeneratorComponent   : public Component,
                                    private ChangeListener,
                                    private ComboBox::Listener,
                                    private Slider::Listener,
                                    private Timer
{
public:
    //==============================================================================
    PatternGeneratorComponent (Engine& e)
        : engine (e)
    {
        setupEdit();
        refreshMidiInputs();
        create4OSCPlugins();
        createMIDIClip();
        addUIComponents();

        startTimerHz (2);
        
        setSize (600, 400);
    }

    ~PatternGeneratorComponent() override
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
            refreshMidiDevicesButton.setBounds (topR.removeFromLeft (w).reduced (2));
            playPauseButton.setBounds (topR.removeFromLeft (w).reduced (2));
            midiInputsBox.setBounds (topR.removeFromLeft (w).reduced (2));
            
            r.removeFromTop (8);
            topR = r.removeFromTop (30);
            modeBox.setBounds (topR.removeFromLeft (w).reduced (2));
            keyBox.setBounds (topR.removeFromLeft (w).reduced (2));
            scaleBox.setBounds (topR.removeFromLeft (w).reduced (2));
            
            r.removeFromTop (8);
            topR = r.removeFromTop (30);
            int cw = topR.getWidth() / 8;
            
            for (auto b : chordBoxes)
                b->setBounds (topR.removeFromLeft (cw).reduced (2));

            topR = r.removeFromTop (30);
            cpuUsage.setBounds (topR.removeFromLeft (w).reduced (2));
        }

        {
            auto bottomR = r.removeFromBottom (60);
            tempoSlider.setBounds (bottomR.reduced (2));
        }
    }
    
    //==============================================================================
    void comboBoxChanged (ComboBox* b) override
    {
        if (b == &midiInputsBox)
        {
            auto& dm = engine.getDeviceManager();
            if (auto t = EngineHelpers::getOrInsertAudioTrackAt (edit, 1))
                if (auto dev = dm.getMidiInDevice (midiInputsBox.getSelectedItemIndex()))
                    for (auto instance : edit.getAllInputDevices())
                        if (&instance->getInputDevice() == dev)
                            instance->setTargetTrack (*t, 0, true);
            
            edit.restartPlayback();
        }
        else if (b == &modeBox)
        {
            auto& pg = getPatternGenerator();
            pg.mode = (te::PatternGenerator::Mode) (modeBox.getSelectedItemIndex() + 1);
            updatePattern();
        }
        else if  (b == &keyBox)
        {
            auto& pg = getPatternGenerator();
            pg.scaleRoot = keyBox.getSelectedItemIndex();
            updatePattern();
        }
        else if (b == &scaleBox)
        {
            auto& pg = getPatternGenerator();
            pg.scaleType = (te::Scale::ScaleType) scaleBox.getSelectedItemIndex();
            updatePattern();
        }
        else if (chordBoxes.contains (b))
        {
            auto& pg = getPatternGenerator();
            if (auto item = pg.getChordProgression()[chordBoxes.indexOf (b)])
                item->setChordName (indexToChord (b->getSelectedItemIndex()));
            
            updatePattern();
        }
    }
    
    void timerCallback() override
    {
        auto& dm = engine.getDeviceManager();
        cpuUsage.setText (String::formatted ("%d%% CPU", int (dm.getCpuUsage() * 100)), dontSendNotification);
    }
    
    void sliderValueChanged (Slider*) override
    {
        if (! ModifierKeys::getCurrentModifiers().isAnyMouseButtonDown())
            edit.tempoSequence.getTempos()[0]->setBpm (tempoSlider.getValue());
    }
    
    void sliderDragEnded (Slider*) override
    {
        edit.tempoSequence.getTempos()[0]->setBpm (tempoSlider.getValue());
    }

private:
    void addUIComponents()
    {
        transport.addChangeListener (this);
        
        Helpers::addAndMakeVisible (*this, { &refreshMidiDevicesButton, &playPauseButton, &midiInputsBox, &modeBox, &keyBox, &scaleBox, &cpuUsage, &tempoSlider });
        
        refreshMidiDevicesButton.onClick  = [this] { refreshMidiInputs(); };
        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (edit); };
        
        midiInputsBox.addListener (this);
        modeBox.addListener (this);
        keyBox.addListener (this);
        scaleBox.addListener (this);
        
        modeBox.addItemList ({"Arpeggios", "Chords", "Bass"}, 1);
        
        for (int i = 0; i < 12; i++)
            keyBox.addItem (MidiMessage::getMidiNoteName (i, true, false, 4), i + 1);
        
        scaleBox.addItemList (te::Scale::getScaleStrings(), 1);
        
        modeBox.setSelectedItemIndex (0);
        keyBox.setSelectedItemIndex (0);
        scaleBox.setSelectedItemIndex (0);
        
        for (int i = 0; i < 8; i++)
        {
            auto* cb = new ComboBox();
            addAndMakeVisible (cb);
            
            cb->addListener (this);
            
            for (int j = 0; j < 7; j++)
                cb->addItem (indexToChord (j), j + 1);
            
            auto& pg = getPatternGenerator();
            auto chords = pg.getChordProgressionChordNames (true);
            
            cb->setSelectedItemIndex (chordToIndex (chords[i]));
            
            chordBoxes.add (cb);
        }
        
        tempoSlider.setRange (30.0, 220.0, 0.1);
        tempoSlider.setValue (edit.tempoSequence.getTempos()[0]->getBpm(), dontSendNotification);
        tempoSlider.addListener (this);
    }
    
    void setupEdit()
    {
        auto& dm = engine.getDeviceManager();
        for (int i = 0; i < dm.getNumMidiInDevices(); i++)
        {
            auto dev = dm.getMidiInDevice (i);
            dev->setEnabled (true);
            dev->setEndToEndEnabled (true);
        }
        
        edit.playInStopEnabled = true;
    }
    
    void create4OSCPlugins()
    {
        //==============================================================================
        if (auto synth = dynamic_cast<te::FourOscPlugin*> (edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}).get()))
        {
            if (auto e = parseXML (organPatch))
            {
                auto vt = ValueTree::fromXml (*e);

                if (vt.isValid())
                    synth->restorePluginStateFromValueTree (vt);
            }
            
            if (auto t = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
                t->pluginList.insertPlugin (*synth, 0, nullptr);
        }
        
        //==============================================================================
        if (auto synth = dynamic_cast<te::FourOscPlugin*> (edit.getPluginCache().createNewPlugin (te::FourOscPlugin::xmlTypeName, {}).get()))
        {
            if (auto e = parseXML (leadPatch))
            {
                auto vt = ValueTree::fromXml (*e);

                if (vt.isValid())
                    synth->restorePluginStateFromValueTree (vt);
            }
            
            if (auto t = EngineHelpers::getOrInsertAudioTrackAt (edit, 1))
                t->pluginList.insertPlugin (*synth, 0, nullptr);
        }
    }
    
    te::MidiClip::Ptr createMIDIClip()
    {
        if (auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
        {
            // Find length of 8 bars
            const tracktion::TimeRange editTimeRange (0s, edit.tempoSequence.toTime ({ 8, {} }));
            track->insertNewClip (te::TrackItem::Type::midi, "MIDI Clip", editTimeRange, nullptr);
            
            if (auto midiClip = getClip())
            {
                auto& pg = *midiClip->getPatternGenerator();
                pg.setChordProgressionFromChordNames ({"I", "V", "VI", "III", "IV", "I", "IV", "V"});
                pg.mode = te::PatternGenerator::Mode::arpeggio;
                pg.scaleRoot = 0;
                pg.octave = 7;
                pg.velocity = 30;
                pg.generatePattern();
                
                return EngineHelpers::loopAroundClip (*midiClip);
            }
        }
        
        return {};
    }
    
    void updatePattern()
    {
        auto& pg = getPatternGenerator();
        pg.generatePattern();
    }
    
    te::MidiClip::Ptr getClip()
    {
        if (auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
            if (auto clip = dynamic_cast<te::MidiClip*> (track->getClips()[0]))
                return *clip;
        
        return {};
    }
    
    te::PatternGenerator& getPatternGenerator()
    {
        return *getClip()->getPatternGenerator();
    }
    
    void refreshMidiInputs()
    {
        auto& dm = engine.getDeviceManager();
        
        midiInputsBox.clear();
        for (int i = 0; i < dm.getNumMidiInDevices(); i++)
        {
            auto dev = dm.getMidiInDevice (i);
            if (dev->isEnabled())
                midiInputsBox.addItem (dev->getName(), i + 1);
        }
        midiInputsBox.setSelectedItemIndex (0, sendNotification);
    }

    //==============================================================================
    te::Engine& engine;
    te::Edit edit { engine, te::createEmptyEdit (engine), te::Edit::forEditing, nullptr, 0 };
    te::TransportControl& transport { edit.getTransport() };

    TextButton refreshMidiDevicesButton { "Refresh MIDI Devices" }, playPauseButton { "Play" };
    ComboBox midiInputsBox { "MIDI Inputs" }, modeBox, keyBox, scaleBox;
    OwnedArray<ComboBox> chordBoxes;
    Slider tempoSlider;
    
    Label cpuUsage;

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }
    
    void updatePlayButtonText()
    {
        playPauseButton.setButtonText (transport.isPlaying() ? "Pause" : "Play");
    }
    
    //==============================================================================
    int chordToIndex (String chord)
    {
        chord = chord.toUpperCase();
        
        if (chord == "I")   return 0;
        if (chord == "II")  return 1;
        if (chord == "III") return 2;
        if (chord == "IV")  return 3;
        if (chord == "V")   return 4;
        if (chord == "VI")  return 5;
        if (chord == "VII") return 6;
        jassertfalse;
        return 0;
    }

    String indexToChord (int idx)
    {
        if (idx == 0)   return "I";
        if (idx == 1)   return "II";
        if (idx == 2)   return "III";
        if (idx == 3)   return "IV";
        if (idx == 4)   return "V";
        if (idx == 5)   return "VI";
        if (idx == 6)   return "VII";

        jassertfalse;
        return "I";
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternGeneratorComponent)
};

//==============================================================================
static DemoTypeBase<PatternGeneratorComponent> patternGeneratorComponent ("Pattern Generator");
