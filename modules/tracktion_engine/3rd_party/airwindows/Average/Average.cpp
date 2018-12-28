/* ========================================
 *  Average - Average.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Average_H
#include "Average.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Average(audioMaster);}

Average::Average(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	B = 1.0;

	for(int count = 0; count < 11; count++) {bL[count] = 0.0; bR[count] = 0.0; f[count] = 0.0;}

	fpNShapeLA = 0.0;
	fpNShapeLB = 0.0;
	fpNShapeRA = 0.0;
	fpNShapeRB = 0.0;
	fpFlip = true;
	//this is reset: values being initialized only once. Startup values, whatever they are.

    _canDo.insert("plugAsChannelInsert"); // plug-in can be used as a channel insert effect.
    _canDo.insert("plugAsSend"); // plug-in can be used as a send effect.
    _canDo.insert("x2in2out");
    setNumInputs(kNumInputs);
    setNumOutputs(kNumOutputs);
    setUniqueID(kUniqueId);
    canProcessReplacing();     // supports output replacing
    canDoubleReplacing();      // supports double precision processing
	programsAreChunks(true);
    vst_strncpy (_programName, "Default", kVstMaxProgNameLen); // default program name
}

Average::~Average() {}
VstInt32 Average::getVendorVersion () {return 1000;}
void Average::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Average::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Average::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
	 started with. */

	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 Average::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	/* We're ignoring byteSize as we found it to be a filthy liar */

	/* calculate any other fields you need here - you could copy in
	 code from setParameter() here. */
	return 0;
}

void Average::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break; //percent. Using this value, it'll be 0-100 everywhere
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Average::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Average::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Average", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Average::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string ((A * 9.0)+1.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break; //also display 0-1 as percent
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Average::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "taps", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, " ", kVstMaxParamStrLen); break; //the percent
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Average::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Average::getEffectName(char* name) {
    vst_strncpy(name, "Average", kVstMaxProductStrLen); return true;
}

VstPlugCategory Average::getPlugCategory() {return kPlugCategEffect;}

bool Average::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Average", kVstMaxProductStrLen); return true;
}

bool Average::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
