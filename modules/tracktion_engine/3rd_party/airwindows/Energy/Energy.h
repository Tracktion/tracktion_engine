/* ========================================
 *  Energy - Energy.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Energy_H
#define __Energy_H

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
	kParamF = 5,
	kParamG = 6,
	kParamH = 7,
	kParamI = 8,
  kNumParameters = 9
}; //

const int kNumPrograms = 0;
const int kNumInputs = 2;
const int kNumOutputs = 2;
const unsigned long kUniqueId = 'ener';    //Change this to what the AU identity is!

class Energy :
    public AudioEffectX
{
public:
    Energy(audioMasterCallback audioMaster);
    ~Energy();
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

	long double fpNShapeL;
	long double fpNShapeR;
	//default stuff

	double duoEvenL;
	double duoOddL;
	double duoFactorL;
	bool flip;

	double tripletAL;
	double tripletBL;
	double tripletCL;
	double tripletFactorL;
	int countA;

	double quadAL;
	double quadBL;
	double quadCL;
	double quadDL;
	double quadFactorL;
	int countB;

	double quintAL;
	double quintBL;
	double quintCL;
	double quintDL;
	double quintEL;
	double quintFactorL;
	int countC;

	double sextAL;
	double sextBL;
	double sextCL;
	double sextDL;
	double sextEL;
	double sextFL;
	double sextFactorL;
	int countD;

	double septAL;
	double septBL;
	double septCL;
	double septDL;
	double septEL;
	double septFL;
	double septGL;
	double septFactorL;
	int countE;

	double octAL;
	double octBL;
	double octCL;
	double octDL;
	double octEL;
	double octFL;
	double octGL;
	double octHL;
	double octFactorL;
	int countF;

	double nintAL;
	double nintBL;
	double nintCL;
	double nintDL;
	double nintEL;
	double nintFL;
	double nintGL;
	double nintHL;
	double nintIL;
	double nintFactorL;
	int countG;

	double PrevHL;
	double PrevGL;
	double PrevFL;
	double PrevEL;
	double PrevDL;
	double PrevCL;
	double PrevBL;
	double PrevAL;

	double duoEvenR;
	double duoOddR;
	double duoFactorR;

	double tripletAR;
	double tripletBR;
	double tripletCR;
	double tripletFactorR;

	double quadAR;
	double quadBR;
	double quadCR;
	double quadDR;
	double quadFactorR;

	double quintAR;
	double quintBR;
	double quintCR;
	double quintDR;
	double quintER;
	double quintFactorR;

	double sextAR;
	double sextBR;
	double sextCR;
	double sextDR;
	double sextER;
	double sextFR;
	double sextFactorR;

	double septAR;
	double septBR;
	double septCR;
	double septDR;
	double septER;
	double septFR;
	double septGR;
	double septFactorR;

	double octAR;
	double octBR;
	double octCR;
	double octDR;
	double octER;
	double octFR;
	double octGR;
	double octHR;
	double octFactorR;

	double nintAR;
	double nintBR;
	double nintCR;
	double nintDR;
	double nintER;
	double nintFR;
	double nintGR;
	double nintHR;
	double nintIR;
	double nintFactorR;

	double PrevHR;
	double PrevGR;
	double PrevFR;
	double PrevER;
	double PrevDR;
	double PrevCR;
	double PrevBR;
	double PrevAR;

    float A;
    float B;
    float C;
    float D;
    float E;
    float F;
    float G;
    float H;
    float I;

};

#endif
