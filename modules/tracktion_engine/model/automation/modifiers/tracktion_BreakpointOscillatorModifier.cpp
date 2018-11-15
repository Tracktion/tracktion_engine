/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace BezierHelpers
{
    template<typename Type>
    static Point<Type> getQuadraticControlPoint (Point<Type> start, Point<Type> end, Type curve)
    {
        static_assert (std::is_floating_point<Type>::value, "");
        jassert (curve >= -0.5 && curve <= 0.5);

        const Type x1 = start.x;
        const Type y1 = start.y;
        const Type x2 = end.x;
        const Type y2 = end.y;
        const Type c  = jlimit (-1.0f, 1.0f, curve * 2.0f);

        if (y2 > y1)
        {
            const Type run  = x2 - x1;
            const Type rise = y2 - y1;

            const Type xc = x1 + run / 2;
            const Type yc = y1 + rise / 2;

            const Type x = xc - run / 2 * -c;
            const Type y = yc + rise / 2 * -c;

            return { x, y };
        }

        const Type run  = x2 - x1;
        const Type rise = y1 - y2;

        const Type xc = x1 + run / 2;
        const Type yc = y2 + rise / 2;

        const Type x = xc - run / 2 * -c;
        const Type y = yc - rise / 2 * -c;

        return { x, y };
    }

    template<typename Type>
    static Type getQuadraticYFromX (Type x, Type x1, Type y1, Type xb, Type yb, Type x2, Type y2)
    {
        // test for straight lines and bail out
        if (x1 == x2 || y1 == y2)
            return y1;

        // ok, we have a bezier curve with one control point,
        // we know x, we need to find y

        // flip the bezier equation around so its an quadratic equation
        const Type a = x1 - 2 * xb + x2;
        const Type b = -2 * x1 + 2 * xb;
        const Type c = x1 - x;

        // solve for t, [0..1]
        Type t = 0;

        if (a == 0.0f)
        {
            t = -c / b;
        }
        else
        {
            t = (-b + std::sqrt (b * b - 4 * a * c)) / (2 * a);

            if (t < 0.0f || t > 1.0f)
                t = (-b - std::sqrt (b * b - 4 * a * c)) / (2 * a);
        }

        // find y using the t we just found
        const Type y = (std::pow (1 - t, 2.0f) * y1) + 2 * t * (1 - t) * yb + std::pow (t, 2.0f) * y2;
        return (Type) y;
    }

    template<typename Type>
    static Type getQuadraticYFromX (Type x, Point<Type> startPoint, Point<Type> controlPoint, Point<Type> endPoint)
    {
        return getQuadraticYFromX (x, startPoint.x, startPoint.y, controlPoint.x, controlPoint.y, endPoint.x, endPoint.y);
    }
}

//==============================================================================
struct BreakpointOscillatorModifier::BreakpointOscillatorModifierTimer    : public ModifierTimer
{
    BreakpointOscillatorModifierTimer (BreakpointOscillatorModifier& bom)
        : modifier (bom), tempoSequence (bom.edit.tempoSequence)
    {
    }

