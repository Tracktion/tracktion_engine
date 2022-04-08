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

/**
*/
class Clipboard   : private juce::DeletedAtShutdown
{
public:
    //==============================================================================
    Clipboard();
    ~Clipboard();

    JUCE_DECLARE_SINGLETON (Clipboard, false)

    //==============================================================================
    struct ContentType
    {
        virtual ~ContentType();

        struct EditPastingOptions
        {
            EditPastingOptions (Edit&, EditInsertPoint&);
            EditPastingOptions (Edit&, EditInsertPoint&, SelectionManager*);

            Edit& edit;
            EditInsertPoint& insertPoint;
            Track::Ptr startTrack;
            TimePosition startTime;
            SelectionManager* selectionManager = nullptr;
            bool silent = false;
            FileDragList::PreferredLayout preferredLayout = FileDragList::horizontal;
            bool setTransportToEnd = false;
        };

        virtual bool pasteIntoEdit (Edit&, EditInsertPoint&, SelectionManager*) const;
        virtual bool pasteIntoEdit (const EditPastingOptions&) const;
    };

    struct ProjectItems  : public ContentType
    {
        ProjectItems();
        ~ProjectItems() override;

        using ContentType::pasteIntoEdit;
        bool pasteIntoEdit (const EditPastingOptions&) const override;
        bool pasteIntoProject (Project&) const;

        struct ItemInfo
        {
            ProjectItemID itemID;
            TimeRange range;
        };

        std::vector<ItemInfo> itemIDs;
    };

    struct Clips  : public ContentType
    {
        Clips();
        ~Clips() override;

        bool pasteIntoEdit (Edit&, EditInsertPoint&, SelectionManager*) const override;
        bool pasteIntoEdit (const EditPastingOptions&) const override;

        bool pasteInsertingAtCursorPos (Edit&, EditInsertPoint&, SelectionManager&) const;
        bool pasteAfterSelected (Edit&, EditInsertPoint&, SelectionManager&) const;

        enum class AutomationLocked
        {
            no, /**< Don't copy autmation. */
            yes /**< Do copy autmation. */
        };
        
        void addClip (int trackOffset, const juce::ValueTree& state);
        void addSelectedClips (const SelectableList&, TimeRange, AutomationLocked);
        void addAutomation (const juce::Array<TrackSection>&, TimeRange);

        struct ClipInfo
        {
            juce::ValueTree state;
            int trackOffset = 0;
            bool hasBeatTimes = false;
            BeatPosition startBeats;
            BeatDuration lengthBeats;
            BeatPosition offsetBeats;
        };

        std::vector<ClipInfo> clips;

        struct AutomationCurveSection
        {
            juce::String pluginName, paramID;
            int trackOffset = 0;
            std::vector<AutomationCurve::AutomationPoint> points;
            juce::Range<float> valueRange;
        };

        std::vector<AutomationCurveSection> automationCurves;
    };

    struct Tracks  : public ContentType
    {
        Tracks();
        ~Tracks() override;

        using ContentType::pasteIntoEdit;
        bool pasteIntoEdit (const EditPastingOptions&) const override;

        std::vector<juce::ValueTree> tracks;
    };

    struct TempoChanges  : public ContentType
    {
        TempoChanges (const TempoSequence&, TimeRange);
        ~TempoChanges() override;

        using ContentType::pasteIntoEdit;
        bool pasteIntoEdit (const EditPastingOptions&) const override;

        bool pasteTempoSequence (TempoSequence&, TimeRange targetRange) const;

        struct TempoChange
        {
            BeatPosition beat;
            double bpm;
            float curve;
        };

        std::vector<TempoChange> changes;
    };

    struct AutomationPoints  : public ContentType
    {
        AutomationPoints (const AutomationCurve&, TimeRange);
        ~AutomationPoints() override;

        using ContentType::pasteIntoEdit;
        bool pasteIntoEdit (const EditPastingOptions&) const override;

