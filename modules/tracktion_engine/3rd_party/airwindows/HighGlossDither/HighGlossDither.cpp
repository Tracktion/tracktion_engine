/* ========================================
 *  HighGlossDither - HighGlossDither.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __HighGlossDither_H
#include "HighGlossDither.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new HighGlossDither(audioMaster);}

HighGlossDither::HighGlossDither(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	Position = 99999999;
	flip = false;
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

HighGlossDither::~HighGlossDither() {}
VstInt32 HighGlossDither::getVendorVersion () {return 1000;}
void HighGlossDither::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void HighGlossDither::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!


VstInt32 HighGlossDither::getChunk (void** data, bool isPreset)
{
	return kNumParameters * sizeof(float);
}

VstInt32 HighGlossDither::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	return 0;
}

void HighGlossDither::setParameter(VstInt32 index, float value) {
}

float HighGlossDither::getParameter(VstInt32 index) {
	return 0.0; //we only need to update the relevant name, this is simple to manage
}

void HighGlossDither::getParameterName(VstInt32 index, char *text) {
}

void HighGlossDither::getParameterDisplay(VstInt32 index, char *text) {
}

void HighGlossDither::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 HighGlossDither::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool HighGlossDither::getEffectName(char* name) {
    vst_strncpy(name, "HighGlossDither", kVstMaxProductStrLen); return true;
}

VstPlugCategory HighGlossDither::getPlugCategory() {return kPlugCategEffect;}

bool HighGlossDither::getProductString(char* text) {
  	vst_strncpy (text, "airwindows HighGlossDither", kVstMaxProductStrLen); return true;
}

bool HighGlossDither::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
