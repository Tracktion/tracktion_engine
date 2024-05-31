/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion { inline namespace engine
{

#define __audioeffect__
#define VstInt32                int32_t
#define AudioEffect             AirWindowsBase
#define AudioEffectX            AirWindowsBase
#define audioMasterCallback     AirWindowsCallback*
#define VstPlugCategory         int
#define kPlugCategEffect        1
#define kVstMaxProgNameLen      64
#define kVstMaxParamStrLen      64
#define kVstMaxProductStrLen    64
#define kVstMaxVendorStrLen     64
#define vst_strncpy             strncpy

inline void float2string (float f, char* text, int len)
{
    int decimals = 0;
    if (std::fabs (f) >= 10.0)
        decimals = 1;
    else if (std::fabs (f) > 1.0)
        decimals = 2;
    else
        decimals = 3;

    juce::String str (f, decimals);
    str.copyToUTF8 (text, (size_t)len);
}

inline void int2string (float i, char* text, int len)
{
    juce::String str (i);
    str.copyToUTF8 (text, (size_t)len);
}

inline void dB2string (float value, char* text, int maxLen)
{
    if (value <= 0)
        vst_strncpy (text, "-oo", (size_t) maxLen);
    else
        float2string ((float)(20. * log10 (value)), text, maxLen);
}

//==============================================================================
class AirWindowsBase
{
public:
    //==============================================================================
    AirWindowsBase (AirWindowsCallback* c, int prog, int param)
        : numPrograms (prog), numParams (param), callback (c)
    {
    }

    virtual ~AirWindowsBase() = default;

    int getNumInputs()                  { return numInputs;     }
    int getNumOutputs()                 { return numOutputs;    }
    int getNumParameters()              { return numParams;     }

    //==============================================================================
    virtual bool getEffectName(char* name)                        = 0;
    virtual VstPlugCategory getPlugCategory()                     = 0;
    virtual bool getProductString(char* text)                     = 0;
    virtual bool getVendorString(char* text)                      = 0;
    virtual VstInt32 getVendorVersion()                           = 0;
    virtual void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames) = 0;
    virtual void processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames) = 0;
    virtual void getProgramName(char *name)                       = 0;
    virtual void setProgramName(char *name)                       = 0;
    virtual VstInt32 getChunk (void** data, bool isPreset)                          { juce::ignoreUnused (data, isPreset); return 0; }
    virtual VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset)        { juce::ignoreUnused (data, byteSize, isPreset); return 0; }
    virtual float getParameter(VstInt32 index)                                      { juce::ignoreUnused (index); return 0; }
    virtual void setParameter(VstInt32 index, float value)                          { juce::ignoreUnused (index, value); }
    virtual void getParameterLabel(VstInt32 index, char *text)                      { juce::ignoreUnused (index, text); }
    virtual void getParameterName(VstInt32 index, char *text)                       { juce::ignoreUnused (index, text); }
    virtual void getParameterDisplay(VstInt32 index, char *text)                    { juce::ignoreUnused (index, text); }
    virtual VstInt32 canDo(char *text)                            = 0;

protected:
    //==============================================================================
    void setNumInputs (int numIn)       { numInputs = numIn;    }
    void setNumOutputs (int numOut)     { numOutputs = numOut;  }
    void setUniqueID (int)              {}
    void canProcessReplacing()          {}
    void canDoubleReplacing()           {}
    void programsAreChunks (bool)       {}

    int numInputs = 0, numOutputs = 0, numPrograms = 0, numParams = 0;

    AirWindowsCallback* callback;

    double getSampleRate()              { return callback->getSampleRate(); }
};

//==============================================================================
class AirWindowsAutomatableParameter   : public AutomatableParameter
{
public:
    AirWindowsAutomatableParameter (AirWindowsPlugin& p, int idx)
        : AutomatableParameter (getParamId (p, idx), getParamName (p, idx), p, {0, 1}),
          awPlugin (p), index (idx)
    {
        valueToStringFunction = [] (float v)         { return juce::String (v); };
        stringToValueFunction = [] (juce::String v)  { return v.getFloatValue(); };

        defaultValue = awPlugin.impl->getParameter (idx);

        setParameter (defaultValue, juce::sendNotification);

        autodetectRange();
    }

    ~AirWindowsAutomatableParameter() override
    {
        notifyListenersOfDeletion();
    }

    void resetToDefault()
    {
        setParameter (defaultValue, juce::sendNotificationSync);
    }

    void autodetectRange()
    {
        float current = awPlugin.impl->getParameter (index);

        awPlugin.impl->setParameter (index, 0.0f);
        float v1 = getCurrentValueAsString().retainCharacters ("1234567890-.").getFloatValue();
        awPlugin.impl->setParameter (index, 1.0f);
        float v2 = getCurrentValueAsString().retainCharacters ("1234567890-.").getFloatValue();

        if (v2 > v1 && (v1 != 0.0f || v2 != 1.0f))
            setConversionRange ({v1, v2});

        awPlugin.impl->setParameter (index, current);
    }

    void refresh()
    {
        currentValue = currentParameterValue = awPlugin.impl->getParameter (index);
        curveHasChanged();
        listeners.call (&Listener::currentValueChanged, *this);
    }

    void setConversionRange (juce::NormalisableRange<float> range)
    {
        conversionRange = range;

        stringToValueFunction = [this] (juce::String v) { return conversionRange.convertTo0to1 (v.getFloatValue()); };
    }

    void parameterChanged (float newValue, bool byAutomation) override
    {
        if (awPlugin.impl->getParameter (index) != newValue)
        {
            if (! byAutomation)
                markAsChanged();

            awPlugin.impl->setParameter (index, newValue);
        }
    }

    juce::String getCurrentValueAsString() override
    {
        char paramText[kVstMaxParamStrLen];
        awPlugin.impl->getParameterDisplay (index, paramText);
        auto t1 = juce::String (paramText);

        char labelText[kVstMaxParamStrLen];
        awPlugin.impl->getParameterLabel (index, labelText);
        auto t2 = juce::String (labelText);

        if (t2.isNotEmpty())
            return t1 + " " + t2;
        return t1;
    }

    void markAsChanged() const noexcept
    {
        getEdit().pluginChanged (awPlugin);
    }

    static juce::String getParamId (AirWindowsPlugin& p, int idx)
    {
        return getParamName (p, idx).toLowerCase().retainCharacters ("abcdefghijklmnopqrstuvwxyz");
    }

    static juce::String getParamName (AirWindowsPlugin& p, int idx)
    {
        char paramName[kVstMaxParamStrLen];
        p.impl->getParameterName (idx, paramName);
        return juce::String (paramName);
    }

    AirWindowsPlugin& awPlugin;
    int index = 0;
    float defaultValue = 0.0f;
    juce::NormalisableRange<float> conversionRange;
};

}} // namespace tracktion { inline namespace engine
