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
#include "DistortionEffectDemo.h"
#include "../common/PlaybackDemoAudio.h"

using namespace tracktion_engine;

//==============================================================================
class PitchAndTimeComponent   : public Component,
                                private ChangeListener
{
public:
    //==============================================================================
    PitchAndTimeComponent (Engine& e)
        : engine (e)
    {
        setSize (600, 400);

        transport.addChangeListener (this);
        updatePlayButtonText();

        Helpers::addAndMakeVisible (*this,
                                    { &playPauseButton, &loadFileButton, &thumbnail,
                                      &rootNoteEditor, &rootTempoEditor, &keySlider, &tempoSlider,
                                      &pitchShiftLabel, &pitchShiftSlider, &modeButton });

        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (edit); };
        loadFileButton.onClick  = [this] { EngineHelpers::browseForAudioFile (engine, [this] (const File& f) { setFile (f); }); };

        rootNoteEditor.onFocusLost = [this] { updateTempoAndKey(); };
        rootTempoEditor.onFocusLost = [this] { updateTempoAndKey(); };

        keySlider.setRange (-12.0, 12.0, 1.0);
        keySlider.onDragEnd = [this] { updateTempoAndKey(); };
        tempoSlider.setRange (30.0, 220.0, 0.1);
        tempoSlider.onDragEnd = [this] { updateTempoAndKey(); };

        keySlider.textFromValueFunction = [this] (double v)
                                          {
                                              const int numSemitones = roundToInt (v);
                                              const auto semitonesText = (numSemitones > 0 ? "+" : "") + String (numSemitones);

                                              if (auto clip = getClip())
                                              {
                                                  const auto audioFileInfo = te::AudioFile (engine, clip->getSourceFileReference().getFile()).getInfo();
                                                  const auto rootNote = audioFileInfo.loopInfo.getRootNote();

                                                  if (rootNote <= 0)
                                                      return semitonesText;

                                                  return semitonesText + " (" + MidiMessage::getMidiNoteName (rootNote + numSemitones, true, false, 3) + ")";
                                              }

                                              return semitonesText;
                                          };

        // Setup real-time pitch shifting
        {
            // Register the PitchShiftPlugin with the engine so it can be found using PluginCache::createNewPlugin
            engine.getPluginManager().createBuiltInType<PitchShiftPlugin>();

            // Creates new instance of PitchShiftPlugin Plugin and inserts to track 1
            auto pitchShiftPlugin = edit.getPluginCache().createNewPlugin (PitchShiftPlugin::xmlTypeName, {});
            auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0);
            track->pluginList.insertPlugin (pitchShiftPlugin, 0, nullptr);

            // Setup slider value source
            auto pitchShiftParam = pitchShiftPlugin->getAutomatableParameterByID ("semitones up");
            bindSliderToParameter (pitchShiftSlider, *pitchShiftParam);
            pitchShiftSlider.setSkewFactorFromMidPoint (0.0);

            modeButton.onClick = [this, p = dynamic_cast<te::PitchShiftPlugin*> (pitchShiftPlugin.get())]
                                 { showModeMenu (*p); };
        }
        
        // Load some example audio to start
        {
            defaultTempFile = std::make_unique<TemporaryFile> (".ogg");
            auto f = defaultTempFile->getFile();
            f.replaceWithData (PlaybackDemoAudio::guitar_loop_ogg, PlaybackDemoAudio::guitar_loop_oggSize);
            setFile (f);
        }
    }

    ~PitchAndTimeComponent() override
    {
        edit.getTempDirectory (false).deleteRecursively();
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        const auto c = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
        g.fillAll (c);
        
        g.setColour (c.contrasting (0.2f));
        g.fillRect (getLocalBounds().reduced (2).withHeight (2).withY (pitchShiftLabel.getY() - 6));
    }

    void resized() override
    {
        auto r = getLocalBounds();

        {
            auto topR = r.removeFromTop (30);
            playPauseButton.setBounds (topR.removeFromLeft (topR.getWidth() / 2).reduced (2));
            loadFileButton.setBounds (topR.reduced (2));
        }

        {
            auto bottomR = r.removeFromBottom (70).withTrimmedTop (10);
            pitchShiftLabel.setBounds (bottomR.removeFromTop (30).reduced (2));
            modeButton.setBounds (bottomR.removeFromRight (80).reduced (2));
            pitchShiftSlider.setBounds (bottomR.reduced (2));
        }
        
        {
            auto bottomR = r.removeFromBottom (60);
            auto left = bottomR.removeFromLeft (bottomR.getWidth() / 2);
            rootNoteEditor.setBounds (left.removeFromTop (left.getHeight() / 2).reduced (2));
            rootTempoEditor.setBounds (left.reduced (2));

            keySlider.setBounds (bottomR.removeFromTop (bottomR.getHeight() / 2).reduced (2));
            tempoSlider.setBounds (bottomR.reduced (2));
        }

        thumbnail.setBounds (r.reduced (2));
    }

