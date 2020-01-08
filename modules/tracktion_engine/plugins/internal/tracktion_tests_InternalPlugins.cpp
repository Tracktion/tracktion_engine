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

#if TRACKTION_UNIT_TESTS

//==============================================================================
//==============================================================================
class InternalPluginTests : public UnitTest
{
public:
    InternalPluginTests() : UnitTest ("Internal Plugins", "Tracktion") {}

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
                        { "feedback",       -30.0f, IDs::feedback },
                        { "mix proportion", 0.5f,   IDs::mix }
                    });
    }

    struct ParamTest
    {
        const char* paramID;
        float desiredValue;
        const Identifier valueProperty;
    };

    void testPreset (const String& pluginName, std::initializer_list<ParamTest> params)
    {
        beginTest ("Testing plugin parameters: " + pluginName);

        // Create Edit for testing and a plugin instance
        auto edit = Edit::createSingleTrackEdit (Engine::getInstance());

        Plugin::Ptr pluginPtr = edit->getPluginCache().createNewPlugin (pluginName, {});
        expect (pluginPtr != nullptr);

        // Create a state for restoration
        ValueTree preset (IDs::PLUGIN);
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

}
