/* ========================================
 *  C5RawChannel - C5RawChannel.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __C5RawChannel_H
#include "C5RawChannel.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new C5RawChannel(audioMaster);}

C5RawChannel::C5RawChannel(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.0;
	lastFXChannelL = 0.0;
	lastSampleChannelL = 0.0;
	lastFXChannelR = 0.0;
	lastSampleChannelR = 0.0;
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

C5RawChannel::~C5RawChannel() {}
VstInt32 C5RawChannel::getVendorVersion () {return 1000;}
void C5RawChannel::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void C5RawChannel::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 C5RawChannel::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
	 started with. */

	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 C5RawChannel::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	/* We're ignoring byteSize as we found it to be a filthy liar */

	/* calculate any other fields you need here - you could copy in
	 code from setParameter() here. */
	return 0;
}

void C5RawChannel::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float C5RawChannel::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void C5RawChannel::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Center", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void C5RawChannel::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void C5RawChannel::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 C5RawChannel::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool C5RawChannel::getEffectName(char* name) {
    vst_strncpy(name, "C5RawChannel", kVstMaxProductStrLen); return true;
}

VstPlugCategory C5RawChannel::getPlugCategory() {return kPlugCategEffect;}

bool C5RawChannel::getProductString(char* text) {
  	vst_strncpy (text, "airwindows C5RawChannel", kVstMaxProductStrLen); return true;
}

bool C5RawChannel::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
