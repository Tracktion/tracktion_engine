/* ========================================
 *  Ensemble - Ensemble.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Ensemble_H
#include "Ensemble.h"
#endif

void Ensemble::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double spd = pow(0.4+(B/12),10);
	spd *= overallscale;
	double depth = 0.002 / spd;
	double tupi = 3.141592653589793238 * 2.0;
	double taps = floor((A*46.0)+2.9);
	double brighten = C;
	double wet = D;
	double dry = 1.0 - wet;
	double hapi = 3.141592653589793238 / taps;
	double offset;
	double floffset;
	double start[49];
	double sinoffset[49];
	double speed[49];
	int count;
	int ensemble;
	double tempL;
	double tempR;

	float fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	long double inputSampleL;
	long double inputSampleR;
	double drySampleL;
	double drySampleR;
	//now we'll precalculate some stuff that needn't be in every sample

	for(count = 1; count <= taps; count++)
	{
		start[count] = depth * count;
		sinoffset[count] = hapi * (count-1);
		speed[count] = spd / (1 + (count/taps));
	}
	//that's for speeding up things in the sample-processing area

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

		airFactorL = airPrevL - inputSampleL;
		if (fpFlip) {airEvenL += airFactorL; airOddL -= airFactorL; airFactorL = airEvenL;}
		else {airOddL += airFactorL; airEvenL -= airFactorL; airFactorL = airOddL;}
		airOddL = (airOddL - ((airOddL - airEvenL)/256.0)) / 1.0001;
		airEvenL = (airEvenL - ((airEvenL - airOddL)/256.0)) / 1.0001;
		airPrevL = inputSampleL;
		inputSampleL += (airFactorL*brighten);
		//air, compensates for loss of highs in flanger's interpolation

		airFactorR = airPrevR - inputSampleR;
		if (fpFlip) {airEvenR += airFactorR; airOddR -= airFactorR; airFactorR = airEvenR;}
		else {airOddR += airFactorR; airEvenR -= airFactorR; airFactorR = airOddR;}
		airOddR = (airOddR - ((airOddR - airEvenR)/256.0)) / 1.0001;
		airEvenR = (airEvenR - ((airEvenR - airOddR)/256.0)) / 1.0001;
		airPrevR = inputSampleR;
		inputSampleR += (airFactorR*brighten);
		//air, compensates for loss of highs in flanger's interpolation

		if (gcount < 1 || gcount > 32767) {gcount = 32767;}
		count = gcount;
		dL[count+32767] = dL[count] = tempL = inputSampleL;
		dR[count+32767] = dR[count] = tempR = inputSampleR;
		//double buffer

		for(ensemble = 1; ensemble <= taps; ensemble++)
		{
			offset = start[ensemble] + (depth * sin(sweep[ensemble]+sinoffset[ensemble]));
			floffset = offset-floor(offset);
			count = gcount + (int)floor(offset);

			tempL += dL[count] * (1-floffset); //less as value moves away from .0
			tempL += dL[count+1]; //we can assume always using this in one way or another?
			tempL += dL[count+2] * floffset; //greater as value moves away from .0
			tempL -= ((dL[count]-dL[count+1])-(dL[count+1]-dL[count+2]))/50; //interpolation hacks 'r us

			tempR += dR[count] * (1-floffset); //less as value moves away from .0
			tempR += dR[count+1]; //we can assume always using this in one way or another?
			tempR += dR[count+2] * floffset; //greater as value moves away from .0
			tempR -= ((dR[count]-dR[count+1])-(dR[count+1]-dR[count+2]))/50; //interpolation hacks 'r us

			sweep[ensemble] += speed[ensemble];
			if (sweep[ensemble] > tupi){sweep[ensemble] -= tupi;}
		}
		gcount--;
		//still scrolling through the samples, remember

		inputSampleL = tempL/(4.0*sqrt(taps));
		inputSampleR = tempR/(4.0*sqrt(taps));


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

void Ensemble::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double spd = pow(0.4+(B/12),10);
	spd *= overallscale;
	double depth = 0.002 / spd;
	double tupi = 3.141592653589793238 * 2.0;
	double taps = floor((A*46.0)+2.9);
	double brighten = C;
	double wet = D;
	double dry = 1.0 - wet;
	double hapi = 3.141592653589793238 / taps;
	double offset;
	double floffset;
	double start[49];
	double sinoffset[49];
	double speed[49];
	int count;
	int ensemble;
	double tempL;
	double tempR;

	double fpTemp; //this is different from singlereplacing
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	long double inputSampleL;
	long double inputSampleR;
	double drySampleL;
	double drySampleR;
	//now we'll precalculate some stuff that needn't be in every sample

	for(count = 1; count <= taps; count++)
	{
		start[count] = depth * count;
		sinoffset[count] = hapi * (count-1);
		speed[count] = spd / (1 + (count/taps));
	}
	//that's for speeding up things in the sample-processing area

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

		airFactorL = airPrevL - inputSampleL;
		if (fpFlip) {airEvenL += airFactorL; airOddL -= airFactorL; airFactorL = airEvenL;}
		else {airOddL += airFactorL; airEvenL -= airFactorL; airFactorL = airOddL;}
		airOddL = (airOddL - ((airOddL - airEvenL)/256.0)) / 1.0001;
		airEvenL = (airEvenL - ((airEvenL - airOddL)/256.0)) / 1.0001;
		airPrevL = inputSampleL;
		inputSampleL += (airFactorL*brighten);
		//air, compensates for loss of highs in flanger's interpolation

		airFactorR = airPrevR - inputSampleR;
		if (fpFlip) {airEvenR += airFactorR; airOddR -= airFactorR; airFactorR = airEvenR;}
		else {airOddR += airFactorR; airEvenR -= airFactorR; airFactorR = airOddR;}
		airOddR = (airOddR - ((airOddR - airEvenR)/256.0)) / 1.0001;
		airEvenR = (airEvenR - ((airEvenR - airOddR)/256.0)) / 1.0001;
		airPrevR = inputSampleR;
		inputSampleR += (airFactorR*brighten);
		//air, compensates for loss of highs in flanger's interpolation

		if (gcount < 1 || gcount > 32767) {gcount = 32767;}
		count = gcount;
		dL[count+32767] = dL[count] = tempL = inputSampleL;
		dR[count+32767] = dR[count] = tempR = inputSampleR;
		//double buffer

		for(ensemble = 1; ensemble <= taps; ensemble++)
		{
			offset = start[ensemble] + (depth * sin(sweep[ensemble]+sinoffset[ensemble]));
			floffset = offset-floor(offset);
			count = gcount + (int)floor(offset);

			tempL += dL[count] * (1-floffset); //less as value moves away from .0
			tempL += dL[count+1]; //we can assume always using this in one way or another?
			tempL += dL[count+2] * floffset; //greater as value moves away from .0
			tempL -= ((dL[count]-dL[count+1])-(dL[count+1]-dL[count+2]))/50; //interpolation hacks 'r us

			tempR += dR[count] * (1-floffset); //less as value moves away from .0
			tempR += dR[count+1]; //we can assume always using this in one way or another?
			tempR += dR[count+2] * floffset; //greater as value moves away from .0
			tempR -= ((dR[count]-dR[count+1])-(dR[count+1]-dR[count+2]))/50; //interpolation hacks 'r us

			sweep[ensemble] += speed[ensemble];
			if (sweep[ensemble] > tupi){sweep[ensemble] -= tupi;}
		}
		gcount--;
		//still scrolling through the samples, remember

		inputSampleL = tempL/(4.0*sqrt(taps));
		inputSampleR = tempR/(4.0*sqrt(taps));


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