    void updateStreamTime (PlayHead& ph, EditTimeRange streamTime, int /*numSamples*/) override
    {
        using namespace ModifierCommon;
        auto editTime = (float) ph.streamTimeToSourceTime (streamTime.getStart());
        modifier.updateParameterStreams (editTime);

        const auto syncTypeThisBlock = getTypedParamValue<SyncType> (*modifier.syncTypeParam);
        const auto rateTypeThisBlock = getTypedParamValue<RateType> (*modifier.rateTypeParam);
        const auto rateThisBlock = modifier.rateParam->getCurrentValue();

        if (rateTypeThisBlock == hertz)
        {
            auto durationPerPattern = 1.0f / rateThisBlock;
            ramp.setDuration (durationPerPattern);

            if (syncTypeThisBlock == transport)
                ramp.setPosition (std::fmod (editTime, durationPerPattern));

            modifier.setPhase (ramp.getProportion());

            // Move the ramp on for the next block
            ramp.process ((float) streamTime.getLength());
        }
        else
        {
            tempoSequence.setTime (editTime);
            auto currentTempo = tempoSequence.getCurrentTempo();
            auto proportionOfBar = ModifierCommon::getBarFraction (rateTypeThisBlock);

            if (syncTypeThisBlock == transport)
            {
                auto editTimeInBeats = (float) (currentTempo.startBeatInEdit + (editTime - currentTempo.startTime) * currentTempo.beatsPerSecond);
                auto bars = (editTimeInBeats / currentTempo.numerator) * rateThisBlock;

                if (rateTypeThisBlock >= fourBars && rateTypeThisBlock <= sixtyFourthD)
                {
                    auto virtualBars = jmax (0.0, bars / proportionOfBar);
                    modifier.setPhase ((float) std::fmod (virtualBars, 1.0f));
                }
            }
            else
            {
                auto bpm = (currentTempo.bpm * rateThisBlock) / proportionOfBar;
                auto secondsPerBeat = 60.0 / bpm;
                auto secondsPerStep = static_cast<float> (secondsPerBeat * currentTempo.numerator);
                auto secondsPerPattern = secondsPerStep;
                ramp.setDuration (secondsPerPattern);

                modifier.setPhase (ramp.getProportion());

                // Move the ramp on for the next block
                ramp.process ((float) streamTime.getLength());
            }
        }
    }

    void resync (EditTimeRange streamTime)
    {
        if (roundToInt (modifier.syncTypeParam->getCurrentValue()) == ModifierCommon::note)
        {
            ramp.setPosition (0.0f);
            modifier.setPhase (0.0f);

            // Move the ramp on for the next block
            ramp.process ((float) streamTime.getLength());
        }
    }

    BreakpointOscillatorModifier& modifier;
    Ramp ramp;
    TempoSequencePosition tempoSequence;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BreakpointOscillatorModifierTimer)
};

//==============================================================================
struct BreakpointOscillatorModifier::ModifierAudioNode    : public SingleInputAudioNode
{
    ModifierAudioNode (AudioNode* input, BreakpointOscillatorModifier& rm)
        : SingleInputAudioNode (input),
          modifier (&rm)
    {
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        SingleInputAudioNode::renderOver (rc);

        if (rc.bufferForMidiMessages != nullptr)
        {
            for (auto& m : *rc.bufferForMidiMessages)
            {
                if (m.isNoteOn())
                {
                    modifier->modifierTimer->resync (rc.streamTime);
                    break;
                }
            }
        }
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        callRenderOver (rc);
    }

    BreakpointOscillatorModifier::Ptr modifier;
};

