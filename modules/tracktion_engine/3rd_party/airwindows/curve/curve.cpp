/* ========================================
 *  curve - curve.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __curve_H
#include "curve.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new curve(audioMaster);}

curve::curve(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    gain = 1.0;
    fpd = 17;
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

curve::~curve() {}
VstInt32 curve::getVendorVersion () {return 1000;}
void curve::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void curve::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 curve::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 curve::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    return 0;
}

void curve::setParameter(VstInt32 index, float value) {
    switch (index) {
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float curve::getParameter(VstInt32 index) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void curve::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void curve::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void curve::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 curve::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool curve::getEffectName(char* name) {
    vst_strncpy(name, "curve", kVstMaxProductStrLen); return true;
}

VstPlugCategory curve::getPlugCategory() {return kPlugCategEffect;}

bool curve::getProductString(char* text) {
    vst_strncpy (text, "airwindows curve", kVstMaxProductStrLen); return true;
}

bool curve::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
