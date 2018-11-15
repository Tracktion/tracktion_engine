/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


static const juce::String flat = "b";
static auto oslash = juce::String (juce::CharPointer_UTF8 ("\xc3\xb8"));

static inline bool isRoman (const juce::String& str) noexcept
{
    auto firstChar = str[0];
    return firstChar == 'i' || firstChar == 'I' || firstChar == 'v' || firstChar == 'V';
}

static bool chordNameHasSymbol (const juce::String& name, const juce::String& symbol)
{
    if (isRoman (name))
        return false;

    auto str = name;

    if (str.substring (0, 1).containsAnyOf ("ABCDEFG"))
        str = str.substring (1);

    if (str.substring (0, 1) == "b" || str.substring (0, 1) == "#")
        str = str.substring (1);

    return str == symbol;
}

static juce::String chordNote (const juce::String& name)
{
    if (isRoman (name))
        return name;

    auto note = name.substring (0, 1);
    auto sharpFlat = name.substring (1, 2);

    if (sharpFlat == "b" || sharpFlat == "#")
        return note + sharpFlat;
    return note;
}

static juce::String chordSymbol (const juce::String& name)
{
    if (isRoman (name))
        return name;

    auto note = name.substring (0, 1);
    auto sharpFlat = name.substring (1, 2);

    if (sharpFlat == "b" || sharpFlat == "#")
        return name.substring (2);

    return name.substring (1);
}

juce::String fixLegacyChordNames (const juce::String& oldName)
{
    if (chordNameHasSymbol (oldName, "maj"))        return chordNote (oldName) + Chord (Chord::majorTriad).getSymbol();
    if (chordNameHasSymbol (oldName, "min"))        return chordNote (oldName) + Chord (Chord::minorTriad).getSymbol();
    if (chordNameHasSymbol (oldName, "dim"))        return chordNote (oldName) + Chord (Chord::diminishedTriad).getSymbol();
    if (chordNameHasSymbol (oldName, "aug"))        return chordNote (oldName) + Chord (Chord::augmentedTriad).getSymbol();
    if (chordNameHasSymbol (oldName, "maj6"))       return chordNote (oldName) + Chord (Chord::majorSixthChord).getSymbol();
    if (chordNameHasSymbol (oldName, "min6"))       return chordNote (oldName) + Chord (Chord::minorSixthChord).getSymbol();
    if (chordNameHasSymbol (oldName, "dom7"))       return chordNote (oldName) + Chord (Chord::dominatSeventhChord).getSymbol();
    if (chordNameHasSymbol (oldName, "maj7"))       return chordNote (oldName) + Chord (Chord::majorSeventhChord).getSymbol();
    if (chordNameHasSymbol (oldName, "min7"))       return chordNote (oldName) + Chord (Chord::minorSeventhChord).getSymbol();
    if (chordNameHasSymbol (oldName, "aug7"))       return chordNote (oldName) + Chord (Chord::augmentedSeventhChord).getSymbol();
    if (chordNameHasSymbol (oldName, "dim7"))       return chordNote (oldName) + Chord (Chord::diminishedSeventhChord).getSymbol();
    if (chordNameHasSymbol (oldName, oslash))       return chordNote (oldName) + Chord (Chord::halfDiminishedSeventhChord).getSymbol();
    if (chordNameHasSymbol (oldName, oslash + "7")) return chordNote (oldName) + Chord (Chord::minorMajorSeventhChord).getSymbol();
    return oldName;
}

//==============================================================================
Chord::Chord (ChordType c) : type (c)
{
}

Chord::Chord (juce::Array<int> steps_)  : type (Chord::customChord), steps (steps_)
{
}

juce::Array<Chord::ChordType> Chord::getAllChordType()
{
    juce::Array<ChordType> res;

    for (int i = majorTriad; i <= diminishedMinorNinthChord; i++)
        res.add ((ChordType) i);

    return res;
}

juce::String Chord::getName() const
{
    switch (type)
    {
        case majorTriad:                    return TRANS("Major Triad");
        case minorTriad:                    return TRANS("Minor Triad");
        case diminishedTriad:               return TRANS("Diminished Triad");
        case augmentedTriad:                return TRANS("Augmented Triad");
        case majorSixthChord:               return TRANS("Major Sixth");
        case minorSixthChord:               return TRANS("Minor Sixth");
        case dominatSeventhChord:           return TRANS("Dominant Seventh");
        case majorSeventhChord:             return TRANS("Major Seventh");
        case minorSeventhChord:             return TRANS("Minor Seventh");
        case augmentedSeventhChord:         return TRANS("Augmented Seventh");
        case diminishedSeventhChord:        return TRANS("Diminished Seventh");
        case halfDiminishedSeventhChord:    return TRANS("Half Diminished Seventh");
        case minorMajorSeventhChord:        return TRANS("Minor Major Seventh");
        case powerChord:                    return TRANS("Power");
        case suspendedSecond:               return TRANS("Suspended Second");
        case suspendedFourth:               return TRANS("Suspended Fourth");
        case majorNinthChord:               return TRANS("Major Ninth");
        case dominantNinthChord:            return TRANS("Dominant Ninth");
        case minorMajorNinthChord:          return TRANS("Minor Major Ninth");
        case minorDominantNinthChord:       return TRANS("Minor Dominant Ninth");
        case augmentedMajorNinthChord:      return TRANS("Augmented Major Ninth");
        case augmentedDominantNinthChord:   return TRANS("Augmented Dominant Ninth");
        case halfDiminishedNinthChord:      return TRANS("Half Diminished Ninth");
        case halfDiminishedMinorNinthChord: return TRANS("Half Diminished Minor Ninth");
        case diminishedNinthChord:          return TRANS("Diminished Ninth");
        case diminishedMinorNinthChord:     return TRANS("Diminished Minor Ninth");
        default: jassertfalse;              return {};
    }
}

juce::String Chord::getSymbol() const
{
    switch (type)
    {
        case majorTriad:                    return "M";
        case minorTriad:                    return "m";
        case diminishedTriad:               return "o";
        case augmentedTriad:                return "+";
        case majorSixthChord:               return "6";
        case minorSixthChord:               return "m6";
        case dominatSeventhChord:           return "7";
        case majorSeventhChord:             return "M7";
        case minorSeventhChord:             return "m7";
        case augmentedSeventhChord:         return "+7";
        case diminishedSeventhChord:        return "o7";
        case halfDiminishedSeventhChord:    return oslash + "7";
        case minorMajorSeventhChord:        return "mM7";
        case suspendedSecond:               return "sus2";
        case suspendedFourth:               return "sus4";
        case powerChord:                    return "5";
        case majorNinthChord:               return "M9";
        case dominantNinthChord:            return "9";
        case minorMajorNinthChord:          return "mM9";
        case minorDominantNinthChord:       return "m9";
        case augmentedMajorNinthChord:      return "+M9";
        case augmentedDominantNinthChord:   return "+9";
        case halfDiminishedNinthChord:      return oslash + "9";
        case halfDiminishedMinorNinthChord: return oslash + flat + "9";
        case diminishedNinthChord:          return "o9";
        case diminishedMinorNinthChord:     return "o" + flat + "9";
        default: jassertfalse;              return {};
    }
}

