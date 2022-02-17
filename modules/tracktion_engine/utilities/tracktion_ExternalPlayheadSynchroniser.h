/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
/** Converts an Edit's internal transport information to a juce::AudioPlayHead::CurrentPositionInfo. */
juce::AudioPlayHead::CurrentPositionInfo getCurrentPositionInfo (Edit&);

/** Syncs an Edit's transport and tempo sequence to a juce AudioPlayHead. */
void synchroniseEditPosition (Edit&, const juce::AudioPlayHead::CurrentPositionInfo&);


//==============================================================================
//==============================================================================
/** An ExternalPlayheadSynchroniser is used to synchronise the internal Edit's playhead with an
    AudioProcessor, for use in plugins.
    
    Just create one when you create your Edit and repeatedly call the synchronise method
    in your process block like this:
    @code
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        // Update position info first
        externalPlayheadSynchroniser.synchronise (*this);
        ...
    @endcode
*/
class ExternalPlayheadSynchroniser
{
public:
    /** Constructs an ExternalPlayheadSynchroniser. */
    ExternalPlayheadSynchroniser (Edit&);
    
    /** Synchronises the Edit's playback position with an AudioPlayHead if possible.
        Generally you'd call this at the start of your plugin's processBlock method.
        @returns true if it was able to synchronise, false otherwise.
    */
    bool synchronise (juce::AudioPlayHead&);

    /** Synchronises the Edit's playback position with the AudioProcessor's playhead if possible.
        Generally you'd call this at the start of your plugin's processBlock method.
        @returns true if it was able to synchronise, false otherwise.
    */
    bool synchronise (juce::AudioProcessor&);

    /** Returns the current position info, useful for graphical displays etc. */
    juce::AudioPlayHead::CurrentPositionInfo getPositionInfo() const;

private:
    Edit& edit;
    juce::AudioPlayHead::CurrentPositionInfo positionInfo;
    mutable juce::SpinLock positionInfoLock;
};

}} // namespace tracktion { inline namespace engine
