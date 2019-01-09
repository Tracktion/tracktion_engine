/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion_engine
{

struct Pitch
{
    static juce::String getPitchAsString (Engine& engine, int pitch)
    {
        auto middleC = engine.getEngineBehaviour().getMiddleCOctave();
        auto sharp = juce::MidiMessage::getMidiNoteName (pitch, true,  false, middleC);
        auto flat  = juce::MidiMessage::getMidiNoteName (pitch, false, false, middleC);

        if (sharp == flat)
            return sharp;

        return sharp + " / " + flat;
    }

    static juce::StringArray getPitchAsStrings (Engine& engine, int pitch)
    {
        auto middleC = engine.getEngineBehaviour().getMiddleCOctave();
        auto sharp = juce::MidiMessage::getMidiNoteName (pitch, true,  false, middleC);
        auto flat  = juce::MidiMessage::getMidiNoteName (pitch, false, false, middleC);

        juce::StringArray res;

        if (sharp == flat)
        {
            res.add (sharp);
            return res;
        }

        res.add (sharp);
        res.add (flat);
        res.add (sharp + " / " + flat);

        return res;
    }

    static int getPitchFromString (Engine& engine, const juce::String& str)
    {
        for (int i = 0; i < 12; ++i)
            if (getPitchAsStrings (engine, i + 60).contains (str))
                return i + 60;

        return 60;
    }

    static juce::StringArray getPitchStrings (Engine& engine, bool separateSharpFlat)
    {
        juce::StringArray pitchChoices;

        if (separateSharpFlat)
        {
            for (int i = 0; i < 12; ++i)
                for (auto s : getPitchAsStrings (engine, i + 60))
                    if (! s.contains ("/"))
                        pitchChoices.add (s);
        }
        else
        {
            for (int i = 0; i < 12; ++i)
                pitchChoices.add (getPitchAsString (engine, i + 60));
        }

        return pitchChoices;
    }

    // Converts a relative number of semitones up or down into a speed ratio where 0 semitones = 1.0
    static float semitonesToRatio (float semitonesUp) noexcept
    {
        if (semitonesUp == 0.0f)
            return 1.0f;

        auto oneSemitone = std::pow (2.0, 1.0 / 12.0);
        return (float) std::pow (oneSemitone, (double) semitonesUp);
    }

};

} // namespace tracktion_engine
