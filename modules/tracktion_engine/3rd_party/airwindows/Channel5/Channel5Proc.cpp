/* ========================================
 *  Channel5 - Channel5.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Channel5_H
#include "Channel5.h"
#endif

void Channel5::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	const double localiirAmount = iirAmount / overallscale;
	const double localthreshold = threshold / overallscale;
	const double density = pow(drive,2); //this doesn't relate to the plugins Density and Drive much

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
		//effectively digital black, we'll subtract it again. We want a 'air' hiss

		if (fpFlip)
		{
			iirSampleLA = (iirSampleLA * (1 - localiirAmount)) + (inputSampleL * localiirAmount);
			inputSampleL = inputSampleL - iirSampleLA;
			iirSampleRA = (iirSampleRA * (1 - localiirAmount)) + (inputSampleR * localiirAmount);
			inputSampleR = inputSampleR - iirSampleRA;
		}
		else
		{
			iirSampleLB = (iirSampleLB * (1 - localiirAmount)) + (inputSampleL * localiirAmount);
			inputSampleL = inputSampleL - iirSampleLB;
			iirSampleRB = (iirSampleRB * (1 - localiirAmount)) + (inputSampleR * localiirAmount);
			inputSampleR = inputSampleR - iirSampleRB;
		}
		//highpass section

		long double bridgerectifier = fabs(inputSampleL)*1.57079633;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-density))+(bridgerectifier*density);
		else inputSampleL = (inputSampleL*(1-density))-(bridgerectifier*density);

		bridgerectifier = fabs(inputSampleR)*1.57079633;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0) inputSampleR = (inputSampleR*(1-density))+(bridgerectifier*density);
		else inputSampleR = (inputSampleR*(1-density))-(bridgerectifier*density);
		//drive section

		double clamp = inputSampleL - lastSampleL;
		if (clamp > localthreshold)
			inputSampleL = lastSampleL + localthreshold;
		if (-clamp > localthreshold)
			inputSampleL = lastSampleL - localthreshold;
		lastSampleL = inputSampleL;

		clamp = inputSampleR - lastSampleR;
		if (clamp > localthreshold)
			inputSampleR = lastSampleR + localthreshold;
		if (-clamp > localthreshold)
			inputSampleR = lastSampleR - localthreshold;
		lastSampleR = inputSampleR;
		//slew section
		fpFlip = !fpFlip;

		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}

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

void Channel5::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	const double localiirAmount = iirAmount / overallscale;
	const double localthreshold = threshold / overallscale;
	const double density = pow(drive,2); //this doesn't relate to the plugins Density and Drive much

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
		//effectively digital black, we'll subtract it again. We want a 'air' hiss

		if (fpFlip)
		{
			iirSampleLA = (iirSampleLA * (1 - localiirAmount)) + (inputSampleL * localiirAmount);
			inputSampleL = inputSampleL - iirSampleLA;
			iirSampleRA = (iirSampleRA * (1 - localiirAmount)) + (inputSampleR * localiirAmount);
			inputSampleR = inputSampleR - iirSampleRA;
		}
		else
		{
			iirSampleLB = (iirSampleLB * (1 - localiirAmount)) + (inputSampleL * localiirAmount);
			inputSampleL = inputSampleL - iirSampleLB;
			iirSampleRB = (iirSampleRB * (1 - localiirAmount)) + (inputSampleR * localiirAmount);
			inputSampleR = inputSampleR - iirSampleRB;
		}
		//highpass section

		long double bridgerectifier = fabs(inputSampleL)*1.57079633;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-density))+(bridgerectifier*density);
		else inputSampleL = (inputSampleL*(1-density))-(bridgerectifier*density);

		bridgerectifier = fabs(inputSampleR)*1.57079633;
		if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
		else bridgerectifier = sin(bridgerectifier);
		if (inputSampleR > 0) inputSampleR = (inputSampleR*(1-density))+(bridgerectifier*density);
		else inputSampleR = (inputSampleR*(1-density))-(bridgerectifier*density);
		//drive section

		double clamp = inputSampleL - lastSampleL;
		if (clamp > localthreshold)
			inputSampleL = lastSampleL + localthreshold;
		if (-clamp > localthreshold)
			inputSampleL = lastSampleL - localthreshold;
		lastSampleL = inputSampleL;

		clamp = inputSampleR - lastSampleR;
		if (clamp > localthreshold)
			inputSampleR = lastSampleR + localthreshold;
		if (-clamp > localthreshold)
			inputSampleR = lastSampleR - localthreshold;
		lastSampleR = inputSampleR;
		//slew section
		fpFlip = !fpFlip;

		if (output < 1.0) {
			inputSampleL *= output;
			inputSampleR *= output;
		}

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
