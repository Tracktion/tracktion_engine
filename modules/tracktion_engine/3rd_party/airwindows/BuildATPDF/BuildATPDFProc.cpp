/* ========================================
 *  BuildATPDF - BuildATPDF.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __BuildATPDF_H
#include "BuildATPDF.h"
#endif

void BuildATPDF::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	f[0] = (A*2)-1;
	f[1] = (B*2)-1;
	f[2] = (C*2)-1;
	f[3] = (D*2)-1;
	f[4] = (E*2)-1;
	f[5] = (F*2)-1;
	f[6] = (G*2)-1;
	f[7] = (H*2)-1;
	f[8] = (I*2)-1;
	f[9] = (J*2)-1;
	double currentDither;
	double inputSampleL;
	double inputSampleR;

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

		bL[9] = bL[8]; bL[8] = bL[7]; bL[7] = bL[6]; bL[6] = bL[5];
		bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1];
		bL[1] = bL[0]; bL[0] = (rand()/(double)RAND_MAX);

		currentDither  = (bL[0] * f[0]);
		currentDither += (bL[1] * f[1]);
		currentDither += (bL[2] * f[2]);
		currentDither += (bL[3] * f[3]);
		currentDither += (bL[4] * f[4]);
		currentDither += (bL[5] * f[5]);
		currentDither += (bL[6] * f[6]);
		currentDither += (bL[7] * f[7]);
		currentDither += (bL[8] * f[8]);
		currentDither += (bL[9] * f[9]);
		inputSampleL += currentDither;


		bR[9] = bR[8]; bR[8] = bR[7]; bR[7] = bR[6]; bR[6] = bR[5];
		bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1];
		bR[1] = bR[0]; bR[0] = (rand()/(double)RAND_MAX);

		currentDither  = (bR[0] * f[0]);
		currentDither += (bR[1] * f[1]);
		currentDither += (bR[2] * f[2]);
		currentDither += (bR[3] * f[3]);
		currentDither += (bR[4] * f[4]);
		currentDither += (bR[5] * f[5]);
		currentDither += (bR[6] * f[6]);
		currentDither += (bR[7] * f[7]);
		currentDither += (bR[8] * f[8]);
		currentDither += (bR[9] * f[9]);
		inputSampleR += currentDither;

		inputSampleL = floor(inputSampleL);
		inputSampleR = floor(inputSampleR);

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

void BuildATPDF::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	f[0] = (A*2)-1;
	f[1] = (B*2)-1;
	f[2] = (C*2)-1;
	f[3] = (D*2)-1;
	f[4] = (E*2)-1;
	f[5] = (F*2)-1;
	f[6] = (G*2)-1;
	f[7] = (H*2)-1;
	f[8] = (I*2)-1;
	f[9] = (J*2)-1;
	double currentDither;
	double inputSampleL;
	double inputSampleR;

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

		bL[9] = bL[8]; bL[8] = bL[7]; bL[7] = bL[6]; bL[6] = bL[5];
		bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1];
		bL[1] = bL[0]; bL[0] = (rand()/(double)RAND_MAX);

		currentDither  = (bL[0] * f[0]);
		currentDither += (bL[1] * f[1]);
		currentDither += (bL[2] * f[2]);
		currentDither += (bL[3] * f[3]);
		currentDither += (bL[4] * f[4]);
		currentDither += (bL[5] * f[5]);
		currentDither += (bL[6] * f[6]);
		currentDither += (bL[7] * f[7]);
		currentDither += (bL[8] * f[8]);
		currentDither += (bL[9] * f[9]);
		inputSampleL += currentDither;


		bR[9] = bR[8]; bR[8] = bR[7]; bR[7] = bR[6]; bR[6] = bR[5];
		bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1];
		bR[1] = bR[0]; bR[0] = (rand()/(double)RAND_MAX);

		currentDither  = (bR[0] * f[0]);
		currentDither += (bR[1] * f[1]);
		currentDither += (bR[2] * f[2]);
		currentDither += (bR[3] * f[3]);
		currentDither += (bR[4] * f[4]);
		currentDither += (bR[5] * f[5]);
		currentDither += (bR[6] * f[6]);
		currentDither += (bR[7] * f[7]);
		currentDither += (bR[8] * f[8]);
		currentDither += (bR[9] * f[9]);
		inputSampleR += currentDither;

		inputSampleL = floor(inputSampleL);
		inputSampleR = floor(inputSampleR);

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
