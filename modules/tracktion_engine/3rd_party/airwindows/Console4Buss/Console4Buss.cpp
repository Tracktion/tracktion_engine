/* ========================================
 *  Console4Buss - Console4Buss.h
 *  Created 8/12/11 by SPIAdmin
 *  Copyright (c) 2011 __MyCompanyName__, All rights reserved
 * ======================================== */

#ifndef __Console4Buss_H
#include "Console4Buss.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster)
{
    return new Console4Buss(audioMaster);
}

Console4Buss::Console4Buss(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
    gain = 1.0;
    lastSampleL = 0.0;
    lastSampleR = 0.0;
    gainchase = -90.0;
    settingchase = -90.0;
    chasespeed = 350.0;
    fpNShapeL = 0.0;
    fpNShapeR = 0.0;

    // TODO: uncomment canDo entries according to your plugin's capabilities
//    _canDo.insert("sendVstEvents"); // plug-in will send Vst events to Host.
//    _canDo.insert("sendVstMidiEvent"); // plug-in will send MIDI events to Host.
//    _canDo.insert("sendVstTimeInfo"); // unknown
//    _canDo.insert("receiveVstEvents"); // plug-in can receive Vst events from Host.
//    _canDo.insert("receiveVstMidiEvent"); // plug-in can receive MIDI events from Host.
//    _canDo.insert("receiveVstTimeInfo"); // plug-in can receive Time info from Host.
//    _canDo.insert("offline"); // plug-in supports offline functions (#offlineNotify, #offlinePrepare, #offlineRun).
    _canDo.insert("plugAsChannelInsert"); // plug-in can be used as a channel insert effect.
    _canDo.insert("plugAsSend"); // plug-in can be used as a send effect.
//    _canDo.insert("mixDryWet"); // dry/wet mix control
//    _canDo.insert("noRealTime"); // no real-time processing
//    _canDo.insert("multipass"); // unknown
//    _canDo.insert("metapass"); // unknown
//    _canDo.insert("x1in1out");
//    _canDo.insert("x1in2out");
//    _canDo.insert("x2in1out");
    _canDo.insert("x2in2out");
//    _canDo.insert("x2in4out");
//    _canDo.insert("x4in2out");
//    _canDo.insert("x4in4out");
//    _canDo.insert("x4in8out"); // 4:2 matrix to surround bus
//    _canDo.insert("x8in4out"); // surround bus to 4:2 matrix
//    _canDo.insert("x8in8out");
//    _canDo.insert("midiProgramNames"); // plug-in supports function #getMidiProgramName().
//    _canDo.insert("conformsToWindowRules"); // mac: doesn't mess with grafport.
//    _canDo.insert("bypass"); // plug-in supports function #setBypass().


    // these configuration values are established in the header
    setNumInputs(kNumInputs);
    setNumOutputs(kNumOutputs);
    setUniqueID(kUniqueId);
    canProcessReplacing();     // supports output replacing
    canDoubleReplacing();      // supports double precision processing
    programsAreChunks(true);

    vst_strncpy (_programName, "Default", kVstMaxProgNameLen); // default program name
}

Console4Buss::~Console4Buss()
{
}

VstInt32 Console4Buss::getVendorVersion ()
{
    // TODO: return version number
    return 1000;
}

void Console4Buss::setProgramName(char *name) {
    vst_strncpy (_programName, name, kVstMaxProgNameLen);
}

void Console4Buss::getProgramName(char *name) {
    vst_strncpy (name, _programName, kVstMaxProgNameLen);
}

static float pinParameter(float data)
{
    if (data < 0.0f) return 0.0f;
    if (data > 1.0f) return 1.0f;
    return data;
}

VstInt32 Console4Buss::getChunk (void** data, bool isPreset)
{
    float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
    chunkData[0] = gain;
    /* Note: The way this is set up, it will break if you manage to save settings on an Intel
     machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
     started with. */

    *data = chunkData;
    return kNumParameters * sizeof(float);
}

VstInt32 Console4Buss::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
    float *chunkData = (float *)data;
    gain = pinParameter(chunkData[0]);
    /* We're ignoring byteSize as we found it to be a filthy liar */

    /* calculate any other fields you need here - you could copy in
     code from setParameter() here. */
    return 0;
}

void Console4Buss::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kConsole4BussParam:
            gain = value;
            break;
        default: // unknown parameter, shouldn't happen!
            throw;
    }
}

float Console4Buss::getParameter(VstInt32 index) {
    switch (index) {
        case kConsole4BussParam:
            return gain;
            break;
        default: // unknown parameter, shouldn't happen!
            break;
    }
    return 0.0;
}

void Console4Buss::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kConsole4BussParam:
            vst_strncpy (text, "Trim", kVstMaxParamStrLen);
            break;
        default: // unknown parameter, shouldn't happen!
            break;
    }
}

void Console4Buss::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kConsole4BussParam:
            float2string (gain, text, kVstMaxParamStrLen);
            break;
        default: // unknown parameter, shouldn't happen!
            break;
    }
}

void Console4Buss::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kConsole4BussParam:
            vst_strncpy (text, " ", kVstMaxParamStrLen);
            break;
        default: // unknown parameter, shouldn't happen!
            break;
    }
}

VstInt32 Console4Buss::canDo(char *text)
{
    // 1 = yes, -1 = no, 0 = don't know
    return (_canDo.find(text) == _canDo.end()) ? 0 : 1;
}

bool Console4Buss::getEffectName(char* name) {
    vst_strncpy(name, "Console4Buss", kVstMaxProductStrLen);
    return true;
}

VstPlugCategory Console4Buss::getPlugCategory() {
    return kPlugCategEffect;
}

bool Console4Buss::getProductString(char* text) {
    vst_strncpy (text, "Console4Buss", kVstMaxProductStrLen);
    return true;
}

bool Console4Buss::getVendorString(char* text) {
    vst_strncpy (text, "airwindows", kVstMaxVendorStrLen);
    return true;
}
