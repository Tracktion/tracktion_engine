/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
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

void float2string (float f, char* text, int len)
{
    int decimals = 0;
    if (std::fabs (f) >= 10.0)
        decimals = 1;
    else if (std::fabs (f) > 1.0)
        decimals = 2;
    else
        decimals = 3;

    String str (f, decimals);
    str.copyToUTF8 (text, (size_t)len);
}

void int2string (float i, char* text, int len)
{
    String str (i);
    str.copyToUTF8 (text, (size_t)len);
}

void dB2string (float value, char* text, int maxLen)
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
    AirWindowsBase (AirWindowsCallback* callback_, int prog, int param)
        : numPrograms (prog), numParams (param), callback (callback_)
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
    virtual VstInt32 getChunk (void** data, bool isPreset)                          { ignoreUnused (data, isPreset); return 0; };
    virtual VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset)        { ignoreUnused (data, byteSize, isPreset); return 0; };
    virtual float getParameter(VstInt32 index)                    = 0;
    virtual void setParameter(VstInt32 index, float value)        = 0;
    virtual void getParameterLabel(VstInt32 index, char *text)    = 0;
    virtual void getParameterName(VstInt32 index, char *text)     = 0;
    virtual void getParameterDisplay(VstInt32 index, char *text)  = 0;
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

}
