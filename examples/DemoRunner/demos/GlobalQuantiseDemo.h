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
#include "../common/PlaybackDemoAudio.h"
#include "DistortionEffectDemo.h"
#include "BinaryData.h"

using namespace tracktion_engine;

//==============================================================================
class GlobalQuantiseComponent   : public Component,
                                  private ChangeListener
{
public:
    //==============================================================================
    GlobalQuantiseComponent (Engine& e)
        : engine (e)
    {
        setSize (600, 400);

        transport.addChangeListener (this);
        updatePlayButtonText();

        Helpers::addAndMakeVisible (*this,
                                    { &playPauseButton, &loadFileButton, &thumbnail,
                                      &fileTempoSlider, &tempoSlider, &quantisationSlider, &modeButton });

        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (edit); };
        loadFileButton.onClick  = [this] { EngineHelpers::browseForAudioFile (engine, [this] (const File& f) { setFile (f, {}); }); };

        fileTempoSlider.setRange (30.0, 220.0, 0.1);
        fileTempoSlider.onDragEnd = [this] { updateClipBPM(); };

        tempoSlider.setRange (30.0, 220.0, 0.1);
        tempoSlider.onValueChange = [this] { edit.tempoSequence.getTempo (0)->setBpm (tempoSlider.getValue()); };

        quantisationSlider.setRange (1.0, 8.0, 1.0);
        quantisationSlider.onDragEnd = [this] { thumbnail.setQuantisation (static_cast<int> (quantisationSlider.getValue())); };

        modeButton.onClick = [this] { showModeMenu(); };

        // One bar quantisation to start
        thumbnail.setQuantisation (1);

        // Load some example audio to start
        {
            // This demo song is at 140bpm
            defaultTempFile = std::make_unique<TemporaryFile> (".ogg");
            auto f = defaultTempFile->getFile();
            f.replaceWithData (BinaryData::edm_song_ogg, BinaryData::edm_song_oggSize);
            setFile (f, 140.0);
        }
    }

    ~GlobalQuantiseComponent() override
    {
        edit.getTempDirectory (false).deleteRecursively();
    }

    //==============================================================================
    void paint (Graphics& g) override
    {
        const auto c = getLookAndFeel().findColour (ResizableWindow::backgroundColourId);
        g.fillAll (c);

        g.setColour (findColour (juce::Label::textColourId));

        {
            const auto sliderR = fileTempoSlider.getBounds();
            g.drawText ("File Tempo: ", sliderR.withWidth (100).withRightX (sliderR.getX()), juce::Justification::centredRight);
        }

        {
            const auto sliderR = tempoSlider.getBounds();
            g.drawText ("Edit Tempo: ", sliderR.withWidth (100).withRightX (sliderR.getX()), juce::Justification::centredRight);
        }

        {
            const auto sliderR = quantisationSlider.getBounds();
            g.drawText ("Quantisation (Bars): ", sliderR.withWidth (100).withRightX (sliderR.getX()), juce::Justification::centredRight);
        }
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
            auto bottomR = r.removeFromBottom (60);

            modeButton.setBounds (bottomR.removeFromRight (80).reduced (2));

            auto left = bottomR.removeFromLeft (bottomR.getWidth() / 2);
            fileTempoSlider.setBounds (left.removeFromTop (30).withTrimmedLeft (100).reduced (2));
            quantisationSlider.setBounds (left.withTrimmedLeft (100).reduced (2));

            tempoSlider.setBounds (bottomR.removeFromTop (30).withTrimmedLeft (100).reduced (2));
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
    Slider fileTempoSlider, tempoSlider, quantisationSlider;
    TextButton modeButton { "Algorithm" };

    //==============================================================================
    te::WaveAudioClip::Ptr getClip()
    {
        if (auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
            if (auto clip = dynamic_cast<te::WaveAudioClip*> (track->getClips()[0]))
                return *clip;

        return {};
    }

    void updateClipBPM()
    {
        if (auto c = getClip())
        {
            auto li = c->getLoopInfo();
            li.setBpm (jlimit (30.0, 220.0, fileTempoSlider.getValue()), te::AudioFileInfo::parse (c->getAudioFile()));
            c->setLoopInfo (li);

            edit.restartPlayback();
        }
    }

    File getSourceFile()
    {
        if (auto clip = getClip())
            return clip->getSourceFileReference().getFile();

        return {};
    }

    void setFile (const File& f, std::optional<double> fileTempo)
    {
        tempoSlider.setValue (120.0, dontSendNotification);

        if (auto clip = EngineHelpers::loadAudioFileAsClip (edit, f))
        {
            clip->setUsesProxy (false);
            clip->setAutoTempo (true);
            clip->setTimeStretchMode (te::TimeStretcher::defaultMode);
            thumbnail.setFile (EngineHelpers::loopAroundClip (*clip)->getAudioFile());

            const auto audioFileInfo = te::AudioFile (engine, f).getInfo();
            const auto loopInfo = audioFileInfo.loopInfo;
            const auto tempo = fileTempo ? *fileTempo
                                         : loopInfo.getBpm (audioFileInfo);

            fileTempoSlider.setValue (tempo, dontSendNotification);

            if (tempo > 0)
                tempoSlider.setValue (tempo, dontSendNotification);
        }
        else
        {
            thumbnail.setFile ({engine});
            fileTempoSlider.setValue (120.0, dontSendNotification);
        }
    }

    void showModeMenu()
    {
        PopupMenu m;

        if (auto clip = getClip())
        {
            const auto currentMode = clip->getActualTimeStretchMode();

            for (auto modeName : tracktion_engine::TimeStretcher::getPossibleModes (engine, true))
            {
                const auto mode = tracktion_engine::TimeStretcher::getModeFromName (engine, modeName);
                m.addItem (PopupMenu::Item (modeName).setTicked (currentMode == mode).setAction ([clip, mode] { clip->setTimeStretchMode (mode); }));
            }
        }
        else
        {
            m.addItem (TRANS("Load a file first to set the time-stretch algorithm"), false, false, {});
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalQuantiseComponent)
};

//==============================================================================
static DemoTypeBase<GlobalQuantiseComponent> globalQuantiseComponent ("Global Quantise");
