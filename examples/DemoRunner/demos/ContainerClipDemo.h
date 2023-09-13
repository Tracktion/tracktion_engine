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
#include <BinaryData.h>

using namespace tracktion_engine;

//==============================================================================
class ContainerClipDemo : public Component,
                          private ChangeListener
{
public:
    //==============================================================================
    ContainerClipDemo (Engine& e)
        : engine (e)
    {
        transport.addChangeListener (this);
        updatePlayButtonText();

        playPauseButton.onClick = [this] { EngineHelpers::togglePlay (edit); };

        tempoSlider.setRange (30.0, 220.0, 0.1);
        tempoSlider.setValue (edit.tempoSequence.getTempo (0)->getBpm());
        tempoSlider.onValueChange = [this] { edit.tempoSequence.getTempo (0)->setBpm (tempoSlider.getValue()); };

        // Load our files to temp files
        {
            drumTempFile = std::make_unique<TemporaryFile> (".wav");
            drumTempFile->getFile().replaceWithData (BinaryData::drum_loop_wav, BinaryData::drum_loop_wavSize);

            synthTempFile = std::make_unique<TemporaryFile> (".wav");
            synthTempFile->getFile().replaceWithData (BinaryData::synth_loop_wav, BinaryData::synth_loop_wavSize);
        }

        // Load some example audio to start
        if (auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
        {
            auto& ts = edit.tempoSequence;
            insertNewClip (*track, TrackItem::Type::container, ts.toTime ({ 0_bp, 32_bp }));
            auto cc = getContainerClip();
            cc->setAutoTempo (true);

            auto drumClip   = insertWaveClip (*cc, {}, drumTempFile->getFile(), createClipPosition (ts, { 0_bp, 8_bp }), DeleteExistingClips::no);
            auto synthClip  = insertWaveClip (*cc, {}, synthTempFile->getFile(), createClipPosition (ts, { 0_bp, 16_bp }), DeleteExistingClips::no);

            drumClip->setUsesProxy (false);
            drumClip->setNumberOfLoops (4);

            synthClip->setUsesProxy (false);
            synthClip->setNumberOfLoops (2);

            containedClipThumbs.push_back (std::make_unique<SmartThumbnail> (engine, AudioFile (engine, drumTempFile->getFile()), *this, nullptr));
            containedClipThumbs.push_back (std::make_unique<SmartThumbnail> (engine, AudioFile (engine, synthTempFile->getFile()), *this, nullptr));

            cc->setLoopRange (cc->getPosition().time);
            EngineHelpers::loopAroundClip (*cc);

            loopInComp = std::make_unique<LoopComponent> (*cc, true);
            loopOutComp = std::make_unique<LoopComponent> (*cc, false);
        }

        Helpers::addAndMakeVisible (*this,
                                    { &playPauseButton, &loadFileButton, &thumbnail, &tempoSlider,
                                      loopInComp.get(), loopOutComp.get() });

        thumbnail.start();

        setSize (600, 400);
    }

    ~ContainerClipDemo() override
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
            const auto sliderR = tempoSlider.getBounds();
            g.drawText ("Edit Tempo: ", sliderR.withWidth (100).withRightX (sliderR.getX()), juce::Justification::centredRight);
        }

        g.drawText ("Container clip contents and loop range:", containerClipArea.withHeight (30).withBottomY (containerClipArea.getY()), juce::Justification::centredLeft);
        g.drawText ("Edit timeline and container clip loop repititions:", thumbnail.getBounds().withHeight (30).withBottomY (thumbnail.getY()), juce::Justification::centredLeft);

        paintContainerClipQuickAndDirty (g, thumbnail.getBounds());
        paintContainedClipsQuickAndDirty (g, containerClipArea, getContainerClip()->getPosition().time);
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
            auto left = r.removeFromBottom (30).removeFromLeft (r.getWidth() / 2);
            tempoSlider.setBounds (left.withTrimmedLeft (100).reduced (2));
        }

        r = r.reduced (2);

        thumbnail.setBounds (r.removeFromBottom (100).withTrimmedTop (34));

        containerClipArea = r.withTrimmedTop (30);
        loopInComp->setBounds (containerClipArea);
        loopOutComp->setBounds (containerClipArea);
    }