juce::Array<int> Chord::getSteps() const
{
    switch (type)
    {
        case majorTriad:                    return { 0, 4, 7 };
        case minorTriad:                    return { 0, 3, 7 };
        case diminishedTriad:               return { 0, 3, 6 };
        case augmentedTriad:                return { 0, 4, 8 };
        case majorSixthChord:               return { 0, 4, 7, 9 };
        case minorSixthChord:               return { 0, 3, 7, 9 };
        case dominatSeventhChord:           return { 0, 4, 7, 10 };
        case majorSeventhChord:             return { 0, 4, 7, 11 };
        case minorSeventhChord:             return { 0, 3, 7, 10 };
        case augmentedSeventhChord:         return { 0, 4, 8, 10 };
        case diminishedSeventhChord:        return { 0, 3, 6, 9  };
        case halfDiminishedSeventhChord:    return { 0, 3, 6, 10 };
        case minorMajorSeventhChord:        return { 0, 3, 7, 11 };
        case suspendedSecond:               return { 0, 2, 7 };
        case suspendedFourth:               return { 0, 5, 7 };
        case powerChord:                    return { 0, 7 };
        case majorNinthChord:               return { 0, 4, 7, 11, 14 };
        case dominantNinthChord:            return { 0, 4, 7, 10, 14 };
        case minorMajorNinthChord:          return { 0, 3, 7, 11, 14 };
        case minorDominantNinthChord:       return { 0, 3, 7, 10, 14 };
        case augmentedMajorNinthChord:      return { 0, 4, 8, 11, 14 };
        case augmentedDominantNinthChord:   return { 0, 4, 8, 10, 14 };
        case halfDiminishedNinthChord:      return { 0, 3, 6, 10, 14 };
        case halfDiminishedMinorNinthChord: return { 0, 3, 6, 10, 13 };
        case diminishedNinthChord:          return { 0, 3, 6, 9, 14 };
        case diminishedMinorNinthChord:     return { 0, 3, 6, 9, 13 };
        case customChord:                   return steps;
        default:               return {};
    }
}

juce::Array<int> Chord::getSteps (int inversion) const
{
    auto res = getSteps();

    if (inversion > 0)
    {
        for (int i = 0; i < inversion; i++)
        {
            int step = res.removeAndReturn (0) + 12;
            res.add (step);
        }
    }
    else if (inversion < 0)
    {
        for (int i = 0; i < -inversion; i++)
        {
            int step = res.removeAndReturn (res.size() - 1) - 12;
            res.insert (0, step);
        }
    }
    return res;
}

//==============================================================================

Scale::Scale (ScaleType type_) : type (type_)
{
    switch (type)
    {
        case major:
        case ionian:
            steps = { Whole, Whole, Half, Whole, Whole, Whole, Half };
            triads = generateTriads (0);
            sixths = generateSixths (0);
            sevenths = generateSevenths (0);
            break;
        case dorian:
            steps = { Whole, Half, Whole, Whole, Whole, Half, Whole };
            triads = generateTriads (1);
            sixths = generateSixths (1);
            sevenths = generateSevenths (1);
            break;
        case phrygian:
            steps = { Half, Whole, Whole, Whole, Half, Whole, Whole };
            triads = generateTriads (2);
            sixths = generateSixths (2);
            sevenths = generateSevenths (2);
            break;
        case lydian:
            steps = { Whole, Whole, Whole, Half, Whole, Whole, Half };
            triads = generateTriads (3);
            sixths = generateSixths (3);
            sevenths = generateSevenths (3);
            break;
        case mixolydian:
            steps = { Whole, Whole, Half, Whole, Whole, Half, Whole };
            triads = generateTriads (4);
            sixths = generateSixths (4);
            sevenths = generateSevenths (4);
            break;
        case minor:
        case aeolian:
            steps = { Whole, Half, Whole, Whole, Half, Whole, Whole };
            triads = generateTriads (5);
            sixths = generateSixths (5);
            sevenths = generateSevenths (5);
            break;
        case locrian:
            steps = { Half, Whole, Whole, Half, Whole, Whole, Whole };
            triads = generateTriads (6);
            sixths = generateSixths (6);
            sevenths = generateSevenths (6);
            break;
        case melodicMinor:
            steps = { Whole, Half, Whole, Whole, Whole, Whole, Half };
            triads = { Chord::minorTriad, Chord::minorTriad, Chord::augmentedTriad, Chord::majorTriad, Chord::majorTriad, Chord::diminishedTriad, Chord::diminishedTriad };
            sixths = { Chord::invalidChord, Chord::invalidChord, Chord::invalidChord, Chord::invalidChord, Chord::invalidChord, Chord::invalidChord, Chord::invalidChord };
            sevenths = { Chord::minorMajorSeventhChord, Chord::minorSeventhChord, Chord::augmentedSeventhChord, Chord::dominatSeventhChord, Chord::dominatSeventhChord, Chord::halfDiminishedSeventhChord, Chord::halfDiminishedSeventhChord };
            break;
        case harmonicMinor:
            steps = { Whole, Half, Whole, Whole, Half, WholeHalf, Half };
            triads = { Chord::minorTriad, Chord::diminishedTriad, Chord::augmentedTriad, Chord::minorTriad, Chord::majorTriad, Chord::majorTriad, Chord::diminishedTriad };
            sixths = { Chord::invalidChord, Chord::invalidChord, Chord::invalidChord, Chord::invalidChord, Chord::invalidChord, Chord::invalidChord, Chord::invalidChord };
            sevenths = { Chord::minorMajorSeventhChord, Chord::halfDiminishedSeventhChord, Chord::augmentedSeventhChord, Chord::minorSeventhChord, Chord::dominatSeventhChord, Chord::majorSeventhChord, Chord::diminishedSeventhChord };
            break;
    }
}

juce::Array<Scale::ScaleType> Scale::getAllScaleTypes()
{
    juce::Array<ScaleType> res;

    for (int i = major; i <= harmonicMinor; i++)
        res.add ((ScaleType)i);

    return res;
}

juce::StringArray Scale::getScaleStrings()
{
    juce::StringArray res;

    for (int i = major; i <= harmonicMinor; i++)
        res.add (getNameForType ((ScaleType) i));

    return res;
}

Scale::ScaleType Scale::getTypeFromName (juce::String name)
{
    for (int i = major; i <= harmonicMinor; i++)
        if (name == getNameForType ((ScaleType) i))
            return (ScaleType) i;

    jassertfalse;
    return major;
}

juce::String Scale::getName() const
{
    return getNameForType (type);
}

juce::String Scale::getNameForType (ScaleType type)
{
    switch (type)
    {
        case ionian:            return TRANS("Ionian");
        case dorian:            return TRANS("Dorian");
        case phrygian:          return TRANS("Phrygian");
        case lydian:            return TRANS("Lydian");
        case mixolydian:        return TRANS("Mixolydian");
        case aeolian:           return TRANS("Aeolian");
        case locrian:           return TRANS("Locrian");
        case major:             return TRANS("Major");
        case minor:             return TRANS("Minor");
        case melodicMinor:      return TRANS("Melodic Minor");
        case harmonicMinor:     return TRANS("Harmonic Minor");
        default: jassertfalse;  return {};
    }
}

juce::Array<Chord> Scale::generateTriads (int offset) const
{
    juce::Array<Chord> res;
    juce::Array<Chord> base { Chord::majorTriad, Chord::minorTriad, Chord::minorTriad, Chord::majorTriad, Chord::majorTriad, Chord::minorTriad, Chord::diminishedTriad };

    for (int i = 0; i < base.size(); i++)
        res.add (base[(i + offset) % base.size()]);

    return res;
}

juce::Array<Chord> Scale::generateSixths (int offset) const
{
    juce::Array<Chord> res;
    juce::Array<Chord> base { Chord::majorSixthChord, Chord::minorSixthChord, Chord::invalidChord, Chord::majorSixthChord, Chord::majorSixthChord, Chord::invalidChord, Chord::invalidChord };

    for (int i = 0; i < base.size(); i++)
        res.add (base[(i + offset) % base.size()]);

    return res;
}

juce::Array<Chord> Scale::generateSevenths (int offset) const
{
    juce::Array<Chord> res;
    juce::Array<Chord> base { Chord::majorSeventhChord, Chord::minorSeventhChord, Chord::minorSeventhChord, Chord::majorSeventhChord, Chord::dominatSeventhChord, Chord::minorSeventhChord, Chord::halfDiminishedSeventhChord };

    for (int i = 0; i < base.size(); i++)
        res.add (base[(i + offset) % base.size()]);

    return res;
}

