/* ========================================
 *  kCathedral2 - kCathedral2.h
 *  Copyright (c) airwindows, Airwindows uses the MIT license
 * ======================================== */

#ifndef __kCathedral2_H
#include "kCathedral2.h"
#endif

AudioEffect* createEffectInstance(audioMasterCallback audioMaster) {return new kCathedral2(audioMaster);}

kCathedral2::kCathedral2(audioMasterCallback audioMaster) :
    AudioEffectX(audioMaster, kNumPrograms, kNumParameters)
{
	A = 1.0;
	
	gainOutL = gainOutR = 1.0;
	
	for(int count = 0; count < shortA+2; count++) {eAL[count] = 0.0; eAR[count] = 0.0;}
	for(int count = 0; count < shortB+2; count++) {eBL[count] = 0.0; eBR[count] = 0.0;}
	for(int count = 0; count < shortC+2; count++) {eCL[count] = 0.0; eCR[count] = 0.0;}
	for(int count = 0; count < shortD+2; count++) {eDL[count] = 0.0; eDR[count] = 0.0;}
	for(int count = 0; count < shortE+2; count++) {eEL[count] = 0.0; eER[count] = 0.0;}
	for(int count = 0; count < shortF+2; count++) {eFL[count] = 0.0; eFR[count] = 0.0;}
	for(int count = 0; count < shortG+2; count++) {eGL[count] = 0.0; eGR[count] = 0.0;}
	for(int count = 0; count < shortH+2; count++) {eHL[count] = 0.0; eHR[count] = 0.0;}
	for(int count = 0; count < shortI+2; count++) {eIL[count] = 0.0; eIR[count] = 0.0;}
	for(int count = 0; count < shortJ+2; count++) {eJL[count] = 0.0; eJR[count] = 0.0;}
	for(int count = 0; count < shortK+2; count++) {eKL[count] = 0.0; eKR[count] = 0.0;}
	for(int count = 0; count < shortL+2; count++) {eLL[count] = 0.0; eLR[count] = 0.0;}
	for(int count = 0; count < shortM+2; count++) {eML[count] = 0.0; eMR[count] = 0.0;}
	for(int count = 0; count < shortN+2; count++) {eNL[count] = 0.0; eNR[count] = 0.0;}
	for(int count = 0; count < shortO+2; count++) {eOL[count] = 0.0; eOR[count] = 0.0;}
	for(int count = 0; count < shortP+2; count++) {ePL[count] = 0.0; ePR[count] = 0.0;}		
	
	
	shortAL = 1;
	shortBL = 1;
	shortCL = 1;
	shortDL = 1;	
	shortEL = 1;
	shortFL = 1;
	shortGL = 1;
	shortHL = 1;
	shortIL = 1;
	shortJL = 1;
	shortKL = 1;
	shortLL = 1;
	shortML = 1;
	shortNL = 1;
	shortOL = 1;
	shortPL = 1;
	
	shortAR = 1;
	shortBR = 1;
	shortCR = 1;
	shortDR = 1;	
	shortER = 1;
	shortFR = 1;
	shortGR = 1;
	shortHR = 1;
	shortIR = 1;
	shortJR = 1;
	shortKR = 1;
	shortLR = 1;
	shortMR = 1;
	shortNR = 1;
	shortOR = 1;
	shortPR = 1;
	
	
	for(int count = 0; count < delayA+2; count++) {aAL[count] = 0.0; aAR[count] = 0.0;}
	for(int count = 0; count < delayB+2; count++) {aBL[count] = 0.0; aBR[count] = 0.0;}
	for(int count = 0; count < delayC+2; count++) {aCL[count] = 0.0; aCR[count] = 0.0;}
	for(int count = 0; count < delayD+2; count++) {aDL[count] = 0.0; aDR[count] = 0.0;}
	for(int count = 0; count < delayE+2; count++) {aEL[count] = 0.0; aER[count] = 0.0;}
	for(int count = 0; count < delayF+2; count++) {aFL[count] = 0.0; aFR[count] = 0.0;}
	for(int count = 0; count < delayG+2; count++) {aGL[count] = 0.0; aGR[count] = 0.0;}
	for(int count = 0; count < delayH+2; count++) {aHL[count] = 0.0; aHR[count] = 0.0;}
	for(int count = 0; count < delayI+2; count++) {aIL[count] = 0.0; aIR[count] = 0.0;}
	for(int count = 0; count < delayJ+2; count++) {aJL[count] = 0.0; aJR[count] = 0.0;}
	for(int count = 0; count < delayK+2; count++) {aKL[count] = 0.0; aKR[count] = 0.0;}
	for(int count = 0; count < delayL+2; count++) {aLL[count] = 0.0; aLR[count] = 0.0;}
	for(int count = 0; count < delayM+2; count++) {aML[count] = 0.0; aMR[count] = 0.0;}
	for(int count = 0; count < delayN+2; count++) {aNL[count] = 0.0; aNR[count] = 0.0;}
	for(int count = 0; count < delayO+2; count++) {aOL[count] = 0.0; aOR[count] = 0.0;}
	for(int count = 0; count < delayP+2; count++) {aPL[count] = 0.0; aPR[count] = 0.0;}
	for(int count = 0; count < delayQ+2; count++) {aQL[count] = 0.0; aQR[count] = 0.0;}
	for(int count = 0; count < delayR+2; count++) {aRL[count] = 0.0; aRR[count] = 0.0;}
	for(int count = 0; count < delayS+2; count++) {aSL[count] = 0.0; aSR[count] = 0.0;}
	for(int count = 0; count < delayT+2; count++) {aTL[count] = 0.0; aTR[count] = 0.0;}
	for(int count = 0; count < delayU+2; count++) {aUL[count] = 0.0; aUR[count] = 0.0;}
	for(int count = 0; count < delayV+2; count++) {aVL[count] = 0.0; aVR[count] = 0.0;}
	for(int count = 0; count < delayW+2; count++) {aWL[count] = 0.0; aWR[count] = 0.0;}
	for(int count = 0; count < delayX+2; count++) {aXL[count] = 0.0; aXR[count] = 0.0;}
	for(int count = 0; count < delayY+2; count++) {aYL[count] = 0.0; aYR[count] = 0.0;}
	
	for(int count = 0; count < predelay+2; count++) {aZL[count] = 0.0; aZR[count] = 0.0;}
	for(int count = 0; count < vlfpredelay+2; count++) {aVLFL[count] = 0.0; aVLFR[count] = 0.0;}
	
	feedbackAL = 0.0;
	feedbackBL = 0.0;
	feedbackCL = 0.0;
	feedbackDL = 0.0;
	feedbackEL = 0.0;
	
	feedbackER = 0.0;
	feedbackJR = 0.0;
	feedbackOR = 0.0;
	feedbackTR = 0.0;
	feedbackYR = 0.0;
	
	for(int count = 0; count < 6; count++) {lastRefL[count] = 0.0; lastRefR[count] = 0.0;}
	
	countAL = 1;
	countBL = 1;
	countCL = 1;
	countDL = 1;	
	countEL = 1;
	countFL = 1;
	countGL = 1;
	countHL = 1;
	countIL = 1;
	countJL = 1;
	countKL = 1;
	countLL = 1;
	countML = 1;
	countNL = 1;
	countOL = 1;
	countPL = 1;
	countQL = 1;
	countRL = 1;
	countSL = 1;
	countTL = 1;
	countUL = 1;
	countVL = 1;
	countWL = 1;
	countXL = 1;
	countYL = 1;
	
	countAR = 1;
	countBR = 1;
	countCR = 1;
	countDR = 1;	
	countER = 1;
	countFR = 1;
	countGR = 1;
	countHR = 1;
	countIR = 1;
	countJR = 1;
	countKR = 1;
	countLR = 1;
	countMR = 1;
	countNR = 1;
	countOR = 1;
	countPR = 1;
	countQR = 1;
	countRR = 1;
	countSR = 1;
	countTR = 1;
	countUR = 1;
	countVR = 1;
	countWR = 1;
	countXR = 1;
	countYR = 1;
	
	countZ = 1;
	countVLF = 1;
	
	cycle = 0;
	
	for (int x = 0; x < pear_total; x++) {pearA[x] = 0.0; pearB[x] = 0.0; pearC[x] = 0.0; pearD[x] = 0.0; pearE[x] = 0.0; pearF[x] = 0.0;}
	//from PearEQ

	vibratoL = vibAL = vibAR = vibBL = vibBR = 0.0;
	vibratoR = 0.785398163397448309616; // M_PI_4;

	subAL = subAR = subBL = subBR = subCL = subCR = 0.0;
	sbAL = sbAR = sbBL = sbBR = sbCL = sbCR = 0.0;
	//from SubTight
	
	fpdL = 1.0; while (fpdL < 16386) fpdL = rand()*UINT32_MAX;
	fpdR = 1.0; while (fpdR < 16386) fpdR = rand()*UINT32_MAX;
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

kCathedral2::~kCathedral2() {}
VstInt32 kCathedral2::getVendorVersion () {return 1000;}
void kCathedral2::setProgramName(char *name) {vst_strncpy (_programName, name, kVstMaxProgNameLen);}
void kCathedral2::getProgramName(char *name) {vst_strncpy (name, _programName, kVstMaxProgNameLen);}
//airwindows likes to ignore this stuff. Make your own programs, and make a different plugin rather than
//trying to do versioning and preventing people from using older versions. Maybe they like the old one!

static float pinParameter(float data)
{
	if (data < 0.0f) return 0.0f;
	if (data > 1.0f) return 1.0f;
	return data;
}

VstInt32 kCathedral2::getChunk (void** data, bool isPreset)
{
	float *chunkData = (float *)calloc(kNumParameters, sizeof(float));
	chunkData[0] = A;
	/* Note: The way this is set up, it will break if you manage to save settings on an Intel
	 machine and load them on a PPC Mac. However, it's fine if you stick to the machine you 
	 started with. */
	
	*data = chunkData;
	return kNumParameters * sizeof(float);
}

VstInt32 kCathedral2::setChunk (void* data, VstInt32 byteSize, bool isPreset)
{	
	float *chunkData = (float *)data;
	A = pinParameter(chunkData[0]);
	/* We're ignoring byteSize as we found it to be a filthy liar */
	
	/* calculate any other fields you need here - you could copy in 
	 code from setParameter() here. */
	return 0;
}

void kCathedral2::setParameter(VstInt32 index, float value) {
    switch (index) {
        case kParamA: A = value; break;
        default: throw; // unknown parameter, shouldn't happen!
    }
}

float kCathedral2::getParameter(VstInt32 index) {
    switch (index) {
        case kParamA: return A; break;
        default: break; // unknown parameter, shouldn't happen!
    } return 0.0; //we only need to update the relevant name, this is simple to manage
}

void kCathedral2::getParameterName(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "Wetness", kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
    } //this is our labels for displaying in the VST host
}

void kCathedral2::getParameterDisplay(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: float2string (A, text, kVstMaxParamStrLen); break;
        default: break; // unknown parameter, shouldn't happen!
	} //this displays the values and handles 'popups' where it's discrete choices
}

void kCathedral2::getParameterLabel(VstInt32 index, char *text) {
    switch (index) {
        case kParamA: vst_strncpy (text, "", kVstMaxParamStrLen); break;
		default: break; // unknown parameter, shouldn't happen!
    }
}

VstInt32 kCathedral2::canDo(char *text) 
{ return (_canDo.find(text) == _canDo.end()) ? -1: 1; } // 1 = yes, -1 = no, 0 = don't know

bool kCathedral2::getEffectName(char* name) {
    vst_strncpy(name, "kCathedral2", kVstMaxProductStrLen); return true;
}

VstPlugCategory kCathedral2::getPlugCategory() {return kPlugCategEffect;}

bool kCathedral2::getProductString(char* text) {
  	vst_strncpy (text, "airwindows kCathedral2", kVstMaxProductStrLen); return true;
}

bool kCathedral2::getVendorString(char* text) {
  	vst_strncpy (text, "airwindows", kVstMaxVendorStrLen); return true;
}