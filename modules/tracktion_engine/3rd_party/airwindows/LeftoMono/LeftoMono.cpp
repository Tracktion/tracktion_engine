/* ========================================
 *  LeftoMono - LeftoMono.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __LeftoMono_H
#include "LeftoMono.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new LeftoMono(audioMaster);}

LeftoMono::LeftoMono(audioMasterCallback audioMaster) :
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

LeftoMono::~LeftoMono() {}
VstInt32 LeftoMono::getVendorVersion () {return 1000;}
void LeftoMono::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void LeftoMono::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 LeftoMono::getChunk (void** data, bool isPreset)
{
    return kNumParameters * sizeof(float);
}

VstInt32 LeftoMono::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    return 0;
}

void LeftoMono::setParameter(VstInt32 index, float value) {
}

float LeftoMono::getParameter(VstInt32 index) {
    return 0.0; //we only need to update the relevant name, this is simple to manage
}

void LeftoMono::getParameterName(VstInt32 index, char *text) {
}

void LeftoMono::getParameterDisplay(VstInt32 index, char *text) {
}

void LeftoMono::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 LeftoMono::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool LeftoMono::getEffectName(char* name) {
    vst_strncpy(name, "LeftoMono", kVstMaxProductStrLen); return true;
}

VstPlugCategory LeftoMono::getPlugCategory() {return kPlugCategEffect;}

bool LeftoMono::getProductString(char* text) {
    vst_strncpy (text, "airwindows LeftoMono", kVstMaxProductStrLen); return true;
}

bool LeftoMono::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
