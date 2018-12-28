/* ========================================
 *  AQuickVoiceClip - AQuickVoiceClip.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __AQuickVoiceClip_H
#define __AQuickVoiceClip_H

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
const unsigned long kUniqueId = 'aqvc';    //Change this to what the AU identity is!

class AQuickVoiceClip :
    public AudioEffectX
{
public:
    AQuickVoiceClip(audioMasterCallback audioMaster);
    ~AQuickVoiceClip();
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


	double LataLast6Sample;
	double LataLast5Sample;
	double LataLast4Sample;
	double LataLast3Sample;
	double LataLast2Sample;
	double LataLast1Sample;
	double LataHalfwaySample;
	double LataHalfDrySample;
	double LataHalfDiffSample;
	double LataLastDiffSample;
	double LataDrySample;
	double LataDiffSample;
	double LataPrevDiffSample;

	double RataLast6Sample;
	double RataLast5Sample;
	double RataLast4Sample;
	double RataLast3Sample;
	double RataLast2Sample;
	double RataLast1Sample;
	double RataHalfwaySample;
	double RataHalfDrySample;
	double RataHalfDiffSample;
	double RataLastDiffSample;
	double RataDrySample;
	double RataDiffSample;
	double RataPrevDiffSample;

	double ataK1;
	double ataK2;
	double ataK3;
	double ataK4;
	double ataK5;
	double ataK6;
	double ataK7;
	double ataK8; //end antialiasing variables

	double LlastSample;
	double LlastOutSample;
	double LlastOut2Sample;
	double LlastOut3Sample;
	double LlpDepth;
	double Lovershoot;
	double Loverall;
	double LiirSampleA;
	double LiirSampleB;
	double LiirSampleC;
	double LiirSampleD;

	double RlastSample;
	double RlastOutSample;
	double RlastOut2Sample;
	double RlastOut3Sample;
	double RlpDepth;
	double Rovershoot;
	double Roverall;
	double RiirSampleA;
	double RiirSampleB;
	double RiirSampleC;
	double RiirSampleD;
	bool flip;

	long double fpNShapeLA;
	long double fpNShapeLB;
	long double fpNShapeRA;
	long double fpNShapeRB;
	bool fpFlip;
	//default stuff

    float A;
};

#endif
