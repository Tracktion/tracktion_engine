/* ========================================
 *  SurgeTide - SurgeTide.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __SurgeTide_H
#include "SurgeTide.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new SurgeTide(audioMaster);}

SurgeTide::SurgeTide(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.3;
	C = 1.0;
	fpNShapeAL = 0.0;
	fpNShapeBL = 0.0;
	fpNShapeAR = 0.0;
	fpNShapeBR = 0.0;
	flip = true;
	chaseA = 0.0;
	chaseB = 0.0;
	chaseC = 0.0;
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

SurgeTide::~SurgeTide() {}
VstInt32 SurgeTide::getVendorVersion () {return 1000;}
void SurgeTide::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void SurgeTide::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 SurgeTide::getChunk (void** data, bool isPreset)
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

VstInt32 SurgeTide::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void SurgeTide::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break; //percent. Using this value, it'll be 0-100 everywhere
        case kParamC: C = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float SurgeTide::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void SurgeTide::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "SurgeNode", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "SurgeRate", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void SurgeTide::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (C, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void SurgeTide::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, " ", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, " ", kVstMaxParamStrLen); break; //the percent
        case kParamC: vst_strncpy (text, " ", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 SurgeTide::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool SurgeTide::getEffectName(char* name) {
    vst_strncpy(name, "SurgeTide", kVstMaxProductStrLen); return true;
}

VstPlugCategory SurgeTide::getPlugCategory() {return kPlugCategEffect;}

bool SurgeTide::getProductString(char* text) {
  	vst_strncpy (text, "airwindows SurgeTide", kVstMaxProductStrLen); return true;
}

bool SurgeTide::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
