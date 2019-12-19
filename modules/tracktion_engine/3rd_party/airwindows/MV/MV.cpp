/* ========================================
 *  MV - MV.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __MV_H
#include "MV.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new MV(audioMaster);}

MV::MV(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    A = 0.5;
    B = 0.5;
    C = 0.5;
    D = 1.0;
    E = 1.0;

    int count;
    for(count = 0; count < 15149; count++) {aAL[count] = 0.0; aAR[count] = 0.0;}
    for(count = 0; count < 14617; count++) {aBL[count] = 0.0; aBR[count] = 0.0;}
    for(count = 0; count < 14357; count++) {aCL[count] = 0.0; aCR[count] = 0.0;}
    for(count = 0; count < 13817; count++) {aDL[count] = 0.0; aDR[count] = 0.0;}
    for(count = 0; count < 13561; count++) {aEL[count] = 0.0; aER[count] = 0.0;}
    for(count = 0; count < 13045; count++) {aFL[count] = 0.0; aFR[count] = 0.0;}
    for(count = 0; count < 11965; count++) {aGL[count] = 0.0; aGR[count] = 0.0;}
    for(count = 0; count < 11129; count++) {aHL[count] = 0.0; aHR[count] = 0.0;}
    for(count = 0; count < 10597; count++) {aIL[count] = 0.0; aIR[count] = 0.0;}
    for(count = 0; count < 9809; count++) {aJL[count] = 0.0; aJR[count] = 0.0;}
    for(count = 0; count < 9521; count++) {aKL[count] = 0.0; aKR[count] = 0.0;}
    for(count = 0; count < 8981; count++) {aLL[count] = 0.0; aLR[count] = 0.0;}
    for(count = 0; count < 8785; count++) {aML[count] = 0.0; aMR[count] = 0.0;}
    for(count = 0; count < 8461; count++) {aNL[count] = 0.0; aNR[count] = 0.0;}
    for(count = 0; count < 8309; count++) {aOL[count] = 0.0; aOR[count] = 0.0;}
    for(count = 0; count < 7981; count++) {aPL[count] = 0.0; aPR[count] = 0.0;}
    for(count = 0; count < 7321; count++) {aQL[count] = 0.0; aQR[count] = 0.0;}
    for(count = 0; count < 6817; count++) {aRL[count] = 0.0; aRR[count] = 0.0;}
    for(count = 0; count < 6505; count++) {aSL[count] = 0.0; aSR[count] = 0.0;}
    for(count = 0; count < 6001; count++) {aTL[count] = 0.0; aTR[count] = 0.0;}
    for(count = 0; count < 5837; count++) {aUL[count] = 0.0; aUR[count] = 0.0;}
    for(count = 0; count < 5501; count++) {aVL[count] = 0.0; aVR[count] = 0.0;}
    for(count = 0; count < 5009; count++) {aWL[count] = 0.0; aWR[count] = 0.0;}
    for(count = 0; count < 4849; count++) {aXL[count] = 0.0; aXR[count] = 0.0;}
    for(count = 0; count < 4295; count++) {aYL[count] = 0.0; aYR[count] = 0.0;}
    for(count = 0; count < 4179; count++) {aZL[count] = 0.0; aZR[count] = 0.0;}

    alpA = 1; delayA = 7573; avgAL = 0.0; avgAR = 0.0;
    alpB = 1; delayB = 7307; avgBL = 0.0; avgBR = 0.0;
    alpC = 1; delayC = 7177; avgCL = 0.0; avgCR = 0.0;
    alpD = 1; delayD = 6907; avgDL = 0.0; avgDR = 0.0;
    alpE = 1; delayE = 6779; avgEL = 0.0; avgER = 0.0;
    alpF = 1; delayF = 6521; avgFL = 0.0; avgFR = 0.0;
    alpG = 1; delayG = 5981; avgGL = 0.0; avgGR = 0.0;
    alpH = 1; delayH = 5563; avgHL = 0.0; avgHR = 0.0;
    alpI = 1; delayI = 5297; avgIL = 0.0; avgIR = 0.0;
    alpJ = 1; delayJ = 4903; avgJL = 0.0; avgJR = 0.0;
    alpK = 1; delayK = 4759; avgKL = 0.0; avgKR = 0.0;
    alpL = 1; delayL = 4489; avgLL = 0.0; avgLR = 0.0;
    alpM = 1; delayM = 4391; avgML = 0.0; avgMR = 0.0;
    alpN = 1; delayN = 4229; avgNL = 0.0; avgNR = 0.0;
    alpO = 1; delayO = 4153; avgOL = 0.0; avgOR = 0.0;
    alpP = 1; delayP = 3989; avgPL = 0.0; avgPR = 0.0;
    alpQ = 1; delayQ = 3659; avgQL = 0.0; avgQR = 0.0;
    alpR = 1; delayR = 3407; avgRL = 0.0; avgRR = 0.0;
    alpS = 1; delayS = 3251; avgSL = 0.0; avgSR = 0.0;
    alpT = 1; delayT = 2999; avgTL = 0.0; avgTR = 0.0;
    alpU = 1; delayU = 2917; avgUL = 0.0; avgUR = 0.0;
    alpV = 1; delayV = 2749; avgVL = 0.0; avgVR = 0.0;
    alpW = 1; delayW = 2503; avgWL = 0.0; avgWR = 0.0;
    alpX = 1; delayX = 2423; avgXL = 0.0; avgXR = 0.0;
    alpY = 1; delayY = 2146; avgYL = 0.0; avgYR = 0.0;
    alpZ = 1; delayZ = 2088; avgZL = 0.0; avgZR = 0.0;

    feedbackL = 0.0;
    feedbackR = 0.0;

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

MV::~MV() {}
VstInt32 MV::getVendorVersion () {return 1000;}
void MV::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void MV::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
    if (data < 0.0f) return 0.0f;
    if (data > 1.0f) return 1.0f;
    return data;
}

VstInt32 MV::getChunk (void** data, bool isPreset)
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

VstInt32 MV::setChunk (void* data, VstInt32 byteSize, bool isPreset)
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

void MV::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float MV::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void MV::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Depth", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "Bright", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "Regen", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "Output", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "Dry/Wet", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void MV::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        case kParamB: float2string (B, text, kVstMaxParamStrLen); break;
        case kParamC: float2string (C, text, kVstMaxParamStrLen); break;
        case kParamD: float2string (D, text, kVstMaxParamStrLen); break;
        case kParamE: float2string (E, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void MV::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 MV::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool MV::getEffectName(char* name) {
    vst_strncpy(name, "MV", kVstMaxProductStrLen); return true;
}

VstPlugCategory MV::getPlugCategory() {return kPlugCategEffect;}

bool MV::getProductString(char* text) {
    vst_strncpy (text, "airwindows MV", kVstMaxProductStrLen); return true;
}

bool MV::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
