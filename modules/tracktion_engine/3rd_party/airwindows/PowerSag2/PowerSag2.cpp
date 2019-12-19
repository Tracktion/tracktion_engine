/* ========================================
 *  PowerSag2 - PowerSag2.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PowerSag2_H
#include "PowerSag2.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new PowerSag2(audioMaster);}

PowerSag2::PowerSag2(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{

    for(int count = 0; count < 16385; count++) {dL[count] = 0; dR[count] = 0;}
    controlL = 0;
    controlR = 0;
    gcount = 0;

    A = 0.3;
    B = 0.5;
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

PowerSag2::~PowerSag2() {}
VstInt32 PowerSag2::getVendorVersion () {return 1000;}
void PowerSag2::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void PowerSag2::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
    if (data < 0.0f) return 0.0f;
    if (data > 1.0f) return 1.0f;
    return data;
}

VstInt32 PowerSag2::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    chunkData[1] = B;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 PowerSag2::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    B = pinParameter(chunkData[1]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void PowerSag2::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float PowerSag2::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void PowerSag2::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Range", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "Inv/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void PowerSag2::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        case kParamB: float2string ((B*2.0)-1.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void PowerSag2::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 PowerSag2::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool PowerSag2::getEffectName(char* name) {
    vst_strncpy(name, "PowerSag2", kVstMaxProductStrLen); return true;
}

VstPlugCategory PowerSag2::getPlugCategory() {return kPlugCategEffect;}

bool PowerSag2::getProductString(char* text) {
    vst_strncpy (text, "airwindows PowerSag2", kVstMaxProductStrLen); return true;
}

bool PowerSag2::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
