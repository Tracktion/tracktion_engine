/* ========================================
 *  RightoMono - RightoMono.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __RightoMono_H
#include "RightoMono.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new RightoMono(audioMaster);}

RightoMono::RightoMono(audioMasterCallback audioMaster) :
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

RightoMono::~RightoMono() {}
VstInt32 RightoMono::getVendorVersion () {return 1000;}
void RightoMono::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void RightoMono::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 RightoMono::getChunk (void** data, bool isPreset)
{
    return kNumParameters * sizeof(float);
}

VstInt32 RightoMono::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    return 0;
}

void RightoMono::setParameter(VstInt32 index, float value) {
}

float RightoMono::getParameter(VstInt32 index) {
    return 0.0; //we only need to update the relevant name, this is simple to manage
}

void RightoMono::getParameterName(VstInt32 index, char *text) {
}

void RightoMono::getParameterDisplay(VstInt32 index, char *text) {
}

void RightoMono::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 RightoMono::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool RightoMono::getEffectName(char* name) {
    vst_strncpy(name, "RightoMono", kVstMaxProductStrLen); return true;
}

VstPlugCategory RightoMono::getPlugCategory() {return kPlugCategEffect;}

bool RightoMono::getProductString(char* text) {
    vst_strncpy (text, "airwindows RightoMono", kVstMaxProductStrLen); return true;
}

bool RightoMono::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
