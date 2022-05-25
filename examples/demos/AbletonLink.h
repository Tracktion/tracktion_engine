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


//==============================================================================
struct AbletonLink : public juce::Component
{
    AbletonLink (te::Engine& e)
        : engine (e)
    {}

    void paint (juce::Graphics& g) override
    {
        g.fillAll (Colours::green);
    }

    te::Engine& engine;
};

//==============================================================================
static DemoTypeBase<AbletonLink> abletonLinkDemo ("AbletonLink");
