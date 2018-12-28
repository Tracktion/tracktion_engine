/* ========================================
 *  CStrip - CStrip.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __CStrip_H
#include "CStrip.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new CStrip(audioMaster);}

CStrip::CStrip(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 0.5; //Treble -12 to 12
	B = 0.5; //Mid -12 to 12
	C = 0.5; //Bass -12 to 12
	D = 1.0; //Lowpass 16.0K log 1 to 16 defaulting to 16K
	E = 0.4; //TrebFrq 6.0 log 1 to 16 defaulting to 6K
	F = 0.4; //BassFrq 100.0 log 30 to 1600 defaulting to 100 hz
	G = 0.0; //Hipass 30.0 log 30 to 1600 defaulting to 30
	H = 0.0; //Gate 0-1
	I = 0.0; //Compres 0-1
	J = 0.0; //CompSpd 0-1
	K = 0.0; //TimeLag 0-1
	L = 0.5; //OutGain -18 to 18

	lastSampleL = 0.0;
	last2SampleL = 0.0;
	lastSampleR = 0.0;
	last2SampleR = 0.0;

	iirHighSampleLA = 0.0;
	iirHighSampleLB = 0.0;
	iirHighSampleLC = 0.0;
	iirHighSampleLD = 0.0;
	iirHighSampleLE = 0.0;
	iirLowSampleLA = 0.0;
	iirLowSampleLB = 0.0;
	iirLowSampleLC = 0.0;
	iirLowSampleLD = 0.0;
	iirLowSampleLE = 0.0;
	iirHighSampleL = 0.0;
	iirLowSampleL = 0.0;

	iirHighSampleRA = 0.0;
	iirHighSampleRB = 0.0;
	iirHighSampleRC = 0.0;
	iirHighSampleRD = 0.0;
	iirHighSampleRE = 0.0;
	iirLowSampleRA = 0.0;
	iirLowSampleRB = 0.0;
	iirLowSampleRC = 0.0;
	iirLowSampleRD = 0.0;
	iirLowSampleRE = 0.0;
	iirHighSampleR = 0.0;
	iirLowSampleR = 0.0;

	tripletLA = 0.0;
	tripletLB = 0.0;
	tripletLC = 0.0;
	tripletFactorL = 0.0;

	tripletRA = 0.0;
	tripletRB = 0.0;
	tripletRC = 0.0;
	tripletFactorR = 0.0;

	lowpassSampleLAA = 0.0;
	lowpassSampleLAB = 0.0;
	lowpassSampleLBA = 0.0;
	lowpassSampleLBB = 0.0;
	lowpassSampleLCA = 0.0;
	lowpassSampleLCB = 0.0;
	lowpassSampleLDA = 0.0;
	lowpassSampleLDB = 0.0;
	lowpassSampleLE = 0.0;
	lowpassSampleLF = 0.0;
	lowpassSampleLG = 0.0;

	lowpassSampleRAA = 0.0;
	lowpassSampleRAB = 0.0;
	lowpassSampleRBA = 0.0;
	lowpassSampleRBB = 0.0;
	lowpassSampleRCA = 0.0;
	lowpassSampleRCB = 0.0;
	lowpassSampleRDA = 0.0;
	lowpassSampleRDB = 0.0;
	lowpassSampleRE = 0.0;
	lowpassSampleRF = 0.0;
	lowpassSampleRG = 0.0;

	highpassSampleLAA = 0.0;
	highpassSampleLAB = 0.0;
	highpassSampleLBA = 0.0;
	highpassSampleLBB = 0.0;
	highpassSampleLCA = 0.0;
	highpassSampleLCB = 0.0;
	highpassSampleLDA = 0.0;
	highpassSampleLDB = 0.0;
	highpassSampleLE = 0.0;
	highpassSampleLF = 0.0;

	highpassSampleRAA = 0.0;
	highpassSampleRAB = 0.0;
	highpassSampleRBA = 0.0;
	highpassSampleRBB = 0.0;
	highpassSampleRCA = 0.0;
	highpassSampleRCB = 0.0;
	highpassSampleRDA = 0.0;
	highpassSampleRDB = 0.0;
	highpassSampleRE = 0.0;
	highpassSampleRF = 0.0;

	flip = false;
	flipthree = 0;
	//end EQ

	//begin Gate
	WasNegativeL = false;
	ZeroCrossL = 0;
	gaterollerL = 0.0;
	gateL = 0.0;

	WasNegativeR = false;
	ZeroCrossR = 0;
	gaterollerR = 0.0;
	gateR = 0.0;
	//end Gate

	//begin Timing
	for(int fcount = 0; fcount < 4098; fcount++) {pL[fcount] = 0.0; pR[fcount] = 0.0;}
	count = 0;
	//end Timing

	//begin ButterComp
	controlAposL = 1.0;
	controlAnegL = 1.0;
	controlBposL = 1.0;
	controlBnegL = 1.0;
	targetposL = 1.0;
	targetnegL = 1.0;
	avgLA = avgLB = 0.0;
	nvgLA = nvgLB = 0.0;

	controlAposR = 1.0;
	controlAnegR = 1.0;
	controlBposR = 1.0;
	controlBnegR = 1.0;
	targetposR = 1.0;
	targetnegR = 1.0;
	avgRA = avgRB = 0.0;
	nvgRA = nvgRB = 0.0;
	//end ButterComp

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

CStrip::~CStrip() {}
VstInt32 CStrip::getVendorVersion () {return 1000;}
void CStrip::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void CStrip::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 CStrip::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	chunkData[1] = B;
	chunkData[2] = C;
	chunkData[3] = D;
	chunkData[4] = E;
	chunkData[5] = F;
	chunkData[6] = G;
	chunkData[7] = H;
	chunkData[8] = I;
	chunkData[9] = J;
	chunkData[10] = K;
	chunkData[11] = L;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you
	 started with. */

	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 CStrip::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	B = pinParameter(chunkData[1]);
	C = pinParameter(chunkData[2]);
	D = pinParameter(chunkData[3]);
	E = pinParameter(chunkData[4]);
	F = pinParameter(chunkData[5]);
	G = pinParameter(chunkData[6]);
	H = pinParameter(chunkData[7]);
	I = pinParameter(chunkData[8]);
	J = pinParameter(chunkData[9]);
	K = pinParameter(chunkData[10]);
	L = pinParameter(chunkData[11]);
	/* We're ignoring byteSize as we found it to be a filthy liar */

	/* calculate any other fields you need here - you could copy in
	 code from setParameter() here. */
	return 0;
}

