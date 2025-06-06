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

TrackInsertPoint::TrackInsertPoint (Track* parent, Track* preceding)
    : parentTrackID (parent != nullptr ? parent->itemID : EditItemID()),
      precedingTrackID (preceding != nullptr ? preceding->itemID : EditItemID())
{
}

TrackInsertPoint::TrackInsertPoint (EditItemID parent, EditItemID preceding)
    : parentTrackID (parent),
      precedingTrackID (preceding)
{
}

TrackInsertPoint::TrackInsertPoint (Track& t, bool insertBefore)
{
    juce::Array<Track*> siblingTracks;

    if (auto parent = t.getParentTrack())
    {
        parentTrackID = parent->itemID;
        siblingTracks = parent->getAllSubTracks (false);
    }
    else
    {
        siblingTracks = getTopLevelTracks (t.edit);
    }

    int index = siblingTracks.indexOf (&t);
    jassert (index >= 0);

    if (! insertBefore && index == 0 && parentTrackID.isValid())
        if (auto tr = siblingTracks[index])
            precedingTrackID = tr->itemID;

    if (index > 0)
        precedingTrackID = siblingTracks.getUnchecked (index - (insertBefore ? 1 : 0))->itemID;
}

TrackInsertPoint::TrackInsertPoint (const juce::ValueTree& v)
{
    {
        auto p = v.getParent();

        if (TrackList::isTrack (p))
            parentTrackID = EditItemID::fromProperty (p, IDs::id);
    }

    {
        auto p = v.getSibling (-1);

        while (p.isValid() && (! TrackList::isTrack (p)))
            p = p.getSibling (-1);

        precedingTrackID = EditItemID::fromProperty (p, IDs::id);
    }
}

TrackInsertPoint TrackInsertPoint::getEndOfTracks (Edit& e)
{
    return TrackInsertPoint (nullptr, getTopLevelTracks (e).getLast());
}

//==============================================================================
TrackList::TrackList (Edit& e, const juce::ValueTree& parentTree)
    : ValueTreeObjectList<Track> (parentTree), edit (e)
{
}

TrackList::~TrackList()
{
    cancelPendingUpdate();
    freeObjects();
}

void TrackList::initialise()
{
    rebuildObjects();
    rebuilding = false;
}

Track* TrackList::getTrackFor (const juce::ValueTree& v) const
{
    for (auto t : objects)
        if (t->state == v)
            return t;

    return {};
}

bool TrackList::visitAllRecursive (const std::function<bool(Track&)>& f) const
{
    for (auto t : objects)
    {
        if (! f (*t))
            return false;

        if (auto subList = t->getSubTrackList())
            if (! subList->visitAllRecursive (f))
                return false;
    }

    return true;
}

void TrackList::visitAllTopLevel (const std::function<bool(Track&)>& f) const
{
    for (auto t : objects)
        if (! f (*t))
            break;
}

void TrackList::visitAllTracks (const std::function<bool(Track&)>& f, bool recursive) const
{
    if (recursive)
        visitAllRecursive (f);
    else
        visitAllTopLevel (f);
}

bool TrackList::isMovableTrack (const juce::ValueTree& v) noexcept
{
    return v.hasType (IDs::TRACK) || v.hasType (IDs::FOLDERTRACK) || v.hasType (IDs::AUTOMATIONTRACK);
}

bool TrackList::isArrangerTrack (const juce::ValueTree& v) noexcept { return v.hasType (IDs::ARRANGERTRACK); }
bool TrackList::isChordTrack (const juce::ValueTree& v) noexcept    { return v.hasType (IDs::CHORDTRACK); }
bool TrackList::isMarkerTrack (const juce::ValueTree& v) noexcept   { return v.hasType (IDs::MARKERTRACK); }
bool TrackList::isTempoTrack (const juce::ValueTree& v) noexcept    { return v.hasType (IDs::TEMPOTRACK); }
bool TrackList::isMasterTrack (const juce::ValueTree& v) noexcept   { return v.hasType (IDs::MASTERTRACK); }

bool TrackList::isFixedTrack (const juce::ValueTree& v) noexcept
{
    return isMarkerTrack (v) || isTempoTrack (v) || isChordTrack (v) || isArrangerTrack (v) || isMasterTrack (v)
        || (v.hasType (IDs::AUTOMATIONTRACK) && isMasterTrack (v.getParent()));
}

