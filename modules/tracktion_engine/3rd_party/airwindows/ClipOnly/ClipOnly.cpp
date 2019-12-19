/* ========================================
 *  ClipOnly - ClipOnly.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __ClipOnly_H
#include "ClipOnly.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new ClipOnly(audioMaster);}

ClipOnly::ClipOnly(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    lastSampleL = 0.0;
    wasPosClipL = false;
    wasNegClipL = false;

    lastSampleR = 0.0;
    wasPosClipR = false;
    wasNegClipR = false;
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

ClipOnly::~ClipOnly() {}
VstInt32 ClipOnly::getVendorVersion () {return 1000;}
void ClipOnly::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void ClipOnly::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!


VstInt32 ClipOnly::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool ClipOnly::getEffectName(char* name) {
    vst_strncpy(name, "ClipOnly", kVstMaxProductStrLen); return true;
}

VstPlugCategory ClipOnly::getPlugCategory() {return kPlugCategEffect;}

bool ClipOnly::getProductString(char* text) {
    vst_strncpy (text, "airwindows ClipOnly", kVstMaxProductStrLen); return true;
}

bool ClipOnly::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