juce::Array<int> Scale::getSteps (int octaves) const
{
    juce::Array<int> res { 0 };
    int step = 0;

    for (int i = 0; i < steps.size() - 1; i++)
    {
        int inc = 0;
        switch (steps[i])
        {
            case Whole:     inc = 2; break;
            case Half:      inc = 1; break;
            case WholeHalf: inc = 3; break;
            default: jassertfalse; break;
        }
        step += inc;
        res.add (step);
    }

    if (octaves > 1)
    {
        const int itemsInScale = res.size();
        for (int o = 1; o < octaves; o++)
            for (int i = 0; i < itemsInScale; i++)
                res.add (res[i] + o * 12);
    }

    return res;
}

juce::StringArray Scale::getIntervalNames()
{
    return {"i", "ii", "iii", "iv", "v", "vi", "vii"};
}

juce::String Scale::getIntervalName (Intervals interval) const
{
    auto name = getIntervalNames()[(int)interval];

    switch (triads[(int)interval].getType())
    {
        case Chord::majorTriad:        name = name.toUpperCase(); break;
        case Chord::minorTriad:        name = name; break;
        case Chord::augmentedTriad:    name = name.toUpperCase() + "+"; break;
        case Chord::diminishedTriad:   name = name + juce::String::charToString (176); break;
        default: jassertfalse; break;
    }

    return name;
}

//==============================================================================
struct PatternGenerator::ProgressionList  : public ValueTreeObjectList<PatternGenerator::ProgressionItem>
{
    ProgressionList (PatternGenerator& g, const juce::ValueTree& v)
        : ValueTreeObjectList<ProgressionItem> (v), generator (g)
    {
        rebuildObjects();
    }

    ~ProgressionList()
    {
        freeObjects();
    }

    bool isSuitableType (const juce::ValueTree& v) const override
    {
        return v.hasType (IDs::PROGRESSIONITEM);
    }

    ProgressionItem* createNewObject (const juce::ValueTree& v) override
    {
        return new ProgressionItem (generator, v);
    }

    void deleteObject (ProgressionItem* c) override
    {
        delete c;
    }

    void newObjectAdded (ProgressionItem*) override {}
    void objectRemoved (ProgressionItem*) override {}
    void objectOrderChanged() override {}

    PatternGenerator& generator;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ProgressionList)
};

//==============================================================================
PatternGenerator::ProgressionItem::ProgressionItem (PatternGenerator& g, const juce::ValueTree& s, bool temporary)
    : generator (g), state (s)
{
    auto um = temporary ? nullptr : g.clip.getUndoManager();

    chordName.referTo (state, IDs::name, um);
    pitches.referTo (state, IDs::pitches, um);
    lengthInBeats.referTo (state, IDs::length, um, 4);
    octave.referTo (state, IDs::octave, um);
    inversion.referTo (state, IDs::inversion, um);

    // Chord name format changed between W8 and W9 - update to new version
    chordName = fixLegacyChordNames (chordName);
}

PatternGenerator::ProgressionItem::~ProgressionItem() noexcept
{
}

bool PatternGenerator::ProgressionItem::operator== (const ProgressionItem& other) const noexcept
{
    return chordName == other.chordName
        && pitches == other.pitches
        && lengthInBeats == other.lengthInBeats;
}

void PatternGenerator::ProgressionItem::setChordName (juce::String name)
{
    if (isRoman (name))
        chordName = name.toLowerCase().retainCharacters ("iv7");
    else
        chordName = name;

    pitches = juce::String();
}

void PatternGenerator::ProgressionItem::setChordName (juce::String name, juce::String pitchesIn)
{
    chordName = name;
    pitches   = pitchesIn;
}

juce::String PatternGenerator::ProgressionItem::getChordName() const
{
    return generator.formatChordName (chordName);
}

void PatternGenerator::ProgressionItem::setRoot (int root)
{
    chordName = juce::MidiMessage::getMidiNoteName (root, true, false, 0) + chordSymbol (chordName);
}

void PatternGenerator::ProgressionItem::setChord (int root, Chord::ChordType type)
{
    chordName = juce::MidiMessage::getMidiNoteName (root, true, false, 0) + Chord (type).getSymbol();
    pitches   = juce::String();
}

Chord PatternGenerator::ProgressionItem::getChord (const Scale& scale) const
{
    if (isRoman (chordName))
    {
        auto progressionsOpts = Scale::getIntervalNames();
        auto interval = progressionsOpts.indexOf (chordName.get().retainCharacters ("iv")); // find where progression we are

        return chordName.get().contains ("7") ? scale.getSevenths()[interval] : scale.getTriads()[interval];
    }

    if (pitches.get().isNotEmpty())
    {
        juce::Array<int> steps;

        for (auto p : juce::StringArray::fromTokens (pitches.get(), ", ", {}))
            if (p.isNotEmpty())
                steps.add (p.getIntValue());

        return Chord (steps);
    }

    for (Chord::ChordType type : Chord::getAllChordType())
    {
        Chord chord (type);

        if (chordNameHasSymbol (chordName.get(), chord.getSymbol()))
            return chord;
    }

    return Chord (Chord::invalidChord);
}

bool PatternGenerator::ProgressionItem::isRomanNumeral() const
{
    return isRoman (chordName);
}

juce::String PatternGenerator::ProgressionItem::getChordSymbol()
{
    if (isRomanNumeral())
    {
        double beat = 0;

        for (auto itm : generator.getChordProgression())
        {
            if (itm == this)
                break;

            beat += itm->lengthInBeats;
        }

        Scale scale = generator.getScaleAtBeat (beat);
        const int root = getRootNote (generator.getNoteAtBeat (beat), scale);
        Chord chord = getChord (scale);

        return juce::MidiMessage::getMidiNoteName (root, true, false, 0) + chord.getSymbol();
    }

    return chordName;
}

int PatternGenerator::ProgressionItem::getRootNote (int key, const Scale& scale)
{
    jassert (chordName.get().isNotEmpty());

    if (isRoman (chordName))
    {
        auto progressionsOpts = Scale::getIntervalNames();
        auto progressionSteps = scale.getSteps (3);

        const int interval = progressionsOpts.indexOf (chordName.get().retainCharacters ("iv")); // find where progression we are
        const int chordRoot = key + progressionSteps[interval]; // find base note of the current chord

        return chordRoot;
    }

    auto name = chordName.get();
    auto noteName = name.substring (1, 2) == "#" ? name.substring (0, 2) : name.substring (0, 1);

    for (int note = 0; note < 12; note++)
        if (juce::MidiMessage::getMidiNoteName (note, true, false, 0) == noteName)
            return note;

    jassertfalse;
    return 0;
}

//==============================================================================
const int PatternGenerator::scaleRootGlobalTrack    = -1;
const int PatternGenerator::scaleRootChordTrack     = -2;

PatternGenerator::PatternGenerator (Clip& c, juce::ValueTree v) : clip (c), state (v)
{
    auto um = c.getUndoManager();

    progressionList = std::make_unique<ProgressionList> (*this, state.getOrCreateChildWithName (IDs::PROGRESSION, um));

    mode.referTo (state, IDs::mode, um, Mode::off);
    allNotes.referTo (state, IDs::allNotes, um);
    octaveUp.referTo (state, IDs::octaveUp, um);
    octaveDown.referTo (state, IDs::octaveDown, um);
    spread.referTo (state, IDs::spread, um);
    autoUpdate.referTo (state, IDs::autoUpdate, um, true);
    patternHash.referTo (state, IDs::hash, um);
    octave.referTo (state, IDs::octave, um, 4);
    velocity.referTo (state, IDs::velocity, um, 100.0f);
    arpUpDown.referTo (state, IDs::arpUpDown, um, false);
    arpPlayRoot.referTo (state, IDs::arpPlayroot, um, true);
    arpPatternLength.referTo (state, IDs::arpPatternLength, um, 4.0f);
    arpStyle.referTo (state, IDs::arpStle, um, "0123");
    arpSteps.referTo (state, IDs::arpSteps, um, 4);
    melodyNoteLength.referTo (state, IDs::melodyNoteLength, um, 1.0/4.0f);
    gate.referTo (state, IDs::gate, um, 100.0f);
    scaleType.referTo (state, IDs::scaleType, um, Scale::major);
    scaleRoot.referTo (state, IDs::scaleRoot, um, -1);

    state.addListener (this);

    editLoadedCallback = std::make_unique<Edit::LoadFinishedCallback<PatternGenerator>> (*this, clip.edit);
}

