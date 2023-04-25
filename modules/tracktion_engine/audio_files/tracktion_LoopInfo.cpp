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

LoopInfo::LoopInfo (Engine& e)
    : engine (e), state (IDs::LOOPINFO)
{
    initialiseMissingProps();
}

LoopInfo::LoopInfo (Engine& e, const juce::File& f)
    : engine (e), state (IDs::LOOPINFO)
{
    auto& formatManager = engine.getAudioFileFormatManager();

    if (auto af = formatManager.getFormatFromFileName (f))
    {
        if (auto fin = f.createInputStream())
        {
            const std::unique_ptr<juce::AudioFormatReader> afr (af->createReaderFor (fin.release(), true));
            init (afr.get(), af);
        }
    }
}

LoopInfo::LoopInfo (Engine& e, const juce::AudioFormatReader* afr, const juce::AudioFormat* af)
    : engine (e), state (IDs::LOOPINFO)
{
    init (afr, af);
}

LoopInfo::LoopInfo (Engine& e, const juce::ValueTree& v, juce::UndoManager* u)
    : engine (e), state (v), um (u), maintainParent (v.getParent().isValid())
{
    initialiseMissingProps();
}

LoopInfo::LoopInfo (const LoopInfo& other)
    : engine (other.engine)
{
    *this = other;
}

LoopInfo& LoopInfo::operator= (const LoopInfo& o)
{
    return copyFrom (o.state);
}

//==============================================================================
double LoopInfo::getBpm (const AudioFileInfo& wi) const
{
    return getBeatsPerSecond (wi) * 60.0;
}

void LoopInfo::setBpm (double newBpm, double currentBpm)
{
    if (newBpm != currentBpm)
    {
        const double ratio = newBpm / currentBpm;
        setNumBeats (juce::jmax (1.0, getNumBeats() * ratio));
    }
}

void LoopInfo::setBpm (double newBpm, const AudioFileInfo& wi)
{
    // this is a bit of a round about way of calculating the number of beats which
    // will in turn update the BPM but should work until we determine a proper fix

    jassert (wi.sampleRate != 0.0);

    if (wi.sampleRate == 0.0)
        return;

    if (newBpm < 30.0 || newBpm > 1000.0)
        return;

    const double currentBpm = getBpm (wi);
    setBpm (newBpm, currentBpm);
}

double LoopInfo::getBeatsPerSecond (const AudioFileInfo& wi) const
{
    CRASH_TRACER

    if (wi.sampleRate == 0.0)
        return 2.0;

    auto in  = getInMarker();
    auto out = getOutMarker();

    if (out == -1 || out > wi.lengthInSamples)
        out = wi.lengthInSamples;

    if (out <= 0)
        return 2.0;

    auto length = (out - in) / wi.sampleRate;

    if (length <= 0)
        return 2.0;

    return getNumBeats() / length;
}

//==============================================================================
int LoopInfo::getDenominator() const              { return getProp<int> (IDs::denominator); }
int LoopInfo::getNumerator() const                { return getProp<int> (IDs::numerator); }
void LoopInfo::setDenominator (int dem)           { setProp (IDs::denominator, dem); }
void LoopInfo::setNumerator (int num)             { setProp (IDs::numerator, num); }
double LoopInfo::getNumBeats() const              { return getProp<double> (IDs::numBeats); }
void LoopInfo::setNumBeats (double b)             { setProp (IDs::numBeats, b); }

//==============================================================================
bool LoopInfo::isLoopable() const                  { const juce::ScopedLock sl (lock); return ! isOneShot() && getNumBeats() > 0.0 && getDenominator() > 0 && getNumerator() > 0; }
bool LoopInfo::isOneShot() const                   { return getProp<bool> (IDs::oneShot); }

//==============================================================================
int LoopInfo::getRootNote() const                  { return getProp<int> (IDs::rootNote); }
void LoopInfo::setRootNote (int note)              { setProp<int> (IDs::rootNote, note); }

//==============================================================================
SampleCount LoopInfo::getInMarker() const          { return getProp<juce::int64> (IDs::inMarker); }
SampleCount LoopInfo::getOutMarker() const         { return getProp<juce::int64> (IDs::outMarker); }