//==============================================================================
BreakpointOscillatorModifier::BreakpointOscillatorModifier (Edit& e, const ValueTree& v)
    : Modifier (e, v)
{
    auto um = &edit.getUndoManager();

    numActivePoints.referTo (state, IDs::numActivePoints, um, 4);
    syncType.referTo (state, IDs::syncType, um, float (ModifierCommon::free));
    rate.referTo (state, IDs::rate, um, 1.0f);
    rateType.referTo (state, IDs::rateType, um, float (ModifierCommon::bar));
    depth.referTo (state, IDs::depth, um, 1.0f);
    bipolar.referTo (state, IDs::bipolar, um);
    stageZeroValue.referTo (state, IDs::stageZeroValue, um, 0.0f);
    stageOneValue.referTo (state, IDs::stageOneValue, um, 1.0f);
    stageOneTime.referTo (state, IDs::stageOneTime, um, 0.25f);
    stageOneCurve.referTo (state, IDs::stageOneCurve, um, 0.0f);
    stageTwoValue.referTo (state, IDs::stageTwoValue, um, 0.5f);
    stageTwoTime.referTo (state, IDs::stageTwoTime, um, 0.5f);
    stageTwoCurve.referTo (state, IDs::stageTwoCurve, um, 0.0f);
    stageThreeValue.referTo (state, IDs::stageThreeValue, um, 0.5f);
    stageThreeTime.referTo (state, IDs::stageThreeTime, um, 0.75f);
    stageThreeCurve.referTo (state, IDs::stageThreeCurve, um, 0.0f);
    stageFourValue.referTo (state, IDs::stageFourValue, um, 0.0f);
    stageFourTime.referTo (state, IDs::stageFourTime, um, 1.0f);
    stageFourCurve.referTo (state, IDs::stageFourCurve, um, 0.0f);

    auto addDiscreteParam = [this] (const String& paramID, const String& name, Range<float> valueRange, CachedValue<float>& val,
                                    const StringArray& labels) -> AutomatableParameter*
    {
        auto* p = new DiscreteLabelledParameter (paramID, name, *this, valueRange, labels.size(), labels);
        addAutomatableParameter (p);
        p->attachToCurrentValue (val);

        return p;
    };

    auto addParam = [this] (const String& paramID, const String& name, NormalisableRange<float> valueRange, float centreVal, CachedValue<float>& val, const String& suffix) -> AutomatableParameter*
    {
        valueRange.setSkewForCentre (centreVal);
        auto* p = new SuffixedParameter (paramID, name, *this, valueRange, suffix);
        addAutomatableParameter (p);
        p->attachToCurrentValue (val);

        return p;
    };

    using namespace ModifierCommon;
    numActivePointsParam    = addParam ("numActivePoints",  TRANS("Num points"),{ 1.0f, 4.0f, 1.0f }, 2.5f,                             numActivePoints,    {});
    syncTypeParam           = addDiscreteParam ("syncType", TRANS("Sync type"), { 0.0f, (float)  getSyncTypeChoices().size() - 1 },     syncType,           getSyncTypeChoices());
    rateParam               = addParam ("rate",             TRANS("Rate"),      { 0.01f, 50.0f }, 1.0f,                                 rate,               {});
    rateTypeParam           = addDiscreteParam ("rateType", TRANS("Rate Type"), { 0.0f, (float) getRateTypeChoices().size() - 1 },      rateType,           getRateTypeChoices());
    depthParam              = addParam ("depth",            TRANS("Depth"),     { 0.0f, 1.0f }, 0.5f,                                   depth,              {});
    bipolarParam            = addDiscreteParam ("biopolar", TRANS("Bipoloar"),  { 0.0f, 1.0f },                                         bipolar,            { NEEDS_TRANS("Uni-polar"), NEEDS_TRANS("Bi-polar") });

    stageZeroValueParam     = addParam ("stageZeroValue",   TRANS("Stage zero value"),   { 0.0f, 1.0f }, 0.5f,      stageZeroValue,  {});
    stageOneValueParam      = addParam ("stageOneValue",    TRANS("Stage one value"),    { 0.0f, 1.0f }, 0.5f,      stageOneValue,   {});
    stageOneTimeParam       = addParam ("stageOneTime",     TRANS("Stage one time"),     { 0.0f, 1.0f }, 0.5f,      stageOneTime,    {});
    stageOneCurveParam      = addParam ("stageOneCurve",    TRANS("Stage one curve"),    { -0.5f, 0.5f }, 0.0f,     stageOneCurve,   {});
    stageTwoValueParam      = addParam ("stageTwoValue",    TRANS("Stage two value"),    { 0.0f, 1.0f }, 0.5f,      stageTwoValue,   {});
    stageTwoTimeParam       = addParam ("stageTwoTime",     TRANS("Stage two time"),     { 0.0f, 1.0f }, 0.5f,      stageTwoTime,    {});
    stageTwoCurveParam      = addParam ("stageTwoCurve",    TRANS("Stage two curve"),    { -0.5f, 0.5f }, 0.0f,     stageTwoCurve,   {});
    stageThreeValueParam    = addParam ("stageThreeValue",  TRANS("Stage three value"),  { 0.0f, 1.0f }, 0.5f,      stageThreeValue, {});
    stageThreeTimeParam     = addParam ("stageThreeTime",   TRANS("Stage three time"),   { 0.0f, 1.0f }, 0.5f,      stageThreeTime,  {});
    stageThreeCurveParam    = addParam ("stageThreeCurve",  TRANS("Stage three curve"),  { -0.5f, 0.5f }, 0.0f,     stageThreeCurve, {});
    stageFourValueParam     = addParam ("stageFourValue",   TRANS("Stage four value"),   { 0.0f, 1.0f }, 0.5f,      stageFourValue,  {});
    stageFourTimeParam      = addParam ("stageFourTime",    TRANS("Stage four time"),    { 0.0f, 1.0f }, 0.5f,      stageFourTime,   {});
    stageFourCurveParam     = addParam ("stageFourCurve",   TRANS("Stage four curve"),   { -0.5f, 0.5f }, 0.0f,     stageFourCurve,  {});

    changedTimer.setCallback ([this]
                              {
                                  changedTimer.stopTimer();
                                  changed();
                              });

    state.addListener (this);
}

