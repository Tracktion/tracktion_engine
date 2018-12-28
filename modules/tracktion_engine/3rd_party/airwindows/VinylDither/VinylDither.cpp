/* ========================================
 *  VinylDither - VinylDither.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __VinylDither_H
#include "VinylDither.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new VinylDither(audioMaster);}

VinylDither::VinylDither(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	NSOddL = 0.0;
	prevL = 0.0;
	nsL[0] = 0;
	nsL[1] = 0;
	nsL[2] = 0;
	nsL[3] = 0;
	nsL[4] = 0;
	nsL[5] = 0;
	nsL[6] = 0;
	nsL[7] = 0;
	nsL[8] = 0;
	nsL[9] = 0;
	nsL[10] = 0;
	nsL[11] = 0;
	nsL[12] = 0;
	nsL[13] = 0;
	nsL[14] = 0;
	nsL[15] = 0;
	NSOddR = 0.0;
	prevR = 0.0;
	nsR[0] = 0;
	nsR[1] = 0;
	nsR[2] = 0;
	nsR[3] = 0;
	nsR[4] = 0;
	nsR[5] = 0;
	nsR[6] = 0;
	nsR[7] = 0;
	nsR[8] = 0;
	nsR[9] = 0;
	nsR[10] = 0;
	nsR[11] = 0;
	nsR[12] = 0;
	nsR[13] = 0;
	nsR[14] = 0;
	nsR[15] = 0;
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

VinylDither::~VinylDither() {}
VstInt32 VinylDither::getVendorVersion () {return 1000;}
void VinylDither::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void VinylDither::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

VstInt32 VinylDither::getChunk (void** data, bool isPreset)
{
	return kNumParameters * sizeof(float);
}

VstInt32 VinylDither::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	return 0;
}

void VinylDither::setParameter(VstInt32 index, float value) {
}

float VinylDither::getParameter(VstInt32 index) {
	return 0.0; //we only need to update the relevant name, this is simple to manage
}

void VinylDither::getParameterName(VstInt32 index, char *text) {
}

void VinylDither::getParameterDisplay(VstInt32 index, char *text) {
}

void VinylDither::getParameterLabel(VstInt32 index, char *text) {
}

VstInt32 VinylDither::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool VinylDither::getEffectName(char* name) {
    vst_strncpy(name, "VinylDither", kVstMaxProductStrLen); return true;
}

VstPlugCategory VinylDither::getPlugCategory() {return kPlugCategEffect;}

bool VinylDither::getProductString(char* text) {
  	vst_strncpy (text, "airwindows VinylDither", kVstMaxProductStrLen); return true;
}

bool VinylDither::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
