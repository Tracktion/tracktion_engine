/* ========================================
 *  Console5DarkCh - Console5DarkCh.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Console5DarkCh_H
#include "Console5DarkCh.h"
#endif

void Console5DarkCh::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
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

	double inputgain = A;
	double differenceL;
	double differenceR;
	double nearZeroL;
	double nearZeroR;
	double servoTrim = 0.0000001 / overallscale;
	double bassTrim = 0.005 / overallscale;
	long double inputSampleL;
	long double inputSampleR;

	if (settingchase != inputgain) {
		chasespeed *= 2.0;
		settingchase = inputgain;
	}
	if (chasespeed > 2500.0) chasespeed = 2500.0;
	if (gainchase < 0.0) gainchase = inputgain;

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

		chasespeed *= 0.9999;
		chasespeed -= 0.01;
		if (chasespeed < 350.0) chasespeed = 350.0;
		//we have our chase speed compensated for recent fader activity

		gainchase = (((gainchase*chasespeed)+inputgain)/(chasespeed+1.0));
		//gainchase is chasing the target, as a simple multiply gain factor

		if (1.0 != gainchase) {
			inputSampleL *= gainchase;
			inputSampleR *= gainchase;
		}
		//done with trim control

		differenceL = lastSampleChannelL - inputSampleL;
		lastSampleChannelL = inputSampleL;
		differenceR = lastSampleChannelR - inputSampleR;
		lastSampleChannelR = inputSampleR;
		//derive slew part off direct sample measurement + from last time

		if (differenceL > 1.0) differenceL = 1.0;
		if (differenceL < -1.0) differenceL = -1.0;
		if (differenceR > 1.0) differenceR = 1.0;
		if (differenceR < -1.0) differenceR = -1.0;
		//clamp the slew correction to prevent invalid math results

		differenceL = lastFXChannelL + sin(differenceL);
		differenceR = lastFXChannelR + sin(differenceR);
		//we're about to use this twice and then not use difference again, so we'll reuse it
		//enhance slew is arcsin(): cutting it back is sin()

		iirCorrectL += inputSampleL - differenceL;
		inputSampleL = differenceL;
		iirCorrectR += inputSampleR - differenceR;
		inputSampleR = differenceR;
		//apply the slew to stored value: can develop DC offsets.
		//store the change we made so we can dial it back

		lastFXChannelL = inputSampleL;
		lastFXChannelR = inputSampleR;
		if (lastFXChannelL > 1.0) lastFXChannelL = 1.0;
		if (lastFXChannelL < -1.0) lastFXChannelL = -1.0;
		if (lastFXChannelR > 1.0) lastFXChannelR = 1.0;
		if (lastFXChannelR < -1.0) lastFXChannelR = -1.0;
		//store current sample as new base for next offset

		nearZeroL = pow(fabs(fabs(lastFXChannelL)-1.0), 2);
		nearZeroR = pow(fabs(fabs(lastFXChannelR)-1.0), 2);
		//if the sample is very near zero this number is higher.
		if (iirCorrectL > 0) iirCorrectL -= servoTrim;
		if (iirCorrectL < 0) iirCorrectL += servoTrim;
		if (iirCorrectR > 0) iirCorrectR -= servoTrim;
		if (iirCorrectR < 0) iirCorrectR += servoTrim;
		//cut back the servo by which we're pulling back the DC
		lastFXChannelL += (iirCorrectL * 0.0000005);
		lastFXChannelR += (iirCorrectR * 0.0000005);
		//apply the servo to the stored value, pulling back the DC
		lastFXChannelL *= (1.0 - (nearZeroL * bassTrim));
		lastFXChannelR *= (1.0 - (nearZeroR * bassTrim));
		//this cuts back the DC offset directly, relative to how near zero we are

		if (inputSampleL > 1.57079633) inputSampleL= 1.57079633;
		if (inputSampleL < -1.57079633) inputSampleL = -1.57079633;
		inputSampleL = sin(inputSampleL);
		//amplitude aspect

		if (inputSampleR > 1.57079633) inputSampleR = 1.57079633;
		if (inputSampleR < -1.57079633) inputSampleR = -1.57079633;
		inputSampleR = sin(inputSampleR);
		//amplitude aspect

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

void Console5DarkCh::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
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

	double inputgain = A;
	double differenceL;
	double differenceR;
	double nearZeroL;
	double nearZeroR;
	double servoTrim = 0.0000001 / overallscale;
	double bassTrim = 0.005 / overallscale;
	long double inputSampleL;
	long double inputSampleR;

	if (settingchase != inputgain) {
		chasespeed *= 2.0;
		settingchase = inputgain;
	}
	if (chasespeed > 2500.0) chasespeed = 2500.0;
	if (gainchase < 0.0) gainchase = inputgain;

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

		chasespeed *= 0.9999;
		chasespeed -= 0.01;
		if (chasespeed < 350.0) chasespeed = 350.0;
		//we have our chase speed compensated for recent fader activity

		gainchase = (((gainchase*chasespeed)+inputgain)/(chasespeed+1.0));
		//gainchase is chasing the target, as a simple multiply gain factor

		if (1.0 != gainchase) {
			inputSampleL *= gainchase;
			inputSampleR *= gainchase;
		}
		//done with trim control

		differenceL = lastSampleChannelL - inputSampleL;
		lastSampleChannelL = inputSampleL;
		differenceR = lastSampleChannelR - inputSampleR;
		lastSampleChannelR = inputSampleR;
		//derive slew part off direct sample measurement + from last time

		if (differenceL > 1.0) differenceL = 1.0;
		if (differenceL < -1.0) differenceL = -1.0;
		if (differenceR > 1.0) differenceR = 1.0;
		if (differenceR < -1.0) differenceR = -1.0;
		//clamp the slew correction to prevent invalid math results

		differenceL = lastFXChannelL + sin(differenceL);
		differenceR = lastFXChannelR + sin(differenceR);
		//we're about to use this twice and then not use difference again, so we'll reuse it
		//enhance slew is arcsin(): cutting it back is sin()

		iirCorrectL += inputSampleL - differenceL;
		inputSampleL = differenceL;
		iirCorrectR += inputSampleR - differenceR;
		inputSampleR = differenceR;
		//apply the slew to stored value: can develop DC offsets.
		//store the change we made so we can dial it back

		lastFXChannelL = inputSampleL;
		lastFXChannelR = inputSampleR;
		if (lastFXChannelL > 1.0) lastFXChannelL = 1.0;
		if (lastFXChannelL < -1.0) lastFXChannelL = -1.0;
		if (lastFXChannelR > 1.0) lastFXChannelR = 1.0;
		if (lastFXChannelR < -1.0) lastFXChannelR = -1.0;
		//store current sample as new base for next offset

		nearZeroL = pow(fabs(fabs(lastFXChannelL)-1.0), 2);
		nearZeroR = pow(fabs(fabs(lastFXChannelR)-1.0), 2);
		//if the sample is very near zero this number is higher.
		if (iirCorrectL > 0) iirCorrectL -= servoTrim;
		if (iirCorrectL < 0) iirCorrectL += servoTrim;
		if (iirCorrectR > 0) iirCorrectR -= servoTrim;
		if (iirCorrectR < 0) iirCorrectR += servoTrim;
		//cut back the servo by which we're pulling back the DC
		lastFXChannelL += (iirCorrectL * 0.0000005);
		lastFXChannelR += (iirCorrectR * 0.0000005);
		//apply the servo to the stored value, pulling back the DC
		lastFXChannelL *= (1.0 - (nearZeroL * bassTrim));
		lastFXChannelR *= (1.0 - (nearZeroR * bassTrim));
		//this cuts back the DC offset directly, relative to how near zero we are

		if (inputSampleL > 1.57079633) inputSampleL= 1.57079633;
		if (inputSampleL < -1.57079633) inputSampleL = -1.57079633;
		inputSampleL = sin(inputSampleL);
		//amplitude aspect

		if (inputSampleR > 1.57079633) inputSampleR = 1.57079633;
		if (inputSampleR < -1.57079633) inputSampleR = -1.57079633;
		inputSampleR = sin(inputSampleR);
		//amplitude aspect

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
