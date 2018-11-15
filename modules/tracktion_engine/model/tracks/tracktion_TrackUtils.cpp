/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


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
    Array<Track*> siblingTracks;

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

TrackInsertPoint::TrackInsertPoint (const ValueTree& v)
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

//==============================================================================
TrackList::TrackList (Edit& e, const juce::ValueTree& parent)
    : ValueTreeObjectList<Track> (parent), edit (e)
{
    rebuildObjects();
    rebuilding = false;
}

TrackList::~TrackList()
{
    freeObjects();
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

bool TrackList::isChordTrack (const juce::ValueTree& v) noexcept  { return v.hasType (IDs::CHORDTRACK); }
bool TrackList::isMarkerTrack (const juce::ValueTree& v) noexcept { return v.hasType (IDs::MARKERTRACK); }
bool TrackList::isTempoTrack (const juce::ValueTree& v) noexcept  { return v.hasType (IDs::TEMPOTRACK); }
bool TrackList::isFixedTrack (const juce::ValueTree& v) noexcept  { return isMarkerTrack (v) || isTempoTrack (v) || isChordTrack (v); }
bool TrackList::isTrack (const juce::ValueTree& v) noexcept       { return isMovableTrack (v) || isFixedTrack (v); }

bool TrackList::isTrack (const juce::Identifier& i) noexcept
{
    return i == IDs::TRACK || i == IDs::FOLDERTRACK || i == IDs::AUTOMATIONTRACK
            || i == IDs::MARKERTRACK || i == IDs::TEMPOTRACK || i == IDs::CHORDTRACK;
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
    if (! edit.isLoading())
    {
        triggerAsyncUpdate();
        t->refreshCurrentAutoParam();

        if (auto tl = t->getSubTrackList())
            tl->visitAllRecursive ([] (Track& track)
                                   {
                                       track.refreshCurrentAutoParam();
                                       return true;
                                   });
    }
}

void TrackList::objectRemoved (Track*) {}

void TrackList::objectOrderChanged()
{
    edit.updateTrackStatusesAsync();
}

void TrackList::sortTracksByType (ValueTree& editState, UndoManager* um)
{
    struct TrackTypeSorter
    {
        static int getPriority (const juce::ValueTree& v) noexcept
        {
            if (isMovableTrack (v)) return 4;
            if (isChordTrack (v))   return 3;
            if (isMarkerTrack (v))  return 2;
            if (isTempoTrack (v))   return 1;
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
bool checkRenderParametersAndConfirmWithUser (const Array<Track*>& tracks, EditTimeRange markedRange, bool silent)
{
    if (tracks.isEmpty())
        return false;

    BigInteger trackBits;
    int numClips = 0;
    double start = Edit::maximumLength;
    double end = 0;

    auto& ui = tracks.getFirst()->edit.engine.getUIBehaviour();

    for (auto track : tracks)
    {
        if (auto at = dynamic_cast<AudioTrack*> (track))
        {
            if (! at->shouldBePlayed())
            {
                if (! silent)
                    ui.showWarningMessage (TRANS("This operation can only be performed on tracks that are not muted or soloed"));
                return false;
            }

            String outputName;
            trackBits.setBit (at->getIndexInEditTrackList());

            if (auto numClipsInTrack = at->getClips().size())
            {
                auto trackRange = at->getTotalRange();
                start = jmin (start, trackRange.getStart());
                end   = jmax (end, trackRange.getEnd());
                numClips += numClipsInTrack;
            }

            for (auto feederTrack : at->getInputTracks())
            {
                if (auto numClipsInTrack = feederTrack->getClips().size())
                {
                    auto trackRange = feederTrack->getTotalRange();
                    start = jmin (start, trackRange.getStart());
                    end   = jmax (end, trackRange.getEnd());
                    numClips += numClipsInTrack;
                }
            }

            if (outputName != at->getOutput().getOutputName()
                 && outputName.isNotEmpty())
            {
                if (! silent)
                    ui.showWarningMessage (TRANS("This operation can only be performed with "
                                                 "tracks which all send their output to the "
                                                 "same output device or track"));
                return false;
            }

            outputName = at->getOutput().getOutputName();
        }
    }

    if (numClips == 0)
    {
        if (! silent)
        {
            if (markedRange.getLength() > 0.2)
            {
                PopupMenu::dismissAllActiveMenus();

                return ui.showOkCancelAlertBox (TRANS("Render"),
                                                TRANS("There aren't any clips in these tracks to render, render anyway?"),
                                                TRANS("Render"),
                                                TRANS("Cancel"));
            }

            ui.showWarningMessage (TRANS("There aren't any clips in these tracks to render"));
        }
        return false;
    }

    if (end <= start + 0.2)
    {
        if (! silent)
            ui.showWarningMessage (TRANS("The material in the selected tracks is too short to render"));
        return false;
    }

    return true;
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
        && position.expanded (0.0001).overlaps (other.position);
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

static bool mergeInto (const TrackAutomationSection& s, Array<TrackAutomationSection>& dst)
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

static void mergeSections (const Array<TrackAutomationSection>& src, Array<TrackAutomationSection>& dst)
{
    for (const auto& srcSeg : src)
        if (! mergeInto (srcSeg, dst))
            dst.add (srcSeg);
}

void moveAutomation (const Array<TrackAutomationSection>& origSections, double offset, bool copy)
{
    if (origSections.isEmpty())
        return;

    Array<TrackAutomationSection> sections;
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
                    TrackAutomationSection::ActiveParameters ap;
                    ap.param = param;
                    ap.curve = param->getCurve();
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

                auto startValue = curve.getValueAt (sectionTime.start - 0.0001);
                auto endValue   = curve.getValueAt (sectionTime.end   + 0.0001);

                auto idx = curve.indexBefore (sectionTime.end + 0.0001);
                auto endCurve = (idx == -1) ? 0.0f : curve.getPointCurve(idx);

                curve.removePointsInRegion (sectionTime.expanded (0.0001));

                if (std::abs (startValue - endValue) < 0.0001f)
                {
                    curve.addPoint (sectionTime.start, startValue, 0.0f);
                    curve.addPoint (sectionTime.end, endValue, endCurve);
                }
                else if (startValue > endValue)
                {
                    curve.addPoint (sectionTime.start, startValue, 0.0f);
                    curve.addPoint (sectionTime.start, endValue, 0.0f);
                    curve.addPoint (sectionTime.end, endValue, endCurve);
                }
                else
                {
                    curve.addPoint (sectionTime.start, startValue, 0.0f);
                    curve.addPoint (sectionTime.end, startValue, 0.0f);
                    curve.addPoint (sectionTime.end, endValue, endCurve);
                }

                curve.removeRedundantPoints (sectionTime.expanded (0.0001));
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
                constexpr double errorMargin = 0.0001;

                auto start    = sectionTime.start;
                auto end      = sectionTime.end;
                auto newStart = start + offset;
                auto newEnd   = end   + offset;

                auto& srcCurve = activeParam.curve;

                auto idx1 = srcCurve.indexBefore (newEnd + errorMargin);
                auto endCurve = idx1 < 0 ? 0 : srcCurve.getPointCurve (idx1);

                auto idx2 = srcCurve.indexBefore (start - errorMargin);
                auto startCurve = idx2 < 0 ? 0 : srcCurve.getPointCurve (idx2);

                auto srcStartVal = srcCurve.getValueAt (start - errorMargin);
                auto srcEndVal   = srcCurve.getValueAt (end   + errorMargin);

                auto dstStartVal = dstCurve->getValueAt (newStart - errorMargin);
                auto dstEndVal   = dstCurve->getValueAt (newEnd   + errorMargin);

                EditTimeRange totalRegionWithMargin  (newStart - errorMargin, newEnd   + errorMargin);
                EditTimeRange startWithMargin        (newStart - errorMargin, newStart + errorMargin);
                EditTimeRange endWithMargin          (newEnd   - errorMargin, newEnd   + errorMargin);

                Array<AutomationCurve::AutomationPoint> origPoints;

                for (int i = 0; i < srcCurve.getNumPoints(); ++i)
                {
                    auto pt = srcCurve.getPoint (i);

                    if (pt.time >= start - errorMargin && pt.time <= sectionTime.end + errorMargin)
                        origPoints.add (pt);
                }

                dstCurve->removePointsInRegion (totalRegionWithMargin);

                for (const auto& pt : origPoints)
                    dstCurve->addPoint (pt.time + offset, pt.value, pt.curve);

                auto startPoints = dstCurve->getPointsInRegion (startWithMargin);
                auto endPoints   = dstCurve->getPointsInRegion (endWithMargin);

                dstCurve->removePointsInRegion (startWithMargin);
                dstCurve->removePointsInRegion (endWithMargin);

                dstCurve->addPoint (newStart, dstStartVal, startCurve);
                dstCurve->addPoint (newStart, srcStartVal, startCurve);

                for (auto& point : startPoints)
                    dstCurve->addPoint (newStart, point.value, point.curve);

                for (auto& point : endPoints)
                    dstCurve->addPoint (newEnd, point.value, point.curve);

                dstCurve->addPoint (newEnd, srcEndVal, endCurve);
                dstCurve->addPoint (newEnd, dstEndVal, endCurve);

                dstCurve->removeRedundantPoints (totalRegionWithMargin);
            }
        }
    }

    // activate the automation curves on the new tracks
    Array<Track*> src, dst;

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
