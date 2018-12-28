/* ========================================
 *  Spiral2 - Spiral2.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Spiral2_H
#include "Spiral2.h"
#endif

void Spiral2::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double gain = pow(A*2.0,2.0);
	double iirAmount = pow(B,3.0)/overallscale;
	double presence = C;
	double output = D;
	double wet = E;

    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;

		static int noisesourceL = 0;
		static int noisesourceR = 850010;
		int residue;
		double applyresidue;

		noisesourceL = noisesourceL % 1700021; noisesourceL++;
		residue = noisesourceL * noisesourceL;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleL += applyresidue;
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			inputSampleL -= applyresidue;
		}

		noisesourceR = noisesourceR % 1700021; noisesourceR++;
		residue = noisesourceR * noisesourceR;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleR += applyresidue;
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			inputSampleR -= applyresidue;
		}
		//for live air, we always apply the dither noise. Then, if our result is
		//effectively digital black, we'll subtract it aSpiral2. We want a 'air' hiss
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;

		if (gain != 1.0) {
			inputSampleL *= gain;
			inputSampleR *= gain;
			prevSampleL *= gain;
			prevSampleR *= gain;
		}

		if (flip)
		{
			iirSampleAL = (iirSampleAL * (1 - iirAmount)) + (inputSampleL * iirAmount);
			iirSampleAR = (iirSampleAR * (1 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleL -= iirSampleAL;
			inputSampleR -= iirSampleAR;
		}
		else
		{
			iirSampleBL = (iirSampleBL * (1 - iirAmount)) + (inputSampleL * iirAmount);
			iirSampleBR = (iirSampleBR * (1 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleL -= iirSampleBL;
			inputSampleR -= iirSampleBR;
		}
		//highpass section

		long double presenceSampleL = sin(inputSampleL * fabs(prevSampleL)) / ((prevSampleL == 0.0) ?1:fabs(prevSampleL));
		long double presenceSampleR = sin(inputSampleR * fabs(prevSampleR)) / ((prevSampleR == 0.0) ?1:fabs(prevSampleR));
		//change from first Spiral: delay of one sample on the scaling factor.
		inputSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((inputSampleL == 0.0) ?1:fabs(inputSampleL));
		inputSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((inputSampleR == 0.0) ?1:fabs(inputSampleR));

		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
			presenceSampleL *= output;
			presenceSampleR *= output;
		}
		if (presence > 0.0) {
			inputSampleL = (inputSampleL * (1.0-presence)) + (presenceSampleL * presence);
			inputSampleR = (inputSampleR * (1.0-presence)) + (presenceSampleR * presence);
		}
		if (wet < 1.0) {
			inputSampleL = (drySampleL * (1.0-wet)) + (inputSampleL * wet);
			inputSampleR = (drySampleR * (1.0-wet)) + (inputSampleR * wet);
		}
		//nice little output stage template: if we have another scale of floating point
		//number, we really don't want to meaninglessly multiply that by 1.0.

		prevSampleL = drySampleL;
		prevSampleR = drySampleR;
		flip = !flip;

		//noise shaping to 32-bit floating point
		float fpTemp = inputSampleL;
		fpNShapeL += (inputSampleL-fpTemp);
		inputSampleL += fpNShapeL;
		//if this confuses you look at the wordlength for fpTemp :)
		fpTemp = inputSampleR;
		fpNShapeR += (inputSampleR-fpTemp);
		inputSampleR += fpNShapeR;
		//for deeper space and warmth, we try a non-oscillating noise shaping
		//that is kind of ruthless: it will forever retain the rounding errors
		//except we'll dial it back a hair at the end of every buffer processed
		//end noise shaping on 32 bit output

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
	fpNShapeL *= 0.999999;
	fpNShapeR *= 0.999999;
	//we will just delicately dial back the FP noise shaping, not even every sample
	//this is a good place to put subtle 'no runaway' calculations, though bear in mind
	//that it will be called more often when you use shorter sample buffers in the DAW.
	//So, very low latency operation will call these calculations more often.
}

void Spiral2::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double gain = pow(A*2.0,2.0);
	double iirAmount = pow(B,3.0)/overallscale;
	double presence = C;
	double output = D;
	double wet = E;


    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;

		static int noisesourceL = 0;
		static int noisesourceR = 850010;
		int residue;
		double applyresidue;

		noisesourceL = noisesourceL % 1700021; noisesourceL++;
		residue = noisesourceL * noisesourceL;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleL += applyresidue;
		if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
			inputSampleL -= applyresidue;
		}

		noisesourceR = noisesourceR % 1700021; noisesourceR++;
		residue = noisesourceR * noisesourceR;
		residue = residue % 170003; residue *= residue;
		residue = residue % 17011; residue *= residue;
		residue = residue % 1709; residue *= residue;
		residue = residue % 173; residue *= residue;
		residue = residue % 17;
		applyresidue = residue;
		applyresidue *= 0.00000001;
		applyresidue *= 0.00000001;
		inputSampleR += applyresidue;
		if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
			inputSampleR -= applyresidue;
		}
		//for live air, we always apply the dither noise. Then, if our result is
		//effectively digital black, we'll subtract it aSpiral2. We want a 'air' hiss
		long double drySampleL = inputSampleL;
		long double drySampleR = inputSampleR;

		if (gain != 1.0) {
			inputSampleL *= gain;
			inputSampleR *= gain;
			prevSampleL *= gain;
			prevSampleR *= gain;
		}

		if (flip)
		{
			iirSampleAL = (iirSampleAL * (1 - iirAmount)) + (inputSampleL * iirAmount);
			iirSampleAR = (iirSampleAR * (1 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleL -= iirSampleAL;
			inputSampleR -= iirSampleAR;
		}
		else
		{
			iirSampleBL = (iirSampleBL * (1 - iirAmount)) + (inputSampleL * iirAmount);
			iirSampleBR = (iirSampleBR * (1 - iirAmount)) + (inputSampleR * iirAmount);
			inputSampleL -= iirSampleBL;
			inputSampleR -= iirSampleBR;
		}
		//highpass section

		long double presenceSampleL = sin(inputSampleL * fabs(prevSampleL)) / ((prevSampleL == 0.0) ?1:fabs(prevSampleL));
		long double presenceSampleR = sin(inputSampleR * fabs(prevSampleR)) / ((prevSampleR == 0.0) ?1:fabs(prevSampleR));
		//change from first Spiral: delay of one sample on the scaling factor.
		inputSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((inputSampleL == 0.0) ?1:fabs(inputSampleL));
		inputSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((inputSampleR == 0.0) ?1:fabs(inputSampleR));

		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
			presenceSampleL *= output;
			presenceSampleR *= output;
		}
		if (presence > 0.0) {
			inputSampleL = (inputSampleL * (1.0-presence)) + (presenceSampleL * presence);
			inputSampleR = (inputSampleR * (1.0-presence)) + (presenceSampleR * presence);
		}
		if (wet < 1.0) {
			inputSampleL = (drySampleL * (1.0-wet)) + (inputSampleL * wet);
			inputSampleR = (drySampleR * (1.0-wet)) + (inputSampleR * wet);
		}
		//nice little output stage template: if we have another scale of floating point
		//number, we really don't want to meaninglessly multiply that by 1.0.

		prevSampleL = drySampleL;
		prevSampleR = drySampleR;
		flip = !flip;

		//noise shaping to 64-bit floating point
		double fpTemp = inputSampleL;
		fpNShapeL += (inputSampleL-fpTemp);
		inputSampleL += fpNShapeL;
		//if this confuses you look at the wordlength for fpTemp :)
		fpTemp = inputSampleR;
		fpNShapeR += (inputSampleR-fpTemp);
		inputSampleR += fpNShapeR;
		//for deeper space and warmth, we try a non-oscillating noise shaping
		//that is kind of ruthless: it will forever retain the rounding errors
		//except we'll dial it back a hair at the end of every buffer processed
		//end noise shaping on 64 bit output

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
	fpNShapeL *= 0.999999;
	fpNShapeR *= 0.999999;
	//we will just delicately dial back the FP noise shaping, not even every sample
	//this is a good place to put subtle 'no runaway' calculations, though bear in mind
	//that it will be called more often when you use shorter sample buffers in the DAW.
	//So, very low latency operation will call these calculations more often.
}
