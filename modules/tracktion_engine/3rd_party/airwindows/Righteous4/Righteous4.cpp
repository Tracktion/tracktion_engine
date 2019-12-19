/* ========================================
 *  Righteous4 - Righteous4.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Righteous4_H
#include "Righteous4.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Righteous4(audioMaster);}

Righteous4::Righteous4(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    A = 0.0;
    B = 0.0;

    leftSampleA = 0.0;
    leftSampleB = 0.0;
    leftSampleC = 0.0;
    leftSampleD = 0.0;
    leftSampleE = 0.0;
    leftSampleF = 0.0;
    leftSampleG = 0.0;
    leftSampleH = 0.0;
    leftSampleI = 0.0;
    leftSampleJ = 0.0;
    leftSampleK = 0.0;
    leftSampleL = 0.0;
    leftSampleM = 0.0;
    leftSampleN = 0.0;
    leftSampleO = 0.0;
    leftSampleP = 0.0;
    leftSampleQ = 0.0;
    leftSampleR = 0.0;
    leftSampleS = 0.0;
    leftSampleT = 0.0;
    leftSampleU = 0.0;
    leftSampleV = 0.0;
    leftSampleW = 0.0;
    leftSampleX = 0.0;
    leftSampleY = 0.0;
    leftSampleZ = 0.0;

    rightSampleA = 0.0;
    rightSampleB = 0.0;
    rightSampleC = 0.0;
    rightSampleD = 0.0;
    rightSampleE = 0.0;
    rightSampleF = 0.0;
    rightSampleG = 0.0;
    rightSampleH = 0.0;
    rightSampleI = 0.0;
    rightSampleJ = 0.0;
    rightSampleK = 0.0;
    rightSampleL = 0.0;
    rightSampleM = 0.0;
    rightSampleN = 0.0;
    rightSampleO = 0.0;
    rightSampleP = 0.0;
    rightSampleQ = 0.0;
    rightSampleR = 0.0;
    rightSampleS = 0.0;
    rightSampleT = 0.0;
    rightSampleU = 0.0;
    rightSampleV = 0.0;
    rightSampleW = 0.0;
    rightSampleX = 0.0;
    rightSampleY = 0.0;
    rightSampleZ = 0.0;

    bynL[0] = 1000;
    bynL[1] = 301;
    bynL[2] = 176;
    bynL[3] = 125;
    bynL[4] = 97;
    bynL[5] = 79;
    bynL[6] = 67;
    bynL[7] = 58;
    bynL[8] = 51;
    bynL[9] = 46;
    bynL[10] = 1000;
    noiseShapingL = 0.0;
    lastSampleL = 0.0;
    IIRsampleL = 0.0;
    gwPrevL = 0.0;
    gwAL = 0.0;
    gwBL = 0.0;

    bynR[0] = 1000;
    bynR[1] = 301;
    bynR[2] = 176;
    bynR[3] = 125;
    bynR[4] = 97;
    bynR[5] = 79;
    bynR[6] = 67;
    bynR[7] = 58;
    bynR[8] = 51;
    bynR[9] = 46;
    bynR[10] = 1000;
    noiseShapingR = 0.0;
    lastSampleR = 0.0;
    IIRsampleR = 0.0;
    gwPrevR = 0.0;
    gwAR = 0.0;
    gwBR = 0.0;

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

Righteous4::~Righteous4() {}
VstInt32 Righteous4::getVendorVersion () {return 1000;}
void Righteous4::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Righteous4::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
    if (data < 0.0f) return 0.0f;
    if (data > 1.0f) return 1.0f;
    return data;
}

VstInt32 Righteous4::getChunk (void** data, bool isPreset)
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

VstInt32 Righteous4::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    B = pinParameter(chunkData[1]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void Righteous4::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Righteous4::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Righteous4::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "LTarget", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "BtDepth", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Righteous4::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string ((A*24.0)-28.0, text, kVstMaxParamStrLen); break;
        case kParamB: switch((VstInt32)( B * 2.999 )) //0 to almost edge of # of params
        {case 0: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
         case 1: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
         case 2: vst_strncpy (text, "32", kVstMaxParamStrLen); break;
         default: break; // unknown parameter, shouldn't happen!
        } break;
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void Righteous4::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "bit", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Righteous4::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Righteous4::getEffectName(char* name) {
    vst_strncpy(name, "Righteous4", kVstMaxProductStrLen); return true;
}

VstPlugCategory Righteous4::getPlugCategory() {return kPlugCategEffect;}

bool Righteous4::getProductString(char* text) {
    vst_strncpy (text, "airwindows Righteous4", kVstMaxProductStrLen); return true;
}

bool Righteous4::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