PatternGenerator::~PatternGenerator()
{
    state.removeListener (this);
}

MidiClip* PatternGenerator::getMidiClip() const
{
    return dynamic_cast<MidiClip*> (&clip);
}

void PatternGenerator::editFinishedLoading()
{
    // Update the note hash from v1 to v2 if required
    if (auto mc = getMidiClip())
        if (patternHash == hashNotes (mc->getSequence(), 1))
            patternHash = hashNotes (mc->getSequence(), 2);

    if (mode != Mode::off)
        autoUpdateManager = std::make_unique<AutoUpdateManager> (*this);
}

double PatternGenerator::getMinimumChordLength() const
{
    return 1.0;
}

double PatternGenerator::getMaximumChordLength() const
{
    switch (mode)
    {
        case Mode::arpeggio:
        case Mode::chords:
        case Mode::bass:
        case Mode::melody:
        default:
            return 16.0;
    }
}

void PatternGenerator::validateChordLengths()
{
    switch (mode)
    {
        case Mode::arpeggio:
        case Mode::off:
        case Mode::chords:
        case Mode::bass:
        case Mode::melody:
        {
            for (auto itm : getChordProgression())
                if (itm->lengthInBeats > getMaximumChordLength())
                    itm->lengthInBeats = getMaximumChordLength();

            break;
        }
    }
}

const juce::Array<PatternGenerator::ProgressionItem*>& PatternGenerator::getChordProgression() const noexcept
{
    return progressionList->objects;
}

void PatternGenerator::setChordProgression (juce::ValueTree src)
{
    jassert (src.isValid() && src.hasType (IDs::PROGRESSION));

    auto um = clip.getUndoManager();

    auto dst = state.getChildWithName (IDs::PROGRESSION);

    dst.removeAllChildren (um);

    for (int i = 0; i < src.getNumChildren(); i++)
        dst.addChild (src.getChild (i).createCopy(), i, um);
}

juce::ValueTree PatternGenerator::getChordPattern()
{
    return state.getChildWithName (IDs::CHORDPATTERN);
}

juce::ValueTree PatternGenerator::getBassPattern()
{
    return state.getChildWithName (IDs::BASSPATTERN);
}

void PatternGenerator::setChordPattern (juce::ValueTree pattern)
{
    auto um = clip.getUndoManager();

    state.removeChild (state.getChildWithName (IDs::CHORDPATTERN), um);

    state.addChild (pattern, -1, um);
}

void PatternGenerator::setBassPattern (juce::ValueTree pattern)
{
    auto um = clip.getUndoManager();

    state.removeChild (state.getChildWithName (IDs::BASSPATTERN), um);

    state.addChild (pattern, -1, um);
}

void PatternGenerator::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& c)
{
    if (c == IDs::arpSteps || c == IDs::arpUpDown)
    {
        // update current style to be of correct length
        int length = arpUpDown ? (arpSteps * 2 - 2) : arpSteps;

        if (arpStyle.get().length() != length)
            arpStyle = getArpStyles()[0];
    }
    else if (c == IDs::mode)
    {
        if (mode == Mode::off)
        {
            autoUpdateManager = nullptr;
        }
        else
        {
            if (autoUpdateManager == nullptr)
                autoUpdateManager = std::make_unique<AutoUpdateManager> (*this);
        }
    }
}

ChordClip* PatternGenerator::getChordClipAt (double t) const
{
    for (auto c : clip.edit.getChordTrack()->getClips())
        if (c->getPosition().time.contains (t))
            if (auto cc = dynamic_cast<ChordClip*> (c))
                return cc;

    return {};
}

Scale PatternGenerator::getScaleAtBeat (double beat) const
{
    Scale scale;

    if (dynamic_cast<ChordClip*> (&clip) != nullptr)
    {
        if (scaleRoot == scaleRootGlobalTrack)
        {
            auto t = clip.getTimeOfContentBeat (beat);
            scale = Scale (clip.edit.pitchSequence.getPitchAt (t).getScale());
        }
        else
        {
            scale = Scale (scaleType);
        }
    }
    else if (mode == Mode::off || scaleRoot == scaleRootChordTrack)
    {
        auto t = clip.getTimeOfContentBeat (beat);

        if (auto cc = getChordClipAt (t))
        {
            const double b = cc->getContentBeatAtTime (t);
            return cc->getPatternGenerator()->getScaleAtBeat (b);
        }

        scale = Scale (clip.edit.pitchSequence.getPitchAt (t).getScale());
    }
    else if (scaleRoot == scaleRootGlobalTrack)
    {
        const double t = clip.getTimeOfContentBeat (beat);
        scale = Scale (clip.edit.pitchSequence.getPitchAt (t).getScale());
    }
    else
    {
        scale = Scale (scaleType);
    }

    return scale;
}

int PatternGenerator::getNoteAtBeat (double beat) const
{
    if (dynamic_cast<ChordClip*>(&clip) != nullptr)
    {
        if (scaleRoot == scaleRootGlobalTrack)
            return clip.edit.pitchSequence.getPitchAtBeat (clip.getStartBeat() + beat).getPitch() % 12;

        return scaleRoot;
    }

    if (mode == Mode::off || scaleRoot == scaleRootChordTrack)
    {
        auto t = clip.getTimeOfContentBeat (beat);

        if (auto cc = getChordClipAt (t))
        {
            auto b = cc->getContentBeatAtTime (t);
            return cc->getPatternGenerator()->getNoteAtBeat (b);
        }

        return clip.edit.pitchSequence.getPitchAtBeat (clip.getStartBeat() + beat).getPitch() % 12;
    }

    if (scaleRoot == scaleRootGlobalTrack)
        return clip.edit.pitchSequence.getPitchAtBeat (clip.getStartBeat() + beat).getPitch() % 12;

    return scaleRoot;
}

juce::StringArray PatternGenerator::getPossibleTriadNames() const
{
    juce::StringArray res;

    auto scale = getScaleAtBeat (0);

    for (int interval = Scale::i; interval <= Scale::vii; interval++)
        res.add (scale.getIntervalName ((Scale::Intervals) interval));

    return res;
}

juce::StringArray PatternGenerator::getPossibleSeventhNames() const
{
    juce::StringArray res;
    auto scale = getScaleAtBeat (0);

    for (int interval = Scale::i; interval <= Scale::vii; interval++)
        res.add (scale.getIntervalName ((Scale::Intervals) interval) + "7");

    return res;
}

juce::String PatternGenerator::formatChordName (juce::String simplifiedChordName) const
{
    if (isRoman (simplifiedChordName))
    {
        auto interval = (Scale::Intervals) Scale::getIntervalNames().indexOf (simplifiedChordName.retainCharacters ("iv"));
        auto scale = getScaleAtBeat (0);
        auto intervalStr = scale.getIntervalName (interval);

        if (simplifiedChordName.contains ("7"))
            return intervalStr + "7";

        return intervalStr;
    }

    return simplifiedChordName;
}

juce::StringArray PatternGenerator::getChordProgressionChordNames (bool simplified) const
{
    juce::StringArray res;

    auto progression = state.getChildWithName (IDs::PROGRESSION);

    if (progression.isValid())
    {
        for (int i = 0; i < progression.getNumChildren(); i++)
        {
            auto progressionItem = progression.getChild (i);

            if (progressionItem.hasType (IDs::PROGRESSIONITEM))
            {
                auto name = progressionItem.getProperty (IDs::name);

                if (simplified)
                    res.add (name);
                else
                    res.add (formatChordName (name));
            }
        }
    }
    return res;
}