void LoopInfo::setInMarker (SampleCount in)        { setProp<juce::int64> (IDs::inMarker, in); }
void LoopInfo::setOutMarker (SampleCount out)      { setProp<juce::int64> (IDs::outMarker, out); }

//==============================================================================
int LoopInfo::getNumLoopPoints() const
{
    const juce::ScopedLock sl (lock);
    return getLoopPoints().getNumChildren();
}

LoopInfo::LoopPoint LoopInfo::getLoopPoint (int idx) const
{
    const juce::ScopedLock sl (lock);
    auto lp = getLoopPoints().getChild (idx);

    if (lp.isValid())
        return { static_cast<juce::int64> (lp.getProperty (IDs::value)),
                 (LoopPointType) static_cast<int> (lp.getProperty (IDs::type)) };

    return {};
}

void LoopInfo::addLoopPoint (SampleCount pos, LoopPointType type)
{
    const juce::ScopedLock sl (lock);
    duplicateIfShared();

    auto t = createValueTree (IDs::LOOPPOINT,
                              IDs::value, (juce::int64) pos,
                              IDs::type, (int) type);

    getOrCreateLoopPoints().addChild (t, -1, nullptr);
}

void LoopInfo::changeLoopPoint (int idx, SampleCount pos, LoopPointType type)
{
    const juce::ScopedLock sl (lock);
    duplicateIfShared();

    auto lp = getLoopPoints().getChild (idx);
    lp.setProperty (IDs::value, (juce::int64) pos, um);
    lp.setProperty (IDs::type, (int) type, um);
}

void LoopInfo::deleteLoopPoint (int idx)
{
    const juce::ScopedLock sl (lock);
    duplicateIfShared();
    getLoopPoints().removeChild (idx, um);
    removeChildIfEmpty (IDs::LOOPPOINTS);
}

void LoopInfo::clearLoopPoints()
{
    const juce::ScopedLock sl (lock);
    duplicateIfShared();
    state.removeChild (getLoopPoints(), um);
}

void LoopInfo::clearLoopPoints (LoopPointType type)
{
    const juce::ScopedLock sl (lock);
    auto lps = getLoopPoints();

    for (int i = lps.getNumChildren(); --i >= 0;)
        if (((LoopPointType) int (lps.getChild (i).getProperty (IDs::type))) == type)
            lps.removeChild (i, nullptr);
}

int LoopInfo::getNumTags() const                { const juce::ScopedLock sl (lock); return getTags().getNumChildren(); }
void LoopInfo::clearTags()                      { const juce::ScopedLock sl (lock); duplicateIfShared(); state.removeChild (getTags(), um); }
juce::String LoopInfo::getTag (int idx) const   { const juce::ScopedLock sl (lock); return getTags().getChild (idx).getProperty (IDs::name).toString(); }

void LoopInfo::addTag (const juce::String& tag)
{
    const juce::ScopedLock sl (lock);
    duplicateIfShared();

    juce::ValueTree t (IDs::TAG);
    t.setProperty (IDs::name, tag, um);
    getOrCreateTags().addChild (t, -1, um);
}

void LoopInfo::addTags (const juce::StringArray& tags)
{
    for (const auto& t : tags)
        addTag (t);
}

void LoopInfo::initialiseMissingProps()
{
    const juce::ScopedLock sl (lock);
    setPropertyIfMissing (state, IDs::numBeats, 0.0, um);
    setPropertyIfMissing (state, IDs::denominator, 0, um);
    setPropertyIfMissing (state, IDs::numerator, 0, um);
    setPropertyIfMissing (state, IDs::oneShot, 0, um);
    setPropertyIfMissing (state, IDs::bpm, 0, um);
    setPropertyIfMissing (state, IDs::rootNote, -1, um);
    setPropertyIfMissing (state, IDs::inMarker, 0, um);
    setPropertyIfMissing (state, IDs::outMarker, -1, um);
}

LoopInfo& LoopInfo::copyFrom (const juce::ValueTree& o)
{
    const juce::ScopedLock sl (lock);
    copyValueTree (state, o, um);
    return *this;
}

void LoopInfo::removeChildIfEmpty (const juce::Identifier& i)
{
    auto v = state.getChildWithName (i);

    if (v.getNumChildren() == 0)
        state.removeChild (v, um);
}

