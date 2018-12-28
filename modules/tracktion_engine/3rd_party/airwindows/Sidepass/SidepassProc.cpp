/* ========================================
 *  Sidepass - Sidepass.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Sidepass_H
#include "Sidepass.h"
#endif

void Sidepass::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
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
	double iirAmount = pow(A,3)/overallscale;
	long double inputSampleL;
	long double inputSampleR;
	long double mid;
	double side;

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

		mid = inputSampleL + inputSampleR;
		side = inputSampleL - inputSampleR;

		if (flip)
		{
			iirSampleA = (iirSampleA * (1 - iirAmount)) + (side * iirAmount);
			side -= iirSampleA;
		}
		else
		{
			iirSampleB = (iirSampleB * (1 - iirAmount)) + (side * iirAmount);
			side -= iirSampleB;
		}
		//highpass section

		inputSampleL = (mid+side)/2.0;
		inputSampleR = (mid-side)/2.0;

		//noise shaping to 32-bit floating point
		if (flip) {
			fpTemp = inputSampleL;
			fpNShapeAL = (fpNShapeAL*fpOld)+((inputSampleL-fpTemp)*fpNew);
			inputSampleL += fpNShapeAL;
			fpTemp = inputSampleR;
			fpNShapeAR = (fpNShapeAR*fpOld)+((inputSampleR-fpTemp)*fpNew);
			inputSampleR += fpNShapeAR;
		}
		else {
			fpTemp = inputSampleL;
			fpNShapeBL = (fpNShapeBL*fpOld)+((inputSampleL-fpTemp)*fpNew);
			inputSampleL += fpNShapeBL;
			fpTemp = inputSampleR;
			fpNShapeBR = (fpNShapeBR*fpOld)+((inputSampleR-fpTemp)*fpNew);
			inputSampleR += fpNShapeBR;
		}
		flip = !flip;
		//end noise shaping on 32 bit output

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}

void Sidepass::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;
	double iirAmount = pow(A,3)/overallscale;
	long double inputSampleL;
	long double inputSampleR;
	long double mid;
	double side;

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

		mid = inputSampleL + inputSampleR;
		side = inputSampleL - inputSampleR;

		if (flip)
		{
			iirSampleA = (iirSampleA * (1 - iirAmount)) + (side * iirAmount);
			side -= iirSampleA;
		}
		else
		{
			iirSampleB = (iirSampleB * (1 - iirAmount)) + (side * iirAmount);
			side -= iirSampleB;
		}
		//highpass section

		inputSampleL = (mid+side)/2.0;
		inputSampleR = (mid-side)/2.0;

		//noise shaping to 64-bit floating point
		if (flip) {
			fpTemp = inputSampleL;
			fpNShapeAL = (fpNShapeAL*fpOld)+((inputSampleL-fpTemp)*fpNew);
			inputSampleL += fpNShapeAL;
			fpTemp = inputSampleR;
			fpNShapeAR = (fpNShapeAR*fpOld)+((inputSampleR-fpTemp)*fpNew);
			inputSampleR += fpNShapeAR;
		}
		else {
			fpTemp = inputSampleL;
			fpNShapeBL = (fpNShapeBL*fpOld)+((inputSampleL-fpTemp)*fpNew);
			inputSampleL += fpNShapeBL;
			fpTemp = inputSampleR;
			fpNShapeBR = (fpNShapeBR*fpOld)+((inputSampleR-fpTemp)*fpNew);
			inputSampleR += fpNShapeBR;
		}
		flip = !flip;
		//end noise shaping on 64 bit output

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}
