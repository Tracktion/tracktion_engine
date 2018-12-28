/* ========================================
 *  Console5DarkCh - Console5DarkCh.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Console5DarkCh_H
#include "Console5DarkCh.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new Console5DarkCh(audioMaster);}

Console5DarkCh::Console5DarkCh(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 1.0;
	lastSampleChannelL = 0.0;
	lastSampleChannelR = 0.0;
	lastFXChannelL = 0.0;
	lastFXChannelR = 0.0;
	iirCorrectL = 0.0;
	iirCorrectR = 0.0;
	gainchase = -90.0;
	settingchase = -90.0;
	chasespeed = 350.0;
	fpNShapeLA = 0.0;
	fpNShapeLB = 0.0;
	fpNShapeRA = 0.0;
	fpNShapeRB = 0.0;
	fpFlip = true;
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

Console5DarkCh::~Console5DarkCh() {}
VstInt32 Console5DarkCh::getVendorVersion () {return 1000;}
void Console5DarkCh::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void Console5DarkCh::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 Console5DarkCh::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
	 started with. */

	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 Console5DarkCh::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	/* We're ignoring byteSize as we found it to be a filthy liar */

	/* calculate any other fields you need here - you could copy in
	 code from setParameter() here. */
	return 0;
}

void Console5DarkCh::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float Console5DarkCh::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void Console5DarkCh::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Input", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void Console5DarkCh::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void Console5DarkCh::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 Console5DarkCh::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool Console5DarkCh::getEffectName(char* name) {
    vst_strncpy(name, "Console5DarkCh", kVstMaxProductStrLen); return true;
}

VstPlugCategory Console5DarkCh::getPlugCategory() {return kPlugCategEffect;}

bool Console5DarkCh::getProductString(char* text) {
  	vst_strncpy (text, "airwindows Console5DarkCh", kVstMaxProductStrLen); return true;
}

bool Console5DarkCh::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
