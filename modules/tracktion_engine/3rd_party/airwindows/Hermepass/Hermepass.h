/* ========================================
 *  Hermepass - Hermepass.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Hermepass_H
#define __Hermepass_H

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
const unsigned long kUniqueId = 'herm';    //Change this to what the AU identity is!

class Hermepass :
    public AudioEffectX
{
public:
    Hermepass(audioMasterCallback audioMaster);
    ~Hermepass();
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

	long double fpNShapeLA;
	long double fpNShapeLB;
	long double fpNShapeRA;
	long double fpNShapeRB;
	bool fpFlip;
	//default stuff
	double iirAL;
	double iirBL; //first stage is the flipping one, for lowest slope. It is always engaged, and is the highest one
	double iirCL; //we introduce the second pole at the same frequency, to become a pseudo-Capacitor behavior
	double iirDL;
	double iirEL;
	double iirFL; //our slope control will have a pow() on it so that the high orders are way to the right side
	double iirGL;
	double iirHL; //seven poles max, and the final pole is always at 20hz directly.

	double iirAR;
	double iirBR; //first stage is the flipping one, for lowest slope. It is always engaged, and is the highest one
	double iirCR; //we introduce the second pole at the same frequency, to become a pseudo-Capacitor behavior
	double iirDR;
	double iirER;
	double iirFR; //our slope control will have a pow() on it so that the high orders are way to the right side
	double iirGR;
	double iirHR; //seven poles max, and the final pole is always at 20hz directly.



    float A;
    float B;
};

#endif
