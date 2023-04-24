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

#if TRACKTION_UNIT_TESTS

//==============================================================================
//==============================================================================
class InternalPluginTests : public juce::UnitTest
{
public:
    InternalPluginTests() : juce::UnitTest ("Internal Plugins", "Tracktion") {}

    //==============================================================================
    void runTest() override
    {
        runRestoreStateTests();
    }

private:
    void runRestoreStateTests()
    {
        testPreset (DelayPlugin::xmlTypeName,
                    {
                        { "feedback",       -30.0f,     IDs::feedback },
                        { "mix proportion", 0.5f,       IDs::mix }
                    });
        testPreset (CompressorPlugin::xmlTypeName,
                    {
                        { "threshold",      0.9f,       IDs::threshold },
                        { "ratio",          0.6f,       IDs::ratio },
                        { "attack",         150.0f,     IDs::attack },
                        { "release",        200.0f,     IDs::release },
                        { "output gain",    10.0f,      IDs::outputDb },
                        { "input gain",     6.0f,       IDs::inputDb }
                    });
        testPreset (EqualiserPlugin::xmlTypeName,
                    {
                        { "Low-pass freq",  30.0f,      IDs::loFreq },
                        { "Low-pass gain",  -10.0f,     IDs::loGain },
                        { "Low-pass Q",     0.2f,       IDs::loQ },
                        { "Mid freq 1",     10000.0f,   IDs::midFreq1 },
                        { "Mid gain 1",     1.0f,       IDs::midGain1 },
                        { "Mid Q 1",        2.0f,       IDs::midQ1 },
                        { "Mid freq 2",     6000.0f,    IDs::midFreq2 },
                        { "Mid gain 2",     6.0f,       IDs::midGain2 },
                        { "Mid Q 2",        3.5f,       IDs::midQ2 },
                        { "High-pass freq", 15000.0f,   IDs::hiFreq },
                        { "High-pass gain", -6.0f,      IDs::hiGain },
                        { "High-pass Q",    0.1f,       IDs::hiQ }
                    });
        testPreset (PitchShiftPlugin::xmlTypeName,
                    {
                        { "semitones up",   -12.0f,     IDs::semitonesUp },
                    });
        testPreset (PitchShiftPlugin::xmlTypeName,
                    {
                        { "semitones up",   6.0f,       IDs::semitonesUp },
                    });
        testPreset (ReverbPlugin::xmlTypeName,
                    {
                        { "room size",      0.2f,       IDs::roomSize },
                        { "damping",        0.3f,       IDs::damp },
                        { "wet level",      0.9f,       IDs::wet },
                        { "dry level",      0.5f,       IDs::dry },
                        { "width",          0.7f,       IDs::width },
                        { "mode",           0.0f,       IDs::mode },
                    });
        testPreset (VolumeAndPanPlugin::xmlTypeName,
                    {
                        { "volume",         0.5f,       IDs::volume },
                        { "pan",            -0.25f,     IDs::pan }
                    });
    }

    struct ParamTest
    {
        const char* paramID;
        float desiredValue;
        const juce::Identifier valueProperty;
    };

    void testPreset (const juce::String& pluginName, std::initializer_list<ParamTest> params)
    {
        beginTest ("Testing plugin parameters: " + pluginName);

        // Create Edit for testing and a plugin instance
        auto edit = Edit::createSingleTrackEdit (*Engine::getEngines()[0]);

        Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (pluginName, {});
        expect (pluginPtr != nullptr);

        // Create a state for restoration
        juce::ValueTree preset (IDs::PLUGIN);
        preset.setProperty (IDs::type, pluginName, nullptr);

        for (auto param : params)
            preset.setProperty (param.valueProperty, param.desiredValue, nullptr);

        // Test loading this state and then saving the plugin back tot he value tree
        pluginPtr->restorePluginStateFromValueTree (preset);
        pluginPtr->flushPluginStateToValueTree();

        // Ensure the parameters have the correct value from the preset
        for (auto param : params)
        {
            auto parameter = pluginPtr->getAutomatableParameterByID (param.paramID);
            expect (parameter->getValueRange().contains (param.desiredValue));

            expect (parameter != nullptr);
            expectWithinAbsoluteError ((float) pluginPtr->state[param.valueProperty],
                                       (float) preset[param.valueProperty],
                                       0.0001f);
            expectWithinAbsoluteError (parameter->getCurrentExplicitValue(), param.desiredValue, 0.0001f);
            expectWithinAbsoluteError (parameter->getCurrentValue(), param.desiredValue, 0.0001f);
            expectWithinAbsoluteError (parameter->getCurrentValue(), parameter->getCurrentExplicitValue(), 0.0001f);
            expect (! pluginPtr->state.hasProperty (IDs::parameters), "State has erroneous parameters property");
        }
    }
};

static InternalPluginTests internalPluginTests;

#endif // TRACKTION_UNIT_TESTS

}} // namespace tracktion { inline namespace engine