BreakpointOscillatorModifier::~BreakpointOscillatorModifier()
{
    state.removeListener (this);
    notifyListenersOfDeletion();

    edit.removeModifierTimer (*modifierTimer);

    for (auto p : getAutomatableParameters())
        p->detachFromCurrentValue();

    deleteAutomatableParameters();
}

void BreakpointOscillatorModifier::initialise()
{
    // Do this here in case the audio code starts using the parameters before the constructor has finished
    modifierTimer = std::make_unique<BreakpointOscillatorModifierTimer> (*this);
    edit.addModifierTimer (*modifierTimer);

    restoreChangedParametersFromState();
}

//==============================================================================
float BreakpointOscillatorModifier::getCurrentValue()
{
    return currentValue.load (std::memory_order_acquire);
}

float BreakpointOscillatorModifier::getCurrentEnvelopeValue() const noexcept
{
    return currentEnvelopeValue.load (std::memory_order_acquire);
}

float BreakpointOscillatorModifier::getCurrentPhase() const noexcept
{
    return currentPhase.load (std::memory_order_acquire);
}

float BreakpointOscillatorModifier::getTotalTime() const
{
    return 1.0f;
}

AutomatableParameter::ModifierAssignment* BreakpointOscillatorModifier::createAssignment (const ValueTree& v)
{
    return new Assignment (v, *this);
}

AudioNode* BreakpointOscillatorModifier::createPreFXAudioNode (AudioNode* an)
{
    return new ModifierAudioNode (an, *this);
}

//==============================================================================
BreakpointOscillatorModifier::Assignment::Assignment (const juce::ValueTree& v, const BreakpointOscillatorModifier& bom)
    : AutomatableParameter::ModifierAssignment (bom.edit, v),
      breakpointOscillatorModifierID (bom.itemID)
{
}

bool BreakpointOscillatorModifier::Assignment::isForModifierSource (const ModifierSource& source) const
{
    if (auto* mod = dynamic_cast<const BreakpointOscillatorModifier*> (&source))
        return mod->itemID == breakpointOscillatorModifierID;

    return false;
}

BreakpointOscillatorModifier::Ptr BreakpointOscillatorModifier::Assignment::getModifier() const
{
    if (auto mod = findModifierTypeForID<BreakpointOscillatorModifier> (edit, breakpointOscillatorModifierID))
        return mod;

    return {};
}

//==============================================================================
BreakpointOscillatorModifier::Stage BreakpointOscillatorModifier::getStage (int index) const
{
    jassert (isPositiveAndNotGreaterThan (index, 4));
    switch (index)
    {
        case 0: return { stageZeroValueParam.get() };
        case 1: return { stageOneValueParam.get(),      stageOneTimeParam.get(),    stageOneCurveParam.get() };
        case 2: return { stageTwoValueParam.get(),      stageTwoTimeParam.get(),    stageTwoCurveParam.get() };
        case 3: return { stageThreeValueParam.get(),    stageThreeTimeParam.get(),  stageThreeCurveParam.get() };
        case 4: return { stageFourValueParam.get(),     stageFourTimeParam.get(),   stageFourCurveParam.get() };
        default: jassertfalse; return {};
    };
}

