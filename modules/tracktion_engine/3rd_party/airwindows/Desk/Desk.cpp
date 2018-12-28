/* ========================================
 *  Desk - Desk.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Desk_H
#include "Desk.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Desk(audioMaster);}

Desk::Desk(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	lastSampleL = 0.0;
	lastOutSampleL = 0.0;
	lastSlewL = 0.0;
	lastSampleR = 0.0;
	lastOutSampleR = 0.0;
	lastSlewR = 0.0;
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

Desk::~Desk() {}
VstInt32 Desk::getVendorVersion () {return 1000;}
void Desk::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Desk::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 Desk::getChunk (void** data, bool isPreset)
{
	return kNumParameters * sizeof(float);
}

VstInt32 Desk::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	return 0;
}

void Desk::setParameter(VstInt32 index, float value) {
}

float Desk::getParameter(VstInt32 index) {
	return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Desk::getParameterName(VstInt32 index, char *text) {
}

void Desk::getParameterDisplay(VstInt32 index, char *text) {
}

void Desk::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 Desk::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Desk::getEffectName(char* name) {
    vst_strncpy(name, "Desk", kVstMaxProductStrLen); return true;
}

VstPlugCategory Desk::getPlugCategory() {return kPlugCategEffect;}

bool Desk::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Desk", kVstMaxProductStrLen); return true;
}

bool Desk::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
