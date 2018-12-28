/* ========================================
 *  TransDesk - TransDesk.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __TransDesk_H
#include "TransDesk.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new TransDesk(audioMaster);}

TransDesk::TransDesk(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	for(int count = 0; count < 19; count++) {dL[count] = 0; dR[count] = 0;}
	gcount = 0;

	controlL = 0;
	lastSampleL = 0.0;
	lastOutSampleL = 0.0;
	lastSlewL = 0.0;

	controlR = 0;
	lastSampleR = 0.0;
	lastOutSampleR = 0.0;
	lastSlewR = 0.0;

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

TransDesk::~TransDesk() {}
VstInt32 TransDesk::getVendorVersion () {return 1000;}
void TransDesk::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void TransDesk::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 TransDesk::getChunk (void** data, bool isPreset)
{
	return kNumParameters * sizeof(float);
}

VstInt32 TransDesk::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	return 0;
}

void TransDesk::setParameter(VstInt32 index, float value) {
}

float TransDesk::getParameter(VstInt32 index) {
	return 0.0; //we only need to update the relevant name, this is simple to manage
}

void TransDesk::getParameterName(VstInt32 index, char *text) {
}

void TransDesk::getParameterDisplay(VstInt32 index, char *text) {
}

void TransDesk::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 TransDesk::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool TransDesk::getEffectName(char* name) {
    vst_strncpy(name, "TransDesk", kVstMaxProductStrLen); return true;
}

VstPlugCategory TransDesk::getPlugCategory() {return kPlugCategEffect;}

bool TransDesk::getProductString(char* text) {
  	vst_strncpy (text, "airwindows TransDesk", kVstMaxProductStrLen); return true;
}

bool TransDesk::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
