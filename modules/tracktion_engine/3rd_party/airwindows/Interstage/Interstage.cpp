/* ========================================
 *  Interstage - Interstage.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Interstage_H
#include "Interstage.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Interstage(audioMaster);}

Interstage::Interstage(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    iirSampleAL = 0.0;
    iirSampleBL = 0.0;
    iirSampleCL = 0.0;
    iirSampleDL = 0.0;
    iirSampleEL = 0.0;
    iirSampleFL = 0.0;
    lastSampleL = 0.0;
    iirSampleAR = 0.0;
    iirSampleBR = 0.0;
    iirSampleCR = 0.0;
    iirSampleDR = 0.0;
    iirSampleER = 0.0;
    iirSampleFR = 0.0;
    lastSampleR = 0.0;
    fpd = 17;
    flip = true;
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

Interstage::~Interstage() {}
VstInt32 Interstage::getVendorVersion () {return 1000;}
void Interstage::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Interstage::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 Interstage::getChunk (void** data, bool isPreset)
{
    return 0;
}

VstInt32 Interstage::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    return 0;
}

void Interstage::setParameter(VstInt32 index, float value) {
    switch (index) {
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Interstage::getParameter(VstInt32 index) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Interstage::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Interstage::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void Interstage::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Interstage::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Interstage::getEffectName(char* name) {
    vst_strncpy(name, "Interstage", kVstMaxProductStrLen); return true;
}

VstPlugCategory Interstage::getPlugCategory() {return kPlugCategEffect;}

bool Interstage::getProductString(char* text) {
    vst_strncpy (text, "airwindows Interstage", kVstMaxProductStrLen); return true;
}

bool Interstage::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
