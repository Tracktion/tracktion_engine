/* ========================================
 *  TransDesk - TransDesk.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __TransDesk_H
#include "TransDesk.h"
#endif

void TransDesk::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	float fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	double intensity = 0.02198359;
	double depthA = 3.0;
	int offsetA = (int)(depthA * overallscale);
	if (offsetA < 1) offsetA = 1;
	if (offsetA > 8) offsetA = 8;

	double clamp;
	double thickness;
	double out;

	double gain = 0.130;
	double slewgain = 0.197;
	double prevslew = 0.255;
	double balanceB = 0.0001;
	slewgain *= overallscale;
	prevslew *= overallscale;
	balanceB /= overallscale;
	double balanceA = 1.0 - balanceB;
	double slew;
	double bridgerectifier;
	double combSample;

	long double inputSampleL;
	long double inputSampleR;
	long double drySampleL;
	long double drySampleR;

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
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;


		if (gcount < 0 || gcount > 9) {gcount = 9;}

		//begin L
		dL[gcount+9] = dL[gcount] = fabs(inputSampleL)*intensity;
		controlL += (dL[gcount] / offsetA);
		controlL -= (dL[gcount+offsetA] / offsetA);
		controlL -= 0.000001;
		clamp = 1;
		if (controlL < 0) {controlL = 0;}
		if (controlL > 1) {clamp -= (controlL - 1); controlL = 1;}
		if (clamp < 0.5) {clamp = 0.5;}
		//control = 0 to 1
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
		slew = inputSampleL - lastSampleL;
		lastSampleL = inputSampleL;
		//Set up direct reference for slew
		bridgerectifier = fabs(slew*slewgain);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (slew > 0) slew = bridgerectifier/slewgain;
		else slew = -(bridgerectifier/slewgain);
		inputSampleL = (lastOutSampleL*balanceA) + (lastSampleL*balanceB) + slew;
		//go from last slewed, but include some raw values
		lastOutSampleL = inputSampleL;
		//Set up slewed reference
		combSample = fabs(drySampleL*lastSampleL);
		if (combSample > 1.0) combSample = 1.0;
		//bailout for very high input gains
		inputSampleL -= (lastSlewL * combSample * prevslew);
		lastSlewL = slew;
		//slew interaction with previous slew
		inputSampleL *= gain;
		bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0) inputSampleL = bridgerectifier;
		else inputSampleL = -bridgerectifier;
		//drive section
		inputSampleL /= gain;
		//end of Desk section
		//end L

		//begin R
		dR[gcount+9] = dR[gcount] = fabs(inputSampleR)*intensity;
		controlR += (dR[gcount] / offsetA);
		controlR -= (dR[gcount+offsetA] / offsetA);
		controlR -= 0.000001;
		clamp = 1;
		if (controlR < 0) {controlR = 0;}
		if (controlR > 1) {clamp -= (controlR - 1); controlR = 1;}
		if (clamp < 0.5) {clamp = 0.5;}
		//control = 0 to 1
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
		slew = inputSampleR - lastSampleR;
		lastSampleR = inputSampleR;
		//Set up direct reference for slew
		bridgerectifier = fabs(slew*slewgain);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (slew > 0) slew = bridgerectifier/slewgain;
		else slew = -(bridgerectifier/slewgain);
		inputSampleR = (lastOutSampleR*balanceA) + (lastSampleR*balanceB) + slew;
		//go from last slewed, but include some raw values
		lastOutSampleR = inputSampleR;
		//Set up slewed reference
		combSample = fabs(drySampleR*lastSampleR);
		if (combSample > 1.0) combSample = 1.0;
		//bailout for very high input gains
		inputSampleR -= (lastSlewR * combSample * prevslew);
		lastSlewR = slew;
		//slew interaction with previous slew
		inputSampleR *= gain;
		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0) inputSampleR = bridgerectifier;
		else inputSampleR = -bridgerectifier;
		//drive section
		inputSampleR /= gain;
		//end of Desk section
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

void TransDesk::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double fpTemp; //this is different from singlereplacing
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	double intensity = 0.02198359;
	double depthA = 3.0;
	int offsetA = (int)(depthA * overallscale);
	if (offsetA < 1) offsetA = 1;
	if (offsetA > 8) offsetA = 8;

	double clamp;
	double thickness;
	double out;

	double gain = 0.130;
	double slewgain = 0.197;
	double prevslew = 0.255;
	double balanceB = 0.0001;
	slewgain *= overallscale;
	prevslew *= overallscale;
	balanceB /= overallscale;
	double balanceA = 1.0 - balanceB;
	double slew;
	double bridgerectifier;
	double combSample;

	long double inputSampleL;
	long double inputSampleR;
	long double drySampleL;
	long double drySampleR;

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
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;


		if (gcount < 0 || gcount > 9) {gcount = 9;}

		//begin L
		dL[gcount+9] = dL[gcount] = fabs(inputSampleL)*intensity;
		controlL += (dL[gcount] / offsetA);
		controlL -= (dL[gcount+offsetA] / offsetA);
		controlL -= 0.000001;
		clamp = 1;
		if (controlL < 0) {controlL = 0;}
		if (controlL > 1) {clamp -= (controlL - 1); controlL = 1;}
		if (clamp < 0.5) {clamp = 0.5;}
		//control = 0 to 1
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
		slew = inputSampleL - lastSampleL;
		lastSampleL = inputSampleL;
		//Set up direct reference for slew
		bridgerectifier = fabs(slew*slewgain);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (slew > 0) slew = bridgerectifier/slewgain;
		else slew = -(bridgerectifier/slewgain);
		inputSampleL = (lastOutSampleL*balanceA) + (lastSampleL*balanceB) + slew;
		//go from last slewed, but include some raw values
		lastOutSampleL = inputSampleL;
		//Set up slewed reference
		combSample = fabs(drySampleL*lastSampleL);
		if (combSample > 1.0) combSample = 1.0;
		//bailout for very high input gains
		inputSampleL -= (lastSlewL * combSample * prevslew);
		lastSlewL = slew;
		//slew interaction with previous slew
		inputSampleL *= gain;
		bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0) inputSampleL = bridgerectifier;
		else inputSampleL = -bridgerectifier;
		//drive section
		inputSampleL /= gain;
		//end of Desk section
		//end L

		//begin R
		dR[gcount+9] = dR[gcount] = fabs(inputSampleR)*intensity;
		controlR += (dR[gcount] / offsetA);
		controlR -= (dR[gcount+offsetA] / offsetA);
		controlR -= 0.000001;
		clamp = 1;
		if (controlR < 0) {controlR = 0;}
		if (controlR > 1) {clamp -= (controlR - 1); controlR = 1;}
		if (clamp < 0.5) {clamp = 0.5;}
		//control = 0 to 1
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
		slew = inputSampleR - lastSampleR;
		lastSampleR = inputSampleR;
		//Set up direct reference for slew
		bridgerectifier = fabs(slew*slewgain);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (slew > 0) slew = bridgerectifier/slewgain;
		else slew = -(bridgerectifier/slewgain);
		inputSampleR = (lastOutSampleR*balanceA) + (lastSampleR*balanceB) + slew;
		//go from last slewed, but include some raw values
		lastOutSampleR = inputSampleR;
		//Set up slewed reference
		combSample = fabs(drySampleR*lastSampleR);
		if (combSample > 1.0) combSample = 1.0;
		//bailout for very high input gains
		inputSampleR -= (lastSlewR * combSample * prevslew);
		lastSlewR = slew;
		//slew interaction with previous slew
		inputSampleR *= gain;
		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0) inputSampleR = bridgerectifier;
		else inputSampleR = -bridgerectifier;
		//drive section
		inputSampleR /= gain;
		//end of Desk section
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
