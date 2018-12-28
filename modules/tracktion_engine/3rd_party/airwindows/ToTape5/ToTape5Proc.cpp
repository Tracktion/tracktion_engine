/* ========================================
 *  ToTape5 - ToTape5.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __ToTape5_H
#include "ToTape5.h"
#endif

void ToTape5::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double inputgain = pow(A+1.0,3);
	double outputgain = E;
	double wet = F;
	double dry = 1.0 - wet;
	double trim = 0.211324865405187117745425;
	double SoftenControl = pow(B,2);
	double tempRandy = 0.06 + (SoftenControl/10.0);
	double RollAmount = (1.0-(SoftenControl * 0.45))/overallscale;
	double HeadBumpControl = pow(C,2);
	int allpasstemp;
	int maxdelay = (int)(floor(((HeadBumpControl+0.3)*2.2)*overallscale));
	HeadBumpControl *= fabs(HeadBumpControl);
	double HeadBumpFreq = 0.044/overallscale;
	double iirAmount = 0.000001/overallscale;
	double altAmount = 1.0 - iirAmount;
	double iirHBoostAmount = 0.0001/overallscale;
	double altHBoostAmount = 1.0 - iirAmount;
	double depth = pow(D,2)*overallscale;
	double fluttertrim = 0.005/overallscale;
	double sweeptrim = (0.0006*depth)/overallscale;
	double offset;
	double tupi = 3.141592653589793238 * 2.0;
	double newrate = 0.005/overallscale;
	double oldrate = 1.0-newrate;
	double flutterrandy;
	double randy;
	double invrandy;
	int count;

	double HighsSampleL = 0.0;
	double NonHighsSampleL = 0.0;
	double HeadBumpL = 0.0;
	double SubtractL;
	double bridgerectifierL;
	double tempSampleL;
	double drySampleL;

	double HighsSampleR = 0.0;
	double NonHighsSampleR = 0.0;
	double HeadBumpR = 0.0;
	double SubtractR;
	double bridgerectifierR;
	double tempSampleR;
	double drySampleR;

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
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;


		flutterrandy = (rand()/(double)RAND_MAX);
		randy = flutterrandy * tempRandy; //for soften
		invrandy = (1.0-randy);
		randy /= 2.0;
		//we've set up so that we dial in the amount of the alt sections (in pairs) with invrandy being the source section

		//now we've got a random flutter, so we're messing with the pitch before tape effects go on
		if (gcount < 0 || gcount > 300) {gcount = 300;}
		count = gcount;
		dL[count+301] = dL[count] = inputSampleL;
		dR[count+301] = dR[count] = inputSampleR;
		gcount--;
		//we will also keep the buffer going, even when not in use

		if (depth != 0.0) {
			offset = (1.0 + sin(sweep)) * depth;
			count += (int)floor(offset);

			bridgerectifierL = (dL[count] * (1-(offset-floor(offset))));
			bridgerectifierL += (dL[count+1] * (offset-floor(offset)));
			bridgerectifierL -= ((dL[count+2] * (offset-floor(offset)))*trim);

			bridgerectifierR = (dR[count] * (1-(offset-floor(offset))));
			bridgerectifierR += (dR[count+1] * (offset-floor(offset)));
			bridgerectifierR -= ((dR[count+2] * (offset-floor(offset)))*trim);

			rateof = (nextmax * newrate) + (rateof * oldrate);
			sweep += rateof * fluttertrim;
			sweep += sweep * sweeptrim;
			if (sweep >= tupi){sweep = 0.0; nextmax = 0.02 + (flutterrandy*0.98);}
			inputSampleL = bridgerectifierL;
			inputSampleR = bridgerectifierR;
			//apply to input signal only when flutter is present, interpolate samples
		}

		if (inputgain != 1.0) {
			inputSampleL *= inputgain;
			inputSampleR *= inputgain;
		}

		if (flip < 1 || flip > 3) flip = 1;
		switch (flip)
		{
			case 1:
				iirMidRollerAL = (iirMidRollerAL * (1.0 - RollAmount)) + (inputSampleL * RollAmount);
				iirMidRollerAL = (invrandy * iirMidRollerAL) + (randy * iirMidRollerBL) + (randy * iirMidRollerCL);
				HighsSampleL = inputSampleL - iirMidRollerAL;
				NonHighsSampleL = iirMidRollerAL;

				iirHeadBumpAL += (inputSampleL * 0.05);
				iirHeadBumpAL -= (iirHeadBumpAL * iirHeadBumpAL * iirHeadBumpAL * HeadBumpFreq);
				iirHeadBumpAL = (invrandy * iirHeadBumpAL) + (randy * iirHeadBumpBL) + (randy * iirHeadBumpCL);

				iirMidRollerAR = (iirMidRollerAR * (1.0 - RollAmount)) + (inputSampleR * RollAmount);
				iirMidRollerAR = (invrandy * iirMidRollerAR) + (randy * iirMidRollerBR) + (randy * iirMidRollerCR);
				HighsSampleR = inputSampleR - iirMidRollerAR;
				NonHighsSampleR = iirMidRollerAR;

				iirHeadBumpAR += (inputSampleR * 0.05);
				iirHeadBumpAR -= (iirHeadBumpAR * iirHeadBumpAR * iirHeadBumpAR * HeadBumpFreq);
				iirHeadBumpAR = (invrandy * iirHeadBumpAR) + (randy * iirHeadBumpBR) + (randy * iirHeadBumpCR);
				break;
			case 2:
				iirMidRollerBL = (iirMidRollerBL * (1.0 - RollAmount)) + (inputSampleL * RollAmount);
				iirMidRollerBL = (randy * iirMidRollerAL) + (invrandy * iirMidRollerBL) + (randy * iirMidRollerCL);
				HighsSampleL = inputSampleL - iirMidRollerBL;
				NonHighsSampleL = iirMidRollerBL;

				iirHeadBumpBL += (inputSampleL * 0.05);
				iirHeadBumpBL -= (iirHeadBumpBL * iirHeadBumpBL * iirHeadBumpBL * HeadBumpFreq);
				iirHeadBumpBL = (randy * iirHeadBumpAL) + (invrandy * iirHeadBumpBL) + (randy * iirHeadBumpCL);

				iirMidRollerBR = (iirMidRollerBR * (1.0 - RollAmount)) + (inputSampleR * RollAmount);
				iirMidRollerBR = (randy * iirMidRollerAR) + (invrandy * iirMidRollerBR) + (randy * iirMidRollerCR);
				HighsSampleR = inputSampleR - iirMidRollerBR;
				NonHighsSampleR = iirMidRollerBR;

				iirHeadBumpBR += (inputSampleR * 0.05);
				iirHeadBumpBR -= (iirHeadBumpBR * iirHeadBumpBR * iirHeadBumpBR * HeadBumpFreq);
				iirHeadBumpBR = (randy * iirHeadBumpAR) + (invrandy * iirHeadBumpBR) + (randy * iirHeadBumpCR);
				break;
			case 3:
				iirMidRollerCL = (iirMidRollerCL * (1.0 - RollAmount)) + (inputSampleL * RollAmount);
				iirMidRollerCL = (randy * iirMidRollerAL) + (randy * iirMidRollerBL) + (invrandy * iirMidRollerCL);
				HighsSampleL = inputSampleL - iirMidRollerCL;
				NonHighsSampleL = iirMidRollerCL;

				iirHeadBumpCL += (inputSampleL * 0.05);
				iirHeadBumpCL -= (iirHeadBumpCL * iirHeadBumpCL * iirHeadBumpCL * HeadBumpFreq);
				iirHeadBumpCL = (randy * iirHeadBumpAL) + (randy * iirHeadBumpBL) + (invrandy * iirHeadBumpCL);

				iirMidRollerCR = (iirMidRollerCR * (1.0 - RollAmount)) + (inputSampleR * RollAmount);
				iirMidRollerCR = (randy * iirMidRollerAR) + (randy * iirMidRollerBR) + (invrandy * iirMidRollerCR);
				HighsSampleR = inputSampleR - iirMidRollerCR;
				NonHighsSampleR = iirMidRollerCR;

				iirHeadBumpCR += (inputSampleR * 0.05);
				iirHeadBumpCR -= (iirHeadBumpCR * iirHeadBumpCR * iirHeadBumpCR * HeadBumpFreq);
				iirHeadBumpCR = (randy * iirHeadBumpAR) + (randy * iirHeadBumpBR) + (invrandy * iirHeadBumpCR);
				break;
		}
		flip++; //increment the triplet counter

		SubtractL = HighsSampleL;
		bridgerectifierL = fabs(SubtractL)*1.57079633;
		if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
		bridgerectifierL = 1-cos(bridgerectifierL);
		if (SubtractL > 0) SubtractL = bridgerectifierL;
		if (SubtractL < 0) SubtractL = -bridgerectifierL;
		inputSampleL -= SubtractL;

		SubtractR = HighsSampleR;
		bridgerectifierR = fabs(SubtractR)*1.57079633;
		if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
		bridgerectifierR = 1-cos(bridgerectifierR);
		if (SubtractR > 0) SubtractR = bridgerectifierR;
		if (SubtractR < 0) SubtractR = -bridgerectifierR;
		inputSampleR -= SubtractR;
		//Soften works using the MidRoller stuff, defining a bright parallel channel that we apply negative Density
		//to, and then subtract from the main audio. That makes the 'highs channel subtract' hit only the loudest
		//transients, plus we are subtracting any artifacts we got from the negative Density.

		bridgerectifierL = fabs(inputSampleL);
		if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
		bridgerectifierL = sin(bridgerectifierL);
		if (inputSampleL > 0) inputSampleL = bridgerectifierL;
		if (inputSampleL < 0) inputSampleL = -bridgerectifierL;

		bridgerectifierR = fabs(inputSampleR);
		if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
		bridgerectifierR = sin(bridgerectifierR);
		if (inputSampleR > 0) inputSampleR = bridgerectifierR;
		if (inputSampleR < 0) inputSampleR = -bridgerectifierR;
		//drive section: the tape sound includes a very gentle saturation curve, which is always an attenuation.
		//we cut back on highs before hitting this, and then we're going to subtract highs a second time after.

		HeadBumpL = iirHeadBumpAL + iirHeadBumpBL + iirHeadBumpCL;
		HeadBumpR = iirHeadBumpAR + iirHeadBumpBR + iirHeadBumpCR;
		//begin PhaseNudge
		allpasstemp = hcount - 1;
		if (allpasstemp < 0 || allpasstemp > maxdelay) {allpasstemp = maxdelay;}

		HeadBumpL -= eL[allpasstemp] * fpOld;
		eL[hcount] = HeadBumpL;
		inputSampleL *= fpOld;

		HeadBumpR -= eR[allpasstemp] * fpOld;
		eR[hcount] = HeadBumpR;
		inputSampleR *= fpOld;

		hcount--; if (hcount < 0 || hcount > maxdelay) {hcount = maxdelay;}
		HeadBumpL += (eL[hcount]);
		HeadBumpR += (eR[hcount]);
		//end PhaseNudge on head bump in lieu of delay.
		SubtractL -= (HeadBumpL * (HeadBumpControl+iirMinHeadBumpL));
		SubtractR -= (HeadBumpR * (HeadBumpControl+iirMinHeadBumpR));
		//makes a second soften and a single head bump after saturation.
		//we are going to retain this, and then feed it into the highpass filter. That way, we can skip a subtract.
		//Head Bump retains a trace which is roughly as large as what the highpass will do.

		tempSampleL = inputSampleL;
		tempSampleR = inputSampleR;

		iirMinHeadBumpL = (iirMinHeadBumpL * altHBoostAmount) + (fabs(inputSampleL) * iirHBoostAmount);
		if (iirMinHeadBumpL > 0.01) iirMinHeadBumpL = 0.01;

		iirMinHeadBumpR = (iirMinHeadBumpR * altHBoostAmount) + (fabs(inputSampleR) * iirHBoostAmount);
		if (iirMinHeadBumpR > 0.01) iirMinHeadBumpR = 0.01;
		//we want this one rectified so that it's a relatively steady positive value. Boosts can cause it to be
		//greater than 1 so we clamp it in that case.

		iirSampleAL = (iirSampleAL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleAL; SubtractL += iirSampleAL;
		iirSampleBL = (iirSampleBL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleBL; SubtractL += iirSampleBL;
		iirSampleCL = (iirSampleCL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleCL; SubtractL += iirSampleCL;
		iirSampleDL = (iirSampleDL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleDL; SubtractL += iirSampleDL;
		iirSampleEL = (iirSampleEL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleEL; SubtractL += iirSampleEL;
		iirSampleFL = (iirSampleFL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleFL; SubtractL += iirSampleFL;
		iirSampleGL = (iirSampleGL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleGL; SubtractL += iirSampleGL;
		iirSampleHL = (iirSampleHL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleHL; SubtractL += iirSampleHL;
		iirSampleIL = (iirSampleIL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleIL; SubtractL += iirSampleIL;
		iirSampleJL = (iirSampleJL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleJL; SubtractL += iirSampleJL;
		iirSampleKL = (iirSampleKL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleKL; SubtractL += iirSampleKL;
		iirSampleLL = (iirSampleLL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleLL; SubtractL += iirSampleLL;
		iirSampleML = (iirSampleML * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleML; SubtractL += iirSampleML;
		iirSampleNL = (iirSampleNL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleNL; SubtractL += iirSampleNL;
		iirSampleOL = (iirSampleOL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleOL; SubtractL += iirSampleOL;
		iirSamplePL = (iirSamplePL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSamplePL; SubtractL += iirSamplePL;
		iirSampleQL = (iirSampleQL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleQL; SubtractL += iirSampleQL;
		iirSampleRL = (iirSampleRL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleRL; SubtractL += iirSampleRL;
		iirSampleSL = (iirSampleSL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleSL; SubtractL += iirSampleSL;
		iirSampleTL = (iirSampleTL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleTL; SubtractL += iirSampleTL;
		iirSampleUL = (iirSampleUL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleUL; SubtractL += iirSampleUL;
		iirSampleVL = (iirSampleVL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleVL; SubtractL += iirSampleVL;
		iirSampleWL = (iirSampleWL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleWL; SubtractL += iirSampleWL;
		iirSampleXL = (iirSampleXL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleXL; SubtractL += iirSampleXL;
		iirSampleYL = (iirSampleYL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleYL; SubtractL += iirSampleYL;
		iirSampleZL = (iirSampleZL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleZL; SubtractL += iirSampleZL;

		iirSampleAR = (iirSampleAR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleAR; SubtractR += iirSampleAR;
		iirSampleBR = (iirSampleBR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleBR; SubtractR += iirSampleBR;
		iirSampleCR = (iirSampleCR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleCR; SubtractR += iirSampleCR;
		iirSampleDR = (iirSampleDR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleDR; SubtractR += iirSampleDR;
		iirSampleER = (iirSampleER * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleER; SubtractR += iirSampleER;
		iirSampleFR = (iirSampleFR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleFR; SubtractR += iirSampleFR;
		iirSampleGR = (iirSampleGR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleGR; SubtractR += iirSampleGR;
		iirSampleHR = (iirSampleHR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleHR; SubtractR += iirSampleHR;
		iirSampleIR = (iirSampleIR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleIR; SubtractR += iirSampleIR;
		iirSampleJR = (iirSampleJR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleJR; SubtractR += iirSampleJR;
		iirSampleKR = (iirSampleKR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleKR; SubtractR += iirSampleKR;
		iirSampleLR = (iirSampleLR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleLR; SubtractR += iirSampleLR;
		iirSampleMR = (iirSampleMR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleMR; SubtractR += iirSampleMR;
		iirSampleNR = (iirSampleNR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleNR; SubtractR += iirSampleNR;
		iirSampleOR = (iirSampleOR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleOR; SubtractR += iirSampleOR;
		iirSamplePR = (iirSamplePR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSamplePR; SubtractR += iirSamplePR;
		iirSampleQR = (iirSampleQR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleQR; SubtractR += iirSampleQR;
		iirSampleRR = (iirSampleRR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleRR; SubtractR += iirSampleRR;
		iirSampleSR = (iirSampleSR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleSR; SubtractR += iirSampleSR;
		iirSampleTR = (iirSampleTR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleTR; SubtractR += iirSampleTR;
		iirSampleUR = (iirSampleUR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleUR; SubtractR += iirSampleUR;
		iirSampleVR = (iirSampleVR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleVR; SubtractR += iirSampleVR;
		iirSampleWR = (iirSampleWR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleWR; SubtractR += iirSampleWR;
		iirSampleXR = (iirSampleXR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleXR; SubtractR += iirSampleXR;
		iirSampleYR = (iirSampleYR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleYR; SubtractR += iirSampleYR;
		iirSampleZR = (iirSampleZR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleZR; SubtractR += iirSampleZR;
		//do the IIR on a dummy sample, and store up the correction in a variable at the same scale as the very low level
		//numbers being used. Don't keep doing it against the possibly high level signal number.
		//This has been known to add a resonant quality to the cutoff, which we're using on purpose.

		inputSampleL -= SubtractL;
		inputSampleR -= SubtractR;
		//apply stored up tiny corrections.

		if (outputgain != 1.0) {
			inputSampleL *= outputgain;
			inputSampleR *= outputgain;
		}

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

void ToTape5::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double inputgain = pow(A+1.0,3);
	double outputgain = E;
	double wet = F;
	double dry = 1.0 - wet;
	double trim = 0.211324865405187117745425;
	double SoftenControl = pow(B,2);
	double tempRandy = 0.06 + (SoftenControl/10.0);
	double RollAmount = (1.0-(SoftenControl * 0.45))/overallscale;
	double HeadBumpControl = pow(C,2);
	int allpasstemp;
	int maxdelay = (int)(floor(((HeadBumpControl+0.3)*2.2)*overallscale));
	HeadBumpControl *= fabs(HeadBumpControl);
	double HeadBumpFreq = 0.044/overallscale;
	double iirAmount = 0.000001/overallscale;
	double altAmount = 1.0 - iirAmount;
	double iirHBoostAmount = 0.0001/overallscale;
	double altHBoostAmount = 1.0 - iirAmount;
	double depth = pow(D,2)*overallscale;
	double fluttertrim = 0.005/overallscale;
	double sweeptrim = (0.0006*depth)/overallscale;
	double offset;
	double tupi = 3.141592653589793238 * 2.0;
	double newrate = 0.005/overallscale;
	double oldrate = 1.0-newrate;
	double flutterrandy;
	double randy;
	double invrandy;
	int count;

	double HighsSampleL = 0.0;
	double NonHighsSampleL = 0.0;
	double HeadBumpL = 0.0;
	double SubtractL;
	double bridgerectifierL;
	double tempSampleL;
	double drySampleL;

	double HighsSampleR = 0.0;
	double NonHighsSampleR = 0.0;
	double HeadBumpR = 0.0;
	double SubtractR;
	double bridgerectifierR;
	double tempSampleR;
	double drySampleR;

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
		drySampleL = inputSampleL;
		drySampleR = inputSampleR;


		flutterrandy = (rand()/(double)RAND_MAX);
		randy = flutterrandy * tempRandy; //for soften
		invrandy = (1.0-randy);
		randy /= 2.0;
		//we've set up so that we dial in the amount of the alt sections (in pairs) with invrandy being the source section

		//now we've got a random flutter, so we're messing with the pitch before tape effects go on
		if (gcount < 0 || gcount > 300) {gcount = 300;}
		count = gcount;
		dL[count+301] = dL[count] = inputSampleL;
		dR[count+301] = dR[count] = inputSampleR;
		gcount--;
		//we will also keep the buffer going, even when not in use

		if (depth != 0.0) {
			offset = (1.0 + sin(sweep)) * depth;
			count += (int)floor(offset);

			bridgerectifierL = (dL[count] * (1-(offset-floor(offset))));
			bridgerectifierL += (dL[count+1] * (offset-floor(offset)));
			bridgerectifierL -= ((dL[count+2] * (offset-floor(offset)))*trim);

			bridgerectifierR = (dR[count] * (1-(offset-floor(offset))));
			bridgerectifierR += (dR[count+1] * (offset-floor(offset)));
			bridgerectifierR -= ((dR[count+2] * (offset-floor(offset)))*trim);

			rateof = (nextmax * newrate) + (rateof * oldrate);
			sweep += rateof * fluttertrim;
			sweep += sweep * sweeptrim;
			if (sweep >= tupi){sweep = 0.0; nextmax = 0.02 + (flutterrandy*0.98);}
			inputSampleL = bridgerectifierL;
			inputSampleR = bridgerectifierR;
			//apply to input signal only when flutter is present, interpolate samples
		}

		if (inputgain != 1.0) {
			inputSampleL *= inputgain;
			inputSampleR *= inputgain;
		}

		if (flip < 1 || flip > 3) flip = 1;
		switch (flip)
		{
			case 1:
				iirMidRollerAL = (iirMidRollerAL * (1.0 - RollAmount)) + (inputSampleL * RollAmount);
				iirMidRollerAL = (invrandy * iirMidRollerAL) + (randy * iirMidRollerBL) + (randy * iirMidRollerCL);
				HighsSampleL = inputSampleL - iirMidRollerAL;
				NonHighsSampleL = iirMidRollerAL;

				iirHeadBumpAL += (inputSampleL * 0.05);
				iirHeadBumpAL -= (iirHeadBumpAL * iirHeadBumpAL * iirHeadBumpAL * HeadBumpFreq);
				iirHeadBumpAL = (invrandy * iirHeadBumpAL) + (randy * iirHeadBumpBL) + (randy * iirHeadBumpCL);

				iirMidRollerAR = (iirMidRollerAR * (1.0 - RollAmount)) + (inputSampleR * RollAmount);
				iirMidRollerAR = (invrandy * iirMidRollerAR) + (randy * iirMidRollerBR) + (randy * iirMidRollerCR);
				HighsSampleR = inputSampleR - iirMidRollerAR;
				NonHighsSampleR = iirMidRollerAR;

				iirHeadBumpAR += (inputSampleR * 0.05);
				iirHeadBumpAR -= (iirHeadBumpAR * iirHeadBumpAR * iirHeadBumpAR * HeadBumpFreq);
				iirHeadBumpAR = (invrandy * iirHeadBumpAR) + (randy * iirHeadBumpBR) + (randy * iirHeadBumpCR);
				break;
			case 2:
				iirMidRollerBL = (iirMidRollerBL * (1.0 - RollAmount)) + (inputSampleL * RollAmount);
				iirMidRollerBL = (randy * iirMidRollerAL) + (invrandy * iirMidRollerBL) + (randy * iirMidRollerCL);
				HighsSampleL = inputSampleL - iirMidRollerBL;
				NonHighsSampleL = iirMidRollerBL;

				iirHeadBumpBL += (inputSampleL * 0.05);
				iirHeadBumpBL -= (iirHeadBumpBL * iirHeadBumpBL * iirHeadBumpBL * HeadBumpFreq);
				iirHeadBumpBL = (randy * iirHeadBumpAL) + (invrandy * iirHeadBumpBL) + (randy * iirHeadBumpCL);

				iirMidRollerBR = (iirMidRollerBR * (1.0 - RollAmount)) + (inputSampleR * RollAmount);
				iirMidRollerBR = (randy * iirMidRollerAR) + (invrandy * iirMidRollerBR) + (randy * iirMidRollerCR);
				HighsSampleR = inputSampleR - iirMidRollerBR;
				NonHighsSampleR = iirMidRollerBR;

				iirHeadBumpBR += (inputSampleR * 0.05);
				iirHeadBumpBR -= (iirHeadBumpBR * iirHeadBumpBR * iirHeadBumpBR * HeadBumpFreq);
				iirHeadBumpBR = (randy * iirHeadBumpAR) + (invrandy * iirHeadBumpBR) + (randy * iirHeadBumpCR);
				break;
			case 3:
				iirMidRollerCL = (iirMidRollerCL * (1.0 - RollAmount)) + (inputSampleL * RollAmount);
				iirMidRollerCL = (randy * iirMidRollerAL) + (randy * iirMidRollerBL) + (invrandy * iirMidRollerCL);
				HighsSampleL = inputSampleL - iirMidRollerCL;
				NonHighsSampleL = iirMidRollerCL;

				iirHeadBumpCL += (inputSampleL * 0.05);
				iirHeadBumpCL -= (iirHeadBumpCL * iirHeadBumpCL * iirHeadBumpCL * HeadBumpFreq);
				iirHeadBumpCL = (randy * iirHeadBumpAL) + (randy * iirHeadBumpBL) + (invrandy * iirHeadBumpCL);

				iirMidRollerCR = (iirMidRollerCR * (1.0 - RollAmount)) + (inputSampleR * RollAmount);
				iirMidRollerCR = (randy * iirMidRollerAR) + (randy * iirMidRollerBR) + (invrandy * iirMidRollerCR);
				HighsSampleR = inputSampleR - iirMidRollerCR;
				NonHighsSampleR = iirMidRollerCR;

				iirHeadBumpCR += (inputSampleR * 0.05);
				iirHeadBumpCR -= (iirHeadBumpCR * iirHeadBumpCR * iirHeadBumpCR * HeadBumpFreq);
				iirHeadBumpCR = (randy * iirHeadBumpAR) + (randy * iirHeadBumpBR) + (invrandy * iirHeadBumpCR);
				break;
		}
		flip++; //increment the triplet counter

		SubtractL = HighsSampleL;
		bridgerectifierL = fabs(SubtractL)*1.57079633;
		if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
		bridgerectifierL = 1-cos(bridgerectifierL);
		if (SubtractL > 0) SubtractL = bridgerectifierL;
		if (SubtractL < 0) SubtractL = -bridgerectifierL;
		inputSampleL -= SubtractL;

		SubtractR = HighsSampleR;
		bridgerectifierR = fabs(SubtractR)*1.57079633;
		if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
		bridgerectifierR = 1-cos(bridgerectifierR);
		if (SubtractR > 0) SubtractR = bridgerectifierR;
		if (SubtractR < 0) SubtractR = -bridgerectifierR;
		inputSampleR -= SubtractR;
		//Soften works using the MidRoller stuff, defining a bright parallel channel that we apply negative Density
		//to, and then subtract from the main audio. That makes the 'highs channel subtract' hit only the loudest
		//transients, plus we are subtracting any artifacts we got from the negative Density.

		bridgerectifierL = fabs(inputSampleL);
		if (bridgerectifierL > 1.57079633) bridgerectifierL = 1.57079633;
		bridgerectifierL = sin(bridgerectifierL);
		if (inputSampleL > 0) inputSampleL = bridgerectifierL;
		if (inputSampleL < 0) inputSampleL = -bridgerectifierL;

		bridgerectifierR = fabs(inputSampleR);
		if (bridgerectifierR > 1.57079633) bridgerectifierR = 1.57079633;
		bridgerectifierR = sin(bridgerectifierR);
		if (inputSampleR > 0) inputSampleR = bridgerectifierR;
		if (inputSampleR < 0) inputSampleR = -bridgerectifierR;
		//drive section: the tape sound includes a very gentle saturation curve, which is always an attenuation.
		//we cut back on highs before hitting this, and then we're going to subtract highs a second time after.

		HeadBumpL = iirHeadBumpAL + iirHeadBumpBL + iirHeadBumpCL;
		HeadBumpR = iirHeadBumpAR + iirHeadBumpBR + iirHeadBumpCR;
		//begin PhaseNudge
		allpasstemp = hcount - 1;
		if (allpasstemp < 0 || allpasstemp > maxdelay) {allpasstemp = maxdelay;}

		HeadBumpL -= eL[allpasstemp] * fpOld;
		eL[hcount] = HeadBumpL;
		inputSampleL *= fpOld;

		HeadBumpR -= eR[allpasstemp] * fpOld;
		eR[hcount] = HeadBumpR;
		inputSampleR *= fpOld;

		hcount--; if (hcount < 0 || hcount > maxdelay) {hcount = maxdelay;}
		HeadBumpL += (eL[hcount]);
		HeadBumpR += (eR[hcount]);
		//end PhaseNudge on head bump in lieu of delay.
		SubtractL -= (HeadBumpL * (HeadBumpControl+iirMinHeadBumpL));
		SubtractR -= (HeadBumpR * (HeadBumpControl+iirMinHeadBumpR));
		//makes a second soften and a single head bump after saturation.
		//we are going to retain this, and then feed it into the highpass filter. That way, we can skip a subtract.
		//Head Bump retains a trace which is roughly as large as what the highpass will do.

		tempSampleL = inputSampleL;
		tempSampleR = inputSampleR;

		iirMinHeadBumpL = (iirMinHeadBumpL * altHBoostAmount) + (fabs(inputSampleL) * iirHBoostAmount);
		if (iirMinHeadBumpL > 0.01) iirMinHeadBumpL = 0.01;

		iirMinHeadBumpR = (iirMinHeadBumpR * altHBoostAmount) + (fabs(inputSampleR) * iirHBoostAmount);
		if (iirMinHeadBumpR > 0.01) iirMinHeadBumpR = 0.01;
		//we want this one rectified so that it's a relatively steady positive value. Boosts can cause it to be
		//greater than 1 so we clamp it in that case.

		iirSampleAL = (iirSampleAL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleAL; SubtractL += iirSampleAL;
		iirSampleBL = (iirSampleBL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleBL; SubtractL += iirSampleBL;
		iirSampleCL = (iirSampleCL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleCL; SubtractL += iirSampleCL;
		iirSampleDL = (iirSampleDL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleDL; SubtractL += iirSampleDL;
		iirSampleEL = (iirSampleEL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleEL; SubtractL += iirSampleEL;
		iirSampleFL = (iirSampleFL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleFL; SubtractL += iirSampleFL;
		iirSampleGL = (iirSampleGL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleGL; SubtractL += iirSampleGL;
		iirSampleHL = (iirSampleHL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleHL; SubtractL += iirSampleHL;
		iirSampleIL = (iirSampleIL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleIL; SubtractL += iirSampleIL;
		iirSampleJL = (iirSampleJL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleJL; SubtractL += iirSampleJL;
		iirSampleKL = (iirSampleKL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleKL; SubtractL += iirSampleKL;
		iirSampleLL = (iirSampleLL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleLL; SubtractL += iirSampleLL;
		iirSampleML = (iirSampleML * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleML; SubtractL += iirSampleML;
		iirSampleNL = (iirSampleNL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleNL; SubtractL += iirSampleNL;
		iirSampleOL = (iirSampleOL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleOL; SubtractL += iirSampleOL;
		iirSamplePL = (iirSamplePL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSamplePL; SubtractL += iirSamplePL;
		iirSampleQL = (iirSampleQL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleQL; SubtractL += iirSampleQL;
		iirSampleRL = (iirSampleRL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleRL; SubtractL += iirSampleRL;
		iirSampleSL = (iirSampleSL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleSL; SubtractL += iirSampleSL;
		iirSampleTL = (iirSampleTL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleTL; SubtractL += iirSampleTL;
		iirSampleUL = (iirSampleUL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleUL; SubtractL += iirSampleUL;
		iirSampleVL = (iirSampleVL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleVL; SubtractL += iirSampleVL;
		iirSampleWL = (iirSampleWL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleWL; SubtractL += iirSampleWL;
		iirSampleXL = (iirSampleXL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleXL; SubtractL += iirSampleXL;
		iirSampleYL = (iirSampleYL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleYL; SubtractL += iirSampleYL;
		iirSampleZL = (iirSampleZL * altAmount) + (tempSampleL * iirAmount); tempSampleL -= iirSampleZL; SubtractL += iirSampleZL;

		iirSampleAR = (iirSampleAR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleAR; SubtractR += iirSampleAR;
		iirSampleBR = (iirSampleBR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleBR; SubtractR += iirSampleBR;
		iirSampleCR = (iirSampleCR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleCR; SubtractR += iirSampleCR;
		iirSampleDR = (iirSampleDR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleDR; SubtractR += iirSampleDR;
		iirSampleER = (iirSampleER * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleER; SubtractR += iirSampleER;
		iirSampleFR = (iirSampleFR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleFR; SubtractR += iirSampleFR;
		iirSampleGR = (iirSampleGR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleGR; SubtractR += iirSampleGR;
		iirSampleHR = (iirSampleHR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleHR; SubtractR += iirSampleHR;
		iirSampleIR = (iirSampleIR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleIR; SubtractR += iirSampleIR;
		iirSampleJR = (iirSampleJR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleJR; SubtractR += iirSampleJR;
		iirSampleKR = (iirSampleKR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleKR; SubtractR += iirSampleKR;
		iirSampleLR = (iirSampleLR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleLR; SubtractR += iirSampleLR;
		iirSampleMR = (iirSampleMR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleMR; SubtractR += iirSampleMR;
		iirSampleNR = (iirSampleNR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleNR; SubtractR += iirSampleNR;
		iirSampleOR = (iirSampleOR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleOR; SubtractR += iirSampleOR;
		iirSamplePR = (iirSamplePR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSamplePR; SubtractR += iirSamplePR;
		iirSampleQR = (iirSampleQR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleQR; SubtractR += iirSampleQR;
		iirSampleRR = (iirSampleRR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleRR; SubtractR += iirSampleRR;
		iirSampleSR = (iirSampleSR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleSR; SubtractR += iirSampleSR;
		iirSampleTR = (iirSampleTR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleTR; SubtractR += iirSampleTR;
		iirSampleUR = (iirSampleUR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleUR; SubtractR += iirSampleUR;
		iirSampleVR = (iirSampleVR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleVR; SubtractR += iirSampleVR;
		iirSampleWR = (iirSampleWR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleWR; SubtractR += iirSampleWR;
		iirSampleXR = (iirSampleXR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleXR; SubtractR += iirSampleXR;
		iirSampleYR = (iirSampleYR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleYR; SubtractR += iirSampleYR;
		iirSampleZR = (iirSampleZR * altAmount) + (tempSampleR * iirAmount); tempSampleR -= iirSampleZR; SubtractR += iirSampleZR;
		//do the IIR on a dummy sample, and store up the correction in a variable at the same scale as the very low level
		//numbers being used. Don't keep doing it against the possibly high level signal number.
		//This has been known to add a resonant quality to the cutoff, which we're using on purpose.

		inputSampleL -= SubtractL;
		inputSampleR -= SubtractR;
		//apply stored up tiny corrections.

		if (outputgain != 1.0) {
			inputSampleL *= outputgain;
			inputSampleR *= outputgain;
		}

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
