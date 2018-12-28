/* ========================================
 *  FathomFive - FathomFive.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __FathomFive_H
#include "FathomFive.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new FathomFive(audioMaster);}

FathomFive::FathomFive(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 1.0;
	B = 0.0;
	C = 0.5;
	D = 1.0;
	WasNegativeL = false;
	SubOctaveL = false;
	iirSampleLA = 0.0;
	iirSampleLB = 0.0;
	iirSampleLC = 0.0;
	iirSampleLD = 0.0;
	WasNegativeR = false;
	SubOctaveR = false;
	iirSampleRA = 0.0;
	iirSampleRB = 0.0;
	iirSampleRC = 0.0;
	iirSampleRD = 0.0;


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

FathomFive::~FathomFive() {}
VstInt32 FathomFive::getVendorVersion () {return 1000;}
void FathomFive::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void FathomFive::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 FathomFive::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	chunkData[2] = C;
	chunkData[3] = D;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
	 started with. */

	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 FathomFive::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	C = pinParameter(chunkData[2]);
	D = pinParameter(chunkData[3]);
	/* We're ignoring byteSize as we found it to be a filthy liar */

	/* calculate any other fields you need here - you could copy in
	 code from setParameter() here. */
	return 0;
}

void FathomFive::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
	//we can also set other defaults here, and do calculations that only have to happen
	//once when parameters actually change. Here is the 'popup' setting its (global) values.
	//variables can also be set in the processreplacing loop, and there they'll be set every buffersize
	//here they're set when a parameter's actually changed, which should be less frequent, but
	//you must use global variables in the FathomFive.h file to do it.
//	switch((VstInt32)( D * 3.999 ))
//	{
//		case 0: fpFlip = true; break; //choice A
//		case 1: fpFlip = false; break; //choice B
//		case 2: break; //choice C
//		case 3: break; //choice D
//		default: break; //should not happen
//	}
	//this relates to using D as a 'popup' and changing things based on that switch.
	//we are using fpFlip just because it's already there globally, as an example.
}

float FathomFive::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void FathomFive::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Root Note", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "SubOctave", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Frequency", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void FathomFive::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (C, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (D, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void FathomFive::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 FathomFive::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1 : 1; } // 1 = yes, -1 = no, 0 = don't know

bool FathomFive::getEffectName(char* name) {
    vst_strncpy(name, "FathomFive", kVstMaxProductStrLen); return true;
}

VstPlugCategory FathomFive::getPlugCategory() {return kPlugCategEffect;}

bool FathomFive::getProductString(char* text) {
  	vst_strncpy (text, "airwindows FathomFive", kVstMaxProductStrLen); return true;
}

bool FathomFive::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
