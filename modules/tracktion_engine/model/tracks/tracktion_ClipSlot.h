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
    Represents a slot on a track that a Clip can live in to be played as a launched clip.
*/
class ClipSlot : public EditItem,
                 public Selectable,
                 public ClipOwner
{
public:
    /** Creates a ClipSlot for a given Track. */
    ClipSlot (const juce::ValueTree&, Track&);
    /** Destructor. */
    ~ClipSlot() override;

    //==============================================================================
    /** Sets the Clip to be used within this slot. */
    void setClip (Clip*);

    /** Returns the currently set clip. */
    Clip* getClip();

    //==============================================================================
    /** @internal. */
    juce::String getName() const override;
    /** @internal. */
    juce::String getSelectableDescription() override;
    /** @internal. */
    juce::ValueTree& getClipOwnerState() override;
    /** @internal. */
    Selectable* getClipOwnerSelectable() override;
    /** @internal. */
    Edit& getClipOwnerEdit() override;

private:
    juce::ValueTree state;
    Track& track;

    void clipCreated (Clip&) override;
    void clipAddedOrRemoved() override;
    void clipOrderChanged() override;
    void clipPositionChanged() override;
};


}} // namespace tracktion { inline namespace engine
