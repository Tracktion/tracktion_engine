/* ========================================
 *  HermeTrim - HermeTrim.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __HermeTrim_H
#include "HermeTrim.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new HermeTrim(audioMaster);}

HermeTrim::HermeTrim(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.5;
	C = 0.5;
	D = 0.5;
	E = 0.5;
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

HermeTrim::~HermeTrim() {}
VstInt32 HermeTrim::getVendorVersion () {return 1000;}
void HermeTrim::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void HermeTrim::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 HermeTrim::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	chunkData[2] = C;
	chunkData[3] = D;
	chunkData[4] = E;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
	 started with. */

	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 HermeTrim::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	C = pinParameter(chunkData[2]);
	D = pinParameter(chunkData[3]);
	E = pinParameter(chunkData[4]);
	/* We're ignoring byteSize as we found it to be a filthy liar */

	/* calculate any other fields you need here - you could copy in
	 code from setParameter() here. */
	return 0;
}

void HermeTrim::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float HermeTrim::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void HermeTrim::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Left", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Right", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Mid", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Side", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Master", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void HermeTrim::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string ((A*3.0)-1.5, text, kVstMaxParamStrLen); break;
        case kParamB: float2string ((B*3.0)-1.5, text, kVstMaxParamStrLen); break;
        case kParamC: float2string ((C*3.0)-1.5, text, kVstMaxParamStrLen); break;
        case kParamD: float2string ((D*3.0)-1.5, text, kVstMaxParamStrLen); break;
        case kParamE: float2string ((E*3.0)-1.5, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void HermeTrim::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 HermeTrim::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool HermeTrim::getEffectName(char* name) {
    vst_strncpy(name, "HermeTrim", kVstMaxProductStrLen); return true;
}

VstPlugCategory HermeTrim::getPlugCategory() {return kPlugCategEffect;}

bool HermeTrim::getProductString(char* text) {
  	vst_strncpy (text, "airwindows HermeTrim", kVstMaxProductStrLen); return true;
}

bool HermeTrim::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
