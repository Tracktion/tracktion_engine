/* ========================================
 *  Channel4 - Channel4.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Channel4_H
#include "Channel4.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Channel4(audioMaster);}

Channel4::Channel4(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    consoletype = 0.0;
    drive = 0.0;
    fpNShapeLA = 0.0;
    fpNShapeLB = 0.0;
    fpNShapeRA = 0.0;
    fpNShapeRB = 0.0;
    fpFlip = true;
    iirSampleLA = 0.0;
    iirSampleRA = 0.0;
    iirSampleLB = 0.0;
    iirSampleRB = 0.0;
    lastSampleL = 0.0;
    lastSampleR = 0.0;
    iirAmount = 0.005832;
    threshold = 0.33362176; //instantiating with Neve values

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

Channel4::~Channel4() {}
VstInt32 Channel4::getVendorVersion () {return 1000;}
void Channel4::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Channel4::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
    if (data < 0.0f) return 0.0f;
    if (data > 1.0f) return 1.0f;
    return data;
}

VstInt32 Channel4::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = consoletype;
    chunkData[1] = drive;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 Channel4::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    consoletype = pinParameter(chunkData[0]);
    drive = pinParameter(chunkData[1]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void Channel4::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: consoletype = value; break;
        case kParamB: drive = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
    //we can also set other defaults here, and do calculations that only have to happen
    //once when parameters actually change. Here is the 'popup' setting its (global) values.
    //variables can also be set in the processreplacing loop, and there they'll be set every buffersize
    //here they're set when a parameter's actually changed, which should be less frequent, but
    //you must use global variables in the Channel4.h file to do it.
    switch((VstInt32)( consoletype * 2.999 ))
    {
        case 0: iirAmount = 0.005832; threshold = 0.33362176; break; //Neve
        case 1: iirAmount = 0.004096; threshold = 0.59969536; break; //API
        case 2: iirAmount = 0.004913; threshold = 0.84934656; break; //SSL
        default: break; //should not happen
    }
    //this relates to using D as a 'popup' and changing things based on that switch.
    //we are using fpFlip just because it's already there globally, as an example.
}

float Channel4::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return consoletype; break;
        case kParamB: return drive; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Channel4::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Console Type", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "Drive", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Channel4::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: switch((VstInt32)( consoletype * 2.999 )) //0 to almost edge of # of params
        {   case 0: vst_strncpy (text, "Neve", kVstMaxParamStrLen); break;
            case 1: vst_strncpy (text, "API", kVstMaxParamStrLen); break;
            case 2: vst_strncpy (text, "SSL", kVstMaxParamStrLen); break;
            default: break; // unknown parameter, shouldn't happen!
        } break; //completed consoletype 'popup' parameter, exit
        case kParamB: int2string ((VstInt32)(drive*100), text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this displays the values and handles 'popups' where it's discrete choices
}

void Channel4::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "%", kVstMaxParamStrLen); break; //the percent
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Channel4::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1 : 1; } // 1 = yes, -1 = no, 0 = don't know

bool Channel4::getEffectName(char* name) {
    vst_strncpy(name, "Channel4", kVstMaxProductStrLen); return true;
}

VstPlugCategory Channel4::getPlugCategory() {return kPlugCategEffect;}

bool Channel4::getProductString(char* text) {
    vst_strncpy (text, "airwindows Channel4", kVstMaxProductStrLen); return true;
}

bool Channel4::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