bool TrackList::isTrack (const juce::ValueTree& v) noexcept         { return isMovableTrack (v) || isFixedTrack (v); }

bool TrackList::isTrack (const juce::Identifier& i) noexcept
{
    return i == IDs::TRACK || i == IDs::FOLDERTRACK || i == IDs::AUTOMATIONTRACK
        || i == IDs::MARKERTRACK || i == IDs::TEMPOTRACK || i == IDs::CHORDTRACK
        || i == IDs::ARRANGERTRACK || i == IDs::MASTERTRACK;
}

bool TrackList::hasAnySubTracks (const juce::ValueTree& v)
{
    for (int i = v.getNumChildren(); --i >= 0;)
        if (isMovableTrack (v.getChild (i)))
            return true;

    return false;
}

bool TrackList::isSuitableType (const juce::ValueTree& v) const
{
    return isTrack (v);
}

Track* TrackList::createNewObject (const juce::ValueTree& v)
{
    juce::ValueTree vt (v);
    Track::Ptr t;

    if (rebuilding)
    {
        // This logic avoids creating new tracks when they are sub-foldered
        if (! edit.isLoading())
            if (auto trk = dynamic_cast<Track*> (edit.trackCache.findItem (EditItemID::fromID (v))))
                t = trk;

        if (t == nullptr)
            t = edit.loadTrackFrom (vt);
    }
    else
    {
        if (auto trk = dynamic_cast<Track*> (edit.trackCache.findItem (EditItemID::fromID (v))))
            t = trk;
        else
            t = edit.createTrack (v);
    }

    if (t != nullptr)
    {
        t->incReferenceCount();
        edit.updateTrackStatusesAsync();
        return t.get();
    }

    jassertfalse;
    return {};
}

void TrackList::deleteObject (Track* t)
{
    jassert (t != nullptr);
    t->decReferenceCount();
    edit.updateTrackStatusesAsync();
}

void TrackList::newObjectAdded (Track* t)
{
    if (edit.isLoading())
        return;

    // Update parameters once track has been fully created
    {
        auto cursorPos = edit.transportControl->getPosition();

        for (auto ap : t->getAllAutomatableParams())
        {
            ap->updateStream();

            if (ap->isAutomationActive())
                ap->updateFromAutomationSources (cursorPos);
        }
    }

    triggerAsyncUpdate();
    t->refreshCurrentAutoParam();

    if (auto tl = t->getSubTrackList())
        tl->visitAllRecursive ([] (Track& track)
                               {
                                   track.refreshCurrentAutoParam();
                                   return true;
                               });
}

void TrackList::objectRemoved (Track*) {}

void TrackList::objectOrderChanged()
{
    edit.updateTrackStatusesAsync();
}

void TrackList::sortTracksByType (juce::ValueTree& editState, juce::UndoManager* um)
{
    struct TrackTypeSorter
    {
        static int getPriority (const juce::ValueTree& v) noexcept
        {
            if (isMovableTrack (v))     return 6;
            if (isMasterTrack (v))      return 5;
            if (isTempoTrack (v))       return 4;
            if (isMarkerTrack (v))      return 3;
            if (isChordTrack (v))       return 2;
            if (isArrangerTrack (v))    return 1;
            return 0;
        }

        static int compareElements (const juce::ValueTree& first, const juce::ValueTree& second) noexcept
        {
            return getPriority (first) - getPriority (second);
        }
    };

    TrackTypeSorter sorter;
    editState.sort (sorter, um, true);
}

void TrackList::handleAsyncUpdate()
{
    if (! edit.isLoading())
        sortTracksByType (edit.state, &edit.getUndoManager());
}

//==============================================================================
int countNumTracks (const juce::ValueTree& v)
{
    int total = 0;

    for (const auto& c : v)
    {
        if (! TrackList::isTrack (c.getType()))
            continue;

        ++total;
        total += countNumTracks (c);
    }

    return total;
}

//==============================================================================
TrackAutomationSection::ActiveParameters::ActiveParameters (AutomatableParameter& p)
    : param (p)
{
}


//==============================================================================
TrackAutomationSection::TrackAutomationSection (TrackItem& c)
    : position (c.getPosition().time),
      src (c.getTrack()),
      dst (c.getTrack())
{
}

