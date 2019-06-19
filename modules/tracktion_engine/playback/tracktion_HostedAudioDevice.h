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

class HostedAudioDeviceType;
/**
 The HostedAudioDeviceInterface allows an application or plugin
 to pass audio and midi buffers to the engine, rather than the engine
 directly opening the audio devices. This may be required for plugins
 or applications that run multiple copies of the engine.

 Don't create this class directly, it can be optained from the DeviceManager
 via getHostedAudioDeviceInterface()
*/
class HostedAudioDeviceInterface
{
public:
    HostedAudioDeviceInterface (Engine&);

    //==============================================================================
    /** Holds the parameters being used by an HostedAudioDeviceInterface. */
    struct Parameters
    {
        /** Expected sample rate. This can be changed later with prepareToPlay */
        double sampleRate = 44100.0;

        /** Expected block size. This can be changed later with prepareToPlay */
        int blockSize = 512;

        /** If true, the system midi devices will be avaliable to the engine,
            if false, just a single midi input and output will be avaliable,
            which will be fed from the midi buffer provided to processBlock */
        bool useMidiDevices = false;

        /** Number of audio input channels */
        int inputChannels = 2;

        /** Number of audio output channels */
        int outputChannels = 2;

        /** If the size of the audio buffer passed to processBlock will be fixed or not.
            If you are creating a plugin, this should be false, and your plguin will have
            one block of latency. If you are handling the audio device callback yourself,
            this can be true. */
        bool fixedBlockSize = false;

        /** Names of your audio channels. If left empty, names will automatically be generated */
        juce::StringArray inputNames, outputNames;
    };

    void initialise (const Parameters&);

    // Call each time the sample rate or block size changes
    void prepareToPlay (double sampleRate, int blockSize);

    // Pass audio and midi buffers to the engine. If fixedBlockSize == true
    // then the buffer must have the same number of samples as specified in
    // the last call to prepareToPlay.
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&);

private:
    friend DeviceManager;
    friend class HostedAudioDevice;
    friend class HostedAudioDeviceType;
    friend class HostedMidiInputDevice;
    friend class HostedMidiOutputDevice;

    juce::StringArray getInputChannelNames();
    juce::StringArray getOutputChannelNames();

    MidiOutputDevice* createMidiOutput();
    MidiInputDevice* createMidiInput();

    Engine& engine;
    Parameters parameters;
    HostedAudioDeviceType* deviceType = nullptr;

    juce::Array<MidiOutputDevice*> midiOutputs;
    juce::Array<MidiInputDevice*> midiInputs;

    AudioMidiFifo inputFifo, outputFifo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HostedAudioDeviceInterface)
};

} // namespace tracktion_engine
