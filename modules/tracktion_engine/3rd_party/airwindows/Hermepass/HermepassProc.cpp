/* ========================================
 *  Hermepass - Hermepass.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Hermepass_H
#include "Hermepass.h"
#endif

void Hermepass::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
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

	double rangescale = 0.1 / overallscale;

	double cutoff = pow(A,3);
	double slope = pow(B,3) * 6.0;

	double newA = cutoff * rangescale;
	double newB = newA; //other part of interleaved IIR is the same

	double newC = cutoff * rangescale; //first extra pole is the same
	cutoff = (cutoff * fpOld) + (0.00001 * fpNew);
	double newD = cutoff * rangescale;
	cutoff = (cutoff * fpOld) + (0.00001 * fpNew);
	double newE = cutoff * rangescale;
	cutoff = (cutoff * fpOld) + (0.00001 * fpNew);
	double newF = cutoff * rangescale;
	cutoff = (cutoff * fpOld) + (0.00001 * fpNew);
	double newG = cutoff * rangescale;
	cutoff = (cutoff * fpOld) + (0.00001 * fpNew);
	double newH = cutoff * rangescale;
	//converge toward the unvarying fixed cutoff value

	double oldA = 1.0 - newA;
	double oldB = 1.0 - newB;
	double oldC = 1.0 - newC;
	double oldD = 1.0 - newD;
	double oldE = 1.0 - newE;
	double oldF = 1.0 - newF;
	double oldG = 1.0 - newG;
	double oldH = 1.0 - newH;

	double polesC;
	double polesD;
	double polesE;
	double polesF;
	double polesG;
	double polesH;

	polesC = slope; if (slope > 1.0) polesC = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	polesD = slope; if (slope > 1.0) polesD = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	polesE = slope; if (slope > 1.0) polesE = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	polesF = slope; if (slope > 1.0) polesF = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	polesG = slope; if (slope > 1.0) polesG = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	polesH = slope; if (slope > 1.0) polesH = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	//each one will either be 0.0, the fractional slope value, or 1

	long double inputSampleL;
	long double inputSampleR;
	double tempSampleL;
	double tempSampleR;
	double correction;

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

		tempSampleL = inputSampleL;
		tempSampleR = inputSampleR;

		//begin L channel
		if (fpFlip) {
			iirAL = (iirAL * oldA) + (tempSampleL * newA); tempSampleL -= iirAL; correction = iirAL;
		} else {
			iirBL = (iirBL * oldB) + (tempSampleL * newB); tempSampleL -= iirBL; correction = iirBL;
		}
		iirCL = (iirCL * oldC) + (tempSampleL * newC); tempSampleL -= iirCL;
		iirDL = (iirDL * oldD) + (tempSampleL * newD); tempSampleL -= iirDL;
		iirEL = (iirEL * oldE) + (tempSampleL * newE); tempSampleL -= iirEL;
		iirFL = (iirFL * oldF) + (tempSampleL * newF); tempSampleL -= iirFL;
		iirGL = (iirGL * oldG) + (tempSampleL * newG); tempSampleL -= iirGL;
		iirHL = (iirHL * oldH) + (tempSampleL * newH); tempSampleL -= iirHL;
		//set up all the iir filters in case they are used

		if (polesC == 1.0) correction += iirCL; if (polesC > 0.0 && polesC < 1.0) correction += (iirCL * polesC);
		if (polesD == 1.0) correction += iirDL; if (polesD > 0.0 && polesD < 1.0) correction += (iirDL * polesD);
		if (polesE == 1.0) correction += iirEL; if (polesE > 0.0 && polesE < 1.0) correction += (iirEL * polesE);
		if (polesF == 1.0) correction += iirFL; if (polesF > 0.0 && polesF < 1.0) correction += (iirFL * polesF);
		if (polesG == 1.0) correction += iirGL; if (polesG > 0.0 && polesG < 1.0) correction += (iirGL * polesG);
		if (polesH == 1.0) correction += iirHL; if (polesH > 0.0 && polesH < 1.0) correction += (iirHL * polesH);
		//each of these are added directly if they're fully engaged,
		//multiplied by 0-1 if they are the interpolated one, or skipped if they are beyond the interpolated one.
		//the purpose is to do all the math at the floating point exponent nearest to the tiny value in use.
		//also, it's formatted that way to easily substitute the next variable: this could be written as a loop
		//with everything an array value. However, this makes just as much sense for this few poles.

		inputSampleL -= correction;
		//end L channel

		//begin R channel
		if (fpFlip) {
			iirAR = (iirAR * oldA) + (tempSampleR * newA); tempSampleR -= iirAR; correction = iirAR;
		} else {
			iirBR = (iirBR * oldB) + (tempSampleR * newB); tempSampleR -= iirBR; correction = iirBR;
		}
		iirCR = (iirCR * oldC) + (tempSampleR * newC); tempSampleR -= iirCR;
		iirDR = (iirDR * oldD) + (tempSampleR * newD); tempSampleR -= iirDR;
		iirER = (iirER * oldE) + (tempSampleR * newE); tempSampleR -= iirER;
		iirFR = (iirFR * oldF) + (tempSampleR * newF); tempSampleR -= iirFR;
		iirGR = (iirGR * oldG) + (tempSampleR * newG); tempSampleR -= iirGR;
		iirHR = (iirHR * oldH) + (tempSampleR * newH); tempSampleR -= iirHR;
		//set up all the iir filters in case they are used

		if (polesC == 1.0) correction += iirCR; if (polesC > 0.0 && polesC < 1.0) correction += (iirCR * polesC);
		if (polesD == 1.0) correction += iirDR; if (polesD > 0.0 && polesD < 1.0) correction += (iirDR * polesD);
		if (polesE == 1.0) correction += iirER; if (polesE > 0.0 && polesE < 1.0) correction += (iirER * polesE);
		if (polesF == 1.0) correction += iirFR; if (polesF > 0.0 && polesF < 1.0) correction += (iirFR * polesF);
		if (polesG == 1.0) correction += iirGR; if (polesG > 0.0 && polesG < 1.0) correction += (iirGR * polesG);
		if (polesH == 1.0) correction += iirHR; if (polesH > 0.0 && polesH < 1.0) correction += (iirHR * polesH);
		//each of these are added directly if they're fully engaged,
		//multiplied by 0-1 if they are the interpolated one, or skipped if they are beyond the interpolated one.
		//the purpose is to do all the math at the floating point exponent nearest to the tiny value in use.
		//also, it's formatted that way to easily substitute the next variable: this could be written as a loop
		//with everything an array value. However, this makes just as much sense for this few poles.

		inputSampleR -= correction;
		//end R channel

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

void Hermepass::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
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

	double rangescale = 0.1 / overallscale;

	double cutoff = pow(A,3);
	double slope = pow(B,3) * 6.0;

	double newA = cutoff * rangescale;
	double newB = newA; //other part of interleaved IIR is the same

	double newC = cutoff * rangescale; //first extra pole is the same
	cutoff = (cutoff * fpOld) + (0.00001 * fpNew);
	double newD = cutoff * rangescale;
	cutoff = (cutoff * fpOld) + (0.00001 * fpNew);
	double newE = cutoff * rangescale;
	cutoff = (cutoff * fpOld) + (0.00001 * fpNew);
	double newF = cutoff * rangescale;
	cutoff = (cutoff * fpOld) + (0.00001 * fpNew);
	double newG = cutoff * rangescale;
	cutoff = (cutoff * fpOld) + (0.00001 * fpNew);
	double newH = cutoff * rangescale;
	//converge toward the unvarying fixed cutoff value

	double oldA = 1.0 - newA;
	double oldB = 1.0 - newB;
	double oldC = 1.0 - newC;
	double oldD = 1.0 - newD;
	double oldE = 1.0 - newE;
	double oldF = 1.0 - newF;
	double oldG = 1.0 - newG;
	double oldH = 1.0 - newH;

	double polesC;
	double polesD;
	double polesE;
	double polesF;
	double polesG;
	double polesH;

	polesC = slope; if (slope > 1.0) polesC = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	polesD = slope; if (slope > 1.0) polesD = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	polesE = slope; if (slope > 1.0) polesE = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	polesF = slope; if (slope > 1.0) polesF = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	polesG = slope; if (slope > 1.0) polesG = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	polesH = slope; if (slope > 1.0) polesH = 1.0; slope -= 1.0; if (slope < 0.0) slope = 0.0;
	//each one will either be 0.0, the fractional slope value, or 1

	long double inputSampleL;
	long double inputSampleR;
	double tempSampleL;
	double tempSampleR;
	double correction;

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

		tempSampleL = inputSampleL;
		tempSampleR = inputSampleR;

		//begin L channel
		if (fpFlip) {
			iirAL = (iirAL * oldA) + (tempSampleL * newA); tempSampleL -= iirAL; correction = iirAL;
		} else {
			iirBL = (iirBL * oldB) + (tempSampleL * newB); tempSampleL -= iirBL; correction = iirBL;
		}
		iirCL = (iirCL * oldC) + (tempSampleL * newC); tempSampleL -= iirCL;
		iirDL = (iirDL * oldD) + (tempSampleL * newD); tempSampleL -= iirDL;
		iirEL = (iirEL * oldE) + (tempSampleL * newE); tempSampleL -= iirEL;
		iirFL = (iirFL * oldF) + (tempSampleL * newF); tempSampleL -= iirFL;
		iirGL = (iirGL * oldG) + (tempSampleL * newG); tempSampleL -= iirGL;
		iirHL = (iirHL * oldH) + (tempSampleL * newH); tempSampleL -= iirHL;
		//set up all the iir filters in case they are used

		if (polesC == 1.0) correction += iirCL; if (polesC > 0.0 && polesC < 1.0) correction += (iirCL * polesC);
		if (polesD == 1.0) correction += iirDL; if (polesD > 0.0 && polesD < 1.0) correction += (iirDL * polesD);
		if (polesE == 1.0) correction += iirEL; if (polesE > 0.0 && polesE < 1.0) correction += (iirEL * polesE);
		if (polesF == 1.0) correction += iirFL; if (polesF > 0.0 && polesF < 1.0) correction += (iirFL * polesF);
		if (polesG == 1.0) correction += iirGL; if (polesG > 0.0 && polesG < 1.0) correction += (iirGL * polesG);
		if (polesH == 1.0) correction += iirHL; if (polesH > 0.0 && polesH < 1.0) correction += (iirHL * polesH);
		//each of these are added directly if they're fully engaged,
		//multiplied by 0-1 if they are the interpolated one, or skipped if they are beyond the interpolated one.
		//the purpose is to do all the math at the floating point exponent nearest to the tiny value in use.
		//also, it's formatted that way to easily substitute the next variable: this could be written as a loop
		//with everything an array value. However, this makes just as much sense for this few poles.

		inputSampleL -= correction;
		//end L channel

		//begin R channel
		if (fpFlip) {
			iirAR = (iirAR * oldA) + (tempSampleR * newA); tempSampleR -= iirAR; correction = iirAR;
		} else {
			iirBR = (iirBR * oldB) + (tempSampleR * newB); tempSampleR -= iirBR; correction = iirBR;
		}
		iirCR = (iirCR * oldC) + (tempSampleR * newC); tempSampleR -= iirCR;
		iirDR = (iirDR * oldD) + (tempSampleR * newD); tempSampleR -= iirDR;
		iirER = (iirER * oldE) + (tempSampleR * newE); tempSampleR -= iirER;
		iirFR = (iirFR * oldF) + (tempSampleR * newF); tempSampleR -= iirFR;
		iirGR = (iirGR * oldG) + (tempSampleR * newG); tempSampleR -= iirGR;
		iirHR = (iirHR * oldH) + (tempSampleR * newH); tempSampleR -= iirHR;
		//set up all the iir filters in case they are used

		if (polesC == 1.0) correction += iirCR; if (polesC > 0.0 && polesC < 1.0) correction += (iirCR * polesC);
		if (polesD == 1.0) correction += iirDR; if (polesD > 0.0 && polesD < 1.0) correction += (iirDR * polesD);
		if (polesE == 1.0) correction += iirER; if (polesE > 0.0 && polesE < 1.0) correction += (iirER * polesE);
		if (polesF == 1.0) correction += iirFR; if (polesF > 0.0 && polesF < 1.0) correction += (iirFR * polesF);
		if (polesG == 1.0) correction += iirGR; if (polesG > 0.0 && polesG < 1.0) correction += (iirGR * polesG);
		if (polesH == 1.0) correction += iirHR; if (polesH > 0.0 && polesH < 1.0) correction += (iirHR * polesH);
		//each of these are added directly if they're fully engaged,
		//multiplied by 0-1 if they are the interpolated one, or skipped if they are beyond the interpolated one.
		//the purpose is to do all the math at the floating point exponent nearest to the tiny value in use.
		//also, it's formatted that way to easily substitute the next variable: this could be written as a loop
		//with everything an array value. However, this makes just as much sense for this few poles.

		inputSampleR -= correction;
		//end R channel

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
