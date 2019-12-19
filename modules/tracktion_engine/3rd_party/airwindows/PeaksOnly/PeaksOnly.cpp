/* ========================================
 *  PeaksOnly - PeaksOnly.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PeaksOnly_H
#include "PeaksOnly.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new PeaksOnly(audioMaster);}

PeaksOnly::PeaksOnly(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    for(int count = 0; count < 1502; count++) {aL[count] = 0.0; bL[count] = 0.0; cL[count] = 0.0; dL[count] = 0.0;aR[count] = 0.0; bR[count] = 0.0; cR[count] = 0.0; dR[count] = 0.0;}
    ax = 1; bx = 1; cx = 1; dx = 1;
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

PeaksOnly::~PeaksOnly() {}
VstInt32 PeaksOnly::getVendorVersion () {return 1000;}
void PeaksOnly::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void PeaksOnly::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

void PeaksOnly::setParameter(VstInt32 index, float value) {
}

float PeaksOnly::getParameter(VstInt32 index) {
    return 0.0; //we only need to update the relevant name, this is simple to manage
}

void PeaksOnly::getParameterName(VstInt32 index, char *text) {
}

void PeaksOnly::getParameterDisplay(VstInt32 index, char *text) {
}

void PeaksOnly::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 PeaksOnly::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool PeaksOnly::getEffectName(char* name) {
    vst_strncpy(name, "PeaksOnly", kVstMaxProductStrLen); return true;
}

VstPlugCategory PeaksOnly::getPlugCategory() {return kPlugCategEffect;}

bool PeaksOnly::getProductString(char* text) {
    vst_strncpy (text, "airwindows PeaksOnly", kVstMaxProductStrLen); return true;
}

bool PeaksOnly::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
