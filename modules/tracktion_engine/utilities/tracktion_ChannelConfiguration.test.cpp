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
};

static ChannelConfigurationTests channelConfigurationTests;

}} // namespace tracktion { inline namespace engine

#endif // ENGINE_UNIT_TESTS_CHANNELCONFIGURATION