void PatternGenerator::setChordProgressionFromChordNames (juce::StringArray progressionItems)
{
    if (progressionItems.strings.size() > maxChords)
        progressionItems.strings.resize (maxChords);

    auto& um = clip.edit.getUndoManager();

    if (! state.getChildWithName (IDs::PROGRESSION).isValid())
        state.addChild (juce::ValueTree (IDs::PROGRESSION), -1, &um);

    auto progression = state.getChildWithName (IDs::PROGRESSION);

    progression.removeAllChildren (&um);

    for (auto itmName : progressionItems)
    {
        // store in simplest form
        if (isRoman (itmName))
            itmName = itmName.toLowerCase().retainCharacters ("iv7");

        juce::ValueTree item (IDs::PROGRESSIONITEM);
        item.setProperty (IDs::name, itmName, &um);

        progression.addChild (item, -1, &um);
    }
}

void PatternGenerator::duplicateChordInProgression (int idx)
{
    auto progression = state.getChildWithName (IDs::PROGRESSION);
    auto& um = clip.edit.getUndoManager();

    auto vt = progression.getChild (idx).createCopy();
    progression.addChild (vt, idx + 1, &um);
}

void PatternGenerator::removeIndexFromProgression (int idx)
{
    auto progression = state.getChildWithName (IDs::PROGRESSION);
    auto& um = clip.edit.getUndoManager();

    progression.removeChild (idx, &um);
}

void PatternGenerator::removeRangeFromProgression (int startIndex, int numberToRemove)
{
    auto progression = state.getChildWithName (IDs::PROGRESSION);
    auto& um = clip.edit.getUndoManager();

    auto endIndex = juce::jlimit (0, progression.getNumChildren(), startIndex + numberToRemove);
    startIndex = juce::jlimit (0, progression.getNumChildren(), startIndex);

    if (endIndex > startIndex)
    {
        numberToRemove = endIndex - startIndex;

        for (int i = 0; i < numberToRemove; ++i)
            progression.removeChild (i, &um);
    }
}

MidiNote* PatternGenerator::addNote (MidiList& sequence, int pitch, double startBeat, double lengthInBeats,
                                     int vel, int colourIndex, juce::UndoManager* um)
{
    if (pitch < 0 || pitch >= 128)
        return {};

    if (startBeat + lengthInBeats <= 0)
        return {};

    if (startBeat >= clip.getLengthInBeats())
        return {};

    if (lengthInBeats < 0.00001)
        return {};

    if (startBeat < 0)
    {
        lengthInBeats += startBeat;
        startBeat = 0;
    }

    if (startBeat + lengthInBeats > clip.getLengthInBeats())
        lengthInBeats -= startBeat + lengthInBeats - clip.getLengthInBeats();

    return sequence.addNote (pitch, startBeat, lengthInBeats, vel, colourIndex, um);
}

PatternGenerator::NoteType PatternGenerator::getTypeForNote (const MidiClip& mc, const MidiNote& note)
{
    bool inKey = false, inChord = false;

    Scale s = getScaleAtBeat (note.getStartBeat());
    int root = getNoteAtBeat (note.getStartBeat());

    inKey = s.getSteps().contains ((note.getNoteNumber() - root) % 12);

    juce::OwnedArray<ProgressionItem> progressionItems;
    double progressionOffset = getFlattenedChordProgression (progressionItems);

    double curBeat = -progressionOffset;
    int panic = 1000;
    int progressionCur = 0;
    const double clipLength = mc.isLooping() ?  mc.getLoopLengthBeats() :  mc.getLengthInBeats() +  mc.getOffsetInBeats();
    while (curBeat < clipLength)
    {
        if (panic-- == 0) break;

        if (auto item = progressionItems[progressionCur])
        {
            const double len = item->lengthInBeats;

            if (note.getBeatPosition() >= curBeat && note.getBeatPosition() < curBeat + len)
            {
                if (item->getChord (s).isValid())
                {
                    int chordRoot = item->getRootNote (root, s) % 12;

                    auto chordSteps = item->getChord (s).getSteps();

                    for (int i = 0; i < chordSteps.size(); i++)
                        chordSteps.set (i, chordSteps[i] % 12);

                    inChord = chordSteps.contains ((note.getNoteNumber() - chordRoot) % 12);
                }
                break;
            }

            curBeat += len;
        }

        progressionCur++;

        if (progressionCur >= progressionItems.size())
            progressionCur = 0;
    }

    if (inChord && inKey)
        return ChordInKeyNote;

    if (inChord && ! inKey)
        return ChordNotInKeyNote;

    if (inKey)
        return InKeyNote;

    return NotInKeyNote;
}

double PatternGenerator::getFlattenedChordProgression (juce::OwnedArray<ProgressionItem>& progression)
{
    if (mode == Mode::off || scaleRoot == scaleRootChordTrack)
    {
        double pos = 0;

        // Get all the chord clips
        juce::Array<ChordClip*> chordClips;

        for (auto c : clip.edit.getChordTrack()->getClips())
            if (auto cc = dynamic_cast<ChordClip*> (c))
                chordClips.add (cc);

        // Drop any chord clips that are completely overlapped
        for (int i = chordClips.size(); --i >= 0;)
        {
            for (int j = chordClips.size(); --j >= 0;)
            {
                if (i != j)
                {
                    auto c1 = chordClips[i];
                    auto c2 = chordClips[j];

                    if (c1->getStartBeat() >= c2->getStartBeat() &&
                        c1->getEndBeat()   <= c2->getEndBeat())
                    {
                        chordClips.remove (i);
                        break;
                    }
                }
            }
        }

        // Find the starts of chord clips
        juce::Array<double> clipStarts;

        for (auto cc : chordClips)
            clipStarts.add (cc->getStartBeat());

        clipStarts.sort();

        for (auto cc : chordClips)
        {
            // If there is a gap before this chord, insert empty space
            if (cc->getStartBeat() > pos)
            {
                double length = cc->getStartBeat() - pos;
                juce::ValueTree s (IDs::PROGRESSIONITEM);
                auto newItem = new ProgressionItem (*this, s, true);
                newItem->lengthInBeats = length;
                progression.add (newItem);
                pos += cc->getStartBeat() - pos;
            }

            double amountToDrop = 0.0;
            if (cc->getStartBeat() < pos)
                amountToDrop = pos - cc->getStartBeat();

            auto& src = cc->getPatternGenerator()->getChordProgression();
            if (src.size() > 0)
            {
                double avaliableLength = cc->getLengthInBeats();

                // check if following clip is overlapping, and truncate this clip
                for (auto start : clipStarts)
                {
                    double len = start - cc->getStartBeat();
                    if (len > 0 && len < avaliableLength)
                        avaliableLength = len;
                }

                double progressionLength = 0;
                while (progressionLength < avaliableLength)
                {
                    for (auto itm : src)
                    {
                        auto s = itm->state.createCopy();
                        std::unique_ptr<ProgressionItem> newItm (new ProgressionItem (*this, s, true));

                        double len = std::min (newItm->lengthInBeats.get(), avaliableLength - progressionLength);
                        progressionLength += len;

                        // this item is covered by another chord clip,
                        // either drop it or shorten it
                        if (len < amountToDrop)
                        {
                            if (progressionLength >= avaliableLength)
                                break;

                            amountToDrop -= len;
                            continue;
                        }
                        if (len > amountToDrop && amountToDrop > 0.0)
                        {
                            len -= amountToDrop;
                            amountToDrop = 0;
                        }

                        newItm->lengthInBeats = len;

                        progression.add (newItm.release());

                        if (progressionLength >= avaliableLength)
                            break;
                    }
                }
                pos += progressionLength;
            }
        }

        juce::ValueTree s (IDs::PROGRESSIONITEM);
        auto newItem = new ProgressionItem (*this, s, true);
        newItem->lengthInBeats = 100000;
        progression.add (newItem);

        double toChop = clip.getStartBeat();
        while (toChop > 0)
        {
            auto itm = progression[0];
            if (itm->lengthInBeats <= toChop)
            {
                toChop -= itm->lengthInBeats;
                progression.remove (0);
            }
            else
            {
                return toChop;
            }
        }
        return 0;
    }
    else
    {
        for (auto itm : getChordProgression())
            progression.add (new ProgressionItem (*this, itm->state, true));

        return 0;
    }
}

