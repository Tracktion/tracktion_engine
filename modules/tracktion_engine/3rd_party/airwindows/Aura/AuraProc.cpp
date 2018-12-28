/* ========================================
 *  Aura - Aura.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Aura_H
#include "Aura.h"
#endif

void Aura::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	float fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	double correctionL;
	double correctionR;
	double accumulatorSampleL;
	double accumulatorSampleR;
	double velocityL;
	double velocityR;
	double trim = A;
	double wet = B;
	double dry = 1.0 - wet;
	double overallscale = trim * 10.0;
	double gain = overallscale + (pow(wet,3) * 0.187859642462067);
	trim *= (1.0 - (pow(wet,3) * 0.187859642462067));
	long double inputSampleL;
	long double inputSampleR;
	double drySampleL;
	double drySampleR;

	if (gain < 1.0) gain = 1.0;
	if (gain > 1.0) {f[0] = 1.0; gain -= 1.0;} else {f[0] = gain; gain = 0.0;}
	if (gain > 1.0) {f[1] = 1.0; gain -= 1.0;} else {f[1] = gain; gain = 0.0;}
	if (gain > 1.0) {f[2] = 1.0; gain -= 1.0;} else {f[2] = gain; gain = 0.0;}
	if (gain > 1.0) {f[3] = 1.0; gain -= 1.0;} else {f[3] = gain; gain = 0.0;}
	if (gain > 1.0) {f[4] = 1.0; gain -= 1.0;} else {f[4] = gain; gain = 0.0;}
	if (gain > 1.0) {f[5] = 1.0; gain -= 1.0;} else {f[5] = gain; gain = 0.0;}
	if (gain > 1.0) {f[6] = 1.0; gain -= 1.0;} else {f[6] = gain; gain = 0.0;}
	if (gain > 1.0) {f[7] = 1.0; gain -= 1.0;} else {f[7] = gain; gain = 0.0;}
	if (gain > 1.0) {f[8] = 1.0; gain -= 1.0;} else {f[8] = gain; gain = 0.0;}
	if (gain > 1.0) {f[9] = 1.0; gain -= 1.0;} else {f[9] = gain; gain = 0.0;}
	if (gain > 1.0) {f[10] = 1.0; gain -= 1.0;} else {f[10] = gain; gain = 0.0;}
	if (gain > 1.0) {f[11] = 1.0; gain -= 1.0;} else {f[11] = gain; gain = 0.0;}
	if (gain > 1.0) {f[12] = 1.0; gain -= 1.0;} else {f[12] = gain; gain = 0.0;}
	if (gain > 1.0) {f[13] = 1.0; gain -= 1.0;} else {f[13] = gain; gain = 0.0;}
	if (gain > 1.0) {f[14] = 1.0; gain -= 1.0;} else {f[14] = gain; gain = 0.0;}
	if (gain > 1.0) {f[15] = 1.0; gain -= 1.0;} else {f[15] = gain; gain = 0.0;}
	if (gain > 1.0) {f[16] = 1.0; gain -= 1.0;} else {f[16] = gain; gain = 0.0;}
	if (gain > 1.0) {f[17] = 1.0; gain -= 1.0;} else {f[17] = gain; gain = 0.0;}
	if (gain > 1.0) {f[18] = 1.0; gain -= 1.0;} else {f[18] = gain; gain = 0.0;}
	if (gain > 1.0) {f[19] = 1.0; gain -= 1.0;} else {f[19] = gain; gain = 0.0;}

	//there, now we have a neat little moving average with remainders

	if (overallscale < 1.0) overallscale = 1.0;
	f[0] /= overallscale;
	f[1] /= overallscale;
	f[2] /= overallscale;
	f[3] /= overallscale;
	f[4] /= overallscale;
	f[5] /= overallscale;
	f[6] /= overallscale;
	f[7] /= overallscale;
	f[8] /= overallscale;
	f[9] /= overallscale;
	f[10] /= overallscale;
	f[11] /= overallscale;
	f[12] /= overallscale;
	f[13] /= overallscale;
	f[14] /= overallscale;
	f[15] /= overallscale;
	f[16] /= overallscale;
	f[17] /= overallscale;
	f[18] /= overallscale;
	f[19] /= overallscale;
	//and now it's neatly scaled, too

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

		velocityL = lastSampleL - inputSampleL;
		correctionL = previousVelocityL - velocityL;

		bL[19] = bL[18]; bL[18] = bL[17]; bL[17] = bL[16]; bL[16] = bL[15];
		bL[15] = bL[14]; bL[14] = bL[13]; bL[13] = bL[12]; bL[12] = bL[11];
		bL[11] = bL[10]; bL[10] = bL[9];
		bL[9] = bL[8]; bL[8] = bL[7]; bL[7] = bL[6]; bL[6] = bL[5];
		bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1];
		bL[1] = bL[0]; bL[0] = accumulatorSampleL = correctionL;

		//we are accumulating rates of change of the rate of change

		accumulatorSampleL *= f[0];
		accumulatorSampleL += (bL[1] * f[1]);
		accumulatorSampleL += (bL[2] * f[2]);
		accumulatorSampleL += (bL[3] * f[3]);
		accumulatorSampleL += (bL[4] * f[4]);
		accumulatorSampleL += (bL[5] * f[5]);
		accumulatorSampleL += (bL[6] * f[6]);
		accumulatorSampleL += (bL[7] * f[7]);
		accumulatorSampleL += (bL[8] * f[8]);
		accumulatorSampleL += (bL[9] * f[9]);
		accumulatorSampleL += (bL[10] * f[10]);
		accumulatorSampleL += (bL[11] * f[11]);
		accumulatorSampleL += (bL[12] * f[12]);
		accumulatorSampleL += (bL[13] * f[13]);
		accumulatorSampleL += (bL[14] * f[14]);
		accumulatorSampleL += (bL[15] * f[15]);
		accumulatorSampleL += (bL[16] * f[16]);
		accumulatorSampleL += (bL[17] * f[17]);
		accumulatorSampleL += (bL[18] * f[18]);
		accumulatorSampleL += (bL[19] * f[19]);

		velocityL = previousVelocityL + accumulatorSampleL;
		inputSampleL = lastSampleL + velocityL;
		lastSampleL = inputSampleL;
		previousVelocityL = -velocityL * pow(trim,2);
		//left channel done

		velocityR = lastSampleR - inputSampleR;
		correctionR = previousVelocityR - velocityR;

		bR[19] = bR[18]; bR[18] = bR[17]; bR[17] = bR[16]; bR[16] = bR[15];
		bR[15] = bR[14]; bR[14] = bR[13]; bR[13] = bR[12]; bR[12] = bR[11];
		bR[11] = bR[10]; bR[10] = bR[9];
		bR[9] = bR[8]; bR[8] = bR[7]; bR[7] = bR[6]; bR[6] = bR[5];
		bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1];
		bR[1] = bR[0]; bR[0] = accumulatorSampleR = correctionR;

		//we are accumulating rates of change of the rate of change

		accumulatorSampleR *= f[0];
		accumulatorSampleR += (bR[1] * f[1]);
		accumulatorSampleR += (bR[2] * f[2]);
		accumulatorSampleR += (bR[3] * f[3]);
		accumulatorSampleR += (bR[4] * f[4]);
		accumulatorSampleR += (bR[5] * f[5]);
		accumulatorSampleR += (bR[6] * f[6]);
		accumulatorSampleR += (bR[7] * f[7]);
		accumulatorSampleR += (bR[8] * f[8]);
		accumulatorSampleR += (bR[9] * f[9]);
		accumulatorSampleR += (bR[10] * f[10]);
		accumulatorSampleR += (bR[11] * f[11]);
		accumulatorSampleR += (bR[12] * f[12]);
		accumulatorSampleR += (bR[13] * f[13]);
		accumulatorSampleR += (bR[14] * f[14]);
		accumulatorSampleR += (bR[15] * f[15]);
		accumulatorSampleR += (bR[16] * f[16]);
		accumulatorSampleR += (bR[17] * f[17]);
		accumulatorSampleR += (bR[18] * f[18]);
		accumulatorSampleR += (bR[19] * f[19]);
		//we are doing our repetitive calculations on a separate value

		velocityR = previousVelocityR + accumulatorSampleR;
		inputSampleR = lastSampleR + velocityR;
		lastSampleR = inputSampleR;
		previousVelocityR = -velocityR * pow(trim,2);
		//right channel done

		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
			inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
		}

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

void Aura::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
	double* in1  =  inputs[0];
	double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	double correctionL;
	double correctionR;
	double accumulatorSampleL;
	double accumulatorSampleR;
	double velocityL;
	double velocityR;
	double trim = A;
	double wet = B;
	double dry = 1.0 - wet;
	double overallscale = trim * 10.0;
	double gain = overallscale + (pow(wet,3) * 0.187859642462067);
	trim *= (1.0 - (pow(wet,3) * 0.187859642462067));
	long double inputSampleL;
	long double inputSampleR;
	double drySampleL;
	double drySampleR;

	if (gain < 1.0) gain = 1.0;
	if (gain > 1.0) {f[0] = 1.0; gain -= 1.0;} else {f[0] = gain; gain = 0.0;}
	if (gain > 1.0) {f[1] = 1.0; gain -= 1.0;} else {f[1] = gain; gain = 0.0;}
	if (gain > 1.0) {f[2] = 1.0; gain -= 1.0;} else {f[2] = gain; gain = 0.0;}
	if (gain > 1.0) {f[3] = 1.0; gain -= 1.0;} else {f[3] = gain; gain = 0.0;}
	if (gain > 1.0) {f[4] = 1.0; gain -= 1.0;} else {f[4] = gain; gain = 0.0;}
	if (gain > 1.0) {f[5] = 1.0; gain -= 1.0;} else {f[5] = gain; gain = 0.0;}
	if (gain > 1.0) {f[6] = 1.0; gain -= 1.0;} else {f[6] = gain; gain = 0.0;}
	if (gain > 1.0) {f[7] = 1.0; gain -= 1.0;} else {f[7] = gain; gain = 0.0;}
	if (gain > 1.0) {f[8] = 1.0; gain -= 1.0;} else {f[8] = gain; gain = 0.0;}
	if (gain > 1.0) {f[9] = 1.0; gain -= 1.0;} else {f[9] = gain; gain = 0.0;}
	if (gain > 1.0) {f[10] = 1.0; gain -= 1.0;} else {f[10] = gain; gain = 0.0;}
	if (gain > 1.0) {f[11] = 1.0; gain -= 1.0;} else {f[11] = gain; gain = 0.0;}
	if (gain > 1.0) {f[12] = 1.0; gain -= 1.0;} else {f[12] = gain; gain = 0.0;}
	if (gain > 1.0) {f[13] = 1.0; gain -= 1.0;} else {f[13] = gain; gain = 0.0;}
	if (gain > 1.0) {f[14] = 1.0; gain -= 1.0;} else {f[14] = gain; gain = 0.0;}
	if (gain > 1.0) {f[15] = 1.0; gain -= 1.0;} else {f[15] = gain; gain = 0.0;}
	if (gain > 1.0) {f[16] = 1.0; gain -= 1.0;} else {f[16] = gain; gain = 0.0;}
	if (gain > 1.0) {f[17] = 1.0; gain -= 1.0;} else {f[17] = gain; gain = 0.0;}
	if (gain > 1.0) {f[18] = 1.0; gain -= 1.0;} else {f[18] = gain; gain = 0.0;}
	if (gain > 1.0) {f[19] = 1.0; gain -= 1.0;} else {f[19] = gain; gain = 0.0;}

	//there, now we have a neat little moving average with remainders

	if (overallscale < 1.0) overallscale = 1.0;
	f[0] /= overallscale;
	f[1] /= overallscale;
	f[2] /= overallscale;
	f[3] /= overallscale;
	f[4] /= overallscale;
	f[5] /= overallscale;
	f[6] /= overallscale;
	f[7] /= overallscale;
	f[8] /= overallscale;
	f[9] /= overallscale;
	f[10] /= overallscale;
	f[11] /= overallscale;
	f[12] /= overallscale;
	f[13] /= overallscale;
	f[14] /= overallscale;
	f[15] /= overallscale;
	f[16] /= overallscale;
	f[17] /= overallscale;
	f[18] /= overallscale;
	f[19] /= overallscale;
	//and now it's neatly scaled, too

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

		velocityL = lastSampleL - inputSampleL;
		correctionL = previousVelocityL - velocityL;

		bL[19] = bL[18]; bL[18] = bL[17]; bL[17] = bL[16]; bL[16] = bL[15];
		bL[15] = bL[14]; bL[14] = bL[13]; bL[13] = bL[12]; bL[12] = bL[11];
		bL[11] = bL[10]; bL[10] = bL[9];
		bL[9] = bL[8]; bL[8] = bL[7]; bL[7] = bL[6]; bL[6] = bL[5];
		bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1];
		bL[1] = bL[0]; bL[0] = accumulatorSampleL = correctionL;

		//we are accumulating rates of change of the rate of change

		accumulatorSampleL *= f[0];
		accumulatorSampleL += (bL[1] * f[1]);
		accumulatorSampleL += (bL[2] * f[2]);
		accumulatorSampleL += (bL[3] * f[3]);
		accumulatorSampleL += (bL[4] * f[4]);
		accumulatorSampleL += (bL[5] * f[5]);
		accumulatorSampleL += (bL[6] * f[6]);
		accumulatorSampleL += (bL[7] * f[7]);
		accumulatorSampleL += (bL[8] * f[8]);
		accumulatorSampleL += (bL[9] * f[9]);
		accumulatorSampleL += (bL[10] * f[10]);
		accumulatorSampleL += (bL[11] * f[11]);
		accumulatorSampleL += (bL[12] * f[12]);
		accumulatorSampleL += (bL[13] * f[13]);
		accumulatorSampleL += (bL[14] * f[14]);
		accumulatorSampleL += (bL[15] * f[15]);
		accumulatorSampleL += (bL[16] * f[16]);
		accumulatorSampleL += (bL[17] * f[17]);
		accumulatorSampleL += (bL[18] * f[18]);
		accumulatorSampleL += (bL[19] * f[19]);

		velocityL = previousVelocityL + accumulatorSampleL;
		inputSampleL = lastSampleL + velocityL;
		lastSampleL = inputSampleL;
		previousVelocityL = -velocityL * pow(trim,2);
		//left channel done

		velocityR = lastSampleR - inputSampleR;
		correctionR = previousVelocityR - velocityR;

		bR[19] = bR[18]; bR[18] = bR[17]; bR[17] = bR[16]; bR[16] = bR[15];
		bR[15] = bR[14]; bR[14] = bR[13]; bR[13] = bR[12]; bR[12] = bR[11];
		bR[11] = bR[10]; bR[10] = bR[9];
		bR[9] = bR[8]; bR[8] = bR[7]; bR[7] = bR[6]; bR[6] = bR[5];
		bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1];
		bR[1] = bR[0]; bR[0] = accumulatorSampleR = correctionR;

		//we are accumulating rates of change of the rate of change

		accumulatorSampleR *= f[0];
		accumulatorSampleR += (bR[1] * f[1]);
		accumulatorSampleR += (bR[2] * f[2]);
		accumulatorSampleR += (bR[3] * f[3]);
		accumulatorSampleR += (bR[4] * f[4]);
		accumulatorSampleR += (bR[5] * f[5]);
		accumulatorSampleR += (bR[6] * f[6]);
		accumulatorSampleR += (bR[7] * f[7]);
		accumulatorSampleR += (bR[8] * f[8]);
		accumulatorSampleR += (bR[9] * f[9]);
		accumulatorSampleR += (bR[10] * f[10]);
		accumulatorSampleR += (bR[11] * f[11]);
		accumulatorSampleR += (bR[12] * f[12]);
		accumulatorSampleR += (bR[13] * f[13]);
		accumulatorSampleR += (bR[14] * f[14]);
		accumulatorSampleR += (bR[15] * f[15]);
		accumulatorSampleR += (bR[16] * f[16]);
		accumulatorSampleR += (bR[17] * f[17]);
		accumulatorSampleR += (bR[18] * f[18]);
		accumulatorSampleR += (bR[19] * f[19]);
		//we are doing our repetitive calculations on a separate value

		velocityR = previousVelocityR + accumulatorSampleR;
		inputSampleR = lastSampleR + velocityR;
		lastSampleR = inputSampleR;
		previousVelocityR = -velocityR * pow(trim,2);
		//right channel done

		if (wet !=1.0) {
			inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
			inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
		}

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
