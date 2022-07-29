/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "common/Utilities.h"
#include "common/Components.h"
#include "common/PluginWindow.h"

//==============================================================================
class PluginDemo  : public Component,
                    private ChangeListener
{
public:
    //==============================================================================
    PluginDemo (te::Engine& e)
        : engine (e)
    {
        newEditButton.onClick = [this] { createOrLoadEdit(); };
        
        updatePlayButtonText();
        editNameLabel.setJustificationType (Justification::centred);
        Helpers::addAndMakeVisible (*this, { &newEditButton, &playPauseButton, &showEditButton,
                                             &newTrackButton, &deleteButton, &editNameLabel });

        deleteButton.setEnabled (false);
        
        auto d = File::getSpecialLocation (File::tempDirectory).getChildFile ("PluginDemo");
        d.createDirectory();

        auto f = Helpers::findRecentEdit (d);
        if (f.existsAsFile())
            createOrLoadEdit (f);
        else
            createOrLoadEdit (d.getNonexistentChildFile ("Test", ".tracktionedit", false));
        
        selectionManager.addChangeListener (this);
        
        setupButtons();
        
        setSize (600, 400);
    }

    ~PluginDemo() override
    {
        te::EditFileOperations (*edit).save (true, true, false);
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
        int w = r.getWidth() / 6;
        auto topR = r.removeFromTop (30);
        newEditButton.setBounds (topR.removeFromLeft (w).reduced (2));
        playPauseButton.setBounds (topR.removeFromLeft (w).reduced (2));
        showEditButton.setBounds (topR.removeFromLeft (w).reduced (2));
        newTrackButton.setBounds (topR.removeFromLeft (w).reduced (2));
        deleteButton.setBounds (topR.removeFromLeft (w).reduced (2));
        topR = r.removeFromTop (30);
        editNameLabel.setBounds (topR);
        
        if (editComponent != nullptr)
            editComponent->setBounds (r);
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::SelectionManager selectionManager { engine };
    std::unique_ptr<te::Edit> edit;
    std::unique_ptr<EditComponent> editComponent;

    TextButton newEditButton { "New" }, playPauseButton { "Play" },
               showEditButton { "Show Edit" }, newTrackButton { "New Track" }, deleteButton { "Delete" };
    Label editNameLabel { "No Edit Loaded" };
    ToggleButton showWaveformButton { "Show Waveforms" };

    //==============================================================================
    void setupButtons()
    {
        playPauseButton.onClick = [this]
        {
            EngineHelpers::togglePlay (*edit);
        };
        newTrackButton.onClick = [this]
        {
            edit->ensureNumberOfAudioTracks (getAudioTracks (*edit).size() + 1);
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
            else if (auto plugin = dynamic_cast<te::Plugin*> (sel))
            {
                plugin->deleteFromParent();
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
    
    void createOrLoadEdit (File editFile = {})
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

        if (editFile.existsAsFile())
            edit = te::loadEditFromFile (engine, editFile);
        else
            edit = te::createEmptyEdit (engine, editFile);

        edit->playInStopEnabled = true;
        
        auto& transport = edit->getTransport();
        transport.addChangeListener (this);
        
        editNameLabel.setText (editFile.getFileNameWithoutExtension(), dontSendNotification);
        showEditButton.onClick = [this, editFile]
        {
            te::EditFileOperations (*edit).save (true, true, false);
            editFile.revealToUser();
        };
        
        te::EditFileOperations (*edit).save (true, true, false);
        
        enableAllInputs();
        
        editComponent = std::make_unique<EditComponent> (*edit, selectionManager);
        editComponent->getEditViewState().showFooters = true;
        editComponent->getEditViewState().showMidiDevices = true;
        editComponent->getEditViewState().showWaveDevices = true;
        
        addAndMakeVisible (*editComponent);
    }
    
    void enableAllInputs()
    {
        auto& dm = engine.getDeviceManager();
        
        for (int i = 0; i < dm.getNumMidiInDevices(); i++)
        {
            if (auto mip = dm.getMidiInDevice (i))
            {
                mip->setEndToEndEnabled (true);
                mip->setEnabled (true);
            }
        }
        
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
        
        edit->getTransport().ensureContextAllocated();
    }
    
    void changeListenerCallback (ChangeBroadcaster* source) override
    {
        if (edit != nullptr && source == &edit->getTransport())
        {
            updatePlayButtonText();
        }
        else if (source == &selectionManager)
        {
            auto sel = selectionManager.getSelectedObject (0);
            deleteButton.setEnabled (dynamic_cast<te::Clip*> (sel) != nullptr
                                     || dynamic_cast<te::Track*> (sel) != nullptr
                                     || dynamic_cast<te::Plugin*> (sel));
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginDemo)
};

//==============================================================================
static DemoTypeBase<PluginDemo> pluginDemo ("Plugin Hosting");
