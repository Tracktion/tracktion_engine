/* ========================================
 *  FromTape - FromTape.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __FromTape_H
#include "FromTape.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new FromTape(audioMaster);}

FromTape::FromTape(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.5;
	C = 0.5;
	D = 0.5;
	E = 1.0;

	iirMidRollerAL = 0.0;
	iirMidRollerBL = 0.0;
	iirMidRollerCL = 0.0;

	iirMidRollerAR = 0.0;
	iirMidRollerBR = 0.0;
	iirMidRollerCR = 0.0;

	iirSampleAL = 0.0;
	iirSampleBL = 0.0;
	iirSampleCL = 0.0;
	iirSampleDL = 0.0;
	iirSampleEL = 0.0;
	iirSampleFL = 0.0;
	iirSampleGL = 0.0;
	iirSampleHL = 0.0;
	iirSampleIL = 0.0;
	iirSampleJL = 0.0;
	iirSampleKL = 0.0;
	iirSampleLL = 0.0;
	iirSampleML = 0.0;
	iirSampleNL = 0.0;
	iirSampleOL = 0.0;
	iirSamplePL = 0.0;
	iirSampleQL = 0.0;
	iirSampleRL = 0.0;
	iirSampleSL = 0.0;
	iirSampleTL = 0.0;
	iirSampleUL = 0.0;
	iirSampleVL = 0.0;
	iirSampleWL = 0.0;
	iirSampleXL = 0.0;
	iirSampleYL = 0.0;
	iirSampleZL = 0.0;

	iirSampleAR = 0.0;
	iirSampleBR = 0.0;
	iirSampleCR = 0.0;
	iirSampleDR = 0.0;
	iirSampleER = 0.0;
	iirSampleFR = 0.0;
	iirSampleGR = 0.0;
	iirSampleHR = 0.0;
	iirSampleIR = 0.0;
	iirSampleJR = 0.0;
	iirSampleKR = 0.0;
	iirSampleLR = 0.0;
	iirSampleMR = 0.0;
	iirSampleNR = 0.0;
	iirSampleOR = 0.0;
	iirSamplePR = 0.0;
	iirSampleQR = 0.0;
	iirSampleRR = 0.0;
	iirSampleSR = 0.0;
	iirSampleTR = 0.0;
	iirSampleUR = 0.0;
	iirSampleVR = 0.0;
	iirSampleWR = 0.0;
	iirSampleXR = 0.0;
	iirSampleYR = 0.0;
	iirSampleZR = 0.0;

	flip = 0;

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

FromTape::~FromTape() {}
VstInt32 FromTape::getVendorVersion () {return 1000;}
void FromTape::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void FromTape::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 FromTape::getChunk (void** data, bool isPreset)
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

VstInt32 FromTape::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void FromTape::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float FromTape::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void FromTape::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Louder", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Softer", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Weight", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void FromTape::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A*2, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (C, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (D*2, text, kVstMaxParamStrLen); break;
        case kParamE: float2string (E, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void FromTape::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 FromTape::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool FromTape::getEffectName(char* name) {
    vst_strncpy(name, "FromTape", kVstMaxProductStrLen); return true;
}

VstPlugCategory FromTape::getPlugCategory() {return kPlugCategEffect;}

bool FromTape::getProductString(char* text) {
  	vst_strncpy (text, "airwindows FromTape", kVstMaxProductStrLen); return true;
}

bool FromTape::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
