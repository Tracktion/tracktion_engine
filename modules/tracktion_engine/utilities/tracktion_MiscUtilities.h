/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

inline void addSortedListToMenu (juce::PopupMenu& m, juce::StringArray names, int startID)
{
    struct ItemWithID
    {
        juce::String name;
        int id;

        bool operator< (const ItemWithID& other) const  { return id < other.id; }
    };

    juce::Array<ItemWithID> items;

    for (const auto& name : names)
        items.add ({ name, startID++ });

    items.sort();

    for (const auto& i : items)
        m.addItem (i.id, i.name);
}

inline juce::AffineTransform getScaleAroundCentre (juce::Rectangle<float> r, float numPixels)
{
    jassert (! r.isEmpty());
    auto cx = r.getCentreX();
    auto cy = r.getCentreY();
    auto w = r.getWidth();
    auto h = r.getHeight();

    return juce::AffineTransform::scale ((w + numPixels) / w, (h + numPixels) / h, cx, cy);
}

/** Returns the index in the first array of the first match found.
    This is usually just used to find out if there are any matches between the two arrays.
*/
inline int indexOfFirstFoundInSecond (const juce::StringArray& array1,
                                      const juce::StringArray& array2) noexcept
{
    if (array2.isEmpty())
        return -1;

    for (int i1 = 0; i1 < array1.size(); ++i1)
        if (array2.contains (array1[i1]))
            return i1;

    return -1;
}

/** Returns true if all of the strings of the first array are found in the second array. */
inline bool allFirstFoundInSecond (const juce::StringArray& array1,
                                   const juce::StringArray& array2) noexcept
{
    for (const auto& s : array1)
        if (! array2.contains (s))
            return false;

    return true;
}

inline void moveXMLAttributeToStart (juce::XmlElement& xml, juce::StringRef att)
{
    juce::StringArray names, values;
    bool wasFound = false;

    for (int i = 0; i < xml.getNumAttributes(); ++i)
    {
        auto name = xml.getAttributeName (i);
        auto value = xml.getAttributeValue (i);

        if (name == att)
        {
            wasFound = true;
            names.insert (0, name);
            values.insert (0, value);
        }
        else
        {
            names.add (name);
            values.add (value);
        }
    }

    if (wasFound)
    {
        xml.removeAllAttributes();

        for (int i = 0; i < names.size(); ++i)
            xml.setAttribute (names[i], values[i]);
    }
}

template <typename Vector, typename Predicate>
inline bool removeIf (Vector& v, Predicate&& pred)
{
    auto oldEnd = std::end (v);
    auto newEnd = std::remove_if (std::begin (v), oldEnd, pred);

    if (newEnd == oldEnd)
        return false;

    v.erase (newEnd, oldEnd);
    return true;
}

} // namespace tracktion_engine
