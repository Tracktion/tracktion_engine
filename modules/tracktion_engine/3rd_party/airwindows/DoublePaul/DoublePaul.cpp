/* ========================================
 *  DoublePaul - DoublePaul.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DoublePaul_H
#include "DoublePaul.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new DoublePaul(audioMaster);}

DoublePaul::DoublePaul(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    for(int count = 0; count < 11; count++) {bL[count] = 0.0;bR[count] = 0.0;}
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

DoublePaul::~DoublePaul() {}
VstInt32 DoublePaul::getVendorVersion () {return 1000;}
void DoublePaul::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void DoublePaul::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!


VstInt32 DoublePaul::getChunk (void** data, bool isPreset)
{
    return kNumParameters * sizeof(float);
}

VstInt32 DoublePaul::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    return 0;
}

void DoublePaul::setParameter(VstInt32 index, float value) {
}

float DoublePaul::getParameter(VstInt32 index) {
    return 0.0; //we only need to update the relevant name, this is simple to manage
}

void DoublePaul::getParameterName(VstInt32 index, char *text) {
}

void DoublePaul::getParameterDisplay(VstInt32 index, char *text) {
}

void DoublePaul::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 DoublePaul::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool DoublePaul::getEffectName(char* name) {
    vst_strncpy(name, "DoublePaul", kVstMaxProductStrLen); return true;
}

VstPlugCategory DoublePaul::getPlugCategory() {return kPlugCategEffect;}

bool DoublePaul::getProductString(char* text) {
    vst_strncpy (text, "airwindows DoublePaul", kVstMaxProductStrLen); return true;
}

bool DoublePaul::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
