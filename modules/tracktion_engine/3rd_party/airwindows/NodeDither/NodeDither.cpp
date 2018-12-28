/* ========================================
 *  NodeDither - NodeDither.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __NodeDither_H
#include "NodeDither.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new NodeDither(audioMaster);}

NodeDither::NodeDither(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.04;
	B = 0.0;
	for(int count = 0; count < 4999; count++) {dL[count] = 0; dR[count] = 0;}
	gcount = 0;
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

NodeDither::~NodeDither() {}
VstInt32 NodeDither::getVendorVersion () {return 1000;}
void NodeDither::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void NodeDither::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 NodeDither::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
	 started with. */

	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 NodeDither::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	/* We're ignoring byteSize as we found it to be a filthy liar */

	/* calculate any other fields you need here - you could copy in
	 code from setParameter() here. */
	return 0;
}

void NodeDither::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float NodeDither::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void NodeDither::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Node", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Phase", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void NodeDither::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: int2string ((VstInt32) floor(A * 100), text, kVstMaxParamStrLen); break;
        case kParamB: switch((VstInt32)( B * 1.999 )) //0 to almost edge of # of params
		{case 0: vst_strncpy (text, "Out", kVstMaxParamStrLen); break;
		 case 1: vst_strncpy (text, "In", kVstMaxParamStrLen); break;
		 default: break; // unknown parameter, shouldn't happen!
		} break; //completed D 'popup' parameter, exit
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void NodeDither::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "samples", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, " ", kVstMaxParamStrLen); break; //the percent
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 NodeDither::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool NodeDither::getEffectName(char* name) {
    vst_strncpy(name, "NodeDither", kVstMaxProductStrLen); return true;
}

VstPlugCategory NodeDither::getPlugCategory() {return kPlugCategEffect;}

bool NodeDither::getProductString(char* text) {
  	vst_strncpy (text, "airwindows NodeDither", kVstMaxProductStrLen); return true;
}

bool NodeDither::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
