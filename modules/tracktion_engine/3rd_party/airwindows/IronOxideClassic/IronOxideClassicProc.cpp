/* ========================================
 *  IronOxideClassic - IronOxideClassic.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __IronOxideClassic_H
#include "IronOxideClassic.h"
#endif

void IronOxideClassic::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
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

	double inputgain = pow(10.0,((A*36.0)-18.0)/20.0);
	double outputgain = pow(10.0,((C*36.0)-18.0)/20.0);
	double ips = (((B*B)*(B*B)*148.5)+1.5) * 1.1;
	//slight correction to dial in convincing ips settings
	if (ips < 1 || ips > 200){ips=33.0;}
	//sanity checks are always key
	double iirAmount = ips/430.0; //for low leaning
	double bridgerectifierL;
	double bridgerectifierR;
	double fastTaper = ips/15.0;
	double slowTaper = 2.0/(ips*ips);
	double lowspeedscale = (5.0/ips);
	int count;
	double temp;
	if (overallscale == 0) {fastTaper += 1.0; slowTaper += 1.0;}
	else
	{
		iirAmount /= overallscale;
		lowspeedscale *= overallscale;
		fastTaper = 1.0 + (fastTaper / overallscale);
		slowTaper = 1.0 + (slowTaper / overallscale);
	}

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

		if (fpFlip)
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
		//do IIR highpass for leaning out

		if (inputgain != 1.0) {
			inputSampleL *= inputgain;
			inputSampleR *= inputgain;
		}

		bridgerectifierL = fabs(inputSampleL);
		if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
		bridgerectifierL = sin(bridgerectifierL);
		if (inputSampleL > 0.0) inputSampleL = bridgerectifierL;
		else inputSampleL = -bridgerectifierL;
		//preliminary gain stage using antialiasing

		bridgerectifierR = fabs(inputSampleR);
		if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
		bridgerectifierR = sin(bridgerectifierR);
		if (inputSampleR > 0.0) inputSampleR = bridgerectifierR;
		else inputSampleR = -bridgerectifierR;
		//preliminary gain stage using antialiasing

		//over to the Iron Oxide shaping code using inputsample
		if (gcount < 0 || gcount > 131) {gcount = 131;}
		count = gcount;
		//increment the counter

		dL[count+131] = dL[count] = inputSampleL;
		dR[count+131] = dR[count] = inputSampleR;

		if (fpFlip)
		{
			fastIIRAL = fastIIRAL/fastTaper;
			slowIIRAL = slowIIRAL/slowTaper;
			fastIIRAL += dL[count];
			//scale stuff down

			fastIIRAR = fastIIRAR/fastTaper;
			slowIIRAR = slowIIRAR/slowTaper;
			fastIIRAR += dR[count];
			//scale stuff down
			count += 3;

			temp = dL[count+127];
			temp += dL[count+113];
			temp += dL[count+109];
			temp += dL[count+107];
			temp += dL[count+103];
			temp += dL[count+101];
			temp += dL[count+97];
			temp += dL[count+89];
			temp += dL[count+83];
			temp /= 2;
			temp += dL[count+79];
			temp += dL[count+73];
			temp += dL[count+71];
			temp += dL[count+67];
			temp += dL[count+61];
			temp += dL[count+59];
			temp += dL[count+53];
			temp += dL[count+47];
			temp += dL[count+43];
			temp += dL[count+41];
			temp += dL[count+37];
			temp += dL[count+31];
			temp += dL[count+29];
			temp /= 2;
			temp += dL[count+23];
			temp += dL[count+19];
			temp += dL[count+17];
			temp += dL[count+13];
			temp += dL[count+11];
			temp /= 2;
			temp += dL[count+7];
			temp += dL[count+5];
			temp += dL[count+3];
			temp /= 2;
			temp += dL[count+2];
			temp += dL[count+1]; //end L
			slowIIRAL += (temp/128);

			temp = dR[count+127];
			temp += dR[count+113];
			temp += dR[count+109];
			temp += dR[count+107];
			temp += dR[count+103];
			temp += dR[count+101];
			temp += dR[count+97];
			temp += dR[count+89];
			temp += dR[count+83];
			temp /= 2;
			temp += dR[count+79];
			temp += dR[count+73];
			temp += dR[count+71];
			temp += dR[count+67];
			temp += dR[count+61];
			temp += dR[count+59];
			temp += dR[count+53];
			temp += dR[count+47];
			temp += dR[count+43];
			temp += dR[count+41];
			temp += dR[count+37];
			temp += dR[count+31];
			temp += dR[count+29];
			temp /= 2;
			temp += dR[count+23];
			temp += dR[count+19];
			temp += dR[count+17];
			temp += dR[count+13];
			temp += dR[count+11];
			temp /= 2;
			temp += dR[count+7];
			temp += dR[count+5];
			temp += dR[count+3];
			temp /= 2;
			temp += dR[count+2];
			temp += dR[count+1]; //end R
			slowIIRAR += (temp/128);

			inputSampleL = fastIIRAL - (slowIIRAL / slowTaper);
			inputSampleR = fastIIRAR - (slowIIRAR / slowTaper);
		}
		else
		{
			fastIIRBL = fastIIRBL/fastTaper;
			slowIIRBL = slowIIRBL/slowTaper;
			fastIIRBL += dL[count];
			//scale stuff down

			fastIIRBR = fastIIRBR/fastTaper;
			slowIIRBR = slowIIRBR/slowTaper;
			fastIIRBR += dR[count];
			//scale stuff down
			count += 3;

			temp = dL[count+127];
			temp += dL[count+113];
			temp += dL[count+109];
			temp += dL[count+107];
			temp += dL[count+103];
			temp += dL[count+101];
			temp += dL[count+97];
			temp += dL[count+89];
			temp += dL[count+83];
			temp /= 2;
			temp += dL[count+79];
			temp += dL[count+73];
			temp += dL[count+71];
			temp += dL[count+67];
			temp += dL[count+61];
			temp += dL[count+59];
			temp += dL[count+53];
			temp += dL[count+47];
			temp += dL[count+43];
			temp += dL[count+41];
			temp += dL[count+37];
			temp += dL[count+31];
			temp += dL[count+29];
			temp /= 2;
			temp += dL[count+23];
			temp += dL[count+19];
			temp += dL[count+17];
			temp += dL[count+13];
			temp += dL[count+11];
			temp /= 2;
			temp += dL[count+7];
			temp += dL[count+5];
			temp += dL[count+3];
			temp /= 2;
			temp += dL[count+2];
			temp += dL[count+1];
			slowIIRBL += (temp/128);

			temp = dR[count+127];
			temp += dR[count+113];
			temp += dR[count+109];
			temp += dR[count+107];
			temp += dR[count+103];
			temp += dR[count+101];
			temp += dR[count+97];
			temp += dR[count+89];
			temp += dR[count+83];
			temp /= 2;
			temp += dR[count+79];
			temp += dR[count+73];
			temp += dR[count+71];
			temp += dR[count+67];
			temp += dR[count+61];
			temp += dR[count+59];
			temp += dR[count+53];
			temp += dR[count+47];
			temp += dR[count+43];
			temp += dR[count+41];
			temp += dR[count+37];
			temp += dR[count+31];
			temp += dR[count+29];
			temp /= 2;
			temp += dR[count+23];
			temp += dR[count+19];
			temp += dR[count+17];
			temp += dR[count+13];
			temp += dR[count+11];
			temp /= 2;
			temp += dR[count+7];
			temp += dR[count+5];
			temp += dR[count+3];
			temp /= 2;
			temp += dR[count+2];
			temp += dR[count+1];
			slowIIRBR += (temp/128);

			inputSampleL = fastIIRBL - (slowIIRBL / slowTaper);
			inputSampleR = fastIIRBR - (slowIIRBR / slowTaper);
		}

		inputSampleL /= fastTaper;
		inputSampleR /= fastTaper;
		inputSampleL /= lowspeedscale;
		inputSampleR /= lowspeedscale;

		bridgerectifierL = fabs(inputSampleL);
		if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
		bridgerectifierL = sin(bridgerectifierL);
		//can use as an output limiter
		if (inputSampleL > 0.0) inputSampleL = bridgerectifierL;
		else inputSampleL = -bridgerectifierL;
		//second stage of overdrive to prevent overs and allow bloody loud extremeness

		bridgerectifierR = fabs(inputSampleR);
		if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
		bridgerectifierR = sin(bridgerectifierR);
		//can use as an output limiter
		if (inputSampleR > 0.0) inputSampleR = bridgerectifierR;
		else inputSampleR = -bridgerectifierR;
		//second stage of overdrive to prevent overs and allow bloody loud extremeness

		if (outputgain != 1.0) {
			inputSampleL *= outputgain;
			inputSampleR *= outputgain;
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

void IronOxideClassic::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double fpTemp; //this is different from singlereplacing
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	double inputgain = pow(10.0,((A*36.0)-18.0)/20.0);
	double outputgain = pow(10.0,((C*36.0)-18.0)/20.0);
	double ips = (((B*B)*(B*B)*148.5)+1.5) * 1.1;
	//slight correction to dial in convincing ips settings
	if (ips < 1 || ips > 200){ips=33.0;}
	//sanity checks are always key
	double iirAmount = ips/430.0; //for low leaning
	double bridgerectifierL;
	double bridgerectifierR;
	double fastTaper = ips/15.0;
	double slowTaper = 2.0/(ips*ips);
	double lowspeedscale = (5.0/ips);
	int count;
	double temp;
	if (overallscale == 0) {fastTaper += 1.0; slowTaper += 1.0;}
	else
	{
		iirAmount /= overallscale;
		lowspeedscale *= overallscale;
		fastTaper = 1.0 + (fastTaper / overallscale);
		slowTaper = 1.0 + (slowTaper / overallscale);
	}

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

		if (fpFlip)
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
		//do IIR highpass for leaning out

		if (inputgain != 1.0) {
			inputSampleL *= inputgain;
			inputSampleR *= inputgain;
		}

		bridgerectifierL = fabs(inputSampleL);
		if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
		bridgerectifierL = sin(bridgerectifierL);
		if (inputSampleL > 0.0) inputSampleL = bridgerectifierL;
		else inputSampleL = -bridgerectifierL;
		//preliminary gain stage using antialiasing

		bridgerectifierR = fabs(inputSampleR);
		if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
		bridgerectifierR = sin(bridgerectifierR);
		if (inputSampleR > 0.0) inputSampleR = bridgerectifierR;
		else inputSampleR = -bridgerectifierR;
		//preliminary gain stage using antialiasing

		//over to the Iron Oxide shaping code using inputsample
		if (gcount < 0 || gcount > 131) {gcount = 131;}
		count = gcount;
		//increment the counter

		dL[count+131] = dL[count] = inputSampleL;
		dR[count+131] = dR[count] = inputSampleR;

		if (fpFlip)
		{
			fastIIRAL = fastIIRAL/fastTaper;
			slowIIRAL = slowIIRAL/slowTaper;
			fastIIRAL += dL[count];
			//scale stuff down

			fastIIRAR = fastIIRAR/fastTaper;
			slowIIRAR = slowIIRAR/slowTaper;
			fastIIRAR += dR[count];
			//scale stuff down
			count += 3;

			temp = dL[count+127];
			temp += dL[count+113];
			temp += dL[count+109];
			temp += dL[count+107];
			temp += dL[count+103];
			temp += dL[count+101];
			temp += dL[count+97];
			temp += dL[count+89];
			temp += dL[count+83];
			temp /= 2;
			temp += dL[count+79];
			temp += dL[count+73];
			temp += dL[count+71];
			temp += dL[count+67];
			temp += dL[count+61];
			temp += dL[count+59];
			temp += dL[count+53];
			temp += dL[count+47];
			temp += dL[count+43];
			temp += dL[count+41];
			temp += dL[count+37];
			temp += dL[count+31];
			temp += dL[count+29];
			temp /= 2;
			temp += dL[count+23];
			temp += dL[count+19];
			temp += dL[count+17];
			temp += dL[count+13];
			temp += dL[count+11];
			temp /= 2;
			temp += dL[count+7];
			temp += dL[count+5];
			temp += dL[count+3];
			temp /= 2;
			temp += dL[count+2];
			temp += dL[count+1]; //end L
			slowIIRAL += (temp/128);

			temp = dR[count+127];
			temp += dR[count+113];
			temp += dR[count+109];
			temp += dR[count+107];
			temp += dR[count+103];
			temp += dR[count+101];
			temp += dR[count+97];
			temp += dR[count+89];
			temp += dR[count+83];
			temp /= 2;
			temp += dR[count+79];
			temp += dR[count+73];
			temp += dR[count+71];
			temp += dR[count+67];
			temp += dR[count+61];
			temp += dR[count+59];
			temp += dR[count+53];
			temp += dR[count+47];
			temp += dR[count+43];
			temp += dR[count+41];
			temp += dR[count+37];
			temp += dR[count+31];
			temp += dR[count+29];
			temp /= 2;
			temp += dR[count+23];
			temp += dR[count+19];
			temp += dR[count+17];
			temp += dR[count+13];
			temp += dR[count+11];
			temp /= 2;
			temp += dR[count+7];
			temp += dR[count+5];
			temp += dR[count+3];
			temp /= 2;
			temp += dR[count+2];
			temp += dR[count+1]; //end R
			slowIIRAR += (temp/128);

			inputSampleL = fastIIRAL - (slowIIRAL / slowTaper);
			inputSampleR = fastIIRAR - (slowIIRAR / slowTaper);
		}
		else
		{
			fastIIRBL = fastIIRBL/fastTaper;
			slowIIRBL = slowIIRBL/slowTaper;
			fastIIRBL += dL[count];
			//scale stuff down

			fastIIRBR = fastIIRBR/fastTaper;
			slowIIRBR = slowIIRBR/slowTaper;
			fastIIRBR += dR[count];
			//scale stuff down
			count += 3;

			temp = dL[count+127];
			temp += dL[count+113];
			temp += dL[count+109];
			temp += dL[count+107];
			temp += dL[count+103];
			temp += dL[count+101];
			temp += dL[count+97];
			temp += dL[count+89];
			temp += dL[count+83];
			temp /= 2;
			temp += dL[count+79];
			temp += dL[count+73];
			temp += dL[count+71];
			temp += dL[count+67];
			temp += dL[count+61];
			temp += dL[count+59];
			temp += dL[count+53];
			temp += dL[count+47];
			temp += dL[count+43];
			temp += dL[count+41];
			temp += dL[count+37];
			temp += dL[count+31];
			temp += dL[count+29];
			temp /= 2;
			temp += dL[count+23];
			temp += dL[count+19];
			temp += dL[count+17];
			temp += dL[count+13];
			temp += dL[count+11];
			temp /= 2;
			temp += dL[count+7];
			temp += dL[count+5];
			temp += dL[count+3];
			temp /= 2;
			temp += dL[count+2];
			temp += dL[count+1];
			slowIIRBL += (temp/128);

			temp = dR[count+127];
			temp += dR[count+113];
			temp += dR[count+109];
			temp += dR[count+107];
			temp += dR[count+103];
			temp += dR[count+101];
			temp += dR[count+97];
			temp += dR[count+89];
			temp += dR[count+83];
			temp /= 2;
			temp += dR[count+79];
			temp += dR[count+73];
			temp += dR[count+71];
			temp += dR[count+67];
			temp += dR[count+61];
			temp += dR[count+59];
			temp += dR[count+53];
			temp += dR[count+47];
			temp += dR[count+43];
			temp += dR[count+41];
			temp += dR[count+37];
			temp += dR[count+31];
			temp += dR[count+29];
			temp /= 2;
			temp += dR[count+23];
			temp += dR[count+19];
			temp += dR[count+17];
			temp += dR[count+13];
			temp += dR[count+11];
			temp /= 2;
			temp += dR[count+7];
			temp += dR[count+5];
			temp += dR[count+3];
			temp /= 2;
			temp += dR[count+2];
			temp += dR[count+1];
			slowIIRBR += (temp/128);

			inputSampleL = fastIIRBL - (slowIIRBL / slowTaper);
			inputSampleR = fastIIRBR - (slowIIRBR / slowTaper);
		}

		inputSampleL /= fastTaper;
		inputSampleR /= fastTaper;
		inputSampleL /= lowspeedscale;
		inputSampleR /= lowspeedscale;

		bridgerectifierL = fabs(inputSampleL);
		if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
		bridgerectifierL = sin(bridgerectifierL);
		//can use as an output limiter
		if (inputSampleL > 0.0) inputSampleL = bridgerectifierL;
		else inputSampleL = -bridgerectifierL;
		//second stage of overdrive to prevent overs and allow bloody loud extremeness

		bridgerectifierR = fabs(inputSampleR);
		if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
		bridgerectifierR = sin(bridgerectifierR);
		//can use as an output limiter
		if (inputSampleR > 0.0) inputSampleR = bridgerectifierR;
		else inputSampleR = -bridgerectifierR;
		//second stage of overdrive to prevent overs and allow bloody loud extremeness

		if (outputgain != 1.0) {
			inputSampleL *= outputgain;
			inputSampleR *= outputgain;
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
