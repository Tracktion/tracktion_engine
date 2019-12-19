/* ========================================
 *  TubeDesk - TubeDesk.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __TubeDesk_H
#include "TubeDesk.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new TubeDesk(audioMaster);}

TubeDesk::TubeDesk(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    for(int count = 0; count < 4999; count++) {dL[count] = 0; dR[count] = 0;}
    gcount = 0;

    controlL = 0;
    lastSampleL = 0.0;
    lastOutSampleL = 0.0;
    lastSlewL = 0.0;

    controlR = 0;
    lastSampleR = 0.0;
    lastOutSampleR = 0.0;
    lastSlewR = 0.0;

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

TubeDesk::~TubeDesk() {}
VstInt32 TubeDesk::getVendorVersion () {return 1000;}
void TubeDesk::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void TubeDesk::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 TubeDesk::getChunk (void** data, bool isPreset)
{
    return kNumParameters * sizeof(float);
}

VstInt32 TubeDesk::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    return 0;
}

void TubeDesk::setParameter(VstInt32 index, float value) {
}

float TubeDesk::getParameter(VstInt32 index) {
    return 0.0; //we only need to update the relevant name, this is simple to manage
}

void TubeDesk::getParameterName(VstInt32 index, char *text) {
}

void TubeDesk::getParameterDisplay(VstInt32 index, char *text) {
}

void TubeDesk::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 TubeDesk::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool TubeDesk::getEffectName(char* name) {
    vst_strncpy(name, "TubeDesk", kVstMaxProductStrLen); return true;
}

VstPlugCategory TubeDesk::getPlugCategory() {return kPlugCategEffect;}

bool TubeDesk::getProductString(char* text) {
    vst_strncpy (text, "airwindows TubeDesk", kVstMaxProductStrLen); return true;
}

bool TubeDesk::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
