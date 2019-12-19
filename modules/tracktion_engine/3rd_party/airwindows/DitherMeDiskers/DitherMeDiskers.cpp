/* ========================================
 *  DitherMeDiskers - DitherMeDiskers.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DitherMeDiskers_H
#include "DitherMeDiskers.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new DitherMeDiskers(audioMaster);}

DitherMeDiskers::DitherMeDiskers(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    noiseShapingL = 0.0;
    noiseShapingR = 0.0;
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

DitherMeDiskers::~DitherMeDiskers() {}
VstInt32 DitherMeDiskers::getVendorVersion () {return 1000;}
void DitherMeDiskers::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void DitherMeDiskers::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!


VstInt32 DitherMeDiskers::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 DitherMeDiskers::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    return 0;
}

void DitherMeDiskers::setParameter(VstInt32 index, float value) {
}

float DitherMeDiskers::getParameter(VstInt32 index) {
    return 0.0; //we only need to update the relevant name, this is simple to manage
}

void DitherMeDiskers::getParameterName(VstInt32 index, char *text) {
}

void DitherMeDiskers::getParameterDisplay(VstInt32 index, char *text) {
}

void DitherMeDiskers::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 DitherMeDiskers::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool DitherMeDiskers::getEffectName(char* name) {
    vst_strncpy(name, "DitherMeDiskers", kVstMaxProductStrLen); return true;
}

VstPlugCategory DitherMeDiskers::getPlugCategory() {return kPlugCategEffect;}

bool DitherMeDiskers::getProductString(char* text) {
    vst_strncpy (text, "airwindows DitherMeDiskers", kVstMaxProductStrLen); return true;
}

bool DitherMeDiskers::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
