/* ========================================
 *  PurestSquish - PurestSquish.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PurestSquish_H
#include "PurestSquish.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new PurestSquish(audioMaster);}

PurestSquish::PurestSquish(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    A = 0.0;
    B = 0.0;
    C = 1.0;
    D = 1.0;

    muSpeedAL = 10000;
    muSpeedBL = 10000;
    muSpeedCL = 10000;
    muSpeedDL = 10000;
    muSpeedEL = 10000;
    muCoefficientAL = 1;
    muCoefficientBL = 1;
    muCoefficientCL = 1;
    muCoefficientDL = 1;
    muCoefficientEL = 1;
    iirSampleAL = 0.0;
    iirSampleBL = 0.0;
    iirSampleCL = 0.0;
    iirSampleDL = 0.0;
    iirSampleEL = 0.0;
    lastCoefficientAL = 1;
    lastCoefficientBL = 1;
    lastCoefficientCL = 1;
    lastCoefficientDL = 1;
    mergedCoefficientsL = 1;
    muVaryL = 1;

    muSpeedAR = 10000;
    muSpeedBR = 10000;
    muSpeedCR = 10000;
    muSpeedDR = 10000;
    muSpeedER = 10000;
    muCoefficientAR = 1;
    muCoefficientBR = 1;
    muCoefficientCR = 1;
    muCoefficientDR = 1;
    muCoefficientER = 1;
    iirSampleAR = 0.0;
    iirSampleBR = 0.0;
    iirSampleCR = 0.0;
    iirSampleDR = 0.0;
    iirSampleER = 0.0;
    lastCoefficientAR = 1;
    lastCoefficientBR = 1;
    lastCoefficientCR = 1;
    lastCoefficientDR = 1;
    mergedCoefficientsR = 1;
    muVaryR = 1;

    count = 1;
    fpFlip = true;

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

PurestSquish::~PurestSquish() {}
VstInt32 PurestSquish::getVendorVersion () {return 1000;}
void PurestSquish::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void PurestSquish::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
    if (data < 0.0f) return 0.0f;
    if (data > 1.0f) return 1.0f;
    return data;
}

VstInt32 PurestSquish::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    chunkData[1] = B;
    chunkData[2] = C;
    chunkData[3] = D;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 PurestSquish::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    B = pinParameter(chunkData[1]);
    C = pinParameter(chunkData[2]);
    D = pinParameter(chunkData[3]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void PurestSquish::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float PurestSquish::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
         default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void PurestSquish::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Squish", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "BassBlm", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void PurestSquish::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (C, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (D, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void PurestSquish::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 PurestSquish::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool PurestSquish::getEffectName(char* name) {
    vst_strncpy(name, "PurestSquish", kVstMaxProductStrLen); return true;
}

VstPlugCategory PurestSquish::getPlugCategory() {return kPlugCategEffect;}

bool PurestSquish::getProductString(char* text) {
    vst_strncpy (text, "airwindows PurestSquish", kVstMaxProductStrLen); return true;
}

bool PurestSquish::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
