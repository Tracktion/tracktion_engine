/* ========================================
 *  Monitoring - Monitoring.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Monitoring_H
#include "Monitoring.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Monitoring(audioMaster);}

Monitoring::Monitoring(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    bynL[0] = 1000.0;
    bynL[1] = 301.0;
    bynL[2] = 176.0;
    bynL[3] = 125.0;
    bynL[4] = 97.0;
    bynL[5] = 79.0;
    bynL[6] = 67.0;
    bynL[7] = 58.0;
    bynL[8] = 51.0;
    bynL[9] = 46.0;
    bynL[10] = 1000.0;
    noiseShapingL = 0.0;
    bynR[0] = 1000.0;
    bynR[1] = 301.0;
    bynR[2] = 176.0;
    bynR[3] = 125.0;
    bynR[4] = 97.0;
    bynR[5] = 79.0;
    bynR[6] = 67.0;
    bynR[7] = 58.0;
    bynR[8] = 51.0;
    bynR[9] = 46.0;
    bynR[10] = 1000.0;
    noiseShapingR = 0.0;
    //end NJAD
    for(int count = 0; count < 1502; count++) {
        aL[count] = 0.0; bL[count] = 0.0; cL[count] = 0.0; dL[count] = 0.0;
        aR[count] = 0.0; bR[count] = 0.0; cR[count] = 0.0; dR[count] = 0.0;
    }
    ax = 1; bx = 1; cx = 1; dx = 1;
    //PeaksOnly
    lastSampleL = 0.0; lastSampleR = 0.0;
    //SlewOnly
    iirSampleAL = 0.0; iirSampleBL = 0.0; iirSampleCL = 0.0; iirSampleDL = 0.0; iirSampleEL = 0.0; iirSampleFL = 0.0; iirSampleGL = 0.0;
    iirSampleHL = 0.0; iirSampleIL = 0.0; iirSampleJL = 0.0; iirSampleKL = 0.0; iirSampleLL = 0.0; iirSampleML = 0.0; iirSampleNL = 0.0; iirSampleOL = 0.0; iirSamplePL = 0.0;
    iirSampleQL = 0.0; iirSampleRL = 0.0; iirSampleSL = 0.0;
    iirSampleTL = 0.0; iirSampleUL = 0.0; iirSampleVL = 0.0;
    iirSampleWL = 0.0; iirSampleXL = 0.0; iirSampleYL = 0.0; iirSampleZL = 0.0;

    iirSampleAR = 0.0; iirSampleBR = 0.0; iirSampleCR = 0.0; iirSampleDR = 0.0; iirSampleER = 0.0; iirSampleFR = 0.0; iirSampleGR = 0.0;
    iirSampleHR = 0.0; iirSampleIR = 0.0; iirSampleJR = 0.0; iirSampleKR = 0.0; iirSampleLR = 0.0; iirSampleMR = 0.0; iirSampleNR = 0.0; iirSampleOR = 0.0; iirSamplePR = 0.0;
    iirSampleQR = 0.0; iirSampleRR = 0.0; iirSampleSR = 0.0;
    iirSampleTR = 0.0; iirSampleUR = 0.0; iirSampleVR = 0.0;
    iirSampleWR = 0.0; iirSampleXR = 0.0; iirSampleYR = 0.0; iirSampleZR = 0.0; // o/`
    //SubsOnly
    for (int x = 0; x < 11; x++) {biquadL[x] = 0.0; biquadR[x] = 0.0;}
    //Bandpasses
    A = 0.0;
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

Monitoring::~Monitoring() {}
VstInt32 Monitoring::getVendorVersion () {return 1000;}
void Monitoring::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Monitoring::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
    if (data < 0.0f) return 0.0f;
    if (data > 1.0f) return 1.0f;
    return data;
}

VstInt32 Monitoring::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 Monitoring::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void Monitoring::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Monitoring::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Monitoring::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Monitor", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Monitoring::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: switch((VstInt32)( A * 16.999 )) //0 to almost edge of # of params
        {   case 0: vst_strncpy (text, "Out24", kVstMaxParamStrLen); break;
            case 1: vst_strncpy (text, "Out16", kVstMaxParamStrLen); break;
            case 2: vst_strncpy (text, "Peaks", kVstMaxParamStrLen); break;
            case 3: vst_strncpy (text, "Slew", kVstMaxParamStrLen); break;
            case 4: vst_strncpy (text, "Subs", kVstMaxParamStrLen); break;
            case 5: vst_strncpy (text, "Mono", kVstMaxParamStrLen); break;
            case 6: vst_strncpy (text, "Side", kVstMaxParamStrLen); break;
            case 7: vst_strncpy (text, "Vinyl", kVstMaxParamStrLen); break;
            case 8: vst_strncpy (text, "Aurat", kVstMaxParamStrLen); break;
            case 9: vst_strncpy (text, "MonoRat", kVstMaxParamStrLen); break;
            case 10: vst_strncpy (text, "MonoLat", kVstMaxParamStrLen); break;
            case 11: vst_strncpy (text, "Phone", kVstMaxParamStrLen); break;
            case 12: vst_strncpy (text, "Cans A", kVstMaxParamStrLen); break;
            case 13: vst_strncpy (text, "Cans B", kVstMaxParamStrLen); break;
            case 14: vst_strncpy (text, "Cans C", kVstMaxParamStrLen); break;
            case 15: vst_strncpy (text, "Cans D", kVstMaxParamStrLen); break;
            case 16: vst_strncpy (text, "V Trick", kVstMaxParamStrLen); break;
         default: break; // unknown parameter, shouldn't happen!
        } break;
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void Monitoring::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Monitoring::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Monitoring::getEffectName(char* name) {
    vst_strncpy(name, "Monitoring", kVstMaxProductStrLen); return true;
}

VstPlugCategory Monitoring::getPlugCategory() {return kPlugCategEffect;}

bool Monitoring::getProductString(char* text) {
    vst_strncpy (text, "airwindows Monitoring", kVstMaxProductStrLen); return true;
}

bool Monitoring::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
