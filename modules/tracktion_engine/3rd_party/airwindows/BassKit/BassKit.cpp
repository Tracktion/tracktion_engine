/* ========================================
 *  BassKit - BassKit.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __BassKit_H
#include "BassKit.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new BassKit(audioMaster);}

BassKit::BassKit(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.5;
	C = 0.5;
	D = 0.5;

	WasNegative = false;
	SubOctave = false;
	flip = false;
	bflip = 0;
	iirDriveSampleA = 0.0;
	iirDriveSampleB = 0.0;
	iirDriveSampleC = 0.0;
	iirDriveSampleD = 0.0;
	iirDriveSampleE = 0.0;
	iirDriveSampleF = 0.0;

	iirHeadBumpA = 0.0;
	iirHeadBumpB = 0.0;
	iirHeadBumpC = 0.0;

	iirSubBumpA = 0.0;
	iirSubBumpB = 0.0;
	iirSubBumpC = 0.0;

	lastHeadBump = 0.0;
	lastSubBump = 0.0;

	iirSampleA = 0.0;
	iirSampleB = 0.0;
	iirSampleC = 0.0;
	iirSampleD = 0.0;
	iirSampleE = 0.0;
	iirSampleF = 0.0;
	iirSampleG = 0.0;
	iirSampleH = 0.0;
	iirSampleI = 0.0;
	iirSampleJ = 0.0;
	iirSampleK = 0.0;
	iirSampleL = 0.0;
	iirSampleM = 0.0;
	iirSampleN = 0.0;
	iirSampleO = 0.0;
	iirSampleP = 0.0;
	iirSampleQ = 0.0;
	iirSampleR = 0.0;
	iirSampleS = 0.0;
	iirSampleT = 0.0;
	iirSampleU = 0.0;
	iirSampleV = 0.0;
	iirSampleW = 0.0;
	iirSampleX = 0.0;
	iirSampleY = 0.0;
	iirSampleZ = 0.0;

	oscGate = 1.0;

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

BassKit::~BassKit() {}
VstInt32 BassKit::getVendorVersion () {return 1000;}
void BassKit::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void BassKit::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 BassKit::getChunk (void** data, bool isPreset)
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

VstInt32 BassKit::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void BassKit::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float BassKit::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void BassKit::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Drive", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Voicing", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "BassOut", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "SubOut", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void BassKit::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break;
        case kParamC: float2string ((C*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string ((D*2.0)-1.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void BassKit::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 BassKit::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool BassKit::getEffectName(char* name) {
    vst_strncpy(name, "BassKit", kVstMaxProductStrLen); return true;
}

VstPlugCategory BassKit::getPlugCategory() {return kPlugCategEffect;}

bool BassKit::getProductString(char* text) {
  	vst_strncpy (text, "airwindows BassKit", kVstMaxProductStrLen); return true;
}

bool BassKit::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
