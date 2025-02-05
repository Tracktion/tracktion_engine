/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion::inline engine
{

/**
    Determines the behaviour of writing automation.
*/
enum class AutomationMode
{
    read,       ///< No writing.
    touch,      ///< Start writing on first event, punch out on gesture end
    latch,      ///< Start writing on first event, punch out on play stop
    write       ///< Start writing on play start, punch out on gesture end
};

/** Converts an AutomationMode from a string. */
std::optional<AutomationMode> automationModeFromString (juce::String);

/** Converts an AutomationMode to string. */
juce::String toString (AutomationMode);

} // namespace tracktion::inline engine


//==============================================================================
//        _        _           _  _
//     __| |  ___ | |_   __ _ (_)| | ___
//    / _` | / _ \| __| / _` || || |/ __|
//   | (_| ||  __/| |_ | (_| || || |\__ \ _  _  _
//    \__,_| \___| \__| \__,_||_||_||___/(_)(_)(_)
//
//   Code beyond this point is implementation detail...
//
//==============================================================================

namespace juce
{
    template<>
    struct VariantConverter<tracktion::engine::AutomationMode>
    {
        static tracktion::engine::AutomationMode fromVar (const var& v)
        {
            using namespace tracktion::engine;
            return automationModeFromString (v.toString()).value_or (AutomationMode::latch);
        }

        static var toVar (tracktion::engine::AutomationMode v)
        {
            return toString (v);
        }
    };
}
