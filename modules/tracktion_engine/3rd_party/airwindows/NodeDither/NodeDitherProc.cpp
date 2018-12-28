/* ========================================
 *  NodeDither - NodeDither.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __NodeDither_H
#include "NodeDither.h"
#endif

void NodeDither::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	int offsetA = (int)((A*100) * overallscale);
	if (offsetA < 1) offsetA = 1;
	if (offsetA > 2440) offsetA = 2440;

	int phase = floor(B*1.999);
	//0 default is out of phase, 1 is in phase

	long double inputSampleL;
	long double inputSampleR;
	double currentDitherL;
	double currentDitherR;

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

		inputSampleL *= 8388608.0;
		inputSampleR *= 8388608.0;
		//0-1 is now one bit, now we dither

		if (gcount < 0 || gcount > 2450) {gcount = 2450;}

		currentDitherL = (rand()/(double)RAND_MAX);
		inputSampleL += currentDitherL;

		currentDitherR = (rand()/(double)RAND_MAX);
		inputSampleR += currentDitherR;

		if (phase == 1) {
			inputSampleL -= 1.0;
			inputSampleL += dL[gcount+offsetA];
			inputSampleR -= 1.0;
			inputSampleR += dR[gcount+offsetA];
		} else {
			inputSampleL -= dL[gcount+offsetA];
			inputSampleR -= dR[gcount+offsetA];
		}
		//in phase means adding, otherwise we subtract


		inputSampleL = floor(inputSampleL);
		inputSampleR = floor(inputSampleR);
		dL[gcount+2450] = dL[gcount] = currentDitherL;
		dR[gcount+2450] = dR[gcount] = currentDitherR;

		gcount--;

		inputSampleL /= 8388608.0;
		inputSampleR /= 8388608.0;

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}

void NodeDither::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	int offsetA = (int)((A*100) * overallscale);
	if (offsetA < 1) offsetA = 1;
	if (offsetA > 2440) offsetA = 2440;

	int phase = floor(B*1.999);
	//0 default is out of phase, 1 is in phase

	long double inputSampleL;
	long double inputSampleR;
	double currentDitherL;
	double currentDitherR;

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

		inputSampleL *= 8388608.0;
		inputSampleR *= 8388608.0;
		//0-1 is now one bit, now we dither

		if (gcount < 0 || gcount > 2450) {gcount = 2450;}

		currentDitherL = (rand()/(double)RAND_MAX);
		inputSampleL += currentDitherL;

		currentDitherR = (rand()/(double)RAND_MAX);
		inputSampleR += currentDitherR;

		if (phase == 1) {
			inputSampleL -= 1.0;
			inputSampleL += dL[gcount+offsetA];
			inputSampleR -= 1.0;
			inputSampleR += dR[gcount+offsetA];
		} else {
			inputSampleL -= dL[gcount+offsetA];
			inputSampleR -= dR[gcount+offsetA];
		}
		//in phase means adding, otherwise we subtract


		inputSampleL = floor(inputSampleL);
		inputSampleR = floor(inputSampleR);
		dL[gcount+2450] = dL[gcount] = currentDitherL;
		dR[gcount+2450] = dR[gcount] = currentDitherR;

		gcount--;

		inputSampleL /= 8388608.0;
		inputSampleR /= 8388608.0;

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}