void PatternGenerator::clearProgression()
{
    auto progression = state.getChildWithName (IDs::PROGRESSION);

    auto& um = clip.edit.getUndoManager();

    progression.removeAllChildren (&um);
}

void PatternGenerator::insertChordIntoProgression (int idx, juce::String chordName)
{
    auto progression = state.getChildWithName (IDs::PROGRESSION);

    if (progression.getNumChildren() < maxChords)
    {
        auto& um = clip.edit.getUndoManager();

        if (isRoman (chordName))
            chordName = chordName.toLowerCase().retainCharacters ("iv7");

        juce::ValueTree item (IDs::PROGRESSIONITEM);
        item.setProperty (IDs::name, chordName, &um);

        progression.addChild (item, idx, &um);
    }
}

void PatternGenerator::insertChordIntoProgression (int idx, juce::String chordName, juce::String pitches)
{
    auto progression = state.getChildWithName (IDs::PROGRESSION);

    if (progression.getNumChildren() < maxChords)
    {
        auto& um = clip.edit.getUndoManager();

        if (isRoman (chordName))
            chordName = chordName.toLowerCase().retainCharacters ("iv7");

        juce::ValueTree item (IDs::PROGRESSIONITEM);
        item.setProperty (IDs::name, chordName, nullptr);
        item.setProperty (IDs::pitches, pitches, nullptr);

        progression.addChild (item, idx, &um);
    }
}

void PatternGenerator::moveChordInProgression(int srcIdx, int dstIdx)
{
    auto progression = state.getChildWithName (IDs::PROGRESSION);
    auto& um = clip.edit.getUndoManager();
    auto itm = progression.getChild (srcIdx);
    progression.removeChild (srcIdx, &um);

    if (dstIdx > srcIdx)
        progression.addChild (itm, dstIdx - 1, &um);
    else
        progression.addChild (itm, dstIdx, &um);
}

juce::StringArray PatternGenerator::getArpStyles()
{
    juce::StringArray res;
    std::string base;

    for (int i = 0; i < arpSteps; i++)
        base += std::to_string (i);

    do
    {
        res.add (base);
    } while (std::next_permutation (base.begin(), base.end()));

    if (arpUpDown)
    {
        for (int i = 0; i < res.size(); i++)
        {
            auto s = res[i];
            auto suffix = s.substring (1, s.length() - 1);

            for (int j = suffix.length() - 1; j >= 0; j--)
                s += suffix[j];

            res.set (i, s);
        }
    }

    return res;
}

void PatternGenerator::clearPattern()
{
    CRASH_TRACER

    auto um = clip.getUndoManager();
    if (auto mc = getMidiClip())
        mc->getSequence().clear (um);
}

void PatternGenerator::generatePattern()
{
    if (auto mc = getMidiClip())
    {
        if (auto at = mc->getAudioTrack())
            at->injectLiveMidiMessage (juce::MidiMessage::allNotesOff (mc->getMidiChannel().getChannelNumber()),
                                       MidiMessageArray::notMPE);

        switch (mode)
        {
            case Mode::off:      clearPattern();          clearHash();    break;
            case Mode::arpeggio: generateArpPattern();    updateHash();   break;
            case Mode::chords:   generateChordPattern();  updateHash();   break;
            case Mode::bass:     generateBassPattern();   updateHash();   break;
            case Mode::melody:   generateMelodyPattern(); updateHash();   break;
        }
    }
}

void PatternGenerator::generateArpPattern()
{
    auto mc = getMidiClip();

    if (mc == nullptr)
        return;

    clearPattern();

    CRASH_TRACER

    juce::OwnedArray<ProgressionItem> progressionItems;
    double progressionOffset = getFlattenedChordProgression (progressionItems);

    auto progressionLength = progressionItems.size();

    if (progressionLength == 0)
        return;

    auto um = mc->getUndoManager();
    auto& sequence = mc->getSequence();

    int progressionCur = 0;
    int stepCur = 0;

    // Convert the style string into array of ints
    juce::Array<int> styleValues;

    for (int i = 0; i < arpStyle.get().length(); i++)
        styleValues.add (arpStyle.get().substring (i, i + 1).getIntValue());

    const float lengthFactor = gate / 100.0f;
    const double noteLengthBeat = arpPatternLength / styleValues.size();

    // clear offset before generating clip
    mc->setOffset (0);

    // Loop over the length of the clip in incremements of note length
    double stepLengthLeft = progressionItems.getFirst()->lengthInBeats;
    const double clipLength = mc->isLooping() ? mc->getLoopLengthBeats() : mc->getLengthInBeats() + mc->getOffsetInBeats();
    double curBeat = -progressionOffset;
    bool stepStart = true;
    int panic = 1000;
    while (curBeat < clipLength)
    {
        if (panic-- == 0) break;

        auto stepLength = std::min (stepLengthLeft, noteLengthBeat);

        auto scale = getScaleAtBeat (curBeat);
        auto steps = scale.getSteps (3);

        if (auto progressionItem = progressionItems[progressionCur])
        {
            if (progressionItem->isValid())
            {
                const int chordRoot = progressionItem->getRootNote (getNoteAtBeat (curBeat), scale);
                const int octaveOffset = (octave + progressionItem->octave - 1) * 12;

                auto chord = progressionItem->getChord (scale);
                auto intervals = chord.getSteps (progressionItem->inversion);

                for (auto v : chord.getSteps (progressionItem->inversion)) intervals.add (v + 12);
                for (auto v : chord.getSteps (progressionItem->inversion)) intervals.add (v + 24);

                // find which note in the chord we are currently using and incremement by that amount
                const int stepIndex = styleValues[stepCur];
                const int note = chordRoot + intervals[stepIndex] + octaveOffset;

                addNote (sequence, note, curBeat, stepLength * lengthFactor, int (velocity / 100.0f * 127), 0, um);

                // if we are at first beat of a new stage in the progression, play the root note if wanted
                if (stepStart && arpPlayRoot)
                {
                    stepStart = false;

                    const int rootNote = chordRoot + octaveOffset - 12;
                    addNote (sequence, rootNote, curBeat, stepLengthLeft, int (velocity / 100.0f * 127), 0, um);
                }
            }

            // incremement current note in chord, then incremeent position in progression
            stepLengthLeft -= stepLength;
            stepCur++;
            if (stepCur >= styleValues.size())
                stepCur = 0;

            if (stepLengthLeft < 0.0001)
            {
                stepCur = 0;
                stepStart = true;
                progressionCur++;
                if (progressionCur >= progressionLength)
                    progressionCur = 0;

                stepLengthLeft = progressionItems[progressionCur]->lengthInBeats;
            }

            curBeat += stepLength;
        }
        else
        {
            jassertfalse;
            return;
        }
    }
}

struct ChordNote
{
    ChordNote (float start_, float length_, float velocity_) : start (start_), length (length_), velocity (velocity_) {}

    float start = 0;
    float length = 0;
    float velocity = 0;
};

