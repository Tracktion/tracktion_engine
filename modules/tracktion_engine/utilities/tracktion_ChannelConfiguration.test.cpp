/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#if ENGINE_UNIT_TESTS_CHANNELCONFIGURATION

namespace tracktion { inline namespace engine
{

//==============================================================================
class ChannelConfigurationTests : public juce::UnitTest
{
public:
    ChannelConfigurationTests()
        : juce::UnitTest ("ChannelConfiguration", "tracktion_engine")
    {
    }

    void runTest() override
    {
        runFactoryMethodTests();
        runQueryTests();
        runSerializationTests();
        runComparisonTests();
        runIntersectionTests();
    }

private:
    void runFactoryMethodTests()
    {
        beginTest ("Factory methods");

        {
            auto mono = ChannelConfiguration::mono (5);
            expectEquals (mono.getNumChannels(), 1);
            expectEquals (mono[0].indexInDevice, 5);
            expect (mono[0].channel == juce::AudioChannelSet::centre);
        }

        {
            auto stereo = ChannelConfiguration::stereo (2);
            expectEquals (stereo.getNumChannels(), 2);
            expectEquals (stereo[0].indexInDevice, 2);
            expectEquals (stereo[1].indexInDevice, 3);
            expect (stereo[0].channel == juce::AudioChannelSet::left);
            expect (stereo[1].channel == juce::AudioChannelSet::right);
        }

        {
            auto surround = ChannelConfiguration::surround5_1 (0);
            expectEquals (surround.getNumChannels(), 6);
            expect (surround[0].channel == juce::AudioChannelSet::left);
            expect (surround[1].channel == juce::AudioChannelSet::right);
            expect (surround[2].channel == juce::AudioChannelSet::centre);
            expect (surround[3].channel == juce::AudioChannelSet::LFE);
            expect (surround[4].channel == juce::AudioChannelSet::leftSurround);
            expect (surround[5].channel == juce::AudioChannelSet::rightSurround);
        }

        {
            auto surround71 = ChannelConfiguration::surround7_1 (0);
            expectEquals (surround71.getNumChannels(), 8);
        }

        {
            auto discrete = ChannelConfiguration::discreteChannels (16, 0);
            expectEquals (discrete.getNumChannels(), 16);

            for (int i = 0; i < 16; ++i)
                expectEquals (discrete[static_cast<size_t> (i)].indexInDevice, i);
        }
    }

    void runQueryTests()
    {
        beginTest ("Query methods");

        auto mono = ChannelConfiguration::mono();
        expect (mono.isMono());
        expect (! mono.isStereo());
        expect (mono.isMonoOrStereo());
        expect (! mono.isMultichannel());

        auto stereo = ChannelConfiguration::stereo();
        expect (! stereo.isMono());
        expect (stereo.isStereo());
        expect (stereo.isMonoOrStereo());
        expect (! stereo.isMultichannel());

        auto surround = ChannelConfiguration::surround5_1();
        expect (! surround.isMono());
        expect (! surround.isStereo());
        expect (! surround.isMonoOrStereo());
        expect (surround.isMultichannel());

        {
            auto config = ChannelConfiguration::stereo (4);
            auto range = config.getChannelRange();
            expectEquals (range.first, 4);
            expectEquals (range.second, 6);
        }
    }

    void runSerializationTests()
    {
        beginTest ("Serialization");

        {
            auto original = ChannelConfiguration::stereo (2);
            auto json = original.toJSON();
            auto restored = ChannelConfiguration::fromJSON (json);

            expectEquals (restored.getNumChannels(), 2);
            expectEquals (restored[0].indexInDevice, 2);
            expectEquals (restored[1].indexInDevice, 3);
            expect (restored[0].channel == juce::AudioChannelSet::left);
            expect (restored[1].channel == juce::AudioChannelSet::right);
        }

        {
            auto original = ChannelConfiguration::surround5_1 (0);
            auto str = original.toString();
            auto restored = ChannelConfiguration::fromString (str);

            expectEquals (restored.getNumChannels(), 6);
            expect (original == restored);
        }

        {
            auto empty = ChannelConfiguration::fromString ("invalid json");
            expect (empty.isEmpty());
        }
    }

    void runComparisonTests()
    {
        beginTest ("Comparison");

        auto stereo1 = ChannelConfiguration::stereo (0);
        auto stereo2 = ChannelConfiguration::stereo (0);
        auto stereo3 = ChannelConfiguration::stereo (2);

        expect (stereo1 == stereo2);
        expect (stereo1 != stereo3);

        auto mono = ChannelConfiguration::mono();
        expect (stereo1 != mono);
    }

    void runIntersectionTests()
    {
        beginTest ("Intersection");

        {
            // Partial overlap: stereo(0) channels 0,1 intersected with discrete 4 channels 0-3
            auto result = ChannelConfiguration::stereo (0).intersection (ChannelConfiguration::discreteChannels (4, 0));
            expectEquals (result.getNumChannels(), 2);
        }

        {
            // No overlap: stereo(4) channels 4,5 vs discrete 2 channels 0,1
            auto result = ChannelConfiguration::stereo (4).intersection (ChannelConfiguration::discreteChannels (2, 0));
            expect (result.isEmpty());
        }

        {
            // Subset: mono(0) is a subset of stereo(0)
            auto result = ChannelConfiguration::mono (0).intersection (ChannelConfiguration::stereo (0));
            expectEquals (result.getNumChannels(), 1);
        }

        {
            // Empty left operand
            auto result = ChannelConfiguration().intersection (ChannelConfiguration::stereo());
            expect (result.isEmpty());
        }

        {
            // Empty right operand
            auto result = ChannelConfiguration::stereo().intersection (ChannelConfiguration());
            expect (result.isEmpty());
        }

        {
            // Both empty
            auto result = ChannelConfiguration().intersection (ChannelConfiguration());
            expect (result.isEmpty());
        }

        {
            // Identical configs
            auto result = ChannelConfiguration::stereo (0).intersection (ChannelConfiguration::stereo (0));
            expectEquals (result.getNumChannels(), 2);
            expect (result == ChannelConfiguration::stereo (0));
        }
    }
};

static ChannelConfigurationTests channelConfigurationTests;

}} // namespace tracktion { inline namespace engine

#endif // ENGINE_UNIT_TESTS_CHANNELCONFIGURATION
