/* ========================================
 *  Righteous4 - Righteous4.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Righteous4_H
#define __Righteous4_H

#ifndef __audioeffect__
#include "audioeffectx.h"
#endif

#include <set>
#include <string>
#include <math.h>

enum {
    kParamA = 0,
    kParamB = 1,
  kNumParameters = 2
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'rigk';    //Change this to what the AU identity is!

class Righteous4 :
    public AudioEffectX
{
public:
    Righteous4(audioMasterCallback audioMaster);
    ~Righteous4();
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

    double leftSampleA;
    double leftSampleB;
    double leftSampleC;
    double leftSampleD;
    double leftSampleE;
    double leftSampleF;
    double leftSampleG;
    double leftSampleH;
    double leftSampleI;
    double leftSampleJ;
    double leftSampleK;
    double leftSampleL;
    double leftSampleM;
    double leftSampleN;
    double leftSampleO;
    double leftSampleP;
    double leftSampleQ;
    double leftSampleR;
    double leftSampleS;
    double leftSampleT;
    double leftSampleU;
    double leftSampleV;
    double leftSampleW;
    double leftSampleX;
    double leftSampleY;
    double leftSampleZ;

    double rightSampleA;
    double rightSampleB;
    double rightSampleC;
    double rightSampleD;
    double rightSampleE;
    double rightSampleF;
    double rightSampleG;
    double rightSampleH;
    double rightSampleI;
    double rightSampleJ;
    double rightSampleK;
    double rightSampleL;
    double rightSampleM;
    double rightSampleN;
    double rightSampleO;
    double rightSampleP;
    double rightSampleQ;
    double rightSampleR;
    double rightSampleS;
    double rightSampleT;
    double rightSampleU;
    double rightSampleV;
    double rightSampleW;
    double rightSampleX;
    double rightSampleY;
    double rightSampleZ;

    double bynL[13];
    long double noiseShapingL;
    double lastSampleL;
    double IIRsampleL;
    double gwPrevL;
    double gwAL;
    double gwBL;

    double bynR[13];
    long double noiseShapingR;
    double lastSampleR;
    double IIRsampleR;
    double gwPrevR;
    double gwAR;
    double gwBR;

    long double fpNShapeL;
    long double fpNShapeR;
    //default stuff

    float A;
    float B;
};

#endif