void PatternGenerator::generateChordPattern()
{
    auto mc = getMidiClip();
    if (mc == nullptr)
        return;

    clearPattern();

    CRASH_TRACER

    juce::OwnedArray<ProgressionItem> progressionItems;
    double progressionOffset = getFlattenedChordProgression (progressionItems);

    const int progressionLength = progressionItems.size();
    if (progressionLength == 0)
        return;

    juce::Array<juce::Array<ChordNote>> notes;

    // unpack the chord pattern
    auto pattern = getChordPattern();

    if (pattern.isValid())
    {
        for (int i = 0; i < pattern.getNumChildren(); i++)
        {
            notes.add ({});

            auto bar = pattern.getChild (i);
            const int barLen = std::max (1, int (bar.getProperty (IDs::length, 4)));

            for (int j = 0; j < bar.getNumChildren(); j++)
            {
                auto c = bar.getChild (j);

                for (int k = 0; k < 16 / barLen; k++)
                    notes.getReference (i).add (ChordNote (float (c.getProperty (IDs::start)) + barLen * k,
                                                           c.getProperty (IDs::length),
                                                           c.getProperty (IDs::velocity)));
            }
        }
    }
    else
    {
        notes.add ({});
        notes.getReference (0).add (ChordNote (0.0f, 16.0f, 127.0f));
    }

    auto um = mc->getUndoManager();
    MidiList& sequence = mc->getSequence();

    int progressionCur = 0; // Current step in the chord progression
    int patternCur = 0;     // Current step in the rhythm pattern

    const float lengthFactor = gate / 100.0f;

    // clear offset before generating clip
    mc->setOffset (0);

    // Loop over the length of the clip in incremements of pattern length
    double curBeat = -progressionOffset;
    const double clipLength = mc->isLooping() ? mc->getLoopLengthBeats() : mc->getLengthInBeats() + mc->getOffsetInBeats();
    int panic = 1000;

    while (curBeat < clipLength)
    {
        if (panic-- == 0)
            break;

        auto scale = getScaleAtBeat (curBeat);

        if (auto progressionItem = progressionItems[progressionCur])
        {
            const double patternLength = progressionItem->lengthInBeats;
            if (progressionItem->isValid())
            {
                const int chordRoot = progressionItem->getRootNote (getNoteAtBeat (curBeat), scale);
                const int octaveOffset = (octave + progressionItem->octave - 1) * 12;
                const int rootNote = chordRoot + octaveOffset;

                Chord chord = progressionItem->getChord (scale);

                auto steps = chord.getSteps (progressionItem->inversion);

                if (octaveDown)
                    steps.add (steps.getFirst() - 12);

                if (octaveUp)
                    steps.add (steps.getFirst() + 12);

                if (spread)
                {
                    steps.add (steps[1] + 12);
                    steps.remove (1);
                }

                for (int step : steps)
                {
                    for (const ChordNote& chordNote : notes[patternCur])
                    {
                        const int note = rootNote + step;
                        if (chordNote.start < patternLength)
                        {
                            addNote (sequence, note, curBeat + chordNote.start,
                                     std::min (chordNote.length * lengthFactor, float (patternLength) - chordNote.start),
                                     int (velocity / 100.0f * chordNote.velocity), 0, um);
                        }
                    }
                }
            }

            curBeat += patternLength;

            progressionCur++;
            if (progressionCur >= progressionLength)
                progressionCur = 0;

            patternCur++;
            if (patternCur >= notes.size())
                patternCur = 0;
        }
        else
        {
            jassertfalse;
            return;
        }
    }
}

void PatternGenerator::generateMelodyPattern()
{
    auto mc = getMidiClip();

    if (mc == nullptr)
        return;

    CRASH_TRACER
    clearPattern();

    juce::OwnedArray<ProgressionItem> progressionItems;
    auto progressionOffset = getFlattenedChordProgression (progressionItems);
    auto progressionLength = progressionItems.size();

    if (progressionLength == 0)
        return;

    auto um = mc->getUndoManager();
    auto& sequence = mc->getSequence();
    auto& edit = mc->edit;
    auto& tempoSequence = edit.tempoSequence;

    auto beatsPerWhole = tempoSequence.getTimeSigAt (mc->getPosition().getStart()).denominator.get();
    auto noteLengthBeat = beatsPerWhole * melodyNoteLength;

    juce::Array<ChordNote> notes;

    for (int i = 0; i < std::ceil (16 / noteLengthBeat) * 4; i++)
        notes.add (ChordNote (i * noteLengthBeat,
                              std::min (noteLengthBeat, 16 - i * noteLengthBeat), 127.0f));

    int progressionCur = 0; // Current step in the chord progression

    auto lengthFactor = gate / 100.0f;

    // clear offset before generating clip
    mc->setOffset (0);

    // Loop over the length of the clip in incremements of bar length
    auto curBeat = -progressionOffset;
    auto clipLength = mc->isLooping() ? mc->getLoopLengthBeats()
                                      : mc->getLengthInBeats() + mc->getOffsetInBeats();
    int panic = 1000;

    while (curBeat < clipLength)
    {
        if (panic-- == 0)
            break;

        auto scale = getScaleAtBeat (curBeat);
        auto steps = scale.getSteps (3);

        if (auto progressionItem = progressionItems[progressionCur])
        {
            auto patternLength = progressionItem->lengthInBeats.get();

            if (progressionItem->isValid())
            {
                auto chordRoot = progressionItem->getRootNote (getNoteAtBeat (curBeat), scale);
                auto octaveOffset = (octave + progressionItem->octave - 1) * 12;
                auto rootNote = chordRoot + octaveOffset;
                auto chord = progressionItem->getChord (scale);

                if (allNotes)
                {
                    int notesAdded = 0;
                    juce::Array<int> chordSteps;

                    for (auto s : chord.getSteps (0))
                    {
                        chordSteps.add (rootNote + s);
                        chordSteps.add (rootNote + s + 12);
                    }

                    chordSteps.sort();

                    for (int o = 0; o < 3; o++)
                    {
                        for (int step : scale.getSteps())
                        {
                            auto note1 = getNoteAtBeat (curBeat) + octaveOffset + step + o * 12;

                            if (notesAdded < 14 && note1 >= chordSteps.getFirst())
                            {
                                for (auto& chordNote : notes)
                                {
                                    if (chordNote.start < patternLength)
                                    {
                                        if (auto newNote = addNote (sequence, note1, curBeat + chordNote.start,
                                                                     std::min (chordNote.length * lengthFactor,
                                                                               float (patternLength) - chordNote.start),
                                                                     (int) (velocity / 100.0f * chordNote.velocity), 0, um))
                                        {
                                            newNote->setMute (true, um);
                                            newNote->setColour (chordSteps.contains (note1) ? 0 : 2, um);
                                        }
                                    }
                                }

                                ++notesAdded;
                            }
                        }
                    }
                }
                else
                {
                    for (int step : chord.getSteps (progressionItem->inversion))
                    {
                        for (auto& chordNote : notes)
                        {
                            auto note1 = rootNote + step;

                            if (chordNote.start < patternLength)
                                if (auto newNote = addNote (sequence, note1, curBeat + chordNote.start,
                                                             std::min (chordNote.length * lengthFactor,
                                                                       float (patternLength) - chordNote.start),
                                                             (int) (velocity / 100.0f * chordNote.velocity), 0, um))
                                    newNote->setMute (true, um);


                            auto note2 = rootNote + step + 12;

                            if (chordNote.start < patternLength)
                                if (auto newNote = addNote (sequence, note2, curBeat + chordNote.start,
                                                             std::min (chordNote.length * lengthFactor,
                                                                       float (patternLength) - chordNote.start),
                                                             (int) (velocity / 100.0f * chordNote.velocity), 0, um))
                                    newNote->setMute (true, um);

                        }
                    }
                }
            }

            curBeat += patternLength;

            if (++progressionCur >= progressionLength)
                progressionCur = 0;
        }
        else
        {
            jassertfalse;
            return;
        }
    }
}

struct BassNote
{
    float start = 0;
    float length = 0;
    float velocity = 0;
    int pitch = 0;
    int octave = 0;
};

