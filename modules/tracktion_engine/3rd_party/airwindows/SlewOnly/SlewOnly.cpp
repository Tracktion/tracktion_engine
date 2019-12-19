/* ========================================
 *  SlewOnly - SlewOnly.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __SlewOnly_H
#include "SlewOnly.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new SlewOnly(audioMaster);}

SlewOnly::SlewOnly(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    lastSampleL = 0.0;
    lastSampleR = 0.0;

    _canDo.insert("plugAsChannelInsert"); // plug-in can be used as a channel insert effect.
    _canDo.insert("plugAsSend"); // plug-in can be used as a send effect.
    _canDo.insert("x2in2out");
    setNumInputs(kNumInputs);
    setNumOutputs(kNumOutputs);
    setUniqueID(kUniqueId);
    canProcessReplacing();     // supports output replacing
    canDoubleReplacing();      // supports double precision processing
    vst_strncpy (_programName, "Default", kVstMaxProgNameLen); // default program name
}

SlewOnly::~SlewOnly() {}
VstInt32 SlewOnly::getVendorVersion () {return 1000;}
void SlewOnly::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void SlewOnly::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

void SlewOnly::setParameter(VstInt32 index, float value) {
}

float SlewOnly::getParameter(VstInt32 index) {
    return 0.0; //we only need to update the relevant name, this is simple to manage
}

void SlewOnly::getParameterName(VstInt32 index, char *text) {
}

void SlewOnly::getParameterDisplay(VstInt32 index, char *text) {
}

void SlewOnly::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 SlewOnly::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool SlewOnly::getEffectName(char* name) {
    vst_strncpy(name, "SlewOnly", kVstMaxProductStrLen); return true;
}

VstPlugCategory SlewOnly::getPlugCategory() {return kPlugCategEffect;}

bool SlewOnly::getProductString(char* text) {
    vst_strncpy (text, "airwindows SlewOnly", kVstMaxProductStrLen); return true;
}

bool SlewOnly::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
