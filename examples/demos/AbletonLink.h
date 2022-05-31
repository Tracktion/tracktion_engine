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

using namespace tracktion::literals;
using namespace std::literals;

#if TRACKTION_ENABLE_ABLETON_LINK

//==============================================================================
struct AbletonLink : public juce::Component,
                     private te::AbletonLink::Listener,
                     private juce::ChangeListener,
                     private juce::Timer
{
    AbletonLink (te::Engine& e)
        : engine (e)
    {
        transport.addChangeListener (this);
        abletonLink.addListener (this);

        Helpers::addAndMakeVisible (*this, { &playPauseButton, &metronomeButton, &tracktionPhaseSlider,
                                             &linkButton, &linkSyncButton, &linkPhaseSlider, &chaseSlider, &offsetMsSlider });

        playPauseButton.setClickingTogglesState (false);
        playPauseButton.onClick = [this]
        {
            if (linkSyncButton.getToggleState())
                abletonLink.requestStartStopChange (! playPauseButton.getToggleState());
            else
                EngineHelpers::togglePlay (edit);
        };

        metronomeButton.getToggleStateValue().referTo (edit.clickTrackEnabled.getPropertyAsValue());
        metronomeButton.triggerClick();

        linkButton.onClick = [this] { abletonLink.setEnabled (linkButton.getToggleState()); };
        linkButton.triggerClick();

        linkSyncButton.setClickingTogglesState (false);
        linkSyncButton.onClick = [this]
        {
            abletonLink.enableStartStopSync (! abletonLink.isStartStopSyncEnabled());
            linkSyncButton.setToggleState (abletonLink.isStartStopSyncEnabled(), dontSendNotification);
        };
        linkSyncButton.triggerClick();

        tracktionPhaseSlider.setRange (0.0, 1.0);
        tracktionPhaseSlider.setInterceptsMouseClicks (false, false);

        linkPhaseSlider.setRange (0.0, 1.0);
        linkPhaseSlider.setInterceptsMouseClicks (false, false);

        chaseSlider.setRange (-0.1, 0.1);
        chaseSlider.setInterceptsMouseClicks (false, false);

        offsetMsSlider.setTextValueSuffix ("ms offset");
        offsetMsSlider.setRange (-100.0, 100.0, 1.0);
        offsetMsSlider.onValueChange = [this] { abletonLink.setCustomOffset (static_cast<int> (offsetMsSlider.getValue())); };

        lookAndFeelChanged();
        startTimerHz (25);
    }

    void paint (juce::Graphics& g) override
    {
        const auto chaseR = chaseSlider.getBounds().toFloat();
        g.setColour (juce::Colours::white);
        g.fillRect (chaseR.withWidth (1.0f).withX (chaseR.getCentreX() - 0.5f));

        g.drawSingleLineText ("Tracktion:", 2, tracktionPhaseSlider.getBottom() - 6);
        g.drawSingleLineText ("Link:", 2, linkPhaseSlider.getBottom() - 6);
        g.drawSingleLineText ("Chase:", 2, chaseSlider.getBottom() - 6);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (2);

        {
            r.removeFromBottom (4);
            chaseSlider.setBounds (r.removeFromBottom (20).withTrimmedLeft (100));
            linkPhaseSlider.setBounds (r.removeFromBottom (20).withTrimmedLeft (100));
            tracktionPhaseSlider.setBounds (r.removeFromBottom (20).withTrimmedLeft (100));

            {
                auto left = r.removeFromLeft (r.getWidth() / 2).withTrimmedRight (50);
                playPauseButton.setBounds (left.removeFromTop (30).reduced (2));
                metronomeButton.setBounds (left.removeFromTop (30));
            }

            {
                auto right = r.withTrimmedLeft (50);
                right.removeFromTop (30);
                linkButton.setBounds (right.removeFromTop (30));
                linkSyncButton.setBounds (right.removeFromTop (30));
                offsetMsSlider.setBounds (right.removeFromTop (30));
            }
        }
    }

    void lookAndFeelChanged() override
    {
        tracktionPhaseSlider.setColour (juce::Slider::trackColourId, juce::Colours::lightblue);
        linkPhaseSlider.setColour (juce::Slider::trackColourId, juce::Colours::darkorange);
        offsetMsSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentWhite);
    }

private:
    te::Engine& engine;
    te::Edit edit { engine, te::createEmptyEdit (engine), te::Edit::forEditing, nullptr, 0 };
    te::TransportControl& transport { edit.getTransport() };
    te::AbletonLink& abletonLink { edit.getAbletonLink() };

    juce::TextButton playPauseButton { "Play" };
    juce::ToggleButton metronomeButton { "Metronome" }, linkButton { "Link" }, linkSyncButton { "Start Stop Sync" };
    juce::Slider tracktionPhaseSlider { juce::Slider::LinearBar, juce::Slider::NoTextBox };
    juce::Slider linkPhaseSlider { juce::Slider::LinearBar, juce::Slider::NoTextBox };
    juce::Slider chaseSlider { juce::Slider::LinearHorizontal, juce::Slider::NoTextBox };
    juce::Slider offsetMsSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    void linkRequestedStartStopChange (bool isPlaying) override
    {
        if (isPlaying)
            transport.play (false);
        else
            transport.stop (false, false);
    }

    void linkRequestedTempoChange (double newBpm) override
    {
        edit.tempoSequence.getTempoAt (transport.getPosition()).setBpm (newBpm);
    }

    void linkRequestedPositionChange (te::BeatDuration adjustment) override
    {
        if (auto epc = transport.getCurrentPlaybackContext())
        {
            auto& ts = edit.tempoSequence;
            const auto timePos = epc->getPosition();
            const auto beatPos = ts.timeToBeats (timePos);
            const auto newTimePos = ts.beatsToTime (beatPos + adjustment);
            epc->postPosition (newTimePos);
        }
        else
        {
            auto& ts = edit.tempoSequence;
            const auto timePos = transport.getPosition();
            const auto beatPos = ts.timeToBeats (timePos);
            const auto newTimePos = ts.beatsToTime (beatPos + adjustment);
            transport.setPosition (newTimePos);
        }
    }

    void linkConnectionChanged() override
    {
        linkButton.setButtonText (juce::String ("Link ({})").replace ("{}", juce::String (abletonLink.getNumPeers())));
    }

    void changeListenerCallback (juce::ChangeBroadcaster*) override
    {
        playPauseButton.setToggleState (transport.isPlaying(), juce::dontSendNotification);
        playPauseButton.setButtonText (transport.isPlaying() ? "Pause" : "Play");
    }

    void timerCallback() override
    {
        const auto editBarsBeats = edit.tempoSequence.timeToBarsBeats (transport.getPosition());
        const auto barPhase = editBarsBeats.beats.inBeats() / editBarsBeats.numerator;

        tracktionPhaseSlider.setValue (barPhase, juce::dontSendNotification);
        linkPhaseSlider.setValue (abletonLink.getBarPhase(), juce::dontSendNotification);
        chaseSlider.setValue (abletonLink.getChaseProportion(), juce::dontSendNotification);
    }
};

//==============================================================================
static DemoTypeBase<AbletonLink> abletonLinkDemo ("Ableton Link");

#endif
