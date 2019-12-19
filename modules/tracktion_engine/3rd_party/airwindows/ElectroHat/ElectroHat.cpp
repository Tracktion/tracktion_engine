/* ========================================
 *  ElectroHat - ElectroHat.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __ElectroHat_H
#include "ElectroHat.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new ElectroHat(audioMaster);}

ElectroHat::ElectroHat(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    A = 0.0;
    B = 0.5;
    C = 1.0;
    D = 0.1;
    E = 1.0;
    storedSampleL = 0.0;
    storedSampleR = 0.0;
    lastSampleL = 0.0;
    lastSampleR = 0.0;
    tik = 3746926;
    lok = 0;
    flip = true;

    fpNShapeL = 0.0;
    fpNShapeR = 0.0;
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

ElectroHat::~ElectroHat() {}
VstInt32 ElectroHat::getVendorVersion () {return 1000;}
void ElectroHat::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void ElectroHat::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
    if (data < 0.0f) return 0.0f;
    if (data > 1.0f) return 1.0f;
    return data;
}

VstInt32 ElectroHat::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    chunkData[1] = B;
    chunkData[2] = C;
    chunkData[3] = D;
    chunkData[4] = E;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 ElectroHat::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    B = pinParameter(chunkData[1]);
    C = pinParameter(chunkData[2]);
    D = pinParameter(chunkData[3]);
    E = pinParameter(chunkData[4]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void ElectroHat::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float ElectroHat::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void ElectroHat::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "HiHat", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "Trim", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "Bright", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void ElectroHat::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: switch((VstInt32)( A * 5.999 )) //0 to almost edge of # of params
        {
            case 0: vst_strncpy (text, "Syn Hat", kVstMaxParamStrLen); break;
            case 1: vst_strncpy (text, "Electro", kVstMaxParamStrLen); break;
            case 2: vst_strncpy (text, "Dense", kVstMaxParamStrLen); break;
            case 3: vst_strncpy (text, "606 St", kVstMaxParamStrLen); break;
            case 4: vst_strncpy (text, "808 St", kVstMaxParamStrLen); break;
            case 5: vst_strncpy (text, "909 St", kVstMaxParamStrLen); break;
            default: break; // unknown parameter, shouldn't happen!
        } break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (C, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (D, text, kVstMaxParamStrLen); break;
        case kParamE: float2string (E, text, kVstMaxParamStrLen); break;


        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void ElectroHat::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 ElectroHat::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool ElectroHat::getEffectName(char* name) {
    vst_strncpy(name, "ElectroHat", kVstMaxProductStrLen); return true;
}

VstPlugCategory ElectroHat::getPlugCategory() {return kPlugCategEffect;}

bool ElectroHat::getProductString(char* text) {
    vst_strncpy (text, "airwindows ElectroHat", kVstMaxProductStrLen); return true;
}

bool ElectroHat::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
