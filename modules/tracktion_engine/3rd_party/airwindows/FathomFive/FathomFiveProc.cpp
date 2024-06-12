/* ========================================
 *  FathomFive - FathomFive.h
 *  Copyright (c) 2016 airwindows, Airwindows uses the MIT license
 * ======================================== */

#ifndef __FathomFive_H
#include "FathomFive.h"
#endif

void FathomFive::processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames)
{
	float* in1 = inputs[0];
	float* in2 = inputs[1];
	float* out1 = outputs[0];
	float* out2 = outputs[1];

	double EQ = 0.01 + ((pow(C, 4) / getSampleRate()) * 32000.0);
	double dcblock = EQ / 320.0;
	double wet = D * 2.0;
	double dry = 2.0 - wet;
	double bridgerectifier;
	double tempL;
	double tempR;
	double basstrim = (0.01 / EQ) + 1.0;
	if (wet > 1.0) wet = 1.0;
	if (dry > 1.0) dry = 1.0;

	double inputSampleL;
	double inputSampleR;

	while (--sampleFrames >= 0)
	{
		inputSampleL = *in1;
		inputSampleR = *in2;
		if (fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;

		if (inputSampleL > 0)
		{
			if (WasNegativeL) { SubOctaveL = !SubOctaveL; } WasNegativeL = false;
		}
		else { WasNegativeL = true; }
		if (inputSampleR > 0)
		{
			if (WasNegativeR) { SubOctaveR = !SubOctaveR; } WasNegativeR = false;
		}
		else { WasNegativeR = true; }

		iirSampleLD = (iirSampleLD * (1 - EQ)) + (inputSampleL * EQ);
		bridgerectifier = fabs(iirSampleLD);
		if (SubOctaveL) tempL = bridgerectifier * B;
		else tempL = -bridgerectifier * B;
		iirSampleRD = (iirSampleRD * (1 - EQ)) + (inputSampleR * EQ);
		bridgerectifier = fabs(iirSampleRD);
		if (SubOctaveR) tempR = bridgerectifier * B;
		else tempR = -bridgerectifier * B;

		tempL += (inputSampleL * A);
		tempR += (inputSampleR * A);

		iirSampleLA += (tempL * EQ);
		iirSampleLA -= (iirSampleLA * iirSampleLA * iirSampleLA * EQ);
		if (iirSampleLA > 0) iirSampleLA -= dcblock;
		else iirSampleLA += dcblock;
		tempL = iirSampleLA * basstrim;

		iirSampleRA += (tempR * EQ);
		iirSampleRA -= (iirSampleRA * iirSampleRA * iirSampleRA * EQ);
		if (iirSampleRA > 0) iirSampleRA -= dcblock;
		else iirSampleRA += dcblock;
		tempR = iirSampleRA * basstrim;

		iirSampleLB = (iirSampleLB * (1 - EQ)) + (tempL * EQ);
		tempL = iirSampleLB;
		iirSampleRB = (iirSampleRB * (1 - EQ)) + (tempR * EQ);
		tempR = iirSampleRB;

		iirSampleLC = (iirSampleLC * (1 - EQ)) + (tempL * EQ);
		tempL = iirSampleLC;
		iirSampleRC = (iirSampleRC * (1 - EQ)) + (tempR * EQ);
		tempR = iirSampleRC;

		inputSampleL = (inputSampleL * dry) + (tempL * wet);
		inputSampleR = (inputSampleR * dry) + (tempR * wet);

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
	}
}

void FathomFive::processDoubleReplacing(double** inputs, double** outputs, VstInt32 sampleFrames)
{
	double* in1 = inputs[0];
	double* in2 = inputs[1];
	double* out1 = outputs[0];
	double* out2 = outputs[1];

	double EQ = 0.01 + ((pow(C, 4) / getSampleRate()) * 32000.0);
	double dcblock = EQ / 320.0;
	double wet = D * 2.0;
	double dry = 2.0 - wet;
	double bridgerectifier;
	double tempL;
	double tempR;
	double basstrim = (0.01 / EQ) + 1.0;
	if (wet > 1.0) wet = 1.0;
	if (dry > 1.0) dry = 1.0;

	double inputSampleL;
	double inputSampleR;

	while (--sampleFrames >= 0)
	{
		inputSampleL = *in1;
		inputSampleR = *in2;
		if (fabs(inputSampleL) < 1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR) < 1.18e-23) inputSampleR = fpdR * 1.18e-17;

		if (inputSampleL > 0)
		{
			if (WasNegativeL) { SubOctaveL = !SubOctaveL; } WasNegativeL = false;
		}
		else { WasNegativeL = true; }
		if (inputSampleR > 0)
		{
			if (WasNegativeR) { SubOctaveR = !SubOctaveR; } WasNegativeR = false;
		}
		else { WasNegativeR = true; }

		iirSampleLD = (iirSampleLD * (1 - EQ)) + (inputSampleL * EQ);
		bridgerectifier = fabs(iirSampleLD);
		if (SubOctaveL) tempL = bridgerectifier * B;
		else tempL = -bridgerectifier * B;
		iirSampleRD = (iirSampleRD * (1 - EQ)) + (inputSampleR * EQ);
		bridgerectifier = fabs(iirSampleRD);
		if (SubOctaveR) tempR = bridgerectifier * B;
		else tempR = -bridgerectifier * B;

		tempL += (inputSampleL * A);
		tempR += (inputSampleR * A);

		iirSampleLA += (tempL * EQ);
		iirSampleLA -= (iirSampleLA * iirSampleLA * iirSampleLA * EQ);
		if (iirSampleLA > 0) iirSampleLA -= dcblock;
		else iirSampleLA += dcblock;
		tempL = iirSampleLA * basstrim;

		iirSampleRA += (tempR * EQ);
		iirSampleRA -= (iirSampleRA * iirSampleRA * iirSampleRA * EQ);
		if (iirSampleRA > 0) iirSampleRA -= dcblock;
		else iirSampleRA += dcblock;
		tempR = iirSampleRA * basstrim;

		iirSampleLB = (iirSampleLB * (1 - EQ)) + (tempL * EQ);
		tempL = iirSampleLB;
		iirSampleRB = (iirSampleRB * (1 - EQ)) + (tempR * EQ);
		tempR = iirSampleRB;

		iirSampleLC = (iirSampleLC * (1 - EQ)) + (tempL * EQ);
		tempL = iirSampleLC;
		iirSampleRC = (iirSampleRC * (1 - EQ)) + (tempR * EQ);
		tempR = iirSampleRC;

		inputSampleL = (inputSampleL * dry) + (tempL * wet);
		inputSampleR = (inputSampleR * dry) + (tempR * wet);

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
	}
}