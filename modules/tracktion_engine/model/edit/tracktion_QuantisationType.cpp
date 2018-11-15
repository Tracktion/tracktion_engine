/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


struct QuantisationTypeInfo
{
    const char* name;
    double beatFraction;
    int snapLevel;
    bool isTriplets;
};

static const QuantisationTypeInfo quantisationTypes[] =
{
    { NEEDS_TRANS("(none)"),      0.0,          0,  false },
    { NEEDS_TRANS("1/64 beat"),   1.0 / 64.0,   3,  false },
    { NEEDS_TRANS("1/32 beat"),   1.0 / 32.0,   4,  false },
    { NEEDS_TRANS("1/24 beat"),   1.0 / 24.0,   4,  true  },
    { NEEDS_TRANS("1/16 beat"),   1.0 / 16.0,   5,  false },
    { NEEDS_TRANS("1/12 beat"),   1.0 / 12.0,   5,  true  },
    { NEEDS_TRANS("1/9 beat"),    1.0 / 9.0,    6,  true  },
    { NEEDS_TRANS("1/8 beat"),    1.0 / 8.0,    6,  false },
    { NEEDS_TRANS("1/6 beat"),    1.0 / 6.0,    7,  true  },
    { NEEDS_TRANS("1/4 beat"),    1.0 / 4.0,    7,  false },
    { NEEDS_TRANS("1/3 beat"),    1.0 / 3.0,    8,  true  },
    { NEEDS_TRANS("1/2 beat"),    1.0 / 2.0,    8,  false },
    { NEEDS_TRANS("1 beat"),      1.0,          9,  false }
};


//==============================================================================
QuantisationType::QuantisationType()  : state (IDs::QUANTISATION)
{
    initialiseCachedValues (nullptr);
}

QuantisationType::QuantisationType (const ValueTree& vt, UndoManager* um)  : state (vt)
{
    initialiseCachedValues (um);
}

QuantisationType::~QuantisationType()
{
    state.removeListener (this);
}

QuantisationType::QuantisationType (const QuantisationType& other)
    : state (other.state.createCopy()),
      typeIndex (other.typeIndex),
      fractionOfBeat (other.fractionOfBeat)
{
    initialiseCachedValues (nullptr);
}

QuantisationType& QuantisationType::operator= (const QuantisationType& other)
{
    proportion     = other.proportion.get();
    typeName       = other.typeName.get();
    typeIndex      = other.typeIndex;
    fractionOfBeat = other.fractionOfBeat;

    return *this;
}

void QuantisationType::initialiseCachedValues (UndoManager* um)
{
    state.addListener (this);

    typeName.referTo (state, IDs::type, um);
    proportion.referTo (state, IDs::amount, um, 1.0f);
    quantiseNoteOffs.referTo (state, IDs::quantiseNoteOffs, um, true);

    updateType();
}

void QuantisationType::updateType()
{
    auto typeNameToIndex = [] (const String& newTypeName) -> int
    {
        int newType = 0;
        const String targetFraction (newTypeName.retainCharacters ("0123456789/"));

        for (int i = numElementsInArray (quantisationTypes); --i >= 0;)
            if (targetFraction == String (quantisationTypes[i].name).retainCharacters ("0123456789/"))
                return i;

        return newType;
    };

    typeIndex = typeNameToIndex (typeName);
    updateFraction();
}

void QuantisationType::updateFraction()
{
    if (typeIndex >= 0 && typeIndex < numElementsInArray (quantisationTypes))
        fractionOfBeat = quantisationTypes[typeIndex].beatFraction;
    else
        fractionOfBeat = 0.0;
}

String QuantisationType::getType (bool translated) const
{
    if (typeIndex >= 0 && typeIndex < numElementsInArray (quantisationTypes))
        return translated ? TRANS(quantisationTypes[typeIndex].name)
                          : quantisationTypes[typeIndex].name;

    jassertfalse;
    return {};
}

int QuantisationType::getTimecodeSnapTypeLevel (bool& isTriplet) const noexcept
{
    if (! isPositiveAndBelow (typeIndex, numElementsInArray (quantisationTypes)))
    {
        jassertfalse;
        isTriplet = false;
        return 0;
    }

    isTriplet = quantisationTypes[typeIndex].isTriplets;

    return quantisationTypes[typeIndex].snapLevel;
}

