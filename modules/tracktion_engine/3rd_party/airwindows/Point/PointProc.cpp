/* ========================================
 *  Point - Point.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Point_H
#include "Point.h"
#endif

void Point::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double gaintrim = pow(10.0,((A*24.0)-12.0)/20);
	double nibDiv = 1 / pow(C+0.2,7);
	nibDiv /= overallscale;
	double nobDiv;
	if (((B*2.0)-1.0) > 0) nobDiv = nibDiv / (1.001-((B*2.0)-1.0));
	else nobDiv = nibDiv * (1.001-pow(((B*2.0)-1.0)*0.75,2));
	double nibnobFactor = 0.0; //start with the fallthrough value, why not
	double absolute;

	float fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;
	long double inputSampleL;
	long double inputSampleR;

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

		inputSampleL *= gaintrim;
		absolute = fabs(inputSampleL);
		if (fpFlip)
		{
			nibAL = nibAL + (absolute / nibDiv);
			nibAL = nibAL / (1 + (1/nibDiv));
			nobAL = nobAL + (absolute / nobDiv);
			nobAL = nobAL / (1 + (1/nobDiv));
			if (nobAL > 0)
			{
				nibnobFactor = nibAL / nobAL;
			}
		}
		else
		{
			nibBL = nibBL + (absolute / nibDiv);
			nibBL = nibBL / (1 + (1/nibDiv));
			nobBL = nobBL + (absolute / nobDiv);
			nobBL = nobBL / (1 + (1/nobDiv));
			if (nobBL > 0)
			{
				nibnobFactor = nibBL / nobBL;
			}
		}
		inputSampleL *= nibnobFactor;


		inputSampleR *= gaintrim;
		absolute = fabs(inputSampleR);
		if (fpFlip)
		{
			nibAR = nibAR + (absolute / nibDiv);
			nibAR = nibAR / (1 + (1/nibDiv));
			nobAR = nobAR + (absolute / nobDiv);
			nobAR = nobAR / (1 + (1/nobDiv));
			if (nobAR > 0)
			{
				nibnobFactor = nibAR / nobAR;
			}
		}
		else
		{
			nibBR = nibBR + (absolute / nibDiv);
			nibBR = nibBR / (1 + (1/nibDiv));
			nobBR = nobBR + (absolute / nobDiv);
			nobBR = nobBR / (1 + (1/nobDiv));
			if (nobBR > 0)
			{
				nibnobFactor = nibBR / nobBR;
			}
		}
		inputSampleR *= nibnobFactor;

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

void Point::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double gaintrim = pow(10.0,((A*24.0)-12.0)/20);
	double nibDiv = 1 / pow(C+0.2,7);
	nibDiv /= overallscale;
	double nobDiv;
	if (((B*2.0)-1.0) > 0) nobDiv = nibDiv / (1.001-((B*2.0)-1.0));
	else nobDiv = nibDiv * (1.001-pow(((B*2.0)-1.0)*0.75,2));
	double nibnobFactor = 0.0; //start with the fallthrough value, why not
	double absolute;

	double fpTemp; //this is different from singlereplacing
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;
	long double inputSampleL;
	long double inputSampleR;

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

		inputSampleL *= gaintrim;
		absolute = fabs(inputSampleL);
		if (fpFlip)
		{
			nibAL = nibAL + (absolute / nibDiv);
			nibAL = nibAL / (1 + (1/nibDiv));
			nobAL = nobAL + (absolute / nobDiv);
			nobAL = nobAL / (1 + (1/nobDiv));
			if (nobAL > 0)
			{
				nibnobFactor = nibAL / nobAL;
			}
		}
		else
		{
			nibBL = nibBL + (absolute / nibDiv);
			nibBL = nibBL / (1 + (1/nibDiv));
			nobBL = nobBL + (absolute / nobDiv);
			nobBL = nobBL / (1 + (1/nobDiv));
			if (nobBL > 0)
			{
				nibnobFactor = nibBL / nobBL;
			}
		}
		inputSampleL *= nibnobFactor;


		inputSampleR *= gaintrim;
		absolute = fabs(inputSampleR);
		if (fpFlip)
		{
			nibAR = nibAR + (absolute / nibDiv);
			nibAR = nibAR / (1 + (1/nibDiv));
			nobAR = nobAR + (absolute / nobDiv);
			nobAR = nobAR / (1 + (1/nobDiv));
			if (nobAR > 0)
			{
				nibnobFactor = nibAR / nobAR;
			}
		}
		else
		{
			nibBR = nibBR + (absolute / nibDiv);
			nibBR = nibBR / (1 + (1/nibDiv));
			nobBR = nobBR + (absolute / nobDiv);
			nobBR = nobBR / (1 + (1/nobDiv));
			if (nobBR > 0)
			{
				nibnobFactor = nibBR / nobBR;
			}
		}
		inputSampleR *= nibnobFactor;

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
