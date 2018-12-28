/* ========================================
 *  EQ - EQ.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __EQ_H
#include "EQ.h"
#endif

void EQ::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	double compscale = overallscale;
	overallscale = getSampleRate();
	compscale = compscale * overallscale;
	//compscale is the one that's 1 or something like 2.2 for 96K rates
	float fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	long double inputSampleL;
	long double inputSampleR;

	double highSampleL = 0.0;
	double midSampleL = 0.0;
	double bassSampleL = 0.0;

	double highSampleR = 0.0;
	double midSampleR = 0.0;
	double bassSampleR = 0.0;

	double densityA = (A*12.0)-6.0;
	double densityB = (B*12.0)-6.0;
	double densityC = (C*12.0)-6.0;
	bool engageEQ = true;
	if ( (0.0 == densityA) && (0.0 == densityB) && (0.0 == densityC) ) engageEQ = false;

	densityA = pow(10.0,densityA/20.0)-1.0;
	densityB = pow(10.0,densityB/20.0)-1.0;
	densityC = pow(10.0,densityC/20.0)-1.0;
	//convert to 0 to X multiplier with 1.0 being O db
	//minus one gives nearly -1 to ? (should top out at 1)
	//calibrate so that X db roughly equals X db with maximum topping out at 1 internally

	double tripletIntensity = -densityA;

	double iirAmountC = (((D*D*15.0)+1.0)*0.0188) + 0.7;
	if (iirAmountC > 1.0) iirAmountC = 1.0;
	bool engageLowpass = false;
	if (((D*D*15.0)+1.0) < 15.99) engageLowpass = true;

	double iirAmountA = (((E*E*15.0)+1.0)*1000)/overallscale;
	double iirAmountB = (((F*F*1570.0)+30.0)*10)/overallscale;
	double iirAmountD = (((G*G*1570.0)+30.0)*1.0)/overallscale;
	bool engageHighpass = false;
	if (((G*G*1570.0)+30.0) > 30.01) engageHighpass = true;
	//bypass the highpass and lowpass if set to extremes
	double bridgerectifier;
	double outA = fabs(densityA);
	double outB = fabs(densityB);
	double outC = fabs(densityC);
	//end EQ
	double outputgain = pow(10.0,((H*36.0)-18.0)/20.0);

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

		last2SampleL = lastSampleL;
		lastSampleL = inputSampleL;

		last2SampleR = lastSampleR;
		lastSampleR = inputSampleR;

		flip = !flip;
		flipthree++;
		if (flipthree < 1 || flipthree > 3) flipthree = 1;
		//counters

		//begin highpass
		if (engageHighpass)
		{
			if (flip)
			{
				highpassSampleLAA = (highpassSampleLAA * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLAA;
				highpassSampleLBA = (highpassSampleLBA * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLBA;
				highpassSampleLCA = (highpassSampleLCA * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLCA;
				highpassSampleLDA = (highpassSampleLDA * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLDA;
			}
			else
			{
				highpassSampleLAB = (highpassSampleLAB * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLAB;
				highpassSampleLBB = (highpassSampleLBB * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLBB;
				highpassSampleLCB = (highpassSampleLCB * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLCB;
				highpassSampleLDB = (highpassSampleLDB * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLDB;
			}
			highpassSampleLE = (highpassSampleLE * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
			inputSampleL -= highpassSampleLE;
			highpassSampleLF = (highpassSampleLF * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
			inputSampleL -= highpassSampleLF;

			if (flip)
			{
				highpassSampleRAA = (highpassSampleRAA * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRAA;
				highpassSampleRBA = (highpassSampleRBA * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRBA;
				highpassSampleRCA = (highpassSampleRCA * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRCA;
				highpassSampleRDA = (highpassSampleRDA * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRDA;
			}
			else
			{
				highpassSampleRAB = (highpassSampleRAB * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRAB;
				highpassSampleRBB = (highpassSampleRBB * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRBB;
				highpassSampleRCB = (highpassSampleRCB * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRCB;
				highpassSampleRDB = (highpassSampleRDB * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRDB;
			}
			highpassSampleRE = (highpassSampleRE * (1 - iirAmountD)) + (inputSampleR * iirAmountD);
			inputSampleR -= highpassSampleRE;
			highpassSampleRF = (highpassSampleRF * (1 - iirAmountD)) + (inputSampleR * iirAmountD);
			inputSampleR -= highpassSampleRF;

		}
		//end highpass

		//begin EQ
		if (engageEQ)
		{
			switch (flipthree)
			{
				case 1:
					tripletFactorL = last2SampleL - inputSampleL;
					tripletLA += tripletFactorL;
					tripletLC -= tripletFactorL;
					tripletFactorL = tripletLA * tripletIntensity;
					iirHighSampleLC = (iirHighSampleLC * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
					highSampleL = inputSampleL - iirHighSampleLC;
					iirLowSampleLC = (iirLowSampleLC * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
					bassSampleL = iirLowSampleLC;

					tripletFactorR = last2SampleR - inputSampleR;
					tripletRA += tripletFactorR;
					tripletRC -= tripletFactorR;
					tripletFactorR = tripletRA * tripletIntensity;
					iirHighSampleRC = (iirHighSampleRC * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
					highSampleR = inputSampleR - iirHighSampleRC;
					iirLowSampleRC = (iirLowSampleRC * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
					bassSampleR = iirLowSampleRC;
					break;
				case 2:
					tripletFactorL = last2SampleL - inputSampleL;
					tripletLB += tripletFactorL;
					tripletLA -= tripletFactorL;
					tripletFactorL = tripletLB * tripletIntensity;
					iirHighSampleLD = (iirHighSampleLD * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
					highSampleL = inputSampleL - iirHighSampleLD;
					iirLowSampleLD = (iirLowSampleLD * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
					bassSampleL = iirLowSampleLD;

					tripletFactorR = last2SampleR - inputSampleR;
					tripletRB += tripletFactorR;
					tripletRA -= tripletFactorR;
					tripletFactorR = tripletRB * tripletIntensity;
					iirHighSampleRD = (iirHighSampleRD * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
					highSampleR = inputSampleR - iirHighSampleRD;
					iirLowSampleRD = (iirLowSampleRD * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
					bassSampleR = iirLowSampleRD;
					break;
				case 3:
					tripletFactorL = last2SampleL - inputSampleL;
					tripletLC += tripletFactorL;
					tripletLB -= tripletFactorL;
					tripletFactorL = tripletLC * tripletIntensity;
					iirHighSampleLE = (iirHighSampleLE * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
					highSampleL = inputSampleL - iirHighSampleLE;
					iirLowSampleLE = (iirLowSampleLE * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
					bassSampleL = iirLowSampleLE;

					tripletFactorR = last2SampleR - inputSampleR;
					tripletRC += tripletFactorR;
					tripletRB -= tripletFactorR;
					tripletFactorR = tripletRC * tripletIntensity;
					iirHighSampleRE = (iirHighSampleRE * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
					highSampleR = inputSampleR - iirHighSampleRE;
					iirLowSampleRE = (iirLowSampleRE * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
					bassSampleR = iirLowSampleRE;
					break;
			}
			tripletLA /= 2.0;
			tripletLB /= 2.0;
			tripletLC /= 2.0;
			highSampleL = highSampleL + tripletFactorL;

			tripletRA /= 2.0;
			tripletRB /= 2.0;
			tripletRC /= 2.0;
			highSampleR = highSampleR + tripletFactorR;

			if (flip)
			{
				iirHighSampleLA = (iirHighSampleLA * (1.0 - iirAmountA)) + (highSampleL * iirAmountA);
				highSampleL -= iirHighSampleLA;
				iirLowSampleLA = (iirLowSampleLA * (1.0 - iirAmountB)) + (bassSampleL * iirAmountB);
				bassSampleL = iirLowSampleLA;

				iirHighSampleRA = (iirHighSampleRA * (1.0 - iirAmountA)) + (highSampleR * iirAmountA);
				highSampleR -= iirHighSampleRA;
				iirLowSampleRA = (iirLowSampleRA * (1.0 - iirAmountB)) + (bassSampleR * iirAmountB);
				bassSampleR = iirLowSampleRA;
			}
			else
			{
				iirHighSampleLB = (iirHighSampleLB * (1.0 - iirAmountA)) + (highSampleL * iirAmountA);
				highSampleL -= iirHighSampleLB;
				iirLowSampleLB = (iirLowSampleLB * (1.0 - iirAmountB)) + (bassSampleL * iirAmountB);
				bassSampleL = iirLowSampleLB;

				iirHighSampleRB = (iirHighSampleRB * (1.0 - iirAmountA)) + (highSampleR * iirAmountA);
				highSampleR -= iirHighSampleRB;
				iirLowSampleRB = (iirLowSampleRB * (1.0 - iirAmountB)) + (bassSampleR * iirAmountB);
				bassSampleR = iirLowSampleRB;
			}

			iirHighSampleL = (iirHighSampleL * (1.0 - iirAmountA)) + (highSampleL * iirAmountA);
			highSampleL -= iirHighSampleL;
			iirLowSampleL = (iirLowSampleL * (1.0 - iirAmountB)) + (bassSampleL * iirAmountB);
			bassSampleL = iirLowSampleL;

			iirHighSampleR = (iirHighSampleR * (1.0 - iirAmountA)) + (highSampleR * iirAmountA);
			highSampleR -= iirHighSampleR;
			iirLowSampleR = (iirLowSampleR * (1.0 - iirAmountB)) + (bassSampleR * iirAmountB);
			bassSampleR = iirLowSampleR;

			midSampleL = (inputSampleL-bassSampleL)-highSampleL;
			midSampleR = (inputSampleR-bassSampleR)-highSampleR;

			//drive section
			highSampleL *= (densityA+1.0);
			bridgerectifier = fabs(highSampleL)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityA > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (highSampleL > 0) highSampleL = (highSampleL*(1-outA))+(bridgerectifier*outA);
			else highSampleL = (highSampleL*(1-outA))-(bridgerectifier*outA);
			//blend according to densityA control

			highSampleR *= (densityA+1.0);
			bridgerectifier = fabs(highSampleR)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityA > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (highSampleR > 0) highSampleR = (highSampleR*(1-outA))+(bridgerectifier*outA);
			else highSampleR = (highSampleR*(1-outA))-(bridgerectifier*outA);
			//blend according to densityA control

			midSampleL *= (densityB+1.0);
			bridgerectifier = fabs(midSampleL)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityB > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (midSampleL > 0) midSampleL = (midSampleL*(1-outB))+(bridgerectifier*outB);
			else midSampleL = (midSampleL*(1-outB))-(bridgerectifier*outB);
			//blend according to densityB control

			midSampleR *= (densityB+1.0);
			bridgerectifier = fabs(midSampleR)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityB > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (midSampleR > 0) midSampleR = (midSampleR*(1-outB))+(bridgerectifier*outB);
			else midSampleR = (midSampleR*(1-outB))-(bridgerectifier*outB);
			//blend according to densityB control

			bassSampleL *= (densityC+1.0);
			bridgerectifier = fabs(bassSampleL)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityC > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (bassSampleL > 0) bassSampleL = (bassSampleL*(1-outC))+(bridgerectifier*outC);
			else bassSampleL = (bassSampleL*(1-outC))-(bridgerectifier*outC);
			//blend according to densityC control

			bassSampleR *= (densityC+1.0);
			bridgerectifier = fabs(bassSampleR)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityC > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (bassSampleR > 0) bassSampleR = (bassSampleR*(1-outC))+(bridgerectifier*outC);
			else bassSampleR = (bassSampleR*(1-outC))-(bridgerectifier*outC);
			//blend according to densityC control

			inputSampleL = midSampleL;
			inputSampleL += highSampleL;
			inputSampleL += bassSampleL;

			inputSampleR = midSampleR;
			inputSampleR += highSampleR;
			inputSampleR += bassSampleR;
		}
		//end EQ

		//EQ lowpass is after all processing like the compressor that might produce hash
		if (engageLowpass)
		{
			if (flip)
			{
				lowpassSampleLAA = (lowpassSampleLAA * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLAA;
				lowpassSampleLBA = (lowpassSampleLBA * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLBA;
				lowpassSampleLCA = (lowpassSampleLCA * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLCA;
				lowpassSampleLDA = (lowpassSampleLDA * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLDA;
				lowpassSampleLE = (lowpassSampleLE * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLE;

				lowpassSampleRAA = (lowpassSampleRAA * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRAA;
				lowpassSampleRBA = (lowpassSampleRBA * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRBA;
				lowpassSampleRCA = (lowpassSampleRCA * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRCA;
				lowpassSampleRDA = (lowpassSampleRDA * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRDA;
				lowpassSampleRE = (lowpassSampleRE * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRE;
			}
			else
			{
				lowpassSampleLAB = (lowpassSampleLAB * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLAB;
				lowpassSampleLBB = (lowpassSampleLBB * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLBB;
				lowpassSampleLCB = (lowpassSampleLCB * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLCB;
				lowpassSampleLDB = (lowpassSampleLDB * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLDB;
				lowpassSampleLF = (lowpassSampleLF * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLF;

				lowpassSampleRAB = (lowpassSampleRAB * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRAB;
				lowpassSampleRBB = (lowpassSampleRBB * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRBB;
				lowpassSampleRCB = (lowpassSampleRCB * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRCB;
				lowpassSampleRDB = (lowpassSampleRDB * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRDB;
				lowpassSampleRF = (lowpassSampleRF * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRF;
			}
			lowpassSampleLG = (lowpassSampleLG * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
			lowpassSampleRG = (lowpassSampleRG * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);

			inputSampleL = (lowpassSampleLG * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
			inputSampleR = (lowpassSampleRG * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
		}

		//built in output trim and dry/wet if desired
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

void EQ::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	double compscale = overallscale;
	overallscale = getSampleRate();
	compscale = compscale * overallscale;
	//compscale is the one that's 1 or something like 2.2 for 96K rates
	double fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	long double inputSampleL;
	long double inputSampleR;

	double highSampleL = 0.0;
	double midSampleL = 0.0;
	double bassSampleL = 0.0;

	double highSampleR = 0.0;
	double midSampleR = 0.0;
	double bassSampleR = 0.0;

	double densityA = (A*12.0)-6.0;
	double densityB = (B*12.0)-6.0;
	double densityC = (C*12.0)-6.0;
	bool engageEQ = true;
	if ( (0.0 == densityA) && (0.0 == densityB) && (0.0 == densityC) ) engageEQ = false;

	densityA = pow(10.0,densityA/20.0)-1.0;
	densityB = pow(10.0,densityB/20.0)-1.0;
	densityC = pow(10.0,densityC/20.0)-1.0;
	//convert to 0 to X multiplier with 1.0 being O db
	//minus one gives nearly -1 to ? (should top out at 1)
	//calibrate so that X db roughly equals X db with maximum topping out at 1 internally

	double tripletIntensity = -densityA;

	double iirAmountC = (((D*D*15.0)+1.0)*0.0188) + 0.7;
	if (iirAmountC > 1.0) iirAmountC = 1.0;
	bool engageLowpass = false;
	if (((D*D*15.0)+1.0) < 15.99) engageLowpass = true;

	double iirAmountA = (((E*E*15.0)+1.0)*1000)/overallscale;
	double iirAmountB = (((F*F*1570.0)+30.0)*10)/overallscale;
	double iirAmountD = (((G*G*1570.0)+30.0)*1.0)/overallscale;
	bool engageHighpass = false;
	if (((G*G*1570.0)+30.0) > 30.01) engageHighpass = true;
	//bypass the highpass and lowpass if set to extremes
	double bridgerectifier;
	double outA = fabs(densityA);
	double outB = fabs(densityB);
	double outC = fabs(densityC);
	//end EQ
	double outputgain = pow(10.0,((H*36.0)-18.0)/20.0);

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

		last2SampleL = lastSampleL;
		lastSampleL = inputSampleL;

		last2SampleR = lastSampleR;
		lastSampleR = inputSampleR;

		flip = !flip;
		flipthree++;
		if (flipthree < 1 || flipthree > 3) flipthree = 1;
		//counters

		//begin highpass
		if (engageHighpass)
		{
			if (flip)
			{
				highpassSampleLAA = (highpassSampleLAA * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLAA;
				highpassSampleLBA = (highpassSampleLBA * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLBA;
				highpassSampleLCA = (highpassSampleLCA * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLCA;
				highpassSampleLDA = (highpassSampleLDA * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLDA;
			}
			else
			{
				highpassSampleLAB = (highpassSampleLAB * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLAB;
				highpassSampleLBB = (highpassSampleLBB * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLBB;
				highpassSampleLCB = (highpassSampleLCB * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLCB;
				highpassSampleLDB = (highpassSampleLDB * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
				inputSampleL -= highpassSampleLDB;
			}
			highpassSampleLE = (highpassSampleLE * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
			inputSampleL -= highpassSampleLE;
			highpassSampleLF = (highpassSampleLF * (1.0 - iirAmountD)) + (inputSampleL * iirAmountD);
			inputSampleL -= highpassSampleLF;

			if (flip)
			{
				highpassSampleRAA = (highpassSampleRAA * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRAA;
				highpassSampleRBA = (highpassSampleRBA * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRBA;
				highpassSampleRCA = (highpassSampleRCA * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRCA;
				highpassSampleRDA = (highpassSampleRDA * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRDA;
			}
			else
			{
				highpassSampleRAB = (highpassSampleRAB * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRAB;
				highpassSampleRBB = (highpassSampleRBB * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRBB;
				highpassSampleRCB = (highpassSampleRCB * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRCB;
				highpassSampleRDB = (highpassSampleRDB * (1.0 - iirAmountD)) + (inputSampleR * iirAmountD);
				inputSampleR -= highpassSampleRDB;
			}
			highpassSampleRE = (highpassSampleRE * (1 - iirAmountD)) + (inputSampleR * iirAmountD);
			inputSampleR -= highpassSampleRE;
			highpassSampleRF = (highpassSampleRF * (1 - iirAmountD)) + (inputSampleR * iirAmountD);
			inputSampleR -= highpassSampleRF;

		}
		//end highpass

		//begin EQ
		if (engageEQ)
		{
			switch (flipthree)
			{
				case 1:
					tripletFactorL = last2SampleL - inputSampleL;
					tripletLA += tripletFactorL;
					tripletLC -= tripletFactorL;
					tripletFactorL = tripletLA * tripletIntensity;
					iirHighSampleLC = (iirHighSampleLC * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
					highSampleL = inputSampleL - iirHighSampleLC;
					iirLowSampleLC = (iirLowSampleLC * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
					bassSampleL = iirLowSampleLC;

					tripletFactorR = last2SampleR - inputSampleR;
					tripletRA += tripletFactorR;
					tripletRC -= tripletFactorR;
					tripletFactorR = tripletRA * tripletIntensity;
					iirHighSampleRC = (iirHighSampleRC * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
					highSampleR = inputSampleR - iirHighSampleRC;
					iirLowSampleRC = (iirLowSampleRC * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
					bassSampleR = iirLowSampleRC;
					break;
				case 2:
					tripletFactorL = last2SampleL - inputSampleL;
					tripletLB += tripletFactorL;
					tripletLA -= tripletFactorL;
					tripletFactorL = tripletLB * tripletIntensity;
					iirHighSampleLD = (iirHighSampleLD * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
					highSampleL = inputSampleL - iirHighSampleLD;
					iirLowSampleLD = (iirLowSampleLD * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
					bassSampleL = iirLowSampleLD;

					tripletFactorR = last2SampleR - inputSampleR;
					tripletRB += tripletFactorR;
					tripletRA -= tripletFactorR;
					tripletFactorR = tripletRB * tripletIntensity;
					iirHighSampleRD = (iirHighSampleRD * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
					highSampleR = inputSampleR - iirHighSampleRD;
					iirLowSampleRD = (iirLowSampleRD * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
					bassSampleR = iirLowSampleRD;
					break;
				case 3:
					tripletFactorL = last2SampleL - inputSampleL;
					tripletLC += tripletFactorL;
					tripletLB -= tripletFactorL;
					tripletFactorL = tripletLC * tripletIntensity;
					iirHighSampleLE = (iirHighSampleLE * (1.0 - iirAmountA)) + (inputSampleL * iirAmountA);
					highSampleL = inputSampleL - iirHighSampleLE;
					iirLowSampleLE = (iirLowSampleLE * (1.0 - iirAmountB)) + (inputSampleL * iirAmountB);
					bassSampleL = iirLowSampleLE;

					tripletFactorR = last2SampleR - inputSampleR;
					tripletRC += tripletFactorR;
					tripletRB -= tripletFactorR;
					tripletFactorR = tripletRC * tripletIntensity;
					iirHighSampleRE = (iirHighSampleRE * (1.0 - iirAmountA)) + (inputSampleR * iirAmountA);
					highSampleR = inputSampleR - iirHighSampleRE;
					iirLowSampleRE = (iirLowSampleRE * (1.0 - iirAmountB)) + (inputSampleR * iirAmountB);
					bassSampleR = iirLowSampleRE;
					break;
			}
			tripletLA /= 2.0;
			tripletLB /= 2.0;
			tripletLC /= 2.0;
			highSampleL = highSampleL + tripletFactorL;

			tripletRA /= 2.0;
			tripletRB /= 2.0;
			tripletRC /= 2.0;
			highSampleR = highSampleR + tripletFactorR;

			if (flip)
			{
				iirHighSampleLA = (iirHighSampleLA * (1.0 - iirAmountA)) + (highSampleL * iirAmountA);
				highSampleL -= iirHighSampleLA;
				iirLowSampleLA = (iirLowSampleLA * (1.0 - iirAmountB)) + (bassSampleL * iirAmountB);
				bassSampleL = iirLowSampleLA;

				iirHighSampleRA = (iirHighSampleRA * (1.0 - iirAmountA)) + (highSampleR * iirAmountA);
				highSampleR -= iirHighSampleRA;
				iirLowSampleRA = (iirLowSampleRA * (1.0 - iirAmountB)) + (bassSampleR * iirAmountB);
				bassSampleR = iirLowSampleRA;
			}
			else
			{
				iirHighSampleLB = (iirHighSampleLB * (1.0 - iirAmountA)) + (highSampleL * iirAmountA);
				highSampleL -= iirHighSampleLB;
				iirLowSampleLB = (iirLowSampleLB * (1.0 - iirAmountB)) + (bassSampleL * iirAmountB);
				bassSampleL = iirLowSampleLB;

				iirHighSampleRB = (iirHighSampleRB * (1.0 - iirAmountA)) + (highSampleR * iirAmountA);
				highSampleR -= iirHighSampleRB;
				iirLowSampleRB = (iirLowSampleRB * (1.0 - iirAmountB)) + (bassSampleR * iirAmountB);
				bassSampleR = iirLowSampleRB;
			}

			iirHighSampleL = (iirHighSampleL * (1.0 - iirAmountA)) + (highSampleL * iirAmountA);
			highSampleL -= iirHighSampleL;
			iirLowSampleL = (iirLowSampleL * (1.0 - iirAmountB)) + (bassSampleL * iirAmountB);
			bassSampleL = iirLowSampleL;

			iirHighSampleR = (iirHighSampleR * (1.0 - iirAmountA)) + (highSampleR * iirAmountA);
			highSampleR -= iirHighSampleR;
			iirLowSampleR = (iirLowSampleR * (1.0 - iirAmountB)) + (bassSampleR * iirAmountB);
			bassSampleR = iirLowSampleR;

			midSampleL = (inputSampleL-bassSampleL)-highSampleL;
			midSampleR = (inputSampleR-bassSampleR)-highSampleR;

			//drive section
			highSampleL *= (densityA+1.0);
			bridgerectifier = fabs(highSampleL)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityA > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (highSampleL > 0) highSampleL = (highSampleL*(1-outA))+(bridgerectifier*outA);
			else highSampleL = (highSampleL*(1-outA))-(bridgerectifier*outA);
			//blend according to densityA control

			highSampleR *= (densityA+1.0);
			bridgerectifier = fabs(highSampleR)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityA > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (highSampleR > 0) highSampleR = (highSampleR*(1-outA))+(bridgerectifier*outA);
			else highSampleR = (highSampleR*(1-outA))-(bridgerectifier*outA);
			//blend according to densityA control

			midSampleL *= (densityB+1.0);
			bridgerectifier = fabs(midSampleL)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityB > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (midSampleL > 0) midSampleL = (midSampleL*(1-outB))+(bridgerectifier*outB);
			else midSampleL = (midSampleL*(1-outB))-(bridgerectifier*outB);
			//blend according to densityB control

			midSampleR *= (densityB+1.0);
			bridgerectifier = fabs(midSampleR)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityB > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (midSampleR > 0) midSampleR = (midSampleR*(1-outB))+(bridgerectifier*outB);
			else midSampleR = (midSampleR*(1-outB))-(bridgerectifier*outB);
			//blend according to densityB control

			bassSampleL *= (densityC+1.0);
			bridgerectifier = fabs(bassSampleL)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityC > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (bassSampleL > 0) bassSampleL = (bassSampleL*(1-outC))+(bridgerectifier*outC);
			else bassSampleL = (bassSampleL*(1-outC))-(bridgerectifier*outC);
			//blend according to densityC control

			bassSampleR *= (densityC+1.0);
			bridgerectifier = fabs(bassSampleR)*1.57079633;
			if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
			//max value for sine function
			if (densityC > 0) bridgerectifier = sin(bridgerectifier);
			else bridgerectifier = 1-cos(bridgerectifier);
			//produce either boosted or starved version
			if (bassSampleR > 0) bassSampleR = (bassSampleR*(1-outC))+(bridgerectifier*outC);
			else bassSampleR = (bassSampleR*(1-outC))-(bridgerectifier*outC);
			//blend according to densityC control

			inputSampleL = midSampleL;
			inputSampleL += highSampleL;
			inputSampleL += bassSampleL;

			inputSampleR = midSampleR;
			inputSampleR += highSampleR;
			inputSampleR += bassSampleR;
		}
		//end EQ

		//EQ lowpass is after all processing like the compressor that might produce hash
		if (engageLowpass)
		{
			if (flip)
			{
				lowpassSampleLAA = (lowpassSampleLAA * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLAA;
				lowpassSampleLBA = (lowpassSampleLBA * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLBA;
				lowpassSampleLCA = (lowpassSampleLCA * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLCA;
				lowpassSampleLDA = (lowpassSampleLDA * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLDA;
				lowpassSampleLE = (lowpassSampleLE * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLE;

				lowpassSampleRAA = (lowpassSampleRAA * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRAA;
				lowpassSampleRBA = (lowpassSampleRBA * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRBA;
				lowpassSampleRCA = (lowpassSampleRCA * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRCA;
				lowpassSampleRDA = (lowpassSampleRDA * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRDA;
				lowpassSampleRE = (lowpassSampleRE * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRE;
			}
			else
			{
				lowpassSampleLAB = (lowpassSampleLAB * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLAB;
				lowpassSampleLBB = (lowpassSampleLBB * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLBB;
				lowpassSampleLCB = (lowpassSampleLCB * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLCB;
				lowpassSampleLDB = (lowpassSampleLDB * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLDB;
				lowpassSampleLF = (lowpassSampleLF * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
				inputSampleL = lowpassSampleLF;

				lowpassSampleRAB = (lowpassSampleRAB * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRAB;
				lowpassSampleRBB = (lowpassSampleRBB * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRBB;
				lowpassSampleRCB = (lowpassSampleRCB * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRCB;
				lowpassSampleRDB = (lowpassSampleRDB * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRDB;
				lowpassSampleRF = (lowpassSampleRF * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
				inputSampleR = lowpassSampleRF;
			}
			lowpassSampleLG = (lowpassSampleLG * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
			lowpassSampleRG = (lowpassSampleRG * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);

			inputSampleL = (lowpassSampleLG * (1.0 - iirAmountC)) + (inputSampleL * iirAmountC);
			inputSampleR = (lowpassSampleRG * (1.0 - iirAmountC)) + (inputSampleR * iirAmountC);
		}

		//built in output trim and dry/wet if desired
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
