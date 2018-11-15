/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
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

    Chord (juce::Array<int> steps); // creates a custom chord with steps

    static juce::Array<ChordType> getAllChordType();

    ChordType getType() const   { return type; }
    bool isValid() const        { return type != invalidChord; }

    juce::String getName() const;
    juce::String getSymbol() const;
    juce::Array<int> getSteps() const;
    juce::Array<int> getSteps (int inversion) const;

private:
    ChordType type;
    juce::Array<int> steps;
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

    enum Intervals
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

    static juce::StringArray getIntervalNames();

    static juce::Array<ScaleType> getAllScaleTypes();
    static juce::StringArray getScaleStrings();
    static juce::String getNameForType (ScaleType type);
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
    ~PatternGenerator();

    //==============================================================================
    double getMinimumChordLength() const;
    double getMaximumChordLength() const;

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
        juce::CachedValue<double> lengthInBeats;
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
    juce::CachedValue<float> arpPatternLength, melodyNoteLength, velocity, gate;
    juce::CachedValue<juce::String> arpStyle;
    juce::CachedValue<int> scaleRoot, arpSteps, octave;
    juce::CachedValue<juce::int64> patternHash;

    juce::ValueTree getChordPattern();
    juce::ValueTree getBassPattern();

    void setChordPattern (juce::ValueTree pattern);
    void setBassPattern (juce::ValueTree pattern);

    void generatePattern();

    Scale getScaleAtBeat (double beat) const;
    int getNoteAtBeat (double beat) const;

    bool getAutoUpdate();
    void setAutoUpdate (bool on);
    void refreshPatternIfNeeded();

    void editFinishedLoading();

private:
    const int maxChords = 64;

    struct ProgressionList;
    std::unique_ptr<ProgressionList> progressionList;

    void valueTreeChanged() override {};
    void valueTreePropertyChanged (juce::ValueTree& p, const juce::Identifier& c) override;

    ChordClip* getChordClipAt (double beat) const;

    double getFlattenedChordProgression (juce::OwnedArray<ProgressionItem>& progression);
    MidiNote* addNote (MidiList& sequence, int pitch, double startBeat, double lengthInBeats,
                       int velocity, int colourIndex, juce::UndoManager*);

    void clearPattern();
    void generateArpPattern();
    void generateChordPattern();
    void generateBassPattern();
    void generateMelodyPattern();

    void updateHash();
    void clearHash();
    juce::int64 hashNotes (MidiList& sequence, int version);

    MidiClip* getMidiClip() const;

    //==============================================================================
    std::unique_ptr<juce::Timer> editLoadedCallback;

    //==============================================================================
    struct AutoUpdateManager : private ValueTreeAllEventListener,
                               public juce::AsyncUpdater
    {
        AutoUpdateManager (PatternGenerator& owner);
        ~AutoUpdateManager();

        void valueTreeChanged() override {};
        void valueTreePropertyChanged (juce::ValueTree& p, const juce::Identifier& c) override;

        void handleAsyncUpdate() override;

        PatternGenerator& owner;
    };

    std::unique_ptr<AutoUpdateManager> autoUpdateManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternGenerator)
};

//==============================================================================
struct KeyResult
{
    KeyResult() {}
    KeyResult (double r_, int k_, Scale::ScaleType s_) : r (r_), key (k_), scale (s_) {}

    double r = 0; // correlation coefficient
    int key = 0;  // midi note = 0 - 11
    Scale::ScaleType scale; // major or minor

    bool operator< (const KeyResult& other) const { return r < other.r; }
};

juce::Array<KeyResult> determineKeyOfNotes (const juce::Array<MidiNote*>& notes);

} // namespace tracktion_engine

namespace juce
{
    template <>
    struct VariantConverter<tracktion_engine::Chord::ChordType>
    {
        static tracktion_engine::Chord::ChordType fromVar (const var& v)   { return (tracktion_engine::Chord::ChordType) static_cast<int> (v); }
        static var toVar (tracktion_engine::Chord::ChordType v)            { return static_cast<int> (v); }
    };


    template <>
    struct VariantConverter<tracktion_engine::Scale::ScaleType>
    {
        static tracktion_engine::Scale::ScaleType fromVar (const var& v)   { return (tracktion_engine::Scale::ScaleType) static_cast<int> (v); }
        static var toVar (tracktion_engine::Scale::ScaleType v)            { return static_cast<int> (v); }
    };

    template <>
    struct VariantConverter<tracktion_engine::PatternGenerator::Mode>
    {
        static tracktion_engine::PatternGenerator::Mode fromVar (const var& v)   { return (tracktion_engine::PatternGenerator::Mode) static_cast<int> (v); }
        static var toVar (tracktion_engine::PatternGenerator::Mode v)            { return static_cast<int> (v); }
    };
}
