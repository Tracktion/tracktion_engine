/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

struct ARAClipPlayer;

//==============================================================================
/**
    Manages an ARA plugin instance for an audio clip.

    This class handles the ARA (Audio Random Access) integration, allowing
    plugins like Melodyne to provide advanced audio analysis and manipulation.
*/
class ARAFileReader   : public juce::ReferenceCountedObject,
                        public juce::ChangeBroadcaster
{
public:
    ARAFileReader (Edit&, AudioClipBase&);
    ARAFileReader (Edit&, AudioClipBase&, ARAFileReader& oneToClone);
    ~ARAFileReader();

    using Ptr = juce::ReferenceCountedObjectPtr<ARAFileReader>;

    static void cleanUpOnShutdown();

    //==============================================================================
    bool isValid() const noexcept                       { return player != nullptr; }

    ExternalPlugin* getPlugin();
    void showPluginWindow();
    void hidePluginWindow();

    bool isAnalysingContent();
    juce::MidiMessageSequence getAnalysedMIDISequence();

    void sourceClipChanged();

    /** Notifies that the ARA content has changed (e.g. notes edited in Melodyne).
        This re-reads the content and broadcasts a change message. */
    void contentHasChanged();

private:
    std::unique_ptr<ARAClipPlayer> player;
    juce::MidiBuffer midiBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARAFileReader)
};

/** @deprecated Use ARAFileReader instead */
using MelodyneFileReader = ARAFileReader;


//==============================================================================
struct ARADocumentHolder
{
    ARADocumentHolder (Edit&, const juce::ValueTree&);
    ~ARADocumentHolder();

    void flushStateToValueTree();

    struct Pimpl;
    Pimpl* getPimpl();

private:
    Edit& edit;
    juce::ValueTree lastState;
    std::unique_ptr<Pimpl> pimpl;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ARADocumentHolder)
};

}} // namespace tracktion { inline namespace engine
