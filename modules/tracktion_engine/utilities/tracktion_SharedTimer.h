/*
,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

namespace tracktion { inline namespace engine
{

struct HertzTag { explicit HertzTag() = default; };
inline constexpr HertzTag HertzTag_t{};

//==============================================================================
//==============================================================================
class SharedTimer : public juce::Timer
{
public:
    //==============================================================================
    template<typename Type>
    SharedTimer (std::chrono::duration<Type> interval)
        : frequencyHz (static_cast<int> (1.0 / std::chrono::duration<double> (interval).count()))
    {
    }

    SharedTimer (HertzTag, int frequencyInHz)
        : frequencyHz (frequencyInHz)
    {
    }

    static SharedTimer fromFrequency (int frequencyInHz)
    {
        return SharedTimer (std::chrono::duration<double> (1.0 / frequencyInHz));
    }

    //==============================================================================
    struct Listener
    {
        virtual ~Listener() = default;
        virtual void sharedTimerCallback() = 0;
    };

    void addListener (Listener* l)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        listeners.add (l);

        if (! isTimerRunning())
            startTimerHz (frequencyHz);
    }

    void removeListener (Listener* l)
    {
        TRACKTION_ASSERT_MESSAGE_THREAD
        listeners.remove (l);

        if (listeners.isEmpty())
            stopTimer();
    }

private:
    //==============================================================================
    int frequencyHz;
    juce::ListenerList<Listener> listeners;

    //==============================================================================
    void timerCallback() override
    {
        listeners.call (&Listener::sharedTimerCallback);
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SharedTimer)
};

}} // namespace tracktion { inline namespace engine
