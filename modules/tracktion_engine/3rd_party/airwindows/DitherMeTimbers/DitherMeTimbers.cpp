/* ========================================
 *  DitherMeTimbers - DitherMeTimbers.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DitherMeTimbers_H
#include "DitherMeTimbers.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new DitherMeTimbers(audioMaster);}

DitherMeTimbers::DitherMeTimbers(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	noiseShapingL = 0.0;
	noiseShapingR = 0.0;
	lastSampleL = 0.0;
	lastSample2L = 0.0;
	lastSampleR = 0.0;
	lastSample2R = 0.0;
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

DitherMeTimbers::~DitherMeTimbers() {}
VstInt32 DitherMeTimbers::getVendorVersion () {return 1000;}
void DitherMeTimbers::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void DitherMeTimbers::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!


VstInt32 DitherMeTimbers::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
	 started with. */

	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 DitherMeTimbers::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	return 0;
}

void DitherMeTimbers::setParameter(VstInt32 index, float value) {
}

float DitherMeTimbers::getParameter(VstInt32 index) {
	return 0.0; //we only need to update the relevant name, this is simple to manage
}

void DitherMeTimbers::getParameterName(VstInt32 index, char *text) {
}

void DitherMeTimbers::getParameterDisplay(VstInt32 index, char *text) {
}

void DitherMeTimbers::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 DitherMeTimbers::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool DitherMeTimbers::getEffectName(char* name) {
    vst_strncpy(name, "DitherMeTimbers", kVstMaxProductStrLen); return true;
}

VstPlugCategory DitherMeTimbers::getPlugCategory() {return kPlugCategEffect;}

bool DitherMeTimbers::getProductString(char* text) {
  	vst_strncpy (text, "airwindows DitherMeTimbers", kVstMaxProductStrLen); return true;
}

bool DitherMeTimbers::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
