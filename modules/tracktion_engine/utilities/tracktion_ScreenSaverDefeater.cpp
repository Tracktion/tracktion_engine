/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion { inline namespace engine
{

static int numScreenSaverDefeaters = 0;

ScreenSaverDefeater::ScreenSaverDefeater()
{
    if (juce::Desktop::getInstance().isHeadless())
        return;

    TRACKTION_ASSERT_MESSAGE_THREAD
    ++numScreenSaverDefeaters;
    juce::Desktop::setScreenSaverEnabled (numScreenSaverDefeaters == 0);
}

ScreenSaverDefeater::~ScreenSaverDefeater()
{
    if (juce::Desktop::getInstance().isHeadless())
        return;

    TRACKTION_ASSERT_MESSAGE_THREAD
    --numScreenSaverDefeaters;
    jassert (numScreenSaverDefeaters >= 0);
    juce::Desktop::setScreenSaverEnabled (numScreenSaverDefeaters == 0);
}


}} // namespace tracktion { inline namespace engine
