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

//==============================================================================
//==============================================================================
enum class LaunchQType
{
    none,
    eightBars,
    fourBars,
    twoBars,
    bar,
    halfT,
    half,
    halfD,
    quarterT,
    quarter,
    quarterD,
    eighthT,
    eighth,
    eighthD,
    sixteenthT,
    sixteenth,
    sixteenthD,
    thirtySecondT,
    thirtySecond,
    thirtySecondD,
    sixtyFourthT,
    sixtyFourth,
    sixtyFourthD
};

//==============================================================================
/** Returns a list of quantisation type options in the order of LaunchQType. */
juce::StringArray getLaunchQTypeChoices();

/** Returns a the launch Q from a string returned from getLaunchQTypeChoices(). */
std::optional<LaunchQType> launchQTypeFromName (const juce::String&);

/** Retuns the name of a LaunchQType for display purposes. */
juce::String getName (LaunchQType);

/** Returns the fraction of a bar to be used for a given rate type. */
double toBarFraction (LaunchQType) noexcept;

/** Returns the fraction of a bar to be used for a given rate type. */
LaunchQType fromBarFraction (double) noexcept;

/** Returns the next quantised position. */
BeatPosition getNext (LaunchQType, const TempoSequence&, BeatPosition) noexcept;

/** Returns the next quantised position. */
BeatPosition getNext (LaunchQType, const tempo::Sequence&, BeatPosition) noexcept;

//==============================================================================
//==============================================================================
/**
    Represents a launch quantisation.
    Usually this is set globally in the Edit but may be overriden by clips.
*/
class LaunchQuantisation
{
public:
    /** Creates a LaunchQuantisation property on a given state tree.
        The Edit is used to find the TempoSequence and UndoManager.
    */
    LaunchQuantisation (juce::ValueTree&, Edit&);

    /** Returns the next beat quantised to the current type. */
    BeatPosition getNext (BeatPosition) const noexcept;

    //==============================================================================
    Edit& edit;                             /**< The Edit this belongs to.  */
    juce::CachedValue<LaunchQType> type;    /**< The current type property.  */
};

}} // namespace tracktion { inline namespace engine


//==============================================================================
//==============================================================================
namespace juce
{
template <>
struct VariantConverter<tracktion::engine::LaunchQType>
{
    static tracktion::engine::LaunchQType fromVar (const juce::var& v)   { return tracktion::engine::fromBarFraction (v); }
    static juce::var toVar (const tracktion::engine::LaunchQType& v)     { return tracktion::engine::toBarFraction (v); }
};
}
