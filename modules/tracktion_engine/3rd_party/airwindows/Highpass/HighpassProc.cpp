/* ========================================
 *  Highpass - Highpass.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Highpass_H
#include "Highpass.h"
#endif

void Highpass::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double iirAmount = pow(A,3)/overallscale;
	double tight = (B*2.0)-1.0;
	double wet = C;
	double dry = 1.0 - wet;
	double offset;
	double inputSampleL;
	double inputSampleR;
	double outputSampleL;
	double outputSampleR;
	float fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	iirAmount += (iirAmount * tight * tight);
	if (tight > 0) tight /= 1.5;
	else tight /= 3.0;
	//we are setting it up so that to either extreme we can get an audible sound,
	//but sort of scaled so small adjustments don't shift the cutoff frequency yet.
	if (iirAmount <= 0.0) iirAmount = 0.0;
	if (iirAmount > 1.0) iirAmount = 1.0;
	//handle the change in cutoff frequency

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
		outputSampleL = inputSampleL;
		outputSampleR = inputSampleR;

		if (tight > 0) offset = (1 - tight) + (fabs(inputSampleL)*tight);
		else offset = (1 + tight) + ((1-fabs(inputSampleL))*tight);
		if (offset < 0) offset = 0;
		if (offset > 1) offset = 1;
		if (fpFlip)
		{
			iirSampleAL = (iirSampleAL * (1 - (offset * iirAmount))) + (inputSampleL * (offset * iirAmount));
			outputSampleL = outputSampleL - iirSampleAL;
		}
		else
		{
			iirSampleBL = (iirSampleBL * (1 - (offset * iirAmount))) + (inputSampleL * (offset * iirAmount));
			outputSampleL = outputSampleL - iirSampleBL;
		}


		if (tight > 0) offset = (1 - tight) + (fabs(inputSampleR)*tight);
		else offset = (1 + tight) + ((1-fabs(inputSampleR))*tight);
		if (offset < 0) offset = 0;
		if (offset > 1) offset = 1;
		if (fpFlip)
		{
			iirSampleAR = (iirSampleAR * (1 - (offset * iirAmount))) + (inputSampleR * (offset * iirAmount));
			outputSampleR = outputSampleR - iirSampleAR;
		}
		else
		{
			iirSampleBR = (iirSampleBR * (1 - (offset * iirAmount))) + (inputSampleR * (offset * iirAmount));
			outputSampleR = outputSampleR - iirSampleBR;
		}



		if (wet < 1.0) outputSampleL = (outputSampleL * wet) + (inputSampleL * dry);
		if (wet < 1.0) outputSampleR = (outputSampleR * wet) + (inputSampleR * dry);

		//noise shaping to 32-bit floating point
		if (fpFlip) {
			fpTemp = outputSampleL;
			fpNShapeLA = (fpNShapeLA*fpOld)+((outputSampleL-fpTemp)*fpNew);
			outputSampleL += fpNShapeLA;

			fpTemp = outputSampleR;
			fpNShapeRA = (fpNShapeRA*fpOld)+((outputSampleR-fpTemp)*fpNew);
			outputSampleR += fpNShapeRA;
		}
		else {
			fpTemp = outputSampleL;
			fpNShapeLB = (fpNShapeLB*fpOld)+((outputSampleL-fpTemp)*fpNew);
			outputSampleL += fpNShapeLB;

			fpTemp = outputSampleR;
			fpNShapeRB = (fpNShapeRB*fpOld)+((outputSampleR-fpTemp)*fpNew);
			outputSampleR += fpNShapeRB;
		}
		fpFlip = !fpFlip;
		//end noise shaping on 32 bit output

		*out1 = outputSampleL;
		*out2 = outputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}

void Highpass::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double iirAmount = pow(A,3)/overallscale;
	double tight = (B*2.0)-1.0;
	double wet = C;
	double dry = 1.0 - wet;
	double offset;
	double inputSampleL;
	double inputSampleR;
	double outputSampleL;
	double outputSampleR;
	double fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	iirAmount += (iirAmount * tight * tight);
	if (tight > 0) tight /= 1.5;
	else tight /= 3.0;
	//we are setting it up so that to either extreme we can get an audible sound,
	//but sort of scaled so small adjustments don't shift the cutoff frequency yet.
	if (iirAmount <= 0.0) iirAmount = 0.0;
	if (iirAmount > 1.0) iirAmount = 1.0;
	//handle the change in cutoff frequency

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
		outputSampleL = inputSampleL;
		outputSampleR = inputSampleR;

		if (tight > 0) offset = (1 - tight) + (fabs(inputSampleL)*tight);
		else offset = (1 + tight) + ((1-fabs(inputSampleL))*tight);
		if (offset < 0) offset = 0;
		if (offset > 1) offset = 1;
		if (fpFlip)
		{
			iirSampleAL = (iirSampleAL * (1 - (offset * iirAmount))) + (inputSampleL * (offset * iirAmount));
			outputSampleL = outputSampleL - iirSampleAL;
		}
		else
		{
			iirSampleBL = (iirSampleBL * (1 - (offset * iirAmount))) + (inputSampleL * (offset * iirAmount));
			outputSampleL = outputSampleL - iirSampleBL;
		}


		if (tight > 0) offset = (1 - tight) + (fabs(inputSampleR)*tight);
		else offset = (1 + tight) + ((1-fabs(inputSampleR))*tight);
		if (offset < 0) offset = 0;
		if (offset > 1) offset = 1;
		if (fpFlip)
		{
			iirSampleAR = (iirSampleAR * (1 - (offset * iirAmount))) + (inputSampleR * (offset * iirAmount));
			outputSampleR = outputSampleR - iirSampleAR;
		}
		else
		{
			iirSampleBR = (iirSampleBR * (1 - (offset * iirAmount))) + (inputSampleR * (offset * iirAmount));
			outputSampleR = outputSampleR - iirSampleBR;
		}



		if (wet < 1.0) outputSampleL = (outputSampleL * wet) + (inputSampleL * dry);
		if (wet < 1.0) outputSampleR = (outputSampleR * wet) + (inputSampleR * dry);

		//noise shaping to 32-bit floating point
		if (fpFlip) {
			fpTemp = outputSampleL;
			fpNShapeLA = (fpNShapeLA*fpOld)+((outputSampleL-fpTemp)*fpNew);
			outputSampleL += fpNShapeLA;

			fpTemp = outputSampleR;
			fpNShapeRA = (fpNShapeRA*fpOld)+((outputSampleR-fpTemp)*fpNew);
			outputSampleR += fpNShapeRA;
		}
		else {
			fpTemp = outputSampleL;
			fpNShapeLB = (fpNShapeLB*fpOld)+((outputSampleL-fpTemp)*fpNew);
			outputSampleL += fpNShapeLB;

			fpTemp = outputSampleR;
			fpNShapeRB = (fpNShapeRB*fpOld)+((outputSampleR-fpTemp)*fpNew);
			outputSampleR += fpNShapeRB;
		}
		fpFlip = !fpFlip;
		//end noise shaping on 32 bit output

		*out1 = outputSampleL;
		*out2 = outputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}