private:
    //==============================================================================
    te::Engine& engine;
    te::Edit edit { engine, te::createEmptyEdit (engine), te::Edit::forEditing, nullptr, 0 };
    te::TransportControl& transport { edit.getTransport() };

    FileChooser audioFileChooser { "Please select an audio file to load...",
                                   engine.getPropertyStorage().getDefaultLoadSaveDirectory ("pitchAndTimeExample"),
                                   engine.getAudioFileFormatManager().readFormatManager.getWildcardForAllFormats() };
    std::unique_ptr<TemporaryFile> defaultTempFile;

    TextButton playPauseButton { "Play" }, loadFileButton { "Load file" };
    Thumbnail thumbnail { transport };
    TextEditor rootNoteEditor, rootTempoEditor;
    Slider keySlider, tempoSlider, pitchShiftSlider;
    TextButton modeButton { "Algorithm" };
    Label pitchShiftLabel { {}, "Real-time Pitch Shift:" };
    
    //==============================================================================
    te::WaveAudioClip::Ptr getClip()
    {
        if (auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
            if (auto clip = dynamic_cast<te::WaveAudioClip*> (track->getClips()[0]))
                return *clip;

        return {};
    }

    File getSourceFile()
    {
        if (auto clip = getClip())
            return clip->getSourceFileReference().getFile();

        return {};
    }

    void setFile (const File& f)
    {
        keySlider.setValue (0.0, dontSendNotification);
        tempoSlider.setValue (120.0, dontSendNotification);

        if (auto clip = EngineHelpers::loadAudioFileAsClip (edit, f))
        {
            // Disable auto tempo and pitch, we'll handle these manually
            clip->setAutoTempo (false);
            clip->setAutoPitch (false);
            clip->setTimeStretchMode (te::TimeStretcher::defaultMode);

            thumbnail.setFile (EngineHelpers::loopAroundClip (*clip)->getPlaybackFile());

            const auto audioFileInfo = te::AudioFile (engine, f).getInfo();
            const auto loopInfo = audioFileInfo.loopInfo;
            const auto tempo = loopInfo.getBpm (audioFileInfo);
            rootNoteEditor.setText (TRANS("Root Key: ") + Helpers::getStringOrDefault (MidiMessage::getMidiNoteName (loopInfo.getRootNote(), true, false, 3), "Unknown"), false);
            rootTempoEditor.setText (TRANS("Root Tempo: ") + Helpers::getStringOrDefault (String (tempo, 2), "Unknown"), false);

            keySlider.setValue (0.0, dontSendNotification);
            keySlider.updateText();

            if (tempo > 0)
                tempoSlider.setValue (tempo, dontSendNotification);
        }
        else
        {
            thumbnail.setFile ({engine});
            rootNoteEditor.setText (TRANS("Root Key: Unknown"));
            rootTempoEditor.setText (TRANS("Root Tempo:  Unknown"));
        }
    }

    void updateTempoAndKey()
    {
        if (auto clip = getClip())
        {
            auto f = getSourceFile();
            const auto audioFileInfo = te::AudioFile (engine, f).getInfo();
            const double baseTempo = rootTempoEditor.getText().retainCharacters ("+-.0123456789").getDoubleValue();

            // First update the tempo based on the ratio between the root tempo and tempo slider value
            if (baseTempo > 0.0)
            {
                const double ratio = tempoSlider.getValue() / baseTempo;
                clip->setSpeedRatio (ratio);
                clip->setLength (tracktion::TimeDuration::fromSeconds (audioFileInfo.getLengthInSeconds()) / clip->getSpeedRatio(), true);
            }

            // Then update the pitch change based on the slider value
            clip->setPitchChange (float (keySlider.getValue()));

            // Update the thumbnail to show the proxy render progress
            thumbnail.setFile (EngineHelpers::loopAroundClip (*clip)->getPlaybackFile());
        }
    }
    
    void showModeMenu (te::PitchShiftPlugin& plugin)
    {
        PopupMenu m;
        const int currentMode = plugin.mode;
        
        for (auto modeName : tracktion_engine::TimeStretcher::getPossibleModes (plugin.engine, true))
        {
            const auto mode = static_cast<int> (tracktion_engine::TimeStretcher::getModeFromName (plugin.engine, modeName));
            m.addItem (PopupMenu::Item (modeName).setTicked (currentMode == mode).setAction ([&plugin, mode] { plugin.mode = mode; }));
        }
        
        m.showMenuAsync ({});
    }

    void updatePlayButtonText()
    {
        playPauseButton.setButtonText (transport.isPlaying() ? "Pause" : "Play");
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchAndTimeComponent)
};

//==============================================================================
static DemoTypeBase<PitchAndTimeComponent> pitchAndTimeComponent ("Pitch and Time");
