/* ========================================
 *  TPDFDither - TPDFDither.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __TPDFDither_H
#include "TPDFDither.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new TPDFDither(audioMaster);}

TPDFDither::TPDFDither(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{

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

TPDFDither::~TPDFDither() {}
VstInt32 TPDFDither::getVendorVersion () {return 1000;}
void TPDFDither::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void TPDFDither::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 TPDFDither::getChunk (void** data, bool isPreset)
{
	return kNumParameters * sizeof(float);
}

VstInt32 TPDFDither::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	return 0;
}

void TPDFDither::setParameter(VstInt32 index, float value) {
}

float TPDFDither::getParameter(VstInt32 index) {
 return 0.0; //we only need to update the relevant name, this is simple to manage
}

void TPDFDither::getParameterName(VstInt32 index, char *text) {
}

void TPDFDither::getParameterDisplay(VstInt32 index, char *text) {
}

void TPDFDither::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 TPDFDither::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool TPDFDither::getEffectName(char* name) {
    vst_strncpy(name, "TPDFDither", kVstMaxProductStrLen); return true;
}

VstPlugCategory TPDFDither::getPlugCategory() {return kPlugCategEffect;}

bool TPDFDither::getProductString(char* text) {
  	vst_strncpy (text, "airwindows TPDFDither", kVstMaxProductStrLen); return true;
}

bool TPDFDither::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
