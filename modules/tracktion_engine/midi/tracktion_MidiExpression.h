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

struct MidiExpression
{
    /** Returns true if this MidiNote is an expressive note. */
    static bool noteHasExpression (const juce::ValueTree& noteState) noexcept
    {
        jassert (noteState.hasType (IDs::NOTE));
        return noteState.getNumChildren() > 0;
    }

    /** Returns true if this MidiNote is an expressive note of type. */
    static bool noteHasExpression (const juce::ValueTree& noteState, const juce::Identifier& type) noexcept
    {
        jassert (noteState.hasType (IDs::NOTE));

        return noteState.getChildWithName (type).isValid();
    }

    /** Returns true if this MidiList is an expressive note. */
    static bool listHasExpression (const MidiList& midiList) noexcept;

    static bool isExpression (const juce::Identifier& type) noexcept
    {
        return type == IDs::PITCHBEND || type == IDs::PRESSURE || type == IDs::TIMBRE;
    }

    static juce::ValueTree createAndAddExpressionToNote (juce::ValueTree& noteState, const juce::Identifier& expressionType,
                                                         BeatPosition beat, float value, juce::UndoManager* um)
    {
        jassert (noteState.hasType (IDs::NOTE));
        jassert (isExpression (expressionType));

        juce::ValueTree expression (expressionType);
        MidiExpression exp (expression);
        exp.setBeatPosition (beat, um);
        exp.setValue (value, um);
        noteState.addChild (expression, -1, nullptr);

        return expression;
    }

    //==============================================================================
    /** Creates a MidiExpression interface to an existing MIDI note state.
        This can be either a MidiNote or expression tree.
    */
    MidiExpression (const juce::ValueTree& v)
        : state (v)
    {
        jassert (! v[IDs::b].isString());
        jassert (! v[IDs::v].isString());
        jassert (isExpression (v.getType()));
    }

    /** Returns the beat position relative to the note's start. */
    BeatPosition getBeatPosition() const noexcept     { return BeatPosition::fromBeats (static_cast<double> (state[IDs::b])); }

    /** Sets the beat position. */
    void setBeatPosition (BeatPosition newBeat, juce::UndoManager* um)
    {
        state.setProperty (IDs::b, newBeat.inBeats(), um);
    }

    /** Returns the value of the expression.
        N.B. The range will vary depending on the type.
    */
    float getValue() const noexcept             { return state[IDs::v]; }

    /** Sets the value with some range validation. */
    void setValue (float newValue, juce::UndoManager* um)
    {
        if (state.hasType (IDs::PRESSURE) || state.hasType (IDs::TIMBRE))
            jassert (juce::isPositiveAndNotGreaterThan (newValue, 1.0f));

        state.setProperty (IDs::v, newValue, um);
    }

    juce::ValueTree state;
};

}} // namespace tracktion { inline namespace engine
