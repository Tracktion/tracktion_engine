/* ========================================
 *  IronOxideClassic - IronOxideClassic.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __IronOxideClassic_H
#include "IronOxideClassic.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new IronOxideClassic(audioMaster);}

IronOxideClassic::IronOxideClassic(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5;
	B = 0.562341325190349;
	C = 0.5;

	for (int temp = 0; temp < 263; temp++) {dL[temp] = 0.0;dR[temp] = 0.0;}
	gcount = 0;
	fastIIRAL = fastIIRBL = slowIIRAL = slowIIRBL = 0.0;
	iirSampleAL = iirSampleBL = 0.0;
	fastIIRAR = fastIIRBR = slowIIRAR = slowIIRBR = 0.0;
	iirSampleAR = iirSampleBR = 0.0;

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

IronOxideClassic::~IronOxideClassic() {}
VstInt32 IronOxideClassic::getVendorVersion () {return 1000;}
void IronOxideClassic::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void IronOxideClassic::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 IronOxideClassic::getChunk (void** data, bool isPreset)
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

VstInt32 IronOxideClassic::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void IronOxideClassic::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float IronOxideClassic::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void IronOxideClassic::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Input Trim", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Tape Speed", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Output Trim", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void IronOxideClassic::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (((A*36.0)-18.0), text, kVstMaxParamStrLen); break;
        case kParamB: float2string (((B*B)*(B*B)*148.5)+1.5, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (((C*36.0)-18.0), text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void IronOxideClassic::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "ips", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 IronOxideClassic::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool IronOxideClassic::getEffectName(char* name) {
    vst_strncpy(name, "IronOxideClassic", kVstMaxProductStrLen); return true;
}

VstPlugCategory IronOxideClassic::getPlugCategory() {return kPlugCategEffect;}

bool IronOxideClassic::getProductString(char* text) {
  	vst_strncpy (text, "airwindows IronOxideClassic", kVstMaxProductStrLen); return true;
}

bool IronOxideClassic::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
