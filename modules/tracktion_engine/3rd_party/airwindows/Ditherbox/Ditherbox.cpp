/* ========================================
 *  Ditherbox - Ditherbox.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Ditherbox_H
#include "Ditherbox.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Ditherbox(audioMaster);}

Ditherbox::Ditherbox(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    A = 0.86;

    Position = 99999999;
    contingentErrL = 0.0;
    contingentErrR = 0.0;
    currentDitherL = 0.0;
    currentDitherR = 0.0;
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

    NSOddL = 0.0;
    prevL = 0.0;
    nsL[0] = 0;
    nsL[1] = 0;
    nsL[2] = 0;
    nsL[3] = 0;
    nsL[4] = 0;
    nsL[5] = 0;
    nsL[6] = 0;
    nsL[7] = 0;
    nsL[8] = 0;
    nsL[9] = 0;
    nsL[10] = 0;
    nsL[11] = 0;
    nsL[12] = 0;
    nsL[13] = 0;
    nsL[14] = 0;
    nsL[15] = 0;
    NSOddR = 0.0;
    prevR = 0.0;
    nsR[0] = 0;
    nsR[1] = 0;
    nsR[2] = 0;
    nsR[3] = 0;
    nsR[4] = 0;
    nsR[5] = 0;
    nsR[6] = 0;
    nsR[7] = 0;
    nsR[8] = 0;
    nsR[9] = 0;
    nsR[10] = 0;
    nsR[11] = 0;
    nsR[12] = 0;
    nsR[13] = 0;
    nsR[14] = 0;
    nsR[15] = 0;

    lastSampleL = 0.0;
    outSampleL = 0.0;
    lastSampleR = 0.0;
    outSampleR = 0.0;

    iirSampleAL = 0.0;
    iirSampleBL = 0.0;
    iirSampleCL = 0.0;
    iirSampleDL = 0.0;
    iirSampleEL = 0.0;
    iirSampleFL = 0.0;
    iirSampleGL = 0.0;
    iirSampleHL = 0.0;
    iirSampleIL = 0.0;
    iirSampleJL = 0.0;
    iirSampleKL = 0.0;
    iirSampleLL = 0.0;
    iirSampleML = 0.0;
    iirSampleNL = 0.0;
    iirSampleOL = 0.0;
    iirSamplePL = 0.0;
    iirSampleQL = 0.0;
    iirSampleRL = 0.0;
    iirSampleSL = 0.0;
    iirSampleTL = 0.0;
    iirSampleUL = 0.0;
    iirSampleVL = 0.0;
    iirSampleWL = 0.0;
    iirSampleXL = 0.0;
    iirSampleYL = 0.0;
    iirSampleZL = 0.0;

    iirSampleAR = 0.0;
    iirSampleBR = 0.0;
    iirSampleCR = 0.0;
    iirSampleDR = 0.0;
    iirSampleER = 0.0;
    iirSampleFR = 0.0;
    iirSampleGR = 0.0;
    iirSampleHR = 0.0;
    iirSampleIR = 0.0;
    iirSampleJR = 0.0;
    iirSampleKR = 0.0;
    iirSampleLR = 0.0;
    iirSampleMR = 0.0;
    iirSampleNR = 0.0;
    iirSampleOR = 0.0;
    iirSamplePR = 0.0;
    iirSampleQR = 0.0;
    iirSampleRR = 0.0;
    iirSampleSR = 0.0;
    iirSampleTR = 0.0;
    iirSampleUR = 0.0;
    iirSampleVR = 0.0;
    iirSampleWR = 0.0;
    iirSampleXR = 0.0;
    iirSampleYR = 0.0;
    iirSampleZR = 0.0;
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

Ditherbox::~Ditherbox() {}
VstInt32 Ditherbox::getVendorVersion () {return 1000;}
void Ditherbox::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Ditherbox::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
    if (data < 0.0f) return 0.0f;
    if (data > 1.0f) return 1.0f;
    return data;
}

VstInt32 Ditherbox::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 Ditherbox::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void Ditherbox::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Ditherbox::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Ditherbox::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Type", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Ditherbox::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: switch((VstInt32)( A * 24.999 )) //0 to almost edge of # of params
        {
            case 0: vst_strncpy (text, "Trunc", kVstMaxParamStrLen); break;
            case 1: vst_strncpy (text, "Flat", kVstMaxParamStrLen); break;
            case 2: vst_strncpy (text, "TPDF", kVstMaxParamStrLen); break;
            case 3: vst_strncpy (text, "Paul", kVstMaxParamStrLen); break;
            case 4: vst_strncpy (text, "DbPaul", kVstMaxParamStrLen); break;
            case 5: vst_strncpy (text, "Tape", kVstMaxParamStrLen); break;
            case 6: vst_strncpy (text, "HiGloss", kVstMaxParamStrLen); break;
            case 7: vst_strncpy (text, "Vinyl", kVstMaxParamStrLen); break;
            case 8: vst_strncpy (text, "Spatial", kVstMaxParamStrLen); break;
            case 9: vst_strncpy (text, "Natural", kVstMaxParamStrLen); break;
            case 10: vst_strncpy (text, "NJAD", kVstMaxParamStrLen); break;
            case 11: vst_strncpy (text, "Trunc", kVstMaxParamStrLen); break;
            case 12: vst_strncpy (text, "Flat", kVstMaxParamStrLen); break;
            case 13: vst_strncpy (text, "TPDF", kVstMaxParamStrLen); break;
            case 14: vst_strncpy (text, "Paul", kVstMaxParamStrLen); break;
            case 15: vst_strncpy (text, "DbPaul", kVstMaxParamStrLen); break;
            case 16: vst_strncpy (text, "Tape", kVstMaxParamStrLen); break;
            case 17: vst_strncpy (text, "HiGloss", kVstMaxParamStrLen); break;
            case 18: vst_strncpy (text, "Vinyl", kVstMaxParamStrLen); break;
            case 19: vst_strncpy (text, "Spatial", kVstMaxParamStrLen); break;
            case 20: vst_strncpy (text, "Natural", kVstMaxParamStrLen); break;
            case 21: vst_strncpy (text, "NJAD", kVstMaxParamStrLen); break;
            case 22: vst_strncpy (text, "SlewOnl", kVstMaxParamStrLen); break;
            case 23: vst_strncpy (text, "SubsOnl", kVstMaxParamStrLen); break;
            case 24: vst_strncpy (text, "Silhoue", kVstMaxParamStrLen); break;
            default: break; // unknown parameter, shouldn't happen!
        } break;
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void Ditherbox::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: switch((VstInt32)( A * 24.999 )) //0 to almost edge of # of params
        {
            case 0: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
            case 1: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
            case 2: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
            case 3: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
            case 4: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
            case 5: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
            case 6: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
            case 7: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
            case 8: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
            case 9: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
            case 10: vst_strncpy (text, "16", kVstMaxParamStrLen); break;
            case 11: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
            case 12: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
            case 13: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
            case 14: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
            case 15: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
            case 16: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
            case 17: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
            case 18: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
            case 19: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
            case 20: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
            case 21: vst_strncpy (text, "24", kVstMaxParamStrLen); break;
            case 22: vst_strncpy (text, "y", kVstMaxParamStrLen); break;
            case 23: vst_strncpy (text, "y", kVstMaxParamStrLen); break;
            case 24: vst_strncpy (text, "tte", kVstMaxParamStrLen); break;
            default: break; // unknown parameter, shouldn't happen!
        } break;
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

VstInt32 Ditherbox::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Ditherbox::getEffectName(char* name) {
    vst_strncpy(name, "Ditherbox", kVstMaxProductStrLen); return true;
}

VstPlugCategory Ditherbox::getPlugCategory() {return kPlugCategEffect;}

bool Ditherbox::getProductString(char* text) {
    vst_strncpy (text, "airwindows Ditherbox", kVstMaxProductStrLen); return true;
}

bool Ditherbox::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
