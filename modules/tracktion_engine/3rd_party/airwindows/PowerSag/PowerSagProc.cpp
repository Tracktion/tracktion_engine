/* ========================================
 *  PowerSag - PowerSag.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PowerSag_H
#include "PowerSag.h"
#endif

void PowerSag::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double intensity = pow(A,5)*80.0;
	double depthA = pow(B,2);
	int offsetA = (int)(depthA * 3900) + 1;
	double clamp;
	double thickness;
	double out;
	double bridgerectifier;

	float fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	long double inputSampleL;
	long double inputSampleR;

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

		if (gcount < 0 || gcount > 4000) {gcount = 4000;}

		//doing L
		dL[gcount+4000] = dL[gcount] = fabs(inputSampleL)*intensity;
		controlL += (dL[gcount] / offsetA);
		controlL -= (dL[gcount+offsetA] / offsetA);
		controlL -= 0.000001;
		clamp = 1;
		if (controlL < 0) {controlL = 0;}
		if (controlL > 1) {clamp -= (controlL - 1); controlL = 1;}
		if (clamp < 0.5) {clamp = 0.5;}
		thickness = ((1.0 - controlL) * 2.0) - 1.0;
		out = fabs(thickness);
		bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		if (thickness > 0) bridgerectifier = sin(bridgerectifier);
		else bridgerectifier = 1-cos(bridgerectifier);
		//produce either boosted or starved version
		if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-out))+(bridgerectifier*out);
		else inputSampleL = (inputSampleL*(1-out))-(bridgerectifier*out);
		//blend according to density control
		inputSampleL *= clamp;
		//end L

		//doing R
		dR[gcount+4000] = dR[gcount] = fabs(inputSampleR)*intensity;
		controlR += (dR[gcount] / offsetA);
		controlR -= (dR[gcount+offsetA] / offsetA);
		controlR -= 0.000001;
		clamp = 1;
		if (controlR < 0) {controlR = 0;}
		if (controlR > 1) {clamp -= (controlR - 1); controlR = 1;}
		if (clamp < 0.5) {clamp = 0.5;}
		thickness = ((1.0 - controlR) * 2.0) - 1.0;
		out = fabs(thickness);
		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		if (thickness > 0) bridgerectifier = sin(bridgerectifier);
		else bridgerectifier = 1-cos(bridgerectifier);
		//produce either boosted or starved version
		if (inputSampleR > 0) inputSampleR = (inputSampleR*(1-out))+(bridgerectifier*out);
		else inputSampleR = (inputSampleR*(1-out))-(bridgerectifier*out);
		//blend according to density control
		inputSampleR *= clamp;
		//end R

		gcount--;


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

void PowerSag::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double intensity = pow(A,5)*80.0;
	double depthA = pow(B,2);
	int offsetA = (int)(depthA * 3900) + 1;
	double clamp;
	double thickness;
	double out;
	double bridgerectifier;

	double fpTemp; //this is different from singlereplacing
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	long double inputSampleL;
	long double inputSampleR;

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

		if (gcount < 0 || gcount > 4000) {gcount = 4000;}

		//doing L
		dL[gcount+4000] = dL[gcount] = fabs(inputSampleL)*intensity;
		controlL += (dL[gcount] / offsetA);
		controlL -= (dL[gcount+offsetA] / offsetA);
		controlL -= 0.000001;
		clamp = 1;
		if (controlL < 0) {controlL = 0;}
		if (controlL > 1) {clamp -= (controlL - 1); controlL = 1;}
		if (clamp < 0.5) {clamp = 0.5;}
		thickness = ((1.0 - controlL) * 2.0) - 1.0;
		out = fabs(thickness);
		bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		if (thickness > 0) bridgerectifier = sin(bridgerectifier);
		else bridgerectifier = 1-cos(bridgerectifier);
		//produce either boosted or starved version
		if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-out))+(bridgerectifier*out);
		else inputSampleL = (inputSampleL*(1-out))-(bridgerectifier*out);
		//blend according to density control
		inputSampleL *= clamp;
		//end L

		//doing R
		dR[gcount+4000] = dR[gcount] = fabs(inputSampleR)*intensity;
		controlR += (dR[gcount] / offsetA);
		controlR -= (dR[gcount+offsetA] / offsetA);
		controlR -= 0.000001;
		clamp = 1;
		if (controlR < 0) {controlR = 0;}
		if (controlR > 1) {clamp -= (controlR - 1); controlR = 1;}
		if (clamp < 0.5) {clamp = 0.5;}
		thickness = ((1.0 - controlR) * 2.0) - 1.0;
		out = fabs(thickness);
		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		if (thickness > 0) bridgerectifier = sin(bridgerectifier);
		else bridgerectifier = 1-cos(bridgerectifier);
		//produce either boosted or starved version
		if (inputSampleR > 0) inputSampleR = (inputSampleR*(1-out))+(bridgerectifier*out);
		else inputSampleR = (inputSampleR*(1-out))-(bridgerectifier*out);
		//blend according to density control
		inputSampleR *= clamp;
		//end R

		gcount--;


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
