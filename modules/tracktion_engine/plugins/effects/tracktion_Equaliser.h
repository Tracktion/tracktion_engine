/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

class EqualiserPlugin  : public Plugin
{
public:
    EqualiserPlugin (PluginCreationInfo);
    ~EqualiserPlugin();

    //==============================================================================
    /** Finds the gain at a frequency - used to plot the EQ graph. */
    float getDBGainAtFrequency (float f);

    //==============================================================================
    static const char* getPluginName()              { return NEEDS_TRANS("4-Band Equaliser"); }
    static const char* xmlTypeName;

    juce::String getName() override                 { return TRANS("4-Band Equaliser"); }
    juce::String getPluginType() override           { return xmlTypeName; }
    juce::String getShortName (int) override        { return "EQ"; }
    juce::String getTooltip() override;
    bool needsConstantBufferSize() override         { return false; }

    int getNumOutputChannelsGivenInputs (int numInputChannels) override { return juce::jmin (numInputChannels, (int) EQ_CHANS); }

    juce::String getSelectableDescription() override    { return TRANS("4-Band Equaliser Plugin"); }

    void initialise (const PlaybackInitialisationInfo&) override;
    void deinitialise() override;
    void applyToBuffer (const AudioRenderContext&) override;

    void resetToDefault();
    void restorePluginStateFromValueTree (const juce::ValueTree&) override;

    static constexpr float minFreq = 20.0f;
    static constexpr float maxFreq = 20000.0f;
    static constexpr float minGain = -20.0f;
    static constexpr float maxGain = 20.0f;
    static constexpr float minQ = 0.1f;
    static constexpr float maxQ = 4.0f;

    void setLowGain  (float v)  { loGain  ->setParameter (juce::jlimit (minGain, maxGain, v), juce::sendNotification); }
    void setLowFreq  (float v)  { loFreq  ->setParameter (juce::jlimit (minFreq, maxFreq, v), juce::sendNotification); }
    void setLowQ     (float v)  { loQ     ->setParameter (juce::jlimit (minQ, maxQ, v), juce::sendNotification); }
    void setMidGain1 (float v)  { midGain1->setParameter (juce::jlimit (minGain, maxGain, v), juce::sendNotification); }
    void setMidFreq1 (float v)  { midFreq1->setParameter (juce::jlimit (minFreq, maxFreq, v), juce::sendNotification); }
    void setMidQ1    (float v)  { midQ1   ->setParameter (juce::jlimit (minQ, maxQ, v), juce::sendNotification); }
    void setMidGain2 (float v)  { midGain2->setParameter (juce::jlimit (minGain, maxGain, v), juce::sendNotification); }
    void setMidFreq2 (float v)  { midFreq2->setParameter (juce::jlimit (minFreq, maxFreq, v), juce::sendNotification); }
    void setMidQ2    (float v)  { midQ2   ->setParameter (juce::jlimit (minQ, maxQ, v), juce::sendNotification); }
    void setHighGain (float v)  { hiGain  ->setParameter (juce::jlimit (minGain, maxGain, v), juce::sendNotification); }
    void setHighFreq (float v)  { hiFreq  ->setParameter (juce::jlimit (minFreq, maxFreq, v), juce::sendNotification); }
    void setHighQ    (float v)  { hiQ     ->setParameter (juce::jlimit (minQ, maxQ, v), juce::sendNotification); }

    juce::CachedValue<float> loFreqValue, loGainValue, loQValue,
                             hiFreqValue, hiGainValue, hiQValue,
                             midFreqValue1, midGainValue1, midQValue1,
                             midFreqValue2, midGainValue2, midQValue2;

    juce::CachedValue<bool> phaseInvert;

    AutomatableParameter::Ptr loFreq, loGain, loQ,
                              hiFreq, hiGain, hiQ,
                              midFreq1, midGain1, midQ1,
                              midFreq2, midGain2, midQ2;

private:
    class EQAutomatableParameter;

    Spline curve;
    float lastSampleRate = 44100.0f;
    bool curveNeedsUpdating = true;

    enum { EQ_CHANS = 2 };
    juce::IIRFilter low[EQ_CHANS], mid1[EQ_CHANS], mid2[EQ_CHANS], high[EQ_CHANS];

    enum { fftOrder = 10 };
    juce::dsp::FFT fft { fftOrder };

    void updateIIRFilters();
    std::atomic<bool> needToUpdateFilters[4];
    juce::CriticalSection filterLock;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqualiserPlugin)
};

} // namespace tracktion_engine
