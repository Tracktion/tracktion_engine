/*******************************************************************************
 The block below describes the properties of this PIP. A PIP is a short snippet
 of code that can be read by the Projucer and used to generate a JUCE project.

 BEGIN_JUCE_PIP_METADATA

  name:             RecordingDemo
  version:          0.0.1
  vendor:           Tracktion
  website:          www.tracktion.com
  description:      This example simply creates a new project and records from all avaliable inputs.

  dependencies:     juce_audio_basics, juce_audio_devices, juce_audio_formats, juce_audio_processors, juce_audio_utils,
                    juce_core, juce_data_structures, juce_dsp, juce_events, juce_graphics,
                    juce_gui_basics, juce_gui_extra, juce_osc, tracktion_engine
  exporters:        linux_make, vs2017, xcode_iphone, xcode_mac

  moduleFlags:      JUCE_STRICT_REFCOUNTEDPOINTER=1, JUCE_PLUGINHOST_AU=1, JUCE_PLUGINHOST_VST3=1

  type:             Component
  mainClass:        RecordingDemo

 END_JUCE_PIP_METADATA

*******************************************************************************/

#pragma once

#include "common/Utilities.h"
#include "common/Components.h"

//==============================================================================
class RecordingDemo  : public Component,
                       private ChangeListener
{
public:
    //==============================================================================
    RecordingDemo()
    {
        settingsButton.onClick  = [this] { EngineHelpers::showAudioDeviceSettings (engine); createTracksAndAssignInputs(); };
        newEditButton.onClick = [this] { createNewEdit(); };
        
        updatePlayButtonText();
        updateRecordButtonText();
        editNameLabel.setJustificationType (Justification::centred);
        Helpers::addAndMakeVisible (*this, { &settingsButton, &newEditButton, &playPauseButton, &recordButton, &showEditButton,
                                             &newTrackButton, &clearTracksButton, &deleteButton, &editNameLabel, &showWaveformButton });

        deleteButton.setEnabled (false);
        createNewEdit (File::getSpecialLocation (File::tempDirectory).getNonexistentChildFile ("Test", ".tracktionedit", false));
        selectionManager.addChangeListener (this);
        
        setupButtons();
        
        setSize (700, 500);
    }

    ~RecordingDemo()
    {
        engine.getTemporaryFileManager().getTempDirectory().deleteRecursively();
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
    }

    void resized() override
    {
        auto r = getLocalBounds();
        int w = r.getWidth() / 8;
        auto topR = r.removeFromTop (30);
        settingsButton.setBounds (topR.removeFromLeft (w).reduced (2));
        newEditButton.setBounds (topR.removeFromLeft (w).reduced (2));
        playPauseButton.setBounds (topR.removeFromLeft (w).reduced (2));
        recordButton.setBounds (topR.removeFromLeft (w).reduced (2));
        showEditButton.setBounds (topR.removeFromLeft (w).reduced (2));
        newTrackButton.setBounds (topR.removeFromLeft (w).reduced (2));
        clearTracksButton.setBounds (topR.removeFromLeft (w).reduced (2));
        deleteButton.setBounds (topR.removeFromLeft (w).reduced (2));
        topR = r.removeFromTop (30);
        showWaveformButton.setBounds (topR.removeFromLeft (w * 2).reduced (2));
        editNameLabel.setBounds (topR);
        
        if (editComponent != nullptr)
            editComponent->setBounds (r);
    }

private:
    //==============================================================================
    te::Engine engine { ProjectInfo::projectName };
    te::SelectionManager selectionManager { engine };
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<EditComponent> editComponent;

    TextButton settingsButton { "Settings" }, newEditButton { "New" }, playPauseButton { "Play" }, recordButton { "Record" },
               showEditButton { "Show Edit" }, newTrackButton { "New Track" }, clearTracksButton { "Clear Tracks" }, deleteButton { "Delete" };
    Label editNameLabel { "No Edit Loaded" };
    ToggleButton showWaveformButton { "Show Waveforms" };

    //==============================================================================
    void setupButtons()
    {
        playPauseButton.onClick = [this]
        {
            bool wasRecording = edit->getTransport().isRecording();
            EngineHelpers::togglePlay (*edit);
            if (wasRecording)
                te::EditFileOperations (*edit).save (true, true, false);
        };
        recordButton.onClick = [this]
        {
            bool wasRecording = edit->getTransport().isRecording();
            EngineHelpers::toggleRecord (*edit);
            if (wasRecording)
                te::EditFileOperations (*edit).save (true, true, false);
        };
        newTrackButton.onClick = [this]
        {
            edit->ensureNumberOfAudioTracks (getAudioTracks (*edit).size() + 1);
        };
        clearTracksButton.onClick = [this]
        {
            for (auto t : te::getAudioTracks (*edit))
                edit->deleteTrack (t);
                
        };
        deleteButton.onClick = [this]
        {
            auto sel = selectionManager.getSelectedObject (0);
            if (auto clip = dynamic_cast<te::Clip*> (sel))
            {
                clip->removeFromParentTrack();
            }
            else if (auto track = dynamic_cast<te::Track*> (sel))
            {
                if (! (track->isMarkerTrack() || track->isTempoTrack() || track->isChordTrack()))
                    edit->deleteTrack (track);
            }
        };
        showWaveformButton.onClick = [this]
        {
            auto& evs = editComponent->getEditViewState();
            evs.drawWaveforms = ! evs.drawWaveforms.get();
            showWaveformButton.setToggleState (evs.drawWaveforms, dontSendNotification);
        };
    }
    
    void updatePlayButtonText()
    {
        if (edit != nullptr)
            playPauseButton.setButtonText (edit->getTransport().isPlaying() ? "Stop" : "Play");
    }
    
    void updateRecordButtonText()
    {
        if (edit != nullptr)
            recordButton.setButtonText (edit->getTransport().isRecording() ? "Abort" : "Record");
    }

    void createNewEdit (File editFile = {})
    {
        if (editFile == File())
        {
            FileChooser fc ("New Edit", File::getSpecialLocation (File::userDocumentsDirectory), "*.tracktionedit");
            if (fc.browseForFileToSave (true))
                editFile = fc.getResult();
            else
                return;
        }
        
        selectionManager.deselectAll();
        editComponent = nullptr;
        
        edit = std::make_unique<te::Edit> (engine, te::createEmptyEdit(), te::Edit::forEditing, nullptr, 0);
        
        edit->editFileRetriever = [editFile] { return editFile; };
        edit->playInStopEnabled = true;
        
        auto& transport = edit->getTransport();
        transport.addChangeListener (this);
        
        editNameLabel.setText (editFile.getFileNameWithoutExtension(), dontSendNotification);
        showEditButton.onClick = [editFile]
        {
            editFile.revealToUser();
        };
        
        createTracksAndAssignInputs();
        
        te::EditFileOperations (*edit).save (true, true, false);
        
        editComponent = std::make_unique<EditComponent> (*edit, selectionManager);
        addAndMakeVisible (*editComponent);
        resized();
    }
    
    void createTracksAndAssignInputs()
    {
        auto& dm = engine.getDeviceManager();

        for (int i = 0; i < dm.getNumWaveInDevices(); i++)
            if (auto wip = dm.getWaveInDevice (i))
                wip->setStereoPair (false);
        
        for (int i = 0; i < dm.getNumWaveInDevices(); i++)
        {
            if (auto wip = dm.getWaveInDevice (i))
            {
                wip->setEndToEnd (true);
                wip->setEnabled (true);
            }
        }
        
        edit->restartPlayback();
        
        int trackNum = 0;
        for (auto instance : edit->getAllInputDevices())
        {
            if (instance->getInputDevice().getDeviceType() == te::InputDevice::waveDevice)
            {
                if (auto t = EngineHelpers::getOrInsertAudioTrackAt (*edit, trackNum))
                {
                    instance->setTargetTrack (t, 0);
                    instance->setRecordingEnabled (true);
                    
                    trackNum++;
                }
            }
        }
        
        edit->restartPlayback();
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (edit != nullptr && source == &edit->getTransport())
        {
            updatePlayButtonText();
            updateRecordButtonText();
        }
        else if (source == &selectionManager)
        {
            auto sel = selectionManager.getSelectedObject (0);
            deleteButton.setEnabled (dynamic_cast<te::Clip*> (sel) != nullptr || dynamic_cast<te::Track*> (sel) != nullptr);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RecordingDemo)
};
