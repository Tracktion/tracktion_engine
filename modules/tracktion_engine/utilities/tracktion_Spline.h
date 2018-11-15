/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

/**
    An interpolated spline curve, used by the EQ to draw its response graph.
*/
struct Spline
{
    int getNumPoints() const noexcept               { return points.size(); }
    juce::Point<float> getPoint (int index) const   { return points.getReference (index); }

    float getY (float x) const
    {
        const int num = points.size();

        int i = num;
        while (--i >= 0)
            if (points.getReference(i).x <= x)
                break;

        if (i >= num - 1)
            return points.getLast().y;

        if (i < 0)
            return points.getFirst().y;

        auto p0 = points.getReference (juce::jmax (0, i - 1));
        auto p1 = points.getReference (i);
        auto p2 = points.getReference (i + 1);
        auto p3 = points.getReference (juce::jmin (num - 1, i + 2));

        const float alpha = (x - p1.x) / (p2.x - p1.x);
        const float alpha2 = alpha * alpha;
        const float a0 = p3.y - p2.y - p0.y + p1.y;
        return (a0 * alpha * alpha2) + ((p0.y - p1.y - a0) * alpha2) + ((p2.y - p0.y) * alpha) + p1.y;
    }

    //==============================================================================
    void clear()                                { points.clear(); }
    void removePoint (int index)                { points.remove (index); }

    void addPoint (float x, float y)
    {
        int i = points.size();

        while (--i >= 0)
        {
            juce::Point<float>& p = points.getReference(i);

            if (p.x <= x)
            {
                if (p.x == x)
                {
                    p.y = y;
                    return;
                }

                break;
            }
        }

        points.insert (++i, { x, y });
    }


private:
    juce::Array<juce::Point<float>> points;
};

} // namespace tracktion_engine