juce::ValueTree LoopInfo::getLoopPoints() const       { const juce::ScopedLock sl (lock); return state.getChildWithName (IDs::LOOPPOINTS); }
juce::ValueTree LoopInfo::getOrCreateLoopPoints()     { const juce::ScopedLock sl (lock); return state.getOrCreateChildWithName (IDs::LOOPPOINTS, um); }

juce::ValueTree LoopInfo::getTags() const             { const juce::ScopedLock sl (lock); return state.getChildWithName (IDs::TAGS); }
juce::ValueTree LoopInfo::getOrCreateTags()           { const juce::ScopedLock sl (lock); return state.getOrCreateChildWithName (IDs::TAGS, um); }

void LoopInfo::duplicateIfShared()
{
    const juce::ScopedLock sl (lock);

    if (state.getReferenceCount() <= 1)
        return;

    if (maintainParent)
    {
        auto parent = state.getParent();
        int index = -1;

        if (parent.isValid())
        {
            index = parent.indexOf (state);
            parent.removeChild (index, um);
        }

        state = state.createCopy();

        if (parent.isValid())
            parent.addChild (state, index, um);
    }
    else
    {
        state = state.createCopy();
    }
}

static bool isSameFormat (const juce::AudioFormat* af1, const juce::AudioFormat* af2)
{
    return af1 == af2 || (af1 != nullptr && af2 != nullptr
                           && af1->getFormatName() == af2->getFormatName());
}

