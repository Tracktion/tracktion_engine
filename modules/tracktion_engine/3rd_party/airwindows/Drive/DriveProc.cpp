/* ========================================
 *  Drive - Drive.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Drive_H
#include "Drive.h"
#endif

void Drive::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double driveone = pow(A*2.0,2);
	double iirAmount = pow(B,3)/overallscale;
	double output = C;
	double wet = D;
	double dry = 1.0-wet;
	double glitch = 0.60;
	double out;

	float fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

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

		if (fpFlip)
		{
			iirSampleAL = (iirSampleAL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
			inputSampleL -= iirSampleAL;
			iirSampleAR = (iirSampleAR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleR -= iirSampleAR;
		}
		else
		{
			iirSampleBL = (iirSampleBL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
			inputSampleL -= iirSampleBL;
			iirSampleBR = (iirSampleBR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleR -= iirSampleBR;
		}
		//highpass section


		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;

		out = driveone;
		while (out > glitch)
		{
			out -= glitch;
			inputSampleL -= (inputSampleL * (fabs(inputSampleL) * glitch) * (fabs(inputSampleL) * glitch) );
			inputSampleL *= (1.0+glitch);
			inputSampleR -= (inputSampleR * (fabs(inputSampleR) * glitch) * (fabs(inputSampleR) * glitch) );
			inputSampleR *= (1.0+glitch);
		}
		//that's taken care of the really high gain stuff

		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * out) * (fabs(inputSampleL) * out) );
		inputSampleL *= (1.0+out);
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * out) * (fabs(inputSampleR) * out) );
		inputSampleR *= (1.0+out);

		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}
		if (wet < 1.0) {
			inputSampleL = (drySampleL * dry)+(inputSampleL * wet);
			inputSampleR = (drySampleR * dry)+(inputSampleR * wet);
		}
		//nice little output stage template: if we have another scale of floating point
		//number, we really don't want to meaninglessly multiply that by 1.0.

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

void Drive::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double driveone = pow(A*2.0,2);
	double iirAmount = pow(B,3)/overallscale;
	double output = C;
	double wet = D;
	double dry = 1.0-wet;
	double glitch = 0.60;
	double out;

	double fpTemp; //this is different from singlereplacing
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

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

		if (fpFlip)
		{
			iirSampleAL = (iirSampleAL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
			inputSampleL -= iirSampleAL;
			iirSampleAR = (iirSampleAR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleR -= iirSampleAR;
		}
		else
		{
			iirSampleBL = (iirSampleBL * (1.0 - iirAmount)) + (inputSampleL * iirAmount);
			inputSampleL -= iirSampleBL;
			iirSampleBR = (iirSampleBR * (1.0 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleR -= iirSampleBR;
		}
		//highpass section


		if (inputSampleL > 1.0) inputSampleL = 1.0;
		if (inputSampleL < -1.0) inputSampleL = -1.0;
		if (inputSampleR > 1.0) inputSampleR = 1.0;
		if (inputSampleR < -1.0) inputSampleR = -1.0;

		out = driveone;
		while (out > glitch)
		{
			out -= glitch;
			inputSampleL -= (inputSampleL * (fabs(inputSampleL) * glitch) * (fabs(inputSampleL) * glitch) );
			inputSampleL *= (1.0+glitch);
			inputSampleR -= (inputSampleR * (fabs(inputSampleR) * glitch) * (fabs(inputSampleR) * glitch) );
			inputSampleR *= (1.0+glitch);
		}
		//that's taken care of the really high gain stuff

		inputSampleL -= (inputSampleL * (fabs(inputSampleL) * out) * (fabs(inputSampleL) * out) );
		inputSampleL *= (1.0+out);
		inputSampleR -= (inputSampleR * (fabs(inputSampleR) * out) * (fabs(inputSampleR) * out) );
		inputSampleR *= (1.0+out);

		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}
		if (wet < 1.0) {
			inputSampleL = (drySampleL * dry)+(inputSampleL * wet);
			inputSampleR = (drySampleR * dry)+(inputSampleR * wet);
		}
		//nice little output stage template: if we have another scale of floating point
		//number, we really don't want to meaninglessly multiply that by 1.0.

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