void TrackAutomationSection::mergeIn (const TrackAutomationSection& other)
{
    position = position.getUnionWith (other.position);

    for (const auto& p : other.activeParameters)
        if (! containsParameter (p.param.get()))
            activeParameters.add (p);
}

bool TrackAutomationSection::containsParameter (AutomatableParameter* param) const
{
    for (auto&& p : activeParameters)
        if (p.param.get() == param)
            return true;

    return false;
}

bool TrackAutomationSection::overlaps (const TrackAutomationSection& other) const
{
    return src == other.src
        && dst == other.dst
        && position.expanded (TimeDuration::fromSeconds (0.0001)).overlaps (other.position);
}


//==============================================================================
static AutomationCurve* getDestCurve (Track& t, const AutomatableParameter::Ptr& p)
{
    if (p != nullptr)
    {
        if (auto plugin = p->getPlugin())
        {
            auto name = plugin->getName();

            for (auto f : t.getAllPlugins())
                if (f->getName() == name)
                    if (auto param = f->getAutomatableParameter (plugin->indexOfAutomatableParameter (p)))
                        return &param->getCurve();
        }
    }

    return {};
}

static bool mergeInto (const TrackAutomationSection& s,
                       juce::Array<TrackAutomationSection>& dst)
{
    for (auto& dstSeg : dst)
    {
        if (dstSeg.overlaps (s))
        {
            dstSeg.mergeIn (s);
            return true;
        }
    }

    return false;
}

static void mergeSections (const juce::Array<TrackAutomationSection>& src,
                           juce::Array<TrackAutomationSection>& dst)
{
    for (const auto& srcSeg : src)
        if (! mergeInto (srcSeg, dst))
            dst.add (srcSeg);
}

