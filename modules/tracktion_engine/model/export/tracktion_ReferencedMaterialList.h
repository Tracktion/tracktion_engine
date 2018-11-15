/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
    A list of all the source files needed by an edit (or a section of an edit).
*/
class ReferencedMaterialList
{
public:
    ReferencedMaterialList (ProjectManager& pm, double handleSizeToUse)
        : projectManager (pm), handleSize (handleSizeToUse) {}

    //==============================================================================
    /** Adds the whole of a media id to the list. */
    void add (ProjectItemID id)
    {
        if (auto projectItem = projectManager.getProjectItem (id))
            add (projectItem, 0, projectItem->getLength());
    }

    /** Adds just a section of a media id to the list. */
    void add (ProjectItemID id, double startTime, double length)
    {
        if (auto projectItem = projectManager.getProjectItem (id))
        {
            projectItem->verifyLength();
            const double srcLen = projectItem->getLength() + 0.001;

            const double start = juce::jlimit (0.0, srcLen, startTime - handleSize);
            const double end   = juce::jlimit (0.0, srcLen, startTime + length + handleSize);

            add (projectItem, start, end - start);
        }
    }

    void add (const Exportable::ReferencedItem& item)
    {
        add (item.itemID, item.firstTimeUsed, item.lengthUsed);
    }

    void add (const ProjectItem::Ptr& mop, double start, double length)
    {
        if (length <= 0.0)
            return;

        auto itemID = mop->getID();

        for (int i = ids.size(); --i >= 0;)
        {
            if (itemID == ids.getReference(i))
            {
                excerpts[i]->addInterval (start, length);
                return;
            }
        }

        ids.add (itemID);
        auto intervals = new IntervalList();
        intervals->addInterval (start, length);
        excerpts.add (intervals);
    }

    //==============================================================================
    juce::String getReassignedFileName (ProjectItemID id, double requiredTime,
                                        double& newStartTime, double& newLength) const
    {
        requiredTime = juce::jmax (0.0, requiredTime);

        if (auto il = getIntervalsForID (id))
        {
            for (int j = il->getNumIntervals(); --j >= 0;)
            {
                if (requiredTime >= il->getStart(j) - 0.001
                     && requiredTime <= il->getEnd(j))
                {
                    newStartTime = il->getStart(j);
                    newLength = il->getLength(j);

                    if (auto projectItem = projectManager.getProjectItem (id))
                    {
                        auto withoutExtension = juce::File::getCurrentWorkingDirectory()
                                                  .getChildFile (projectItem->getFileName())
                                                  .getFileNameWithoutExtension();

                        if (j > 0)
                            withoutExtension << "_" << j;

                        return withoutExtension
                                 + projectItem->getFileName()
                                     .substring (projectItem->getFileName().lastIndexOfChar ('.'));
                    }
                }
            }

            // should have found a suitable interval
            jassertfalse;
        }

        return {};
    }

    int getTotalNumThingsToExport()
    {
        int tot = 0;

        for (auto e : excerpts)
            tot += e->getNumIntervals();

        return tot;
    }

    //==============================================================================
    /** Represents the sections of a wave file that are being used. */
    struct IntervalList
    {
        IntervalList() {}

        int getNumIntervals() const         { return starts.size(); }
        double getStart (int index) const   { return starts[index]; }
        double getLength (int index) const  { return ends[index] - starts[index]; }
        double getEnd (int index) const     { return ends[index]; }

        void addInterval (double start, double length)
        {
            starts.add (start);
            ends.add (start + length);

            // merge all the intervals
            for (int i = getNumIntervals(); --i >= 0;)
            {
                double s1 = getStart(i);
                double e1 = getEnd(i);

                for (int j = getNumIntervals(); --j > i;)
                {
                    const double s2 = getStart(j);
                    const double e2 = getEnd(j);

                    if (e1 >= s2 && s1 <= e2)
                    {
                        s1 = juce::jmin (s1, s2);
                        e1 = juce::jmax (e1, e2);
                        starts.remove (j);
                        ends.remove (j);
                    }
                }

                starts.set (i, s1);
                ends.set (i, e1);
            }
        }

    private:
        juce::Array<double> starts, ends;
    };

    //==============================================================================
    ProjectManager& projectManager;
    juce::Array<ProjectItemID> ids;

private:
    juce::OwnedArray<IntervalList> excerpts;
    double handleSize;

    const IntervalList* getIntervalsForID (ProjectItemID id) const
    {
        for (int i = ids.size(); --i >= 0;)
            if (id == ids.getReference(i))
                return excerpts[i];

        return {};
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReferencedMaterialList)
};

} // namespace tracktion_engine
