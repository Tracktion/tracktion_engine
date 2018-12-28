/* ========================================
 *  Tremolo - Tremolo.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Tremolo_H
#include "Tremolo.h"
#endif

void Tremolo::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
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

	speedChase = pow(A,4);
	depthChase = B;
	double speedSpeed = 300 / (fabs( lastSpeed - speedChase)+1.0);
	double depthSpeed = 300 / (fabs( lastDepth - depthChase)+1.0);
	lastSpeed = speedChase;
	lastDepth = depthChase;

	double speed;
	double depth;
	double skew;
	double density;

	double tupi = 3.141592653589793238;
	double control;
	double tempcontrol;
	double thickness;
	double out;
	double bridgerectifier;
	double offset;

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

		speedAmount = (((speedAmount*speedSpeed)+speedChase)/(speedSpeed + 1.0));
		depthAmount = (((depthAmount*depthSpeed)+depthChase)/(depthSpeed + 1.0));
		speed = 0.0001+(speedAmount/1000.0);
		speed /= overallscale;
		depth = 1.0 - pow(1.0-depthAmount,5);
		skew = 1.0+pow(depthAmount,9);
		density = ((1.0-depthAmount)*2.0) - 1.0;

		offset = sin(sweep);
		sweep += speed;
		if (sweep > tupi){sweep -= tupi;}
		control = fabs(offset);
		if (density > 0)
		{
			tempcontrol = sin(control);
			control = (control * (1.0-density))+(tempcontrol * density);
		}
		else
		{
			tempcontrol = 1-cos(control);
			control = (control * (1.0+density))+(tempcontrol * -density);
		}
		//produce either boosted or starved version of control signal
		//will go from 0 to 1

		thickness = ((control * 2.0) - 1.0)*skew;
		out = fabs(thickness);

		//do L
		bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		if (thickness > 0) bridgerectifier = sin(bridgerectifier);
		else bridgerectifier = 1-cos(bridgerectifier);
		//produce either boosted or starved version
		if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-out))+(bridgerectifier*out);
		else inputSampleL = (inputSampleL*(1-out))-(bridgerectifier*out);
		//blend according to density control
		inputSampleL *= (1.0 - control);
		inputSampleL *= 2.0;
		//apply tremolo, apply gain boost to compensate for volume loss
		inputSampleL = (drySampleL * (1-depth)) + (inputSampleL*depth);
		//end L

		//do R
		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		if (thickness > 0) bridgerectifier = sin(bridgerectifier);
		else bridgerectifier = 1-cos(bridgerectifier);
		//produce either boosted or starved version
		if (inputSampleR > 0) inputSampleR = (inputSampleR*(1-out))+(bridgerectifier*out);
		else inputSampleR = (inputSampleR*(1-out))-(bridgerectifier*out);
		//blend according to density control
		inputSampleR *= (1.0 - control);
		inputSampleR *= 2.0;
		//apply tremolo, apply gain boost to compensate for volume loss
		inputSampleR = (drySampleR * (1-depth)) + (inputSampleR*depth);
		//end R

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

void Tremolo::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
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

	speedChase = pow(A,4);
	depthChase = B;
	double speedSpeed = 300 / (fabs( lastSpeed - speedChase)+1.0);
	double depthSpeed = 300 / (fabs( lastDepth - depthChase)+1.0);
	lastSpeed = speedChase;
	lastDepth = depthChase;

	double speed;
	double depth;
	double skew;
	double density;

	double tupi = 3.141592653589793238;
	double control;
	double tempcontrol;
	double thickness;
	double out;
	double bridgerectifier;
	double offset;

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


		speedAmount = (((speedAmount*speedSpeed)+speedChase)/(speedSpeed + 1.0));
		depthAmount = (((depthAmount*depthSpeed)+depthChase)/(depthSpeed + 1.0));
		speed = 0.0001+(speedAmount/1000.0);
		speed /= overallscale;
		depth = 1.0 - pow(1.0-depthAmount,5);
		skew = 1.0+pow(depthAmount,9);
		density = ((1.0-depthAmount)*2.0) - 1.0;

		offset = sin(sweep);
		sweep += speed;
		if (sweep > tupi){sweep -= tupi;}
		control = fabs(offset);
		if (density > 0)
		{
			tempcontrol = sin(control);
			control = (control * (1.0-density))+(tempcontrol * density);
		}
		else
		{
			tempcontrol = 1-cos(control);
			control = (control * (1.0+density))+(tempcontrol * -density);
		}
		//produce either boosted or starved version of control signal
		//will go from 0 to 1

		thickness = ((control * 2.0) - 1.0)*skew;
		out = fabs(thickness);

		//do L
		bridgerectifier = fabs(inputSampleL);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		if (thickness > 0) bridgerectifier = sin(bridgerectifier);
		else bridgerectifier = 1-cos(bridgerectifier);
		//produce either boosted or starved version
		if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-out))+(bridgerectifier*out);
		else inputSampleL = (inputSampleL*(1-out))-(bridgerectifier*out);
		//blend according to density control
		inputSampleL *= (1.0 - control);
		inputSampleL *= 2.0;
		//apply tremolo, apply gain boost to compensate for volume loss
		inputSampleL = (drySampleL * (1-depth)) + (inputSampleL*depth);
		//end L

		//do R
		bridgerectifier = fabs(inputSampleR);
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
		//max value for sine function
		if (thickness > 0) bridgerectifier = sin(bridgerectifier);
		else bridgerectifier = 1-cos(bridgerectifier);
		//produce either boosted or starved version
		if (inputSampleR > 0) inputSampleR = (inputSampleR*(1-out))+(bridgerectifier*out);
		else inputSampleR = (inputSampleR*(1-out))-(bridgerectifier*out);
		//blend according to density control
		inputSampleR *= (1.0 - control);
		inputSampleR *= 2.0;
		//apply tremolo, apply gain boost to compensate for volume loss
		inputSampleR = (drySampleR * (1-depth)) + (inputSampleR*depth);
		//end R

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
