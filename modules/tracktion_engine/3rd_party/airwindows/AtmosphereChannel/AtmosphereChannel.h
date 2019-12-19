/* ========================================
 *  AtmosphereChannel - AtmosphereChannel.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __AtmosphereChannel_H
#define __AtmosphereChannel_H

#ifndef __audioeffect__
#include "audioeffectx.h"
#endif

#include <set>
#include <string>
#include <math.h>

enum {
    kParamA = 0,
  kNumParameters = 1
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'atch';    //Change this to what the AU identity is!

class AtmosphereChannel :
    public AudioEffectX
{
public:
    AtmosphereChannel(audioMasterCallback audioMaster);
    ~AtmosphereChannel();
    virtual bool getEffectName(char* name);                       // The plug-in name
    virtual VstPlugCategory getPlugCategory();                    // The general category for the plug-in
    virtual bool getProductString(char* text);                    // This is a unique plug-in string provided by Steinberg
    virtual bool getVendorString(char* text);                     // Vendor info
    virtual VstInt32 getVendorVersion();                          // Version number
    virtual void processReplacing (float** inputs, float** outputs, VstInt32 sampleFrames);
    virtual void processDoubleReplacing (double** inputs, double** outputs, VstInt32 sampleFrames);
    virtual void getProgramName(char *name);                      // read the name from the host
    virtual void setProgramName(char *name);                      // changes the name of the preset displayed in the host
    virtual VstInt32 getChunk (void** data, bool isPreset);
    virtual VstInt32 setChunk (void* data, VstInt32 byteSize, bool isPreset);
    virtual float getParameter(VstInt32 index);                   // get the parameter value at the specified index
    virtual void setParameter(VstInt32 index, float value);       // set the parameter at index to value
    virtual void getParameterLabel(VstInt32 index, char *text);  // label for the parameter (eg dB)
    virtual void getParameterName(VstInt32 index, char *text);    // name of the parameter
    virtual void getParameterDisplay(VstInt32 index, char *text); // text description of the current value
    virtual VstInt32 canDo(char *text);
private:
    char _programName[kVstMaxProgNameLen + 1];
    std::set< std::string > _canDo;

    double gainchase;
    double settingchase;
    double chasespeed;

    long double fpNShapeL;
    long double lastSampleAL;
    long double lastSampleBL;
    long double lastSampleCL;
    long double lastSampleDL;
    long double lastSampleEL;
    long double lastSampleFL;
    long double lastSampleGL;
    long double lastSampleHL;
    long double lastSampleIL;
    long double lastSampleJL;
    long double lastSampleKL;
    long double lastSampleLL;
    long double lastSampleML;

    long double fpNShapeR;
    long double lastSampleAR;
    long double lastSampleBR;
    long double lastSampleCR;
    long double lastSampleDR;
    long double lastSampleER;
    long double lastSampleFR;
    long double lastSampleGR;
    long double lastSampleHR;
    long double lastSampleIR;
    long double lastSampleJR;
    long double lastSampleKR;
    long double lastSampleLR;
    long double lastSampleMR;

    long double thresholdA;
    long double thresholdB;
    long double thresholdC;
    long double thresholdD;
    long double thresholdE;
    long double thresholdF;
    long double thresholdG;
    long double thresholdH;
    long double thresholdI;
    long double thresholdJ;
    long double thresholdK;
    long double thresholdL;
    long double thresholdM;

    float A;
};

#endif
