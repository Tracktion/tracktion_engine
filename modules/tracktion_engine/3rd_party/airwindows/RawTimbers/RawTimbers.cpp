/* ========================================
 *  RawTimbers - RawTimbers.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __RawTimbers_H
#include "RawTimbers.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new RawTimbers(audioMaster);}

RawTimbers::RawTimbers(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    lastSampleL = 0.0;
    lastSample2L = 0.0;
    lastSampleR = 0.0;
    lastSample2R = 0.0;
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

RawTimbers::~RawTimbers() {}
VstInt32 RawTimbers::getVendorVersion () {return 1000;}
void RawTimbers::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void RawTimbers::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 RawTimbers::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 RawTimbers::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    return 0;
}

void RawTimbers::setParameter(VstInt32 index, float value) {
}

float RawTimbers::getParameter(VstInt32 index) {
    return 0.0; //we only need to update the relevant name, this is simple to manage
}

void RawTimbers::getParameterName(VstInt32 index, char *text) {
}

void RawTimbers::getParameterDisplay(VstInt32 index, char *text) {
}

void RawTimbers::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 RawTimbers::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool RawTimbers::getEffectName(char* name) {
    vst_strncpy(name, "RawTimbers", kVstMaxProductStrLen); return true;
}

VstPlugCategory RawTimbers::getPlugCategory() {return kPlugCategEffect;}

bool RawTimbers::getProductString(char* text) {
    vst_strncpy (text, "airwindows RawTimbers", kVstMaxProductStrLen); return true;
}

bool RawTimbers::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
