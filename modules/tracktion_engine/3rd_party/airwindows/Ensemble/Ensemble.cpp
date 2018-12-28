/* ========================================
 *  Ensemble - Ensemble.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Ensemble_H
#include "Ensemble.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Ensemble(audioMaster);}

Ensemble::Ensemble(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.0;
	C = 1.0;
	D = 1.0;

	for(int count = 0; count < totalsamples-1; count++) {dL[count] = 0; dR[count] = 0;}
	for(int count = 0; count < 49; count++) {sweep[count] = 3.141592653589793238 / 2.0;}
	gcount = 0;
	airPrevL = 0.0;
	airEvenL = 0.0;
	airOddL = 0.0;
	airFactorL = 0.0;
	airPrevR = 0.0;
	airEvenR = 0.0;
	airOddR = 0.0;
	airFactorR = 0.0;

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

Ensemble::~Ensemble() {}
VstInt32 Ensemble::getVendorVersion () {return 1000;}
void Ensemble::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Ensemble::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Ensemble::getChunk (void** data, bool isPreset)
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

VstInt32 Ensemble::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void Ensemble::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Ensemble::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Ensemble::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Ensemble", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Fullness", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Brighten", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Ensemble::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (floor((A*46.0)+2.9), text, kVstMaxParamStrLen); break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (C, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (D, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Ensemble::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "vox", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, " ", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, " ", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, " ", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Ensemble::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Ensemble::getEffectName(char* name) {
    vst_strncpy(name, "Ensemble", kVstMaxProductStrLen); return true;
}

VstPlugCategory Ensemble::getPlugCategory() {return kPlugCategEffect;}

bool Ensemble::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Ensemble", kVstMaxProductStrLen); return true;
}

bool Ensemble::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
