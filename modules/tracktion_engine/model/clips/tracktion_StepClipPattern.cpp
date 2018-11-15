/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


StepClip::Pattern::Pattern (StepClip& c, const ValueTree& v) noexcept
    : clip (c), state (v)
{
}

StepClip::Pattern::Pattern (const Pattern& other) noexcept
    : clip (other.clip), state (other.state)
{
}

String StepClip::Pattern::getName() const               { return state [IDs::name]; }
void StepClip::Pattern::setName (const String& name)    { state.setProperty (IDs::name, name, clip.getUndoManager()); }

int StepClip::Pattern::getNumNotes() const              { return state [IDs::numNotes]; }
void StepClip::Pattern::setNumNotes (int n)             { state.setProperty (IDs::numNotes, n, clip.getUndoManager()); }

double StepClip::Pattern::getNoteLength() const         { return state [IDs::noteLength]; }
void StepClip::Pattern::setNoteLength (double n)        { state.setProperty (IDs::noteLength, n, clip.getUndoManager()); }

BigInteger StepClip::Pattern::getChannel (int channel) const
{
    BigInteger b;
    b.parseString (state.getChild (channel) [IDs::pattern].toString(), 2);
    return b;
}

void StepClip::Pattern::setChannel (int channel, const BigInteger& c)
{
    while (channel >= state.getNumChildren())
    {
        if (c.isZero())
            return;

        insertChannel (-1);
    }

    state.getChild (channel).setProperty (IDs::pattern, c.toString (2), clip.getUndoManager());
}

juce::Array<int> StepClip::Pattern::getVelocities (int channel) const
{
    juce::Array<int> v;
    auto sa = StringArray::fromTokens (state.getChild (channel) [IDs::velocities].toString(), false);
    v.ensureStorageAllocated (sa.size());

    for (auto& s : sa)
        v.add (s.getIntValue());

    return v;
}

void StepClip::Pattern::setVelocities (int channel, const juce::Array<int>& va)
{
    if (channel >= state.getNumChildren())
    {
        jassertfalse;
        return;
    }

    StringArray sa;
    sa.ensureStorageAllocated (va.size());

    for (int v : va)
        sa.add (String (v));

    state.getChild (channel).setProperty (IDs::velocities, sa.joinIntoString (" "), clip.getUndoManager());
}

static double parseFraction (const String& s)
{
    const int splitIndex = s.indexOfChar ('/');
    const double num    = s.substring (0, splitIndex).getDoubleValue();
    const double denom  = s.substring (splitIndex + 1, s.length()).getDoubleValue();

    if (denom == 0.0)
        return 0.0;

    return num / denom;
}

juce::Array<double> StepClip::Pattern::getGates (int channel) const
{
    juce::Array<double> v;
    auto sa = StringArray::fromTokens (state.getChild (channel) [IDs::gates].toString(), false);
    v.ensureStorageAllocated (sa.size());

    for (auto& s : sa)
    {
        if (s.containsChar ('/'))
            v.add (parseFraction (s));
        else
            v.add (s.getDoubleValue());
    }

    return v;
}

void StepClip::Pattern::setGates (int channel, const juce::Array<double>& ga)
{
    if (channel >= state.getNumChildren())
    {
        jassertfalse;
        return;
    }

    StringArray sa;
    sa.ensureStorageAllocated (ga.size());

    for (double g : ga)
        sa.add (String (g));

    state.getChild (channel).setProperty (IDs::gates, sa.joinIntoString (" "), clip.getUndoManager());
}

bool StepClip::Pattern::getNote (int channel, int index) const noexcept
{
    return getChannel (channel) [index];
}

void StepClip::Pattern::setNote (int channel, int index, bool value)
{
    if (getNote (channel, index) != value
          && isPositiveAndBelow (channel, (int) maxNumChannels))
    {
        BigInteger b (getChannel (channel));
        b.setBit (index, value);
        setChannel (channel, b);

        if (value)
        {
            if (getVelocity (channel, index) == 0)
                setVelocity (channel, index, defaultNoteValue);

            if (getGate (channel, index) == 0.0)
                setGate (channel, index, 1.0);
        }
    }
}

int StepClip::Pattern::getVelocity (int channel, int index) const
{
    if (! getNote (channel, index))
        return -1;

    auto velocities = getVelocities (channel);

    if (isPositiveAndBelow (index, velocities.size()))
        return velocities[index];

    if (clip.getChannels()[channel] != nullptr)
        return 127;

    return -1;
}

