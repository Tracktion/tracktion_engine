/* ========================================
 *  Pafnuty - Pafnuty.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Pafnuty_H
#include "Pafnuty.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Pafnuty(audioMaster);}

Pafnuty::Pafnuty(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    A = 0.5;
    B = 0.5;
    C = 0.5;
    D = 0.5;
    E = 0.5;
    F = 0.5;
    G = 0.5;
    H = 0.5;
    I = 0.5;
    J = 0.5;
    K = 0.5;
    L = 0.5;
    M = 1.0;
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

Pafnuty::~Pafnuty() {}
VstInt32 Pafnuty::getVendorVersion () {return 1000;}
void Pafnuty::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Pafnuty::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
    if (data < 0.0f) return 0.0f;
    if (data > 1.0f) return 1.0f;
    return data;
}

VstInt32 Pafnuty::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    chunkData[1] = B;
    chunkData[2] = C;
    chunkData[3] = D;
    chunkData[4] = E;
    chunkData[5] = F;
    chunkData[6] = G;
    chunkData[7] = H;
    chunkData[8] = I;
    chunkData[9] = J;
    chunkData[10] = K;
    chunkData[11] = L;
    chunkData[12] = M;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 Pafnuty::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    B = pinParameter(chunkData[1]);
    C = pinParameter(chunkData[2]);
    D = pinParameter(chunkData[3]);
    E = pinParameter(chunkData[4]);
    F = pinParameter(chunkData[5]);
    G = pinParameter(chunkData[6]);
    H = pinParameter(chunkData[7]);
    I = pinParameter(chunkData[8]);
    J = pinParameter(chunkData[9]);
    K = pinParameter(chunkData[10]);
    L = pinParameter(chunkData[11]);
    M = pinParameter(chunkData[12]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void Pafnuty::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        case kParamF: F = value; break;
        case kParamG: G = value; break;
        case kParamH: H = value; break;
        case kParamI: I = value; break;
        case kParamJ: J = value; break;
        case kParamK: K = value; break;
        case kParamL: L = value; break;
        case kParamM: M = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Pafnuty::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        case kParamF: return F; break;
        case kParamG: return G; break;
        case kParamH: return H; break;
        case kParamI: return I; break;
        case kParamJ: return J; break;
        case kParamK: return K; break;
        case kParamL: return L; break;
        case kParamM: return M; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Pafnuty::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Second", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "Third", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "Fourth", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "Fifth", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "Sixth", kVstMaxParamStrLen); break;
        case kParamF: vst_strncpy (text, "Seventh", kVstMaxParamStrLen); break;
        case kParamG: vst_strncpy (text, "Eighth", kVstMaxParamStrLen); break;
        case kParamH: vst_strncpy (text, "Ninth", kVstMaxParamStrLen); break;
        case kParamI: vst_strncpy (text, "Tenth", kVstMaxParamStrLen); break;
        case kParamJ: vst_strncpy (text, "Elevnth", kVstMaxParamStrLen); break;
        case kParamK: vst_strncpy (text, "Twelvth", kVstMaxParamStrLen); break;
        case kParamL: vst_strncpy (text, "Thirtnh", kVstMaxParamStrLen); break;
        case kParamM: vst_strncpy (text, "Inv/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Pafnuty::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string ((A*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamB: float2string ((B*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamC: float2string ((C*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamD: float2string ((D*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamE: float2string ((E*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamF: float2string ((F*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamG: float2string ((G*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamH: float2string ((H*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamI: float2string ((I*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamJ: float2string ((J*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamK: float2string ((K*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamL: float2string ((L*2.0)-1.0, text, kVstMaxParamStrLen); break;
        case kParamM: float2string ((M*2.0)-1.0, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void Pafnuty::getParameterLabel(VstInt32 index, char *text) {
    vst_strncpy (text, "", kVstMaxParamStrLen);
    //in this case I'm just not going to make 13 ways to say ""
}

VstInt32 Pafnuty::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Pafnuty::getEffectName(char* name) {
    vst_strncpy(name, "Pafnuty", kVstMaxProductStrLen); return true;
}

VstPlugCategory Pafnuty::getPlugCategory() {return kPlugCategEffect;}

bool Pafnuty::getProductString(char* text) {
    vst_strncpy (text, "airwindows Pafnuty", kVstMaxProductStrLen); return true;
}

bool Pafnuty::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