bool QuantisationType::isEnabled() const
{
    return typeIndex != 0 && proportion > 0.0f;
}

void QuantisationType::setProportion (float prop)
{
    proportion = jlimit (0.0f, 1.0f, prop);
}

double QuantisationType::roundToNearest (double time, const Edit& edit) const
{
    return roundTo (time, 0.5 - 1.0e-10, edit);
}

double QuantisationType::roundUp (double time, const Edit& edit) const
{
    return roundTo (time, 1.0 - 1.0e-10, edit);
}

double QuantisationType::roundToBeat (double beatNumber, double adjustment) const
{
    if (typeIndex == 0)
        return beatNumber;

    const double beats = fractionOfBeat * floor (beatNumber / fractionOfBeat + adjustment);

    return proportion == 1.0 ? beats
                             : beatNumber + proportion * (beats - beatNumber);
}

double QuantisationType::roundBeatToNearest (double beatNumber) const
{
    return roundToBeat (beatNumber, 0.5);
}

double QuantisationType::roundBeatUp (double beatNumber) const
{
    return roundToBeat (beatNumber, 1.0 - 1.0e-10);
}

double QuantisationType::roundBeatToNearestNonZero (double beatNumber) const
{
    auto t = roundBeatToNearest (beatNumber);

    return t == 0 ? fractionOfBeat : t;
}

double QuantisationType::roundTo (double time, double adjustment, const Edit& edit) const
{
    if (typeIndex == 0)
        return time;

    auto& s = edit.tempoSequence;
    double beats = s.timeToBeats (time);

    beats = fractionOfBeat * floor (beats / fractionOfBeat + adjustment);

    return proportion >= 1.0 ? s.beatsToTime (beats)
                             : time + proportion * (s.beatsToTime (beats) - time);
}

StringArray QuantisationType::getAvailableQuantiseTypes (bool translated)
{
    StringArray s;

    for (int i = 0; i < numElementsInArray (quantisationTypes); ++i)
        s.add (translated ? TRANS(quantisationTypes[i].name)
                          : quantisationTypes[i].name);

    return s;
}

String QuantisationType::getDefaultType (bool translated)
{
    return translated ? TRANS(quantisationTypes[0].name)
                      : quantisationTypes[0].name;
}

void QuantisationType::valueTreePropertyChanged (ValueTree& vt, const Identifier& i)
{
    if (vt == state)
    {
        if (i == IDs::type)
        {
            typeName.forceUpdateOfCachedValue();
            updateType();
        }
        else if (i == IDs::amount)
        {
            updateFraction();
        }
    }
}

void QuantisationType::applyQuantisationToSequence (juce::MidiMessageSequence& ms, Edit& ed, double start)
{
    if (! isEnabled())
        return;

    for (int i = ms.getNumEvents(); --i >= 0;)
    {
        auto* e = ms.getEventPointer (i);
        auto& m = e->message;

        if (m.isNoteOn())
        {
            const auto noteOnTime = roundToNearest (start + m.getTimeStamp(), ed) - start;

            if (auto noteOff = e->noteOffObject)
            {
                auto& mOff = noteOff->message;

                if (quantiseNoteOffs)
                {
                    auto noteOffTime = roundUp (start + mOff.getTimeStamp(), ed) - start;

                    static constexpr double beatsToBumpUpBy = 1.0 / 512.0;

                    if (noteOffTime <= noteOnTime) // Don't want note on and off time the same
                        noteOffTime = roundUp (noteOnTime + beatsToBumpUpBy, ed);

                    mOff.setTimeStamp (noteOffTime);
                }
                else
                {
                    mOff.setTimeStamp (noteOnTime + (mOff.getTimeStamp() - m.getTimeStamp()));
                }
            }

            m.setTimeStamp (noteOnTime);
        }
        else if (m.isNoteOff() && quantiseNoteOffs)
        {
            m.setTimeStamp (roundUp (start + m.getTimeStamp(), ed) - start);
        }
    }
}
