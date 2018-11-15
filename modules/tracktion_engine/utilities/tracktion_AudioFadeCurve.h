/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/

namespace tracktion_engine
{

struct AudioFadeCurve
{
    /** A linear fade curve. */
    struct Linear
    {
        inline static double preadjust (double alpha) noexcept  { return alpha; }
        inline static float get (float alpha) noexcept          { return alpha; }
    };

    /** A convex sine-shaped curve. */
    struct Convex
    {
        inline static double preadjust (double alpha) noexcept  { return alpha * (0.5 * juce::double_Pi); }
        inline static float get (float alpha) noexcept          { return std::sin (alpha); }
    };

    /** A concave sine-shaped curve. */
    struct Concave
    {
        inline static double preadjust (double alpha) noexcept  { return alpha * (0.5 * juce::double_Pi); }
        inline static float get (float alpha) noexcept          { return 1.0f - std::cos (alpha); }
    };

    /** An S-shaped curve. */
    struct SCurve
    {
        inline static double preadjust (double alpha) noexcept  { return alpha; }
        inline static float get (float alpha) noexcept          { return (1.0f - alpha) * (1.0f - std::cos (alpha * (0.5f * juce::float_Pi)))
                                                                            + alpha * std::sin (alpha * (0.5f * juce::float_Pi)); }
    };

    /** A enumeration of the curve classes available. */
    enum Type
    {
        linear  = 1,
        convex  = 2,
        concave = 3,
        sCurve  = 4
    };

    /** Converts an alpha position along the curve (0 to 1.0) into the gain at that point. */
    template <typename CurveClass>
    static float alphaToGain (float alpha) noexcept
    {
        jassert (alpha >= 0 && alpha <= 1.0f);
        return CurveClass::get ((float) CurveClass::preadjust (alpha));
    }

    /** Converts an alpha position along the curve (0 to 1.0) into the gain at that point. */
    static float alphaToGainForType (Type type, float alpha) noexcept
    {
        if (type == convex)    return alphaToGain<Convex>  (alpha);
        if (type == concave)   return alphaToGain<Concave> (alpha);
        if (type == sCurve)    return alphaToGain<SCurve>  (alpha);

        jassert (type == linear);
        return alpha;
    }

    /** Multiplies a block of samples by the curve shape between two alpha-positions.
        The DestSamplePointer object must be a class that implements an apply() method that
        gets called with each gain level.
    */
    template <typename CurveClass, typename DestSamplePointer>
    static void renderBlock (DestSamplePointer& dest, int numSamples,
                             float startAlpha, float endAlpha) noexcept
    {
        jassert (numSamples > 0); // can only be used on non-empty blocks!

        const double a1 = CurveClass::preadjust (startAlpha);
        const double a2 = CurveClass::preadjust (endAlpha);
        const double delta = (a2 - a1) / numSamples;
        double alpha = a1;

        while (--numSamples >= 0)
        {
            dest.apply (CurveClass::get ((float) alpha));
            alpha += delta;
        }
    }

    /** Multiplies a block of samples by the curve shape between two alpha-positions.
        The DestSamplePointer object must be a class that implements an apply() method that
        gets called with each gain level.
    */
    template <typename DestSamplePointer>
    static void renderBlockForType (DestSamplePointer& dest, int numSamples,
                                    float startAlpha, float endAlpha, Type type) noexcept
    {
        jassert (numSamples > 0);

        switch (type)
        {
            case linear:    renderBlock<Linear>  (dest, numSamples, startAlpha, endAlpha); break;
            case convex:    renderBlock<Convex>  (dest, numSamples, startAlpha, endAlpha); break;
            case concave:   renderBlock<Concave> (dest, numSamples, startAlpha, endAlpha); break;
            case sCurve:    renderBlock<SCurve>  (dest, numSamples, startAlpha, endAlpha); break;
            default:        jassertfalse; break;
        }
    }

    /** Calculates the two gain multipliers to use for mixing between two sources, given a position
        alpha from 0 to 1.0.

        An alpha of 0 will produce gain1 = 1, gain2 = 0. An alpha of 1.0 will produce the opposite,
        and in-between, the levels will be calculated based on a pair of convex sine curves.
    */
    struct CrossfadeLevels
    {
        /** Creates the object and calculates the two gain levels.
            An alpha of 0 will produce gain1 = 1 and gain2 = 0. An alpha of 1.0 will produce the opposite,
            and alpha values in-between will be calculated based on a pair of overlapping convex sine curves.
        */
        CrossfadeLevels (float alpha) noexcept
            : gain1 (alphaToGain<AudioFadeCurve::Convex> (alpha)),
              gain2 (alphaToGain<AudioFadeCurve::Convex> (1.0f - alpha))
        {
        }

        float gain1, gain2;
    };

    static void applyCrossfadeSection (juce::AudioBuffer<float>&, int channel,
                                       int startSample, int numSamples,
                                       Type type,
                                       float startAlpha,
                                       float endAlpha);

    static void applyCrossfadeSection (juce::AudioBuffer<float>&,
                                       int startSample, int numSamples,
                                       Type type,
                                       float startAlpha,
                                       float endAlpha);

    static void addWithCrossfade (juce::AudioBuffer<float>& dest,
                                  const juce::AudioBuffer<float>& src,
                                  int destChannel, int destStartIndex,
                                  int sourceChannel, int sourceStartIndex,
                                  int numSamples,
                                  Type type,
                                  float startAlpha,
                                  float endAlpha);

    static void drawFadeCurve (juce::Graphics&, const AudioFadeCurve::Type,
                               float x1, float x2, float top, float bottom, juce::Rectangle<int> clip);
};

} // namespace tracktion_engine


namespace juce
{
    template <>
    struct VariantConverter<tracktion_engine::AudioFadeCurve::Type>
    {
        static tracktion_engine::AudioFadeCurve::Type fromVar (const var& v)   { return (tracktion_engine::AudioFadeCurve::Type) static_cast<int> (v); }
        static var toVar (tracktion_engine::AudioFadeCurve::Type v)            { return static_cast<int> (v); }
    };
}
