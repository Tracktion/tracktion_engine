/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once


namespace tracktion_graph
{

//==============================================================================
//==============================================================================
class PlayHeadStateTests : public juce::UnitTest
{
public:
    PlayHeadStateTests()
        : juce::UnitTest ("PlayHeadStateTests", "tracktion_graph")
    {
    }
    
    void runTest() override
    {
    }

private:
};

static PlayHeadStateTests playHeadStateTests;

}