void StepClip::Pattern::setVelocity (int channel, int index, int value)
{
    if (! isPositiveAndBelow (channel, (int) maxNumChannels))
        return;

    setNote (channel, index, value != 0);

    auto velocities = getVelocities (channel);
    const int size = velocities.size();

    if (! isPositiveAndNotGreaterThan (index, size))
        velocities.insertMultiple (size, 127, index - size);

    velocities.set (index, value);
    setVelocities (channel, velocities);
}

double StepClip::Pattern::getGate (int channel, int index) const
{
    if (! getNote (channel, index))
        return 0.0;

    auto gates = getGates (channel);

    if (isPositiveAndBelow (index, gates.size()))
        return gates[index];

    return 1.0;
}

void StepClip::Pattern::setGate (int channel, int index, double value)
{
    if (! isPositiveAndBelow (channel, (int) maxNumChannels))
        return;

    setNote (channel, index, value != 0.0);

    auto gates = getGates (channel);
    const int size = gates.size();

    if (! isPositiveAndNotGreaterThan (index, size))
        gates.insertMultiple (size, 1.0, index - size);

    gates.set (index, value);
    setGates (channel, gates);
}

void StepClip::Pattern::clear()
{
    state.removeAllChildren (clip.getUndoManager());
}

void StepClip::Pattern::clearChannel (int channel)
{
    setChannel (channel, BigInteger());
}

void StepClip::Pattern::insertChannel (int channel)
{
    state.addChild (ValueTree (IDs::CHANNEL), channel, clip.getUndoManager());
}

void StepClip::Pattern::removeChannel (int channel)
{
    state.removeChild (channel, clip.getUndoManager());
}

void StepClip::Pattern::randomiseChannel (int channel)
{
    clearChannel (channel);

    Random r;
    for (int i = 0; i < getNumNotes(); ++i)
        setNote (channel, i, r.nextBool());
}

void StepClip::Pattern::randomiseSteps()
{
    Random r;
    const int numChannels = clip.getChannels().size();
    const int numSteps = getNumNotes();

    juce::Array<BigInteger> chans;
    chans.insertMultiple (0, BigInteger(), numChannels);

    for (int i = 0; i < numSteps; ++i)
        chans.getReference (r.nextInt (numChannels)).setBit (i);

    for (int i = 0; i < numChannels; ++i)
        setChannel (i, chans.getReference (i));
}

void StepClip::Pattern::shiftChannel (int channel, bool toTheRight)
{
    BigInteger c (getChannel (channel));

    // NB: Notes are added in reverse order
    if (toTheRight)
        c <<= 1;
    else
        c >>= 1;

    setChannel (channel, c);
}

void StepClip::Pattern::toggleAtInterval (int channel, int interval)
{
    BigInteger c = getChannel (channel);

    for (int i = 0; i < getNumNotes(); ++i)
        c.setBit (i, (i % interval) == 0);

    setChannel (channel, c);
}

StepClip::Pattern::CachedPattern::CachedPattern (const Pattern& p, int c)
    : notes (p.getChannel (c)),
      velocities (p.getVelocities (c)), gates (p.getGates (c))
{
}

bool StepClip::Pattern::CachedPattern::getNote (int index) const noexcept
{
    return notes[index];
}

int StepClip::Pattern::CachedPattern::getVelocity (int index) const noexcept
{
    if (! getNote (index))
        return -1;

    if (isPositiveAndBelow (index, velocities.size()))
        return velocities[index];

    return 127;
}

double StepClip::Pattern::CachedPattern::getGate (int index) const noexcept
{
    if (! getNote (index))
        return 0.0;

    if (isPositiveAndBelow (index, gates.size()))
        return gates[index];

    return 1.0;
}

//==============================================================================
StepClip::Pattern StepClip::PatternInstance::getPattern() const
{
    return clip.getPattern (patternIndex);
}

int StepClip::PatternInstance::getSequenceIndex() const
{
    return clip.getPatternSequence().indexOf (this);
}

String StepClip::PatternInstance::getSelectableDescription()
{
    return clip.getName() + "  -  "
         + TRANS("Section 123").replace ("123", String (getSequenceIndex() + 1)) + " ("
         + TRANS("Pattern 123").replace ("123", String (patternIndex + 1)) + ")";
}