void moveAutomation (const juce::Array<TrackAutomationSection>& origSections, TimeDuration offset, bool copy)
{
    if (origSections.isEmpty())
        return;

    juce::Array<TrackAutomationSection> sections;
    mergeSections (origSections, sections);

    // find all the original curves
    for (auto&& section : sections)
    {
        for (auto element : section.src->getAllAutomatableEditItems())
        {
            for (int k = 0; k < element->getNumAutomatableParameters(); k++)
            {
                AutomatableParameter::Ptr param = element->getAutomatableParameter (k);

                if (param->getCurve().getNumPoints() > 0)
                {
                    TrackAutomationSection::ActiveParameters ap (*param);
                    ap.curve.setState (param->getCurve().state);
                    ap.curve.setParentState (param->getCurve().parentState);
                    ap.curve.setParameterID (param->getCurve().getParameterID());

                    section.activeParameters.add (ap);
                }
            }
        }

        for (auto& ap : section.activeParameters)
            ap.curve.state = ap.curve.state.createCopy();
    }

    // delete all the old curves
    if (! copy)
    {
        for (auto& section : sections)
        {
            auto sectionTime = section.position;

            for (auto&& activeParam : section.activeParameters)
            {
                auto param = activeParam.param;
                auto& curve = param->getCurve();
                auto* um = &param->getEdit().getUndoManager();
                constexpr auto tolerance = 0.0001_td;

                auto defaultValue = param->getCurrentBaseValue();
                auto startValue = curve.getValueAt (sectionTime.getStart() - tolerance, defaultValue);
                auto endValue   = curve.getValueAt (sectionTime.getEnd()   + tolerance, defaultValue);

                auto idx = curve.indexBefore (sectionTime.getEnd() + tolerance);
                auto endCurve = (idx == -1) ? 0.0f : curve.getPointCurve(idx);

                curve.removePointsInRegion (sectionTime.expanded (tolerance), um);

                if (std::abs (startValue - endValue) < 0.0001f)
                {
                    curve.addPoint (sectionTime.getStart(), startValue, 0.0f, um);
                    curve.addPoint (sectionTime.getEnd(), endValue, endCurve, um);
                }
                else if (startValue > endValue)
                {
                    curve.addPoint (sectionTime.getStart(), startValue, 0.0f, um);
                    curve.addPoint (sectionTime.getStart(), endValue, 0.0f, um);
                    curve.addPoint (sectionTime.getEnd(), endValue, endCurve, um);
                }
                else
                {
                    curve.addPoint (sectionTime.getStart(), startValue, 0.0f, um);
                    curve.addPoint (sectionTime.getEnd(), startValue, 0.0f, um);
                    curve.addPoint (sectionTime.getEnd(), endValue, endCurve, um);
                }

                curve.removeRedundantPoints (sectionTime.expanded (tolerance), um);
            }
        }
    }

    // recreate the curves
    for (auto& section : sections)
    {
        for (auto& activeParam : section.activeParameters)
        {
            auto sectionTime = section.position;

            if (auto dstCurve = (section.src == section.dst) ? &activeParam.param->getCurve()
                                                             : getDestCurve (*section.dst, activeParam.param))
            {
                auto param = activeParam.param;
                constexpr auto errorMargin = 0.0001_td;

                auto start    = sectionTime.getStart();
                auto end      = sectionTime.getEnd();
                auto newStart = start + offset;
                auto newEnd   = end   + offset;

                auto& srcCurve = activeParam.curve;
                auto& ts = param->getEdit().tempoSequence;
                auto* um = ts.getUndoManager();

                auto idx1 = srcCurve.indexBefore (newEnd + errorMargin);
                auto endCurve = idx1 < 0 ? 0 : srcCurve.getPointCurve (idx1);

                auto idx2 = srcCurve.indexBefore (start - errorMargin);
                auto startCurve = idx2 < 0 ? 0 : srcCurve.getPointCurve (idx2);
                auto defaultValue = param->getCurrentBaseValue();

                auto srcStartVal = srcCurve.getValueAt (start - errorMargin, defaultValue);
                auto srcEndVal   = srcCurve.getValueAt (end   + errorMargin, defaultValue);

                auto dstStartVal = dstCurve->getValueAt (newStart - errorMargin, defaultValue);
                auto dstEndVal   = dstCurve->getValueAt (newEnd   + errorMargin, defaultValue);

                TimeRange totalRegionWithMargin  (newStart - errorMargin, newEnd   + errorMargin);
                TimeRange startWithMargin        (newStart - errorMargin, newStart + errorMargin);
                TimeRange endWithMargin          (newEnd   - errorMargin, newEnd   + errorMargin);

                juce::Array<AutomationCurve::AutomationPoint> origPoints;

                for (int i = 0; i < srcCurve.getNumPoints(); ++i)
                {
                    auto pt = srcCurve.getPoint (i);
                    auto ptTime = toTime (pt.time, ts);

                    if (ptTime >= start - errorMargin && ptTime <= sectionTime.getEnd() + errorMargin)
                        origPoints.add (pt);
                }

                dstCurve->removePointsInRegion (totalRegionWithMargin, um);

                for (const auto& pt : origPoints)
                    dstCurve->addPoint (toTime (pt.time, ts) + offset, pt.value, pt.curve, um);

                auto startPoints = dstCurve->getPointsInRegion (startWithMargin);
                auto endPoints   = dstCurve->getPointsInRegion (endWithMargin);

                dstCurve->removePointsInRegion (startWithMargin, um);
                dstCurve->removePointsInRegion (endWithMargin, um);

                dstCurve->addPoint (newStart, dstStartVal, startCurve, um);
                dstCurve->addPoint (newStart, srcStartVal, startCurve, um);

                for (auto& point : startPoints)
                    dstCurve->addPoint (newStart, point.value, point.curve, um);

                for (auto& point : endPoints)
                    dstCurve->addPoint (newEnd, point.value, point.curve, um);

                dstCurve->addPoint (newEnd, srcEndVal, endCurve, um);
                dstCurve->addPoint (newEnd, dstEndVal, endCurve, um);

                dstCurve->removeRedundantPoints (totalRegionWithMargin, um);
            }
        }
    }

    // activate the automation curves on the new tracks
    juce::Array<Track*> src, dst;

    for (auto& section : sections)
    {
        if (section.src != section.dst)
        {
            if (! src.contains (section.src.get()))
            {
                src.add (section.src.get());
                dst.add (section.dst.get());
            }
        }
    }

    for (int i = 0; i < src.size(); ++i)
    {
        if (auto ap = src.getUnchecked (i)->getCurrentlyShownAutoParam())
        {
            for (auto p : dst.getUnchecked (i)->getAllAutomatableParams())
            {
                if (p->getPluginAndParamName() == ap->getPluginAndParamName())
                {
                    dst.getUnchecked (i)->setCurrentlyShownAutoParam (p);
                    break;
                }
            }
        }
    }
}

}} // namespace tracktion { inline namespace engine
