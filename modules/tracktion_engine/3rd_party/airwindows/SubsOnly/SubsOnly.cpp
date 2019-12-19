/* ========================================
 *  SubsOnly - SubsOnly.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __SubsOnly_H
#include "SubsOnly.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new SubsOnly(audioMaster);}

SubsOnly::SubsOnly(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
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

    _canDo.insert("plugAsChannelInsert"); // plug-in can be used as a channel insert effect.
    _canDo.insert("plugAsSend"); // plug-in can be used as a send effect.
    _canDo.insert("x2in2out");
    setNumInputs(kNumInputs);
    setNumOutputs(kNumOutputs);
    setUniqueID(kUniqueId);
    canProcessReplacing();     // supports output replacing
    canDoubleReplacing();      // supports double precision processing
    vst_strncpy (_programName, "Default", kVstMaxProgNameLen); // default program name
}

SubsOnly::~SubsOnly() {}
VstInt32 SubsOnly::getVendorVersion () {return 1000;}
void SubsOnly::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void SubsOnly::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

void SubsOnly::setParameter(VstInt32 index, float value) {
}

float SubsOnly::getParameter(VstInt32 index) {
    return 0.0; //we only need to update the relevant name, this is simple to manage
}

void SubsOnly::getParameterName(VstInt32 index, char *text) {
}

void SubsOnly::getParameterDisplay(VstInt32 index, char *text) {
}

void SubsOnly::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 SubsOnly::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool SubsOnly::getEffectName(char* name) {
    vst_strncpy(name, "SubsOnly", kVstMaxProductStrLen); return true;
}

VstPlugCategory SubsOnly::getPlugCategory() {return kPlugCategEffect;}

bool SubsOnly::getProductString(char* text) {
    vst_strncpy (text, "airwindows SubsOnly", kVstMaxProductStrLen); return true;
}

bool SubsOnly::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
