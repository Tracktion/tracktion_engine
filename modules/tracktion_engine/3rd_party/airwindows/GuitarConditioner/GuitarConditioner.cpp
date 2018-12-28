/* ========================================
 *  GuitarConditioner - GuitarConditioner.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __GuitarConditioner_H
#include "GuitarConditioner.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new GuitarConditioner(audioMaster);}

GuitarConditioner::GuitarConditioner(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	fpNShapeLA = 0.0;
	fpNShapeLB = 0.0;
	fpNShapeRA = 0.0;
	fpNShapeRB = 0.0;
	fpFlip = true;

	lastSampleTL = 0.0;
	lastSampleBL = 0.0; //for Slews. T for treble, B for bass
	iirSampleTAL = 0.0;
	iirSampleTBL = 0.0;
	iirSampleBAL = 0.0;
	iirSampleBBL = 0.0; //for Highpasses
	//this is reset: values being initialized only once. Startup values, whatever they are.
	lastSampleTR = 0.0;
	lastSampleBR = 0.0; //for Slews. T for treble, B for bass
	iirSampleTAR = 0.0;
	iirSampleTBR = 0.0;
	iirSampleBAR = 0.0;
	iirSampleBBR = 0.0; //for Highpasses
	//this is reset: values being initialized only once. Startup values, whatever they are.

    _canDo.insert("plugAsChannelInsert"); // plug-in can be used as a channel insert effect.
    _canDo.insert("plugAsSend"); // plug-in can be used as a send effect.
    _canDo.insert("x2in2out");
    setNumInputs(kNumInputs);
    setNumOutputs(kNumOutputs);
    setUniqueID(kUniqueId);
    canProcessReplacing();     // supports output replacing
    canDoubleReplacing();      // supports double precision processing
    vst_strncpy (_programName, "Default", kVstMaxProgNameLen); // default program name
}

GuitarConditioner::~GuitarConditioner() {}
VstInt32 GuitarConditioner::getVendorVersion () {return 1000;}
void GuitarConditioner::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void GuitarConditioner::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

void GuitarConditioner::setParameter(VstInt32 index, float value) {
    switch (index) {
        default: throw; // unknown parameter, shouldn't happen!
    }
	//we can also set other defaults here, and do calculations that only have to happen
	//once when parameters actually change. Here is the 'popup' setting its (global) values.
	//variables can also be set in the processreplacing loop, and there they'll be set every buffersize
	//here they're set when a parameter's actually changed, which should be less frequent, but
	//you must use global variables in the GuitarConditioner.h file to do it.
}

float GuitarConditioner::getParameter(VstInt32 index) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void GuitarConditioner::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void GuitarConditioner::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void GuitarConditioner::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 GuitarConditioner::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1 : 1; } // 1 = yes, -1 = no, 0 = don't know

bool GuitarConditioner::getEffectName(char* name) {
    vst_strncpy(name, "GuitarConditioner", kVstMaxProductStrLen); return true;
}

VstPlugCategory GuitarConditioner::getPlugCategory() {return kPlugCategEffect;}

bool GuitarConditioner::getProductString(char* text) {
  	vst_strncpy (text, "airwindows GuitarConditioner", kVstMaxProductStrLen); return true;
}

bool GuitarConditioner::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