void PatternGenerator::generateBassPattern()
{
    auto mc = getMidiClip();

    if (mc == nullptr)
        return;

    CRASH_TRACER
    clearPattern();

    juce::OwnedArray<ProgressionItem> progressionItems;
    auto progressionOffset = getFlattenedChordProgression (progressionItems);
    auto progressionLength = progressionItems.size();

    if (progressionLength == 0)
        return;

    juce::Array<juce::Array<BassNote>> notes;

    // Unpack the bass pattern
    auto pattern = getBassPattern();

    if (pattern.isValid())
    {
        for (int i = 0; i < pattern.getNumChildren(); i++)
        {
            notes.add ({});

            auto bar = pattern.getChild (i);
            auto barLen = std::max (1, int (bar.getProperty (IDs::length, 4)));

            for (int j = 0; j < bar.getNumChildren(); j++)
            {
                auto c = bar.getChild (j);

                for (int k = 0; k < 16 / barLen; k++)
                {
                    notes.getReference (i).add ({ float (c.getProperty (IDs::start)) + barLen * k,
                                                  c.getProperty (IDs::length),
                                                  c.getProperty (IDs::velocity),
                                                  c.getProperty (IDs::pitch),
                                                  c.getProperty (IDs::octave) });
                }
            }
        }
    }
    else
    {
        notes.add ({});
        notes.getReference (0).add ({ 0.0f, 16.0f, 127.0f, 0, 0 });
    }

    auto um = mc->getUndoManager();
    auto& sequence = mc->getSequence();

    int progressionCur = 0; // Current step in the chord progression
    int patternCur = 0;     // Current step in the rhythm pattern

    const float lengthFactor = gate / 100.0f;

    // clear offset before generating clip
    mc->setOffset (0);

    // Loop over the length of the clip in incremements of pattern length
    double curBeat = -progressionOffset;
    const double clipLength = mc->isLooping() ? mc->getLoopLengthBeats() : mc->getLengthInBeats() + mc->getOffsetInBeats();
    int panic = 1000;

    while (curBeat < clipLength)
    {
        if (panic-- == 0)
            break;

        auto scale = getScaleAtBeat (curBeat);
        juce::Array<int> steps = scale.getSteps (3);

        if (auto progressionItem = progressionItems[progressionCur])
        {
            const double patternLength = progressionItem->lengthInBeats;

            if (progressionItem->isValid())
            {
                const int chordRoot = progressionItem->getRootNote (getNoteAtBeat (curBeat), scale);
                const int octaveOffset = (octave + progressionItem->octave - 1) * 12;
                const int rootNote = chordRoot + octaveOffset;

                for (const BassNote& bassNote : notes[patternCur])
                {
                    int pos = bassNote.pitch;
                    int off = bassNote.octave * 12;

                    if (pos < scale.getSteps().size())
                    {
                        const int rootNoteIndex = steps.indexOf (chordRoot);
                        const int note = rootNote + steps[rootNoteIndex + pos] - steps[rootNoteIndex] + off;

                        if (bassNote.start < patternLength)
                        {
                            addNote (sequence, note, curBeat + bassNote.start,
                                     std::min (bassNote.length * lengthFactor,
                                               float (patternLength) - bassNote.start),
                                     (int) (velocity / 100.0f * bassNote.velocity), 0, um);
                        }
                    }
                }
            }

            curBeat += patternLength;

            progressionCur++;
            if (progressionCur >= progressionItems.size())
                progressionCur = 0;

            patternCur++;
            if (patternCur >= notes.size())
                patternCur = 0;
        }
        else
        {
            jassertfalse;
            return;
        }
    }
}

void PatternGenerator::playGuideChord (int idx) const
{
    if (auto mc = getMidiClip())
    {
        auto progressionItems = getChordProgression();

        if (idx < 0 || idx >= progressionItems.size())
        {
            jassertfalse;
            return;
        }

        double curBeat = 0;

        for (int i = 0; i < idx; i++)
            curBeat += progressionItems[i]->lengthInBeats;

        auto scale = getScaleAtBeat (curBeat);
        auto steps = scale.getSteps (3);

        if (auto progressionItem = progressionItems[idx])
        {
            const int chordRoot = progressionItem->getRootNote (getNoteAtBeat (curBeat), scale);
            const int octaveOffset = (octave + progressionItem->octave - 1) * 12;
            const int rootNote = chordRoot + octaveOffset;

            Chord chord = progressionItem->getChord (scale);

            if (auto at = mc->getAudioTrack())
            {
                bool stop = true;

                for (int step : chord.getSteps (progressionItem->inversion))
                {
                    auto note = rootNote + step;

                    if (note >= 0 && note < 128)
                    {
                        at->playGuideNote (note, mc->getMidiChannel(),
                                           (int) (velocity / 100 * 127), stop);
                        stop = false;
                    }
                }
            }
        }
    }
}

void PatternGenerator::updateHash()
{
    if (auto mc = getMidiClip())
        patternHash = hashNotes (mc->getSequence(), 2);
    else
        patternHash = 0;
}

void PatternGenerator::clearHash()
{
    patternHash = 0;
}

juce::int64 PatternGenerator::hashNotes (MidiList& sequence, int version)
{
    // Version 1 of this hash had a bug where just changing mute would
    // generate hash collisions
    juce::int64 hash = sequence.getNumNotes() + 1;

    for (auto note : sequence.getNotes())
    {
        hash ^= juce::int64 (note->getNoteNumber() * 31)
              ^ juce::int64 (note->getStartBeat()  * 73)
              ^ juce::int64 (note->getLengthBeats() * 233)
              ^ juce::int64 (note->getColour() * 467)
              ^ juce::int64 (note->isMute() ? 877 : 947)
              ^ juce::int64 (note->getVelocity() * 3083);

        if (version > 1)
            hash *= 7;
    }

    return hash;
}

void PatternGenerator::setAutoUpdate (bool on)
{
    if (on)
    {
        autoUpdate = true;
        generatePattern();
    }
    else
    {
        autoUpdate = false;
    }
}

bool PatternGenerator::getAutoUpdate()
{
    if (auto mc = getMidiClip())
        return autoUpdate && patternHash == hashNotes (mc->getSequence(), 2);

    return false;
}

void PatternGenerator::refreshPatternIfNeeded()
{
    if (autoUpdateManager != nullptr)
        autoUpdateManager->triggerAsyncUpdate();
}

//==============================================================================
PatternGenerator::AutoUpdateManager::AutoUpdateManager (PatternGenerator& owner_) : owner (owner_)
{
    owner.clip.state.addListener (this);
}

PatternGenerator::AutoUpdateManager::~AutoUpdateManager()
{
    owner.clip.state.removeListener (this);
}

void PatternGenerator::AutoUpdateManager::valueTreePropertyChanged (juce::ValueTree& p, const juce::Identifier& c)
{
    if (Clip::isClipState (p))
        if (c == IDs::start || c == IDs::length || c == IDs::offset)
            triggerAsyncUpdate();
}

void PatternGenerator::AutoUpdateManager::handleAsyncUpdate()
{
    if (owner.getAutoUpdate())
        owner.generatePattern();
}

//==============================================================================
// Krumhansl-Schmuckler Key-Finding Algorithm.
juce::Array<KeyResult> determineKeyOfNotes (const juce::Array<MidiNote*>& notes)
{
    juce::Array<KeyResult> results;

    double durations[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    for (auto n : notes)
        durations[n->getNoteNumber() % 12] += n->getLengthBeats();

    double major[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
    double minor[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };

    double xAveMajor = 0, xAveMinor = 0, yAve = 0;

    for (int i = 0; i < 12; i++)
    {
        xAveMajor += major[i];
        xAveMinor += minor[i];
        yAve      += durations[i];
    }

    xAveMajor /= 12;
    xAveMinor /= 12;
    yAve      /= 12;

    for (int key = 0; key < 12; key++)
    {
        double s1Major = 0, s2Major = 0, s3Major = 0;
        double s1Minor = 0, s2Minor = 0, s3Minor = 0;

        for (int i = 0; i < 12; i++)
        {
            double xiMajor = major[(i - key + 12) % 12];
            double xiMinor = minor[(i - key + 12) % 12];

            s1Major += (xiMajor - xAveMajor) * (durations[i] - yAve);
            s2Major += (xiMajor - xAveMajor) * (xiMajor - xAveMajor);
            s3Major += (durations[i] - yAve) * (durations[i] - yAve);

            s1Minor += (xiMinor - xAveMinor) * (durations[i] - yAve);
            s2Minor += (xiMinor - xAveMinor) * (xiMinor - xAveMinor);
            s3Minor += (durations[i] - yAve) * (durations[i] - yAve);
        }

        double rMajor = s1Major / std::sqrt (s2Major * s3Major);
        double rMinor = s1Minor / std::sqrt (s2Minor * s3Minor);

        results.add ({ rMajor, key, Scale::major });
        results.add ({ rMinor, key, Scale::minor });
    }

    results.sort();

    return results;
}
