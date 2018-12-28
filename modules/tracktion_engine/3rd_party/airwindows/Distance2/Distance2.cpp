/* ========================================
 *  Distance2 - Distance2.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Distance2_H
#include "Distance2.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Distance2(audioMaster);}

Distance2::Distance2(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.85;
	B = 0.618;
	C = 0.618;

	thirdSampleL = lastSampleL = 0.0;
	thirdSampleR = lastSampleR = 0.0;

	lastSampleAL = 0.0;
	lastSampleBL = 0.0;
	lastSampleCL = 0.0;
	lastSampleDL = 0.0;
	lastSampleEL = 0.0;
	lastSampleFL = 0.0;
	lastSampleGL = 0.0;
	lastSampleHL = 0.0;
	lastSampleIL = 0.0;
	lastSampleJL = 0.0;
	lastSampleKL = 0.0;
	lastSampleLL = 0.0;
	lastSampleML = 0.0;

	lastSampleAR = 0.0;
	lastSampleBR = 0.0;
	lastSampleCR = 0.0;
	lastSampleDR = 0.0;
	lastSampleER = 0.0;
	lastSampleFR = 0.0;
	lastSampleGR = 0.0;
	lastSampleHR = 0.0;
	lastSampleIR = 0.0;
	lastSampleJR = 0.0;
	lastSampleKR = 0.0;
	lastSampleLR = 0.0;
	lastSampleMR = 0.0;

	thresholdA = 0.618033988749894;
	thresholdB = 0.679837387624884;
	thresholdC = 0.747821126387373;
	thresholdD = 0.82260323902611;
	thresholdE = 0.904863562928721;
	thresholdF = 0.995349919221593;
	thresholdG = 1.094884911143752;
	thresholdH = 1.204373402258128;
	thresholdI = 1.32481074248394;
	thresholdJ = 1.457291816732335;
	thresholdK = 1.603020998405568;
	thresholdL = 1.763323098246125;
	thresholdM = 1.939655408070737;

	fpNShapeL = 0.0;
	fpNShapeR = 0.0;
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

Distance2::~Distance2() {}
VstInt32 Distance2::getVendorVersion () {return 1000;}
void Distance2::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Distance2::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Distance2::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	chunkData[2] = C;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
	 started with. */

	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 Distance2::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	C = pinParameter(chunkData[2]);
	/* We're ignoring byteSize as we found it to be a filthy liar */

	/* calculate any other fields you need here - you could copy in
	 code from setParameter() here. */
	return 0;
}

void Distance2::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Distance2::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Distance2::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Atmosph", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Darken", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Distance2::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (C, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Distance2::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Distance2::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Distance2::getEffectName(char* name) {
    vst_strncpy(name, "Distance2", kVstMaxProductStrLen); return true;
}

VstPlugCategory Distance2::getPlugCategory() {return kPlugCategEffect;}

bool Distance2::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Distance2", kVstMaxProductStrLen); return true;
}

bool Distance2::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
