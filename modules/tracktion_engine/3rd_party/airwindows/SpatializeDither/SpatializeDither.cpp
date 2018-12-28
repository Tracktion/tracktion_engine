/* ========================================
 *  SpatializeDither - SpatializeDither.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __SpatializeDither_H
#include "SpatializeDither.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new SpatializeDither(audioMaster);}

SpatializeDither::SpatializeDither(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	contingentErrL = 0.0;
	contingentErrR = 0.0;
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

SpatializeDither::~SpatializeDither() {}
VstInt32 SpatializeDither::getVendorVersion () {return 1000;}
void SpatializeDither::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void SpatializeDither::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 SpatializeDither::getChunk (void** data, bool isPreset)
{
	return kNumParameters * sizeof(float);
}

VstInt32 SpatializeDither::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	return 0;
}

void SpatializeDither::setParameter(VstInt32 index, float value) {
 }

float SpatializeDither::getParameter(VstInt32 index) {
	return 0.0; //we only need to update the relevant name, this is simple to manage
}

void SpatializeDither::getParameterName(VstInt32 index, char *text) {
}

void SpatializeDither::getParameterDisplay(VstInt32 index, char *text) {
}

void SpatializeDither::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 SpatializeDither::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool SpatializeDither::getEffectName(char* name) {
    vst_strncpy(name, "SpatializeDither", kVstMaxProductStrLen); return true;
}

VstPlugCategory SpatializeDither::getPlugCategory() {return kPlugCategEffect;}

bool SpatializeDither::getProductString(char* text) {
  	vst_strncpy (text, "airwindows SpatializeDither", kVstMaxProductStrLen); return true;
}

bool SpatializeDither::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