std::array<BreakpointOscillatorModifier::Section, 5> BreakpointOscillatorModifier::getAllSections() const
{
    auto getVal = [] (auto& p) -> float { return p->getCurrentValue(); };

    return {{
        { getVal (stageZeroValueParam),  0.0f,                            0.0f },
        { getVal (stageOneValueParam),   getVal (stageOneTimeParam),      getVal (stageOneCurveParam) },
        { getVal (stageTwoValueParam),   getVal (stageTwoTimeParam),      getVal (stageTwoCurveParam) },
        { getVal (stageThreeValueParam), getVal (stageThreeTimeParam),    getVal (stageThreeCurveParam) },
        { getVal (stageFourValueParam),  getVal (stageFourTimeParam),     getVal (stageFourCurveParam) }
    }};
}

std::pair<BreakpointOscillatorModifier::Section, BreakpointOscillatorModifier::Section> BreakpointOscillatorModifier::getSectionsSurrounding (float time) const
{
    const auto sections = getAllSections();
    const size_t numPoints = (size_t) getIntParamValue (*numActivePointsParam);
    jassert (numPoints < sections.size());

    for (size_t i = 1; i <= numPoints; ++i)
        if (sections[i].time > time)
            return std::make_pair (sections[i - 1], sections[i]);

    auto dummyEndSection = sections[numPoints];
    dummyEndSection.time = 1.0f;

    return std::make_pair (sections[numPoints], dummyEndSection);
}

namespace BreakpointInterpolation
{
    constexpr float linearInterpolate (float x1, float y1, float x2, float y2, float x)
    {
        return (y1 * (x2 - x) + y2 * (x - x1)) / (x2 - x1);
    }

    template<typename Type>
    static Type quadraticInterpolate (Type x, Point<Type> startPoint, Point<Type> endPoint, Type curve)
    {
        auto cp = BezierHelpers::getQuadraticControlPoint (startPoint, endPoint, curve);
        return BezierHelpers::getQuadraticYFromX (x, startPoint, cp, endPoint);
    }
}

void BreakpointOscillatorModifier::setPhase (float newPhase)
{
    // Calculate the total time
    // Find the time for that phase
    // Find the sections before and after that time
    // Skew the time based on the curve
    // Interpolate the value between those two
    // Multiply by depth
    // If bipolar, scale and shift

    using namespace BreakpointInterpolation;
    jassert (isPositiveAndBelow (newPhase, 1.0f));
    currentPhase.store (newPhase, std::memory_order_release);

    const float totalTime = getTotalTime();
    float timeForPhase = totalTime * newPhase;

    auto surroundingSections = getSectionsSurrounding (timeForPhase);
    auto& s1 = surroundingSections.first;
    auto& s2 = surroundingSections.second;
    const float curve = s2.curve;

    float newValue = 0.0f;

    if (curve == 0.0f)
        newValue = linearInterpolate (s1.time, s1.value, s2.time, s2.value, timeForPhase);
    else
        newValue = quadraticInterpolate (timeForPhase, { s1.time, s1.value }, { s2.time, s2.value }, curve);

    currentEnvelopeValue.store (newValue, std::memory_order_release);

    newValue = newValue * depthParam->getCurrentValue();

    if (getBoolParamValue (*bipolarParam))
        newValue = (newValue * 2.0f) - 1.0f;

    jassert (! std::isnan (newValue));
    currentValue.store (newValue, std::memory_order_release);
}

void BreakpointOscillatorModifier::valueTreeChanged()
{
    if (! changedTimer.isTimerRunning())
        changedTimer.startTimerHz (30);
}
