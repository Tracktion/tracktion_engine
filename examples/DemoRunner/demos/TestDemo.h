/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "../../common/Utilities.h"
#include "../../common/Components.h"
#include "../../common/PluginWindow.h"

#include "../../common/PlaybackDemoAudio.h"

using namespace tracktion_engine;

//==============================================================================
class TestDemo  : public Component,
                           private ChangeListener
{
public:
    //==============================================================================
    TestDemo (Engine& e)
        : engine (e)
    {
        newEditButton.onClick = [this] { createOrLoadEdit(); };

        updatePlayButtonText();
        updateRecordButtonText();

        editNameLabel.setJustificationType (Justification::centred);

        Helpers::addAndMakeVisible (*this, { &newEditButton, &playPauseButton, &showEditButton,
                                             &recordButton, &newTrackButton, &deleteButton, &editNameLabel });

        deleteButton.setEnabled (false);

        auto d = File::getSpecialLocation (File::tempDirectory).getChildFile ("TestDemo");
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

    ~TestDemo() override
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
        recordButton.setBounds (topR.removeFromLeft (w).reduced (2));
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
               showEditButton { "Show Edit" }, newTrackButton { "New Track" }, deleteButton { "Delete" }, recordButton { "Record" };
    Label editNameLabel { "No Edit Loaded" };
    ToggleButton showWaveformButton { "Show Waveforms" };
    std::unique_ptr<TemporaryFile> defaultTempFile;

    //==============================================================================
    void setupButtons()
    {
        playPauseButton.onClick = [this]
        {
            EngineHelpers::togglePlay (*edit);
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
        deleteButton.onClick = [this]
        {
            auto sel = selectionManager.getSelectedObject (0);
            if (auto clip = dynamic_cast<te::Clip*> (sel))
            {
                clip->removeFromParent();
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

    void updateRecordButtonText()
    {
        if (edit != nullptr)
            recordButton.setButtonText (edit->getTransport().isRecording() ? "Abort" : "Record");
    }

    void createOrLoadEdit (File editFile = {})
    {
        if (editFile == File())
        {
            editFile = File::getSpecialLocation(File::tempDirectory).getChildFile("DefaultEdit.tracktionedit");
        }

        selectionManager.deselectAll();
        editComponent = nullptr;

        if (editFile.existsAsFile())
            edit = te::loadEditFromFile (engine, editFile);
        else
            edit = te::createEmptyEdit (engine, editFile);

        edit->editFileRetriever = [editFile] { return editFile; };
        edit->playInStopEnabled = true;

        auto& transport = edit->getTransport();
        transport.addChangeListener (this);

        editNameLabel.setText (editFile.getFileNameWithoutExtension(), dontSendNotification);
        showEditButton.onClick = [this, editFile]
        {
            te::EditFileOperations (*edit).save (true, true, false);
            editFile.revealToUser();
        };

        createTracksAndAssignInputs();

        te::EditFileOperations (*edit).save (true, true, false);

        editComponent = std::make_unique<EditComponent> (*edit, selectionManager);
        editComponent->getEditViewState().showFooters = true;
        editComponent->getEditViewState().showMidiDevices = true;
        editComponent->getEditViewState().showWaveDevices = false;

        addAndMakeVisible (*editComponent);

        // Load built-in example audio file into the edit
        defaultTempFile = std::make_unique<TemporaryFile> (".ogg");
        auto audioFile = defaultTempFile->getFile();

        // Write the audio data into the temporary file
        audioFile.replaceWithData (PlaybackDemoAudio::guitar_loop_ogg, PlaybackDemoAudio::guitar_loop_oggSize);

        if (auto track = EngineHelpers::getOrInsertAudioTrackAt (*edit, 0))
        {
            if (auto clip = EngineHelpers::loadAudioFileAsClip (*edit, audioFile))
            {
                clip->setAutoTempo (false);
                clip->setAutoPitch (false);
                clip->setTimeStretchMode (te::TimeStretcher::defaultMode);
            }
        }

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
            deleteButton.setEnabled (dynamic_cast<te::Clip*> (sel) != nullptr
                                     || dynamic_cast<te::Track*> (sel) != nullptr
                                     || dynamic_cast<te::Plugin*> (sel));
        }
    }

    void createTracksAndAssignInputs()
    {
        for (auto& midiIn : engine.getDeviceManager().getMidiInDevices())
        {
            midiIn->setMonitorMode (te::InputDevice::MonitorMode::automatic);
            midiIn->setEnabled (true);
        }

        edit->getTransport().ensureContextAllocated();

        if (te::getAudioTracks (*edit).size() == 0)
        {
            int trackNum = 0;

            for (auto instance : edit->getAllInputDevices())
            {
                if (instance->getInputDevice().getDeviceType() == te::InputDevice::physicalMidiDevice)
                {
                    if (auto t = EngineHelpers::getOrInsertAudioTrackAt (*edit, trackNum))
                    {
                        [[ maybe_unused ]] auto res = instance->setTarget (t->itemID, true, &edit->getUndoManager(), 0);
                        instance->setRecordingEnabled (t->itemID, true);

                        trackNum++;
                    }
                }
            }
        }

        edit->restartPlayback();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TestDemo)
};

//==============================================================================
static DemoTypeBase<TestDemo> TestDemo ("Test Demo");