private:
    //==============================================================================
    class LoopComponent : public juce::Component
    {
    public:
        LoopComponent (Clip& c, bool isLoopIn)
            : clip (c), loopIn (isLoopIn)
        {
            timer.setCallback ([this] { repaint(); });
            timer.startTimerHz (25);
            setRepaintsOnMouseActivity (true);
        }

        bool hitTest (int x, int) override
        {
            const auto pos = getPositionPixel();
            return juce::Range<float> (pos - 2.0f, pos + 2.0f).contains ((float) x);
        }

        void mouseDown (const juce::MouseEvent& e) override     { mouseChanged (e); }
        void mouseDrag (const juce::MouseEvent& e) override     { mouseChanged (e); }
        void mouseUp (const juce::MouseEvent& e) override       { mouseChanged (e); }

        void paint (juce::Graphics& g) override
        {
            const auto loopPixel = getPositionPixel();
            const auto col = (loopIn ? juce::Colours::green : juce::Colours::red)
                                .brighter (isMouseOverOrDragging() ?  0.4f : 0.0f);
            const auto r = getLocalBounds().withWidth (4).toFloat();

            g.setColour (col);
            g.fillRect (r.withX (loopPixel - 2.0f));
        }

    private:
        Clip& clip;
        const bool loopIn;
        te::LambdaTimer timer;

        float getPositionPixel() const
        {
            const auto loopRange = clip.getLoopRange();
            const auto loopTime = loopIn ? loopRange.getStart() : loopRange.getEnd();
            return static_cast<float> (loopTime.inSeconds() / getSecondsPerPixel());
        }

        double getSecondsPerPixel() const
        {
            if (getWidth() == 0)
                return 1.0;

            const auto clipPos = clip.getPosition();
            return (clipPos.getLength() / getWidth()).inSeconds();
        }

        void mouseChanged (const juce::MouseEvent& e)
        {
            if (getWidth() == 0)
                return;

            const auto mouseTime = te::TimePosition::fromSeconds (e.position.x * getSecondsPerPixel());
            const auto loopRange = clip.getLoopRange();
            auto newLoopRange = loopRange;

            if (loopIn)
            {
                const auto minLoopStart = 0_tp;
                const auto maxLoopStart = loopRange.getEnd() - 0.5_td;

                newLoopRange = loopRange.withStart (std::clamp (mouseTime, minLoopStart, maxLoopStart));
            }
            else
            {
                const auto minLoopEnd = loopRange.getStart() + 0.5_td;
                const auto maxLoopEnd = toPosition (clip.getPosition().getLength());
                newLoopRange = loopRange.withEnd (std::clamp (mouseTime, minLoopEnd, maxLoopEnd));
            }

            if (newLoopRange.getLength() < 500ms)
                newLoopRange = newLoopRange.withLength (500ms);

            clip.setLoopRange (newLoopRange);
        }
    };

    //==============================================================================
    te::Engine& engine;
    te::Edit edit { engine, te::createEmptyEdit (engine), te::Edit::forEditing, nullptr, 0 };
    te::TransportControl& transport { edit.getTransport() };

    std::unique_ptr<TemporaryFile> drumTempFile, synthTempFile;

    TextButton playPauseButton { "Play" }, loadFileButton { "Load file" };
    Thumbnail thumbnail { transport };
    Slider tempoSlider;

    std::vector<std::unique_ptr<SmartThumbnail>> containedClipThumbs;
    std::unique_ptr<LoopComponent> loopInComp, loopOutComp;
    juce::Rectangle<int> containerClipArea;

    //==============================================================================
    te::ContainerClip::Ptr getContainerClip()
    {
        if (auto track = EngineHelpers::getOrInsertAudioTrackAt (edit, 0))
            if (auto clip = dynamic_cast<te::ContainerClip*> (track->getClips()[0]))
                return *clip;

        return {};
    }

    te::WaveAudioClip::Ptr getChildClip (int index)
    {
        if (auto cc = getContainerClip())
            return dynamic_cast<WaveAudioClip*> (cc->getClips()[index]);

        return {};
    }

    File getSourceFile (int index)
    {
        if (auto clip = getChildClip (index))
            return clip->getSourceFileReference().getFile();

        return {};
    }

    void updatePlayButtonText()
    {
        playPauseButton.setButtonText (transport.isPlaying() ? "Pause" : "Play");
    }

    void changeListenerCallback (ChangeBroadcaster*) override
    {
        updatePlayButtonText();
    }

    void paintContainerClipQuickAndDirty (juce::Graphics& g, juce::Rectangle<int> r)
    {
        const Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (r);

        const auto baseColour = findColour (juce::Label::textColourId);

        const auto clipTime = getContainerClip()->getPosition().time;
        const auto loopRange = getContainerClip()->getLoopRange();
        const int numLoops = (int) std::ceil (clipTime.getLength() / loopRange.getLength());
        const auto pixelsPerSecond = r.getWidth() / clipTime.getLength().inSeconds();

        for (int i = 0; i < numLoops; ++i)
        {
            const auto start = clipTime.getStart() + (loopRange.getLength() * i);
            const auto end = start + loopRange.getLength();
            auto clipR = r.withX (roundToInt (start.inSeconds() * pixelsPerSecond))
                          .withRight (roundToInt (end.inSeconds() * pixelsPerSecond))
                          .translated (r.getX(), 0);

            g.setColour (baseColour.darker());
            g.fillRect (clipR.reduced (1));
        }
    }

    void paintContainedClipsQuickAndDirty (juce::Graphics& g, juce::Rectangle<int> r, te::TimeRange displayRange)
    {
        const auto baseColour = findColour (juce::Label::textColourId);

        g.setColour (baseColour);
        g.drawRect (r, 2);
        g.fillRect (r.removeFromTop (10));

        const int numClips = getContainerClip()->getClips().size();
        const int clipH = r.getHeight() / numClips;
        const auto pixelsPerSecond = r.getWidth() / displayRange.getLength().inSeconds();

        auto drawClip = [&] (int index)
        {
            if (auto clip = getChildClip (index))
            {
                const auto& thumb = containedClipThumbs[(size_t) index];
                const auto clipArea = r.removeFromTop (clipH);
                const auto clipLoopRange = clip->getLoopRange();
                const auto clipLoopLength = clipLoopRange.getLength();
                const auto clipFileRange = te::TimeRange (0_tp, te::TimePosition::fromSeconds (thumb->getTotalLength()));

                for (auto start = displayRange.getStart(); start < displayRange.getEnd(); start = start + clipLoopLength)
                {
                    const auto drawArea = clipArea.withX (roundToInt (start.inSeconds() * pixelsPerSecond))
                                                  .withRight (roundToInt ((start + clipLoopLength).inSeconds() * pixelsPerSecond))
                                                  .translated (clipArea.getX(), 0);
                    g.setColour (baseColour.darker());
                    thumb->drawChannels (g, drawArea, clipFileRange, 1.0f);

                    g.setColour (baseColour.contrasting (0.2f));
                    g.drawRect (drawArea);
                }
            }
        };

        drawClip (0);
        drawClip (1);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ContainerClipDemo)
};

//==============================================================================
static DemoTypeBase<ContainerClipDemo> containerClipDemo ("Container Clip");
