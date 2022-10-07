/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include <cstddef>
#include <cmath>

namespace tracktion { inline namespace core
{

inline std::pair<double /*x*/, double /*y*/> getBezierPoint (double x1, double y1, double x2, double y2,
                                                             double c) noexcept
{
    if (y2 > y1)
    {
        auto run  = x2 - x1;
        auto rise = y2 - y1;

        auto xc = x1 + run / 2;
        auto yc = y1 + rise / 2;

        auto x = xc - run  / 2 * -c;
        auto y = yc + rise / 2 * -c;

        return { x, y };
    }

    auto run  = x2 - x1;
    auto rise = y1 - y2;

    auto xc = x1 + run / 2;
    auto yc = y2 + rise / 2;

    auto x = xc - run  / 2 * -c;
    auto y = yc - rise / 2 * -c;

    return { x, y };
}

inline void getBezierEnds (const double x1, const double y1, const double x2, const double y2, const double c,
                           double& x1out, double& y1out, double& x2out, double& y2out) noexcept
{
    auto minic = (std::abs (c) - 0.5f) * 2.0f;
    auto run   = minic * (x2 - x1);
    auto rise  = minic * ((y2 > y1) ? (y2 - y1) : (y1 - y2));

    if (c > 0)
    {
        x1out = x1 + run;
        y1out = (float) y1;

        x2out = x2;
        y2out = (float) (y1 < y2 ? (y2 - rise) : (y2 + rise));
    }
    else
    {
        x1out = x1;
        y1out = (float) (y1 < y2 ? (y1 + rise) : (y1 - rise));

        x2out = x2 - run;
        y2out = (float) y2;
    }
}

inline double getBezierYFromX (double x, double x1, double y1, double xb, double yb, double x2, double y2) noexcept
{
    // test for straight lines and bail out
    if (x1 == x2 || y1 == y2)
        return y1;

    // test for endpoints
    if (x <= x1)  return y1;
    if (x >= x2)  return y2;

    // ok, we have a bezier curve with one control point,
    // we know x, we need to find y

    // flip the bezier equation around so its an quadratic equation
    auto a = x1 - 2 * xb + x2;
    auto b = -2 * x1 + 2 * xb;
    auto c = x1 - x;

    // solve for t, [0..1]
    double t;

    if (a == 0)
    {
        t = -c / b;
    }
    else
    {
        t = (-b + std::sqrt (b * b - 4 * a * c)) / (2 * a);

        if (t < 0.0f || t > 1.0f)
            t = (-b - std::sqrt (b * b - 4 * a * c)) / (2 * a);
    }

    jassert (t >= 0.0f && t <= 1.0f);

    // find y using the t we just found
    auto y = (std::pow (1 - t, 2) * y1) + 2 * t * (1 - t) * yb + std::pow (t, 2) * y2;
    return y;
}

}}