void CStrip::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        case kParamB: B = value; break;
        case kParamC: C = value; break;
        case kParamD: D = value; break;
        case kParamE: E = value; break;
        case kParamF: F = value; break;
        case kParamG: G = value; break;
        case kParamH: H = value; break;
        case kParamI: I = value; break;
        case kParamJ: J = value; break;
        case kParamK: K = value; break;
        case kParamL: L = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float CStrip::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        case kParamB: return B; break;
        case kParamC: return C; break;
        case kParamD: return D; break;
        case kParamE: return E; break;
        case kParamF: return F; break;
        case kParamG: return G; break;
        case kParamH: return H; break;
        case kParamI: return I; break;
        case kParamJ: return J; break;
        case kParamK: return K; break;
        case kParamL: return L; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void CStrip::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Treble", kVstMaxParamStrLen); break;
		case kParamB: vst_strncpy (text, "Mid", kVstMaxParamStrLen); break;
		case kParamC: vst_strncpy (text, "Bass", kVstMaxParamStrLen); break;
		case kParamD: vst_strncpy (text, "Lowpass", kVstMaxParamStrLen); break;
		case kParamE: vst_strncpy (text, "TrebFrq", kVstMaxParamStrLen); break;
		case kParamF: vst_strncpy (text, "BassFrq", kVstMaxParamStrLen); break;
		case kParamG: vst_strncpy (text, "Hipass", kVstMaxParamStrLen); break;
		case kParamH: vst_strncpy (text, "Gate", kVstMaxParamStrLen); break;
		case kParamI: vst_strncpy (text, "Compres", kVstMaxParamStrLen); break;
		case kParamJ: vst_strncpy (text, "CompSpd", kVstMaxParamStrLen); break;
		case kParamK: vst_strncpy (text, "TimeLag", kVstMaxParamStrLen); break;
		case kParamL: vst_strncpy (text, "OutGain", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void CStrip::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string ((A*24.0)-12.0, text, kVstMaxParamStrLen); break; //Treble -12 to 12
        case kParamB: float2string ((B*24.0)-12.0, text, kVstMaxParamStrLen); break; //Mid -12 to 12
        case kParamC: float2string ((C*24.0)-12.0, text, kVstMaxParamStrLen); break; //Bass -12 to 12
        case kParamD: float2string ((D*D*15.0)+1.0, text, kVstMaxParamStrLen); break; //Lowpass 16.0K log 1 to 16 defaulting to 16K
        case kParamE: float2string ((E*E*15.0)+1.0, text, kVstMaxParamStrLen); break; //TrebFrq 6.0 log 1 to 16 defaulting to 6K
        case kParamF: float2string ((F*F*1570.0)+30.0, text, kVstMaxParamStrLen); break; //BassFrq 100.0 log 30 to 1600 defaulting to 100 hz
        case kParamG: float2string ((G*G*1570.0)+30.0, text, kVstMaxParamStrLen); break; //Hipass 30.0 log 30 to 1600 defaulting to 30
        case kParamH: float2string (H, text, kVstMaxParamStrLen); break; //Gate 0-1
        case kParamI: float2string (I, text, kVstMaxParamStrLen); break; //Compres 0-1
        case kParamJ: float2string (J, text, kVstMaxParamStrLen); break; //CompSpd 0-1
        case kParamK: float2string (K, text, kVstMaxParamStrLen); break; //TimeLag 0-1
        case kParamL: float2string ((L*36.0)-18.0, text, kVstMaxParamStrLen); break; //OutGain -18 to 18
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void CStrip::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamB: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamC: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
        case kParamD: vst_strncpy (text, "Khz", kVstMaxParamStrLen); break;
        case kParamE: vst_strncpy (text, "Khz", kVstMaxParamStrLen); break;
        case kParamF: vst_strncpy (text, "hz", kVstMaxParamStrLen); break;
        case kParamG: vst_strncpy (text, "hz", kVstMaxParamStrLen); break;
        case kParamH: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamI: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamJ: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamK: vst_strncpy (text, "", kVstMaxParamStrLen); break;
        case kParamL: vst_strncpy (text, "dB", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 CStrip::canDo(char *text)
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool CStrip::getEffectName(char* name) {
    vst_strncpy(name, "CStrip", kVstMaxProductStrLen); return true;
}

VstPlugCategory CStrip::getPlugCategory() {return kPlugCategEffect;}

bool CStrip::getProductString(char* text) {
  	vst_strncpy (text, "airwindows CStrip", kVstMaxProductStrLen); return true;
}

bool CStrip::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}
