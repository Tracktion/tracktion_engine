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

class Chord
{
public:
    enum ChordType
    {
        customChord = -2,
        invalidChord = -1,
        majorTriad = 0,
        minorTriad,
        diminishedTriad,
        augmentedTriad,
        majorSixthChord,
        minorSixthChord,
        dominatSeventhChord,
        majorSeventhChord,
        minorSeventhChord,
        augmentedSeventhChord,
        diminishedSeventhChord,
        halfDiminishedSeventhChord,
        minorMajorSeventhChord,
        suspendedSecond,
        suspendedFourth,
        powerChord,
        majorNinthChord,
        dominantNinthChord,
        minorMajorNinthChord,
        minorDominantNinthChord,
        augmentedMajorNinthChord,
        augmentedDominantNinthChord,
        halfDiminishedNinthChord,
        halfDiminishedMinorNinthChord,
        diminishedNinthChord,
        diminishedMinorNinthChord,
        // Update getAllChordType after adding / removing a chord
    };

    Chord (ChordType type = majorTriad);
    Chord (juce::Array<int> steps, juce::String symbol); // creates a custom chord with steps

    juce::String toString();
    static Chord fromString (const juce::String&);

    static juce::Array<ChordType> getAllChordType();

    ChordType getType() const   { return type; }
    bool isValid() const        { return type != invalidChord; }

    juce::String getName() const;
    juce::String getShortName() const;
    juce::String getSymbol() const;
    juce::Array<int> getSteps() const;
    juce::Array<int> getSteps (int inversion) const;

private:
    ChordType type;
    juce::Array<int> steps; // for custom chords
    juce::String symbol;    // for custom chords
};

//==============================================================================
class Scale
{
public:
    enum ScaleType
    {
        major = 0,
        minor,
        ionian,
        dorian,
        phrygian,
        lydian,
        mixolydian,
        aeolian,
        locrian,
        melodicMinor,
        harmonicMinor,
    };

    enum Steps
    {
        Whole = 0,
        Half,
        WholeHalf,
    };

    enum class Intervals
    {
        i = 0,
        ii,
        iii,
        iv,
        v,
        vi,
        vii
    };

    Scale (ScaleType type = major);

    ScaleType getType() const { return type; }

    juce::String getName() const;
    juce::String getShortName() const;

    static juce::StringArray getIntervalNames();

    static juce::Array<ScaleType> getAllScaleTypes();
    static juce::StringArray getScaleStrings();
    static juce::String getNameForType (ScaleType type);
    static juce::String getShortNameForType (ScaleType type);
    static ScaleType getTypeFromName (juce::String name);

    juce::Array<int> getSteps (int octaves = 1) const;

    juce::String getIntervalName (Intervals interval) const;

    juce::Array<Chord> getTriads()   const { return triads;   }
    juce::Array<Chord> getSixths()   const { return sixths;   }
    juce::Array<Chord> getSevenths() const { return sevenths; }

private:
    juce::Array<Chord> generateTriads (int offset) const;
    juce::Array<Chord> generateSixths (int offset) const;
    juce::Array<Chord> generateSevenths (int offset) const;

    ScaleType type;
    juce::Array<Steps> steps;
    juce::Array<Chord> triads;
    juce::Array<Chord> sixths;
    juce::Array<Chord> sevenths;
};

//==============================================================================
class PatternGenerator  : private ValueTreeAllEventListener
{
public:
    enum class Mode
    {
        off,
        arpeggio,
        chords,
        bass,
        melody
    };

    static const int scaleRootGlobalTrack;
    static const int scaleRootChordTrack;

    //==============================================================================
    PatternGenerator (Clip&, juce::ValueTree);
    ~PatternGenerator() override;

    //==============================================================================
    BeatDuration getMinimumChordLength() const;
    BeatDuration getMaximumChordLength() const;

    void validateChordLengths();

    //==============================================================================
    struct ProgressionItem
    {
        ProgressionItem (PatternGenerator&, const juce::ValueTree&, bool temporary = false);
        ~ProgressionItem() noexcept;

        bool operator== (const ProgressionItem&) const noexcept;
        bool isValid() { return chordName.get().isNotEmpty(); }

        PatternGenerator& generator;
        juce::ValueTree state;

        juce::CachedValue<juce::String> chordName;  // Chord name is simplified, use getChordName() to get unsimplified
        juce::CachedValue<juce::String> pitches;    // A comma seperated list of pitches used for custom chords, otherwise empty
        juce::CachedValue<BeatDuration> lengthInBeats;
        juce::CachedValue<int> octave, inversion;

        void setChordName (juce::String chord);
        void setChordName (juce::String chord, juce::String pitches);

        juce::String getChordName() const;

        void setRoot (int root);
        void setChord (int root, Chord::ChordType);

        bool isRomanNumeral() const;                       // Is this chord stored in it's roman numberal format

        Chord getChord (const Scale&) const;
        juce::String getChordSymbol();              // Gets the chords name as Cmaj, Dmin, etc based on key

        int getRootNote (int key, const Scale& scale);

    private:
        JUCE_LEAK_DETECTOR (ProgressionItem)
    };