        bool pasteAutomationCurve (AutomationCurve&, TimeRange targetRange) const;

        std::vector<AutomationCurve::AutomationPoint> points;
        juce::Range<float> valueRange;
    };

    struct MIDIEvents  : public ContentType
    {
        MIDIEvents();
        ~MIDIEvents() override;

        std::pair<juce::Array<MidiNote*>, juce::Array<MidiControllerEvent*>> pasteIntoClip (MidiClip&,
                                                                                            const juce::Array<MidiNote*>& selectedNotes,
                                                                                            const juce::Array<MidiControllerEvent*>& selectedEvents,
                                                                                            TimePosition cursorPosition, const std::function<BeatPosition (BeatPosition)>& snapBeat,
                                                                                            int destController) const;


        using ContentType::pasteIntoEdit;
        bool pasteIntoEdit (const EditPastingOptions&) const override;

        std::vector<juce::ValueTree> notes;
        std::vector<juce::ValueTree> controllers;

    private:
        juce::Array<MidiNote*> pasteNotesIntoClip (MidiClip&, const juce::Array<MidiNote*>& selectedNotes,
                                                   TimePosition cursorPosition, const std::function<BeatPosition (BeatPosition)>& snapBeat) const;

        juce::Array<MidiControllerEvent*> pasteControllersIntoClip (MidiClip& clip,
                                                                    const juce::Array<MidiNote*>& selectedNotes,
                                                                    const juce::Array<MidiControllerEvent*>& selectedEvents,
                                                                    TimePosition cursorPosition, const std::function<BeatPosition (BeatPosition)>& snapBeat,
                                                                    int destController) const;
    };

    struct Pitches  : public ContentType
    {
        Pitches();
        ~Pitches() override;

        using ContentType::pasteIntoEdit;
        bool pasteIntoEdit (const EditPastingOptions&) const override;

        std::vector<juce::ValueTree> pitches;
    };

    struct TimeSigs  : public ContentType
    {
        TimeSigs();
        ~TimeSigs() override;

        using ContentType::pasteIntoEdit;
        bool pasteIntoEdit (const EditPastingOptions&) const override;

        std::vector<juce::ValueTree> timeSigs;
    };

    struct Plugins  : public ContentType
    {
        Plugins (const Plugin::Array&);
        ~Plugins() override;

        using ContentType::pasteIntoEdit;
        bool pasteIntoEdit (const EditPastingOptions&) const override;

        std::vector<juce::ValueTree> plugins;
        std::vector<std::pair<Selectable::WeakRef /* editRef */, juce::ValueTree>> rackTypes;
    };

    struct Takes  : public ContentType
    {
        Takes (const WaveCompManager&);
        ~Takes() override;

        bool pasteIntoClip (WaveAudioClip&) const;

        juce::ValueTree items;
    };

    struct Modifiers  : public ContentType
    {
        Modifiers();
        ~Modifiers() override;

        using ContentType::pasteIntoEdit;
        bool pasteIntoEdit (const EditPastingOptions&) const override;

        std::vector<juce::ValueTree> modifiers;
    };

    //==============================================================================
    void clear();
    bool isEmpty() const;

    void setContent (std::unique_ptr<ContentType> newContent);
    const ContentType* getContent() const;

    template <typename Type, typename... Args>
    void makeContent (Args&&... args)         { setContent (std::unique_ptr<Type> (new Type (std::forward<Args> (args)...))); }

    template <typename Type>
    const Type* getContentWithType() const    { return dynamic_cast<const Type*> (getContent()); }

    template <typename Type>
    bool hasContentWithType() const           { return getContentWithType<Type>() != nullptr; }

    //==============================================================================
    void addListener (juce::ChangeListener*);
    void removeListener (juce::ChangeListener*);

private:
    std::unique_ptr<ContentType> content;
    juce::ChangeBroadcaster broadcaster;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Clipboard)
};

}} // namespace tracktion { inline namespace engine
