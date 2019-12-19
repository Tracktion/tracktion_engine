/* ========================================
 *  Biquad2 - Biquad2.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Biquad2_H
#define __Biquad2_H

#ifndef __audioeffect__
#include "audioeffectx.h"
#endif

#include <set>
#include <string>
#include <math.h>

enum {
    kParamA = 0,
    kParamB = 1,
    kParamC = 2,
    kParamD = 3,
    kParamE = 4,
  kNumParameters = 5
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'biqe';    //Change this to what the AU identity is!

class Biquad2 :
    public AudioEffectX
{
public:
    Biquad2(audioMasterCallback audioMaster);
    ~Biquad2();
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

    long double biquad[15]; //note that this stereo form doesn't require L and R forms!
    //This is because so much of it is coefficients etc. that are the same on both channels.
    //So the stored samples are in 7-8-9-10 and 11-12-13-14, and freq/res/coefficients serve both.

    double bL[11];
    double bR[11];
    double f[11];
    double frequencychase;
    double resonancechase;
    double outputchase;
    double wetchase;
    double frequencysetting;
    double resonancesetting;
    double outputsetting;
    double wetsetting;
    double chasespeed;

    uint32_t fpd;
    //default stuff

    float A;
    float B;
    float C;
    float D;
    float E;
};

#endif
