/* ========================================
 *  PDBuss - PDBuss.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PDBuss_H
#include "PDBuss.h"
#endif

void PDBuss::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];
	float fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	double inputgain = A;
	double intensity = B;
	double applyL;
	double applyR;

	double drySampleL;
	double drySampleR;
	long double inputSampleL;
	long double inputSampleR;

	if (settingchase != inputgain) {
		chasespeed *= 2.0;
		settingchase = inputgain;
	}
	if (chasespeed > 2500.0) chasespeed = 2500.0;
	if (gainchase < 0.0) gainchase = inputgain;

    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			static int noisesource = 0;
			//this declares a variable before anything else is compiled. It won't keep assigning
			//it to 0 for every sample, it's as if the declaration doesn't exist in this context,
			//but it lets me add this denormalization fix in a single place rather than updating
			//it in three different locations. The variable isn't thread-safe but this is only
			//a random seed and we can share it with whatever.
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleL = applyresidue;
		}
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			static int noisesource = 0;
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleR = applyresidue;
			//this denormalization routine produces a white noise at -300 dB which the noise
			//shaping will interact with to produce a bipolar output, but the noise is actually
			//all positive. That should stop any variables from going denormal, and the routine
			//only kicks in if digital black is input. As a final touch, if you save to 24-bit
			//the silence will return to being digital black again.
		}

		chasespeed *= 0.9999;
		chasespeed -= 0.01;
		if (chasespeed < 350.0) chasespeed = 350.0;
		//we have our chase speed compensated for recent fader activity

		gainchase = (((gainchase*chasespeed)+inputgain)/(chasespeed+1.0));
		//gainchase is chasing the target, as a simple multiply gain factor

		if (1.0 != gainchase) {
			inputSampleL *= gainchase;
			inputSampleR *= gainchase;
		}
		//done with trim control

		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		//without this, you can get a NaN condition where it spits out DC offset at full blast!
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		//without this, you can get a NaN condition where it spits out DC offset at full blast!

		inputSampleL = asin(inputSampleL);
		inputSampleR = asin(inputSampleR);
		//amplitude aspect

		drySampleL = inputSampleL;
		drySampleR = inputSampleR;

		inputSampleL = sin(inputSampleL);
		inputSampleR = sin(inputSampleR);
		//basic distortion factor

		applyL = (fabs(previousSampleL + inputSampleL) / 2.0) * intensity;
		applyR = (fabs(previousSampleR + inputSampleR) / 2.0) * intensity;
		//saturate less if previous sample was undistorted and low level, or if it was
		//inverse polarity. Lets through highs and brightness more.

		inputSampleL = (drySampleL * (1.0 - applyL)) + (inputSampleL * applyL);
		inputSampleR = (drySampleR * (1.0 - applyR)) + (inputSampleR * applyR);
		//dry-wet control for intensity also has FM modulation to clean up highs

		previousSampleL = sin(drySampleL);
		previousSampleR = sin(drySampleR);
		//apply the sine while storing previous sample

		//noise shaping to 32-bit floating point
		if (fpFlip) {
			fpTemp = inputSampleL;
			fpNShapeLA = (fpNShapeLA*fpOld)+((inputSampleL-fpTemp)*fpNew);
			inputSampleL += fpNShapeLA;
			fpTemp = inputSampleR;
			fpNShapeRA = (fpNShapeRA*fpOld)+((inputSampleR-fpTemp)*fpNew);
			inputSampleR += fpNShapeRA;
		}
		else {
			fpTemp = inputSampleL;
			fpNShapeLB = (fpNShapeLB*fpOld)+((inputSampleL-fpTemp)*fpNew);
			inputSampleL += fpNShapeLB;
			fpTemp = inputSampleR;
			fpNShapeRB = (fpNShapeRB*fpOld)+((inputSampleR-fpTemp)*fpNew);
			inputSampleR += fpNShapeRB;
		}
		fpFlip = !fpFlip;
		//end noise shaping on 32 bit output

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}

void PDBuss::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	double fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	double inputgain = A;
	double intensity = B;
	double applyL;
	double applyR;

	double drySampleL;
	double drySampleR;
	long double inputSampleL;
	long double inputSampleR;

	if (settingchase != inputgain) {
		chasespeed *= 2.0;
		settingchase = inputgain;
	}
	if (chasespeed > 2500.0) chasespeed = 2500.0;
	if (gainchase < 0.0) gainchase = inputgain;

    while (--sampleFrames >= 0)
    {
		inputSampleL = *in1;
		inputSampleR = *in2;
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			static int noisesource = 0;
			//this declares a variable before anything else is compiled. It won't keep assigning
			//it to 0 for every sample, it's as if the declaration doesn't exist in this context,
			//but it lets me add this denormalization fix in a single place rather than updating
			//it in three different locations. The variable isn't thread-safe but this is only
			//a random seed and we can share it with whatever.
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleL = applyresidue;
		}
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			static int noisesource = 0;
			noisesource = noisesource % 1700021; noisesource++;
			int residue = noisesource * noisesource;
			residue = residue % 170003; residue *= residue;
			residue = residue % 17011; residue *= residue;
			residue = residue % 1709; residue *= residue;
			residue = residue % 173; residue *= residue;
			residue = residue % 17;
			double applyresidue = residue;
			applyresidue *= 0.00000001;
			applyresidue *= 0.00000001;
			inputSampleR = applyresidue;
			//this denormalization routine produces a white noise at -300 dB which the noise
			//shaping will interact with to produce a bipolar output, but the noise is actually
			//all positive. That should stop any variables from going denormal, and the routine
			//only kicks in if digital black is input. As a final touch, if you save to 24-bit
			//the silence will return to being digital black again.
		}

		chasespeed *= 0.9999;
		chasespeed -= 0.01;
		if (chasespeed < 350.0) chasespeed = 350.0;
		//we have our chase speed compensated for recent fader activity

		gainchase = (((gainchase*chasespeed)+inputgain)/(chasespeed+1.0));
		//gainchase is chasing the target, as a simple multiply gain factor

		if (1.0 != gainchase) {
			inputSampleL *= gainchase;
			inputSampleR *= gainchase;
		}
		//done with trim control

		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		//without this, you can get a NaN condition where it spits out DC offset at full blast!
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;
		//without this, you can get a NaN condition where it spits out DC offset at full blast!

		inputSampleL = asin(inputSampleL);
		inputSampleR = asin(inputSampleR);
		//amplitude aspect

		drySampleL = inputSampleL;
		drySampleR = inputSampleR;

		inputSampleL = sin(inputSampleL);
		inputSampleR = sin(inputSampleR);
		//basic distortion factor

		applyL = (fabs(previousSampleL + inputSampleL) / 2.0) * intensity;
		applyR = (fabs(previousSampleR + inputSampleR) / 2.0) * intensity;
		//saturate less if previous sample was undistorted and low level, or if it was
		//inverse polarity. Lets through highs and brightness more.

		inputSampleL = (drySampleL * (1.0 - applyL)) + (inputSampleL * applyL);
		inputSampleR = (drySampleR * (1.0 - applyR)) + (inputSampleR * applyR);
		//dry-wet control for intensity also has FM modulation to clean up highs

		previousSampleL = sin(drySampleL);
		previousSampleR = sin(drySampleR);
		//apply the sine while storing previous sample

		//noise shaping to 64-bit floating point
		if (fpFlip) {
			fpTemp = inputSampleL;
			fpNShapeLA = (fpNShapeLA*fpOld)+((inputSampleL-fpTemp)*fpNew);
			inputSampleL += fpNShapeLA;
			fpTemp = inputSampleR;
			fpNShapeRA = (fpNShapeRA*fpOld)+((inputSampleR-fpTemp)*fpNew);
			inputSampleR += fpNShapeRA;
		}
		else {
			fpTemp = inputSampleL;
			fpNShapeLB = (fpNShapeLB*fpOld)+((inputSampleL-fpTemp)*fpNew);
			inputSampleL += fpNShapeLB;
			fpTemp = inputSampleR;
			fpNShapeRB = (fpNShapeRB*fpOld)+((inputSampleR-fpTemp)*fpNew);
			inputSampleR += fpNShapeRB;
		}
		fpFlip = !fpFlip;
		//end noise shaping on 64 bit output

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}
