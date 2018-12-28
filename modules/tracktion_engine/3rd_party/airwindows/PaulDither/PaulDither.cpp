/* ========================================
 *  PaulDither - PaulDither.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PaulDither_H
#include "PaulDither.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new PaulDither(audioMaster);}

PaulDither::PaulDither(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	previousDitherL = 0.0;
	previousDitherR = 0.0;
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

PaulDither::~PaulDither() {}
VstInt32 PaulDither::getVendorVersion () {return 1000;}
void PaulDither::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void PaulDither::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 PaulDither::getChunk (void** data, bool isPreset)
{
	return kNumParameters * sizeof(float);
}

VstInt32 PaulDither::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	return 0;
}

void PaulDither::setParameter(VstInt32 index, float value) {
}

float PaulDither::getParameter(VstInt32 index) {
	return 0.0; //we only need to update the relevant name, this is simple to manage
}

void PaulDither::getParameterName(VstInt32 index, char *text) {
}

void PaulDither::getParameterDisplay(VstInt32 index, char *text) {
}

void PaulDither::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 PaulDither::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool PaulDither::getEffectName(char* name) {
    vst_strncpy(name, "PaulDither", kVstMaxProductStrLen); return true;
}

VstPlugCategory PaulDither::getPlugCategory() {return kPlugCategEffect;}

bool PaulDither::getProductString(char* text) {
  	vst_strncpy (text, "airwindows PaulDither", kVstMaxProductStrLen); return true;
}

bool PaulDither::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