void LoopInfo::init (const juce::AudioFormatReader* afr, const juce::AudioFormat* af)
{
    if (afr == nullptr || af == nullptr)
        return;

    auto& formatManager = engine.getAudioFileFormatManager();
    const juce::ScopedLock sl (lock);

   #if TRACKTION_ENABLE_REX
    if (isSameFormat (af, formatManager.getRexFormat()))
    {
        setDenominator (afr->metadataValues[RexAudioFormat::rexDenominator].getIntValue());
        setNumerator (afr->metadataValues[RexAudioFormat::rexDenominator].getIntValue());
        const double bpm = afr->metadataValues[RexAudioFormat::rexTempo].getDoubleValue();
        setProp (IDs::bpm, bpm);
        setProp (IDs::numBeats, bpm * ((afr->lengthInSamples / afr->sampleRate) / 60.0));

        juce::StringArray beatPoints;
        beatPoints.addTokens (afr->metadataValues[RexAudioFormat::rexBeatPoints], ";", {});
        beatPoints.removeEmptyStrings();

        for (int i = 0; i < beatPoints.size(); ++i)
            addLoopPoint (beatPoints[i].getIntValue(), LoopInfo::LoopPointType::manual);
    }
    else
   #endif
    if (isSameFormat (af, formatManager.getAiffFormat()))
    {
        const int rootNote = afr->metadataValues[juce::AiffAudioFormat::appleRootSet] == "1"
                                ? afr->metadataValues[juce::AiffAudioFormat::appleRootNote].getIntValue() : -1;

        setRootNote (rootNote);
        setProp (IDs::oneShot, afr->metadataValues[juce::AiffAudioFormat::appleOneShot] == "1");
        setDenominator (afr->metadataValues[juce::AiffAudioFormat::appleDenominator].getIntValue());
        setNumerator (afr->metadataValues[juce::AiffAudioFormat::appleNumerator].getIntValue());
        const double numBeats = afr->metadataValues[juce::AiffAudioFormat::appleBeats].getDoubleValue();
        setProp (IDs::numBeats, numBeats);
        setProp (IDs::bpm, (numBeats * 60.0) / (afr->lengthInSamples / afr->sampleRate));

        juce::StringArray t;
        t.addTokens (afr->metadataValues[juce::AiffAudioFormat::appleTag], ";", {});
        t.add (afr->metadataValues[juce::AiffAudioFormat::appleKey]);
        t.removeEmptyStrings();

        for (int i = 0; i < t.size(); ++i)
            addTag (t[i]);
    }
    else if (isSameFormat (af, formatManager.getWavFormat()))
    {
        auto s = afr->metadataValues[juce::WavAudioFormat::tracktionLoopInfo];

        if (s.isNotEmpty())
        {
            if (auto n = juce::parseXML (s))
                copyFrom (juce::ValueTree::fromXml (*n));
        }
        else
        {
            const int rootNote = afr->metadataValues[juce::WavAudioFormat::acidRootSet] == "1"
                                    ? afr->metadataValues[juce::WavAudioFormat::acidRootNote].getIntValue() : -1;
            const double numBeats = afr->metadataValues[juce::WavAudioFormat::acidBeats].getDoubleValue();

            setRootNote (rootNote);
            setNumBeats (numBeats);
            setProp (IDs::oneShot, afr->metadataValues[juce::WavAudioFormat::acidOneShot] == "1");
            setDenominator (afr->metadataValues[juce::WavAudioFormat::acidDenominator].getIntValue());
            setNumerator (afr->metadataValues[juce::WavAudioFormat::acidNumerator].getIntValue());
            setNumBeats (afr->metadataValues[juce::WavAudioFormat::acidBeats].getDoubleValue());
            setProp (IDs::bpm, (numBeats * 60.0) / (afr->lengthInSamples / afr->sampleRate));
        }
    }
    else if (isSameFormat (af, formatManager.getNativeAudioFormat()))
    {
        juce::String s = afr->metadataValues[juce::WavAudioFormat::tracktionLoopInfo];

        if (s.isNotEmpty())
        {
            if (auto n = juce::parseXML (s))
                copyFrom (juce::ValueTree::fromXml (*n));
        }
        else
        {
            const juce::String tempoString = afr->metadataValues["tempo"];
            auto bpm = tempoString.getDoubleValue();

            setProp (IDs::bpm, bpm);

            if (tempoString.isNotEmpty())
            {
                const double fileDuration = afr->lengthInSamples / afr->sampleRate;
                setNumBeats ((fileDuration / 60.0) * bpm);
            }

            const juce::String beatCount = afr->metadataValues["beat count"];

            if (beatCount.isNotEmpty() && bpm == 0)
            {
                const double fileDuration = afr->lengthInSamples / afr->sampleRate;
                const int beats = beatCount.getIntValue();

                setProp (IDs::bpm, beats / fileDuration / 60.0);
                setNumBeats (beats);
            }

            const juce::String timeSig (afr->metadataValues["time signature"]);

            if (timeSig.isNotEmpty())
            {
                setDenominator (timeSig.upToFirstOccurrenceOf ("/", false, false).getIntValue());
                setNumerator (timeSig.fromFirstOccurrenceOf ("/", false, false).getIntValue());
            }

            const juce::String keySig (afr->metadataValues["key signature"]);

            if (keySig.isNotEmpty())
            {
                bool sharpOrFlat = keySig[1] == '#' || keySig[1] == 'b';
                setRootNote (Pitch::getPitchFromString (engine, keySig.substring (0, sharpOrFlat ? 2 : 1)));
                addTag (keySig.getLastCharacter() == 'm' ? "minor" : "major");
            }
        }
    }
    else if (afr->metadataValues.containsKey (juce::WavAudioFormat::acidBeats)
              || afr->metadataValues.containsKey (juce::WavAudioFormat::acidNumerator))
    {
        const int rootNote = afr->metadataValues[juce::WavAudioFormat::acidRootSet] == "1"
                                ? afr->metadataValues[juce::WavAudioFormat::acidRootNote].getIntValue() : -1;
        const double numBeats = afr->metadataValues[juce::WavAudioFormat::acidBeats].getDoubleValue();

        setRootNote (rootNote);
        setNumBeats (numBeats);
        setProp (IDs::oneShot, afr->metadataValues[juce::WavAudioFormat::acidOneShot] == "1");
        setDenominator (afr->metadataValues[juce::WavAudioFormat::acidDenominator].getIntValue());
        setNumerator (afr->metadataValues[juce::WavAudioFormat::acidNumerator].getIntValue());
        setNumBeats (afr->metadataValues[juce::WavAudioFormat::acidBeats].getDoubleValue());
        setProp (IDs::bpm, (numBeats * 60.0) / (afr->lengthInSamples / afr->sampleRate));
    }

    initialiseMissingProps();
}

}} // namespace tracktion { inline namespace engine
