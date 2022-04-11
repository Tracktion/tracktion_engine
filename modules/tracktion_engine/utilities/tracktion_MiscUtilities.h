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

//==============================================================================
/** Checks to see if two values are equal within a given precision. */
template <typename FloatingPointType>
inline bool almostEqual (FloatingPointType firstValue, FloatingPointType secondValue, FloatingPointType precision = (FloatingPointType) 0.00001)
{
    return std::abs (firstValue - secondValue) < precision;
}

//==============================================================================
using HashCode = int64_t;

//==============================================================================
//==============================================================================
/**
    Shows and hides the mouse wait cursor where appropriate.
*/
class ScopedWaitCursor
{
public:
    /** Shows the wait cursor. */
    ScopedWaitCursor()
    {
        if (juce::JUCEApplicationBase::getInstance() != nullptr)
            juce::MouseCursor::showWaitCursor();
    }
    
    /** Hides the wait cursor. */
    ~ScopedWaitCursor()
    {
        if (juce::JUCEApplicationBase::getInstance() != nullptr)
            juce::MouseCursor::hideWaitCursor();
    }
};

}} // namespace tracktion { inline namespace engine