    //==============================================================================

    juce::StringArray getPossibleTriadNames() const;
    juce::StringArray getPossibleSeventhNames() const;

    int getChordProgressionLength() const;
    const juce::Array<ProgressionItem*>& getChordProgression() const noexcept;
    void setChordProgression (juce::ValueTree v);

    juce::StringArray getChordProgressionChordNames (bool simplified) const;

    /** Sets a chord progression using chord roman numerals.
        Be careful, this sets everything else to default. */
    void setChordProgressionFromChordNames (juce::StringArray progression);

    void removeIndexFromProgression (int idx);
    void removeRangeFromProgression (int start, int end);
    void clearProgression();
    void insertChordIntoProgression (int idx, juce::String chordName);
    void insertChordIntoProgression (int idx, juce::String chordName, juce::String pitches);
    void moveChordInProgression (int srcIdx, int dstIdx);
    void duplicateChordInProgression (int idx);

    void playGuideChord (int idx) const;

    //==============================================================================
    enum NoteType
    {
        ChordInKeyNote,
        ChordNotInKeyNote,
        InKeyNote,
        NotInKeyNote
    };

    NoteType getTypeForNote (const MidiClip&, const MidiNote&);

    //==============================================================================
    juce::String formatChordName (juce::String simplifiedChordName) const;
    juce::StringArray getArpStyles();

    Clip& clip;
    juce::ValueTree state;
    juce::CachedValue<Mode> mode;
    juce::CachedValue<Scale::ScaleType> scaleType;
    juce::CachedValue<bool> autoUpdate, arpUpDown, arpPlayRoot, allNotes, octaveUp, octaveDown, spread;
    juce::CachedValue<float> arpPatternLength, velocity, gate;
    juce::CachedValue<BeatDuration> melodyNoteLength;
    juce::CachedValue<juce::String> arpStyle;
    juce::CachedValue<int> scaleRoot, arpSteps, octave;
    juce::CachedValue<juce::int64> patternHash;

    juce::ValueTree getChordPattern();
    juce::ValueTree getBassPattern();

    void setChordPattern (juce::ValueTree pattern);
    void setBassPattern (juce::ValueTree pattern);

    void generatePattern();

    Scale getScaleAtBeat (BeatPosition) const;
    int getNoteAtBeat (BeatPosition) const;

    bool getAutoUpdate();
    void setAutoUpdate (bool on);
    void refreshPatternIfNeeded();

    void editFinishedLoading();

    BeatDuration getFlattenedChordProgression (juce::OwnedArray<ProgressionItem>& progression, bool globalTime = false);

private:
    const int maxChords = 64;

    struct ProgressionList;
    std::unique_ptr<ProgressionList> progressionList;

    void valueTreeChanged() override {}
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    ChordClip* getChordClipAt (TimePosition) const;

    MidiNote* addNote (MidiList& sequence, int pitch, BeatPosition startBeat, BeatDuration lengthInBeats,
                       int velocity, int colourIndex, juce::UndoManager*);

    void clearPattern();
    void generateArpPattern();
    void generateChordPattern();
    void generateBassPattern();
    void generateMelodyPattern();

    void updateHash();
    void clearHash();
    HashCode hashNotes (MidiList&, int version);

    MidiClip* getMidiClip() const;

    //==============================================================================
    std::unique_ptr<juce::Timer> editLoadedCallback;

    //==============================================================================
    struct AutoUpdateManager;
    std::unique_ptr<AutoUpdateManager> autoUpdateManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternGenerator)
};

//==============================================================================
struct KeyResult
{
    KeyResult() = default;
    KeyResult (double r_, int k_, Scale::ScaleType s_) : r (r_), key (k_), scale (s_) {}

    double r = 0; // correlation coefficient
    int key = 0;  // midi note = 0 - 11
    Scale::ScaleType scale = Scale::major; // major or minor

    bool operator< (const KeyResult& other) const { return r < other.r; }
};

juce::Array<KeyResult> determineKeyOfNotes (const juce::Array<MidiNote*>& notes);

}} // namespace tracktion { inline namespace engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion::engine::Chord::ChordType>
    {
        static tracktion::engine::Chord::ChordType fromVar (const var& v)   { return (tracktion::engine::Chord::ChordType) static_cast<int> (v); }
        static var toVar (tracktion::engine::Chord::ChordType v)            { return static_cast<int> (v); }
    };


    template <>
    struct VariantConverter<tracktion::engine::Scale::ScaleType>
    {
        static tracktion::engine::Scale::ScaleType fromVar (const var& v)   { return (tracktion::engine::Scale::ScaleType) static_cast<int> (v); }
        static var toVar (tracktion::engine::Scale::ScaleType v)            { return static_cast<int> (v); }
    };

    template <>
    struct VariantConverter<tracktion::engine::PatternGenerator::Mode>
    {
        static tracktion::engine::PatternGenerator::Mode fromVar (const var& v)   { return (tracktion::engine::PatternGenerator::Mode) static_cast<int> (v); }
        static var toVar (tracktion::engine::PatternGenerator::Mode v)            { return static_cast<int> (v); }
    };
}
