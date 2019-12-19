/* ========================================
 *  PodcastDeluxe - PodcastDeluxe.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PodcastDeluxe_H
#include "PodcastDeluxe.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new PodcastDeluxe(audioMaster);}

PodcastDeluxe::PodcastDeluxe(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    A = 0.5;

    for(int count = 0; count < 502; count++) {
        d1L[count] = 0.0;
        d2L[count] = 0.0;
        d3L[count] = 0.0;
        d4L[count] = 0.0;
        d5L[count] = 0.0;
        d1R[count] = 0.0;
        d2R[count] = 0.0;
        d3R[count] = 0.0;
        d4R[count] = 0.0;
        d5R[count] = 0.0;
    }
    c1L = 2.0; c2L = 2.0; c3L = 2.0; c4L = 2.0; c5L = 2.0; //startup comp gains

    tap1 = 1; tap2 = 1; tap3 = 1; tap4 = 1; tap5 = 1;
    maxdelay1 = 9001; maxdelay2 = 9001; maxdelay3 = 9001; maxdelay4 = 9001; maxdelay5 = 9001;
    c1R = 2.0; c2R = 2.0; c3R = 2.0; c4R = 2.0; c5R = 2.0; //startup comp gains

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

PodcastDeluxe::~PodcastDeluxe() {}
VstInt32 PodcastDeluxe::getVendorVersion () {return 1000;}
void PodcastDeluxe::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void PodcastDeluxe::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
    if (data < 0.0f) return 0.0f;
    if (data > 1.0f) return 1.0f;
    return data;
}

VstInt32 PodcastDeluxe::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = A;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 PodcastDeluxe::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    A = pinParameter(chunkData[0]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void PodcastDeluxe::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float PodcastDeluxe::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void PodcastDeluxe::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Boost", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void PodcastDeluxe::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void PodcastDeluxe::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 PodcastDeluxe::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool PodcastDeluxe::getEffectName(char* name) {
    vst_strncpy(name, "PodcastDeluxe", kVstMaxProductStrLen); return true;
}

VstPlugCategory PodcastDeluxe::getPlugCategory() {return kPlugCategEffect;}

bool PodcastDeluxe::getProductString(char* text) {
    vst_strncpy (text, "airwindows PodcastDeluxe", kVstMaxProductStrLen); return true;
}

bool PodcastDeluxe::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
