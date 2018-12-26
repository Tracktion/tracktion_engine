/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

class MelodyneFileReader   : public juce::ReferenceCountedObject,
                             public juce::ChangeBroadcaster
{
public:
    MelodyneFileReader (Edit&, AudioClipBase&);
    MelodyneFileReader (Edit&, AudioClipBase&, MelodyneFileReader& oneToClone);
    ~MelodyneFileReader();

    using Ptr = juce::ReferenceCountedObjectPtr<MelodyneFileReader>;

    static void cleanUpOnShutdown();

    //==============================================================================
    bool isValid() const noexcept                       { return player != nullptr; }

    ExternalPlugin* getPlugin();
    void showPluginWindow();

    bool isAnalysingContent();
    juce::MidiMessageSequence getAnalysedMIDISequence();

    AudioNode* createAudioNode (LiveClipLevel);
    void sourceClipChanged();

private:
    class MelodyneAudioNode;

    std::unique_ptr<ARAClipPlayer> player;
    juce::MidiBuffer midiBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MelodyneFileReader)
};


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

} // namespace tracktion_engine
