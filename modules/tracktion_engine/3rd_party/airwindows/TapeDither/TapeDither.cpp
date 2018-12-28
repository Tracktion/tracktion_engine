/* ========================================
 *  TapeDither - TapeDither.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __TapeDither_H
#include "TapeDither.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new TapeDither(audioMaster);}

TapeDither::TapeDither(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	previousDither1L = 0.0;
	previousDither2L = 0.0;
	previousDither3L = 0.0;
	previousDither4L = 0.0;
	previousDither1R = 0.0;
	previousDither2R = 0.0;
	previousDither3R = 0.0;
	previousDither4R = 0.0;
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

TapeDither::~TapeDither() {}
VstInt32 TapeDither::getVendorVersion () {return 1000;}
void TapeDither::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void TapeDither::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 TapeDither::getChunk (void** data, bool isPreset)
{
	return kNumParameters * sizeof(float);
}

VstInt32 TapeDither::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	return 0;
}

void TapeDither::setParameter(VstInt32 index, float value) {
}

float TapeDither::getParameter(VstInt32 index) {
	return 0.0; //we only need to update the relevant name, this is simple to manage
}

void TapeDither::getParameterName(VstInt32 index, char *text) {
}

void TapeDither::getParameterDisplay(VstInt32 index, char *text) {
}

void TapeDither::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 TapeDither::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool TapeDither::getEffectName(char* name) {
    vst_strncpy(name, "TapeDither", kVstMaxProductStrLen); return true;
}

VstPlugCategory TapeDither::getPlugCategory() {return kPlugCategEffect;}

bool TapeDither::getProductString(char* text) {
  	vst_strncpy (text, "airwindows TapeDither", kVstMaxProductStrLen); return true;
}

bool TapeDither::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
