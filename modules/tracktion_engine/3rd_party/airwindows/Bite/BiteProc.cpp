/* ========================================
 *  Bite - Bite.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Bite_H
#include "Bite.h"
#endif

void Bite::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.3;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	float fpTemp;
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	double gain = ((A*2.0)-1.0)*overallscale;
	double outputgain = B;
	double midA;
	double midB;
	double midC;
	double midD;
	double trigger;
	double inputSampleL;
	double inputSampleR;

    while (--sampleFrames >= 0)
    {
		sampleIL = sampleHL;
		sampleHL = sampleGL;
		sampleGL = sampleFL;
		sampleFL = sampleEL;
		sampleEL = sampleDL;
		sampleDL = sampleCL;
		sampleCL = sampleBL;
		sampleBL = sampleAL;
		sampleAL = *in1;

		sampleIR = sampleHR;
		sampleHR = sampleGR;
		sampleGR = sampleFR;
		sampleFR = sampleER;
		sampleER = sampleDR;
		sampleDR = sampleCR;
		sampleCR = sampleBR;
		sampleBR = sampleAR;
		sampleAR = *in2;
		//rotate the buffer in primitive fashion
		if (sampleAL<1.2e-38 && -sampleAL<1.2e-38) {
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
			sampleAL = applyresidue;
		}
		if (sampleAR<1.2e-38 && -sampleAR<1.2e-38) {
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
			sampleAR = applyresidue;
			//this denormalization routine produces a white noise at -300 dB which the noise
			//shaping will interact with to produce a bipolar output, but the noise is actually
			//all positive. That should stop any variables from going denormal, and the routine
			//only kicks in if digital black is input. As a final touch, if you save to 24-bit
			//the silence will return to being digital black again.
		}

		midA = sampleAL - sampleEL;
		midB = sampleIL - sampleEL;
		midC = sampleCL - sampleEL;
		midD = sampleGL - sampleEL;
		midA *= ((((sampleBL + sampleCL + sampleDL)/3) - ((sampleAL + sampleEL)/2.0))*gain);
		midB *= ((((sampleFL + sampleGL + sampleHL)/3) - ((sampleEL + sampleIL)/2.0))*gain);
		midC *= ((sampleDL - ((sampleCL + sampleEL)/2.0))*gain);
		midD *= ((sampleFL - ((sampleEL + sampleGL)/2.0))*gain);
		trigger = sin(midA + midB + midC + midD);
		inputSampleL = sampleEL + (trigger*8.0);

		midA = sampleAR - sampleER;
		midB = sampleIR - sampleER;
		midC = sampleCR - sampleER;
		midD = sampleGR - sampleER;
		midA *= ((((sampleBR + sampleCR + sampleDR)/3) - ((sampleAR + sampleER)/2.0))*gain);
		midB *= ((((sampleFR + sampleGR + sampleHR)/3) - ((sampleER + sampleIR)/2.0))*gain);
		midC *= ((sampleDR - ((sampleCR + sampleER)/2.0))*gain);
		midD *= ((sampleFR - ((sampleER + sampleGR)/2.0))*gain);
		trigger = sin(midA + midB + midC + midD);
		inputSampleR = sampleER + (trigger*8.0);

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

void Bite::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.3;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	double fpTemp; //this is different from singlereplacing
	long double fpOld = 0.618033988749894848204586; //golden ratio!
	long double fpNew = 1.0 - fpOld;

	double gain = ((A*2.0)-1.0)*overallscale;
	double outputgain = B;
	double midA;
	double midB;
	double midC;
	double midD;
	double trigger;
	double inputSampleL;
	double inputSampleR;

    while (--sampleFrames >= 0)
    {
		sampleIL = sampleHL;
		sampleHL = sampleGL;
		sampleGL = sampleFL;
		sampleFL = sampleEL;
		sampleEL = sampleDL;
		sampleDL = sampleCL;
		sampleCL = sampleBL;
		sampleBL = sampleAL;
		sampleAL = *in1;

		sampleIR = sampleHR;
		sampleHR = sampleGR;
		sampleGR = sampleFR;
		sampleFR = sampleER;
		sampleER = sampleDR;
		sampleDR = sampleCR;
		sampleCR = sampleBR;
		sampleBR = sampleAR;
		sampleAR = *in2;
		//rotate the buffer in primitive fashion
		if (sampleAL<1.2e-38 && -sampleAL<1.2e-38) {
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
			sampleAL = applyresidue;
		}
		if (sampleAR<1.2e-38 && -sampleAR<1.2e-38) {
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
			sampleAR = applyresidue;
			//this denormalization routine produces a white noise at -300 dB which the noise
			//shaping will interact with to produce a bipolar output, but the noise is actually
			//all positive. That should stop any variables from going denormal, and the routine
			//only kicks in if digital black is input. As a final touch, if you save to 24-bit
			//the silence will return to being digital black again.
		}

		midA = sampleAL - sampleEL;
		midB = sampleIL - sampleEL;
		midC = sampleCL - sampleEL;
		midD = sampleGL - sampleEL;
		midA *= ((((sampleBL + sampleCL + sampleDL)/3) - ((sampleAL + sampleEL)/2.0))*gain);
		midB *= ((((sampleFL + sampleGL + sampleHL)/3) - ((sampleEL + sampleIL)/2.0))*gain);
		midC *= ((sampleDL - ((sampleCL + sampleEL)/2.0))*gain);
		midD *= ((sampleFL - ((sampleEL + sampleGL)/2.0))*gain);
		trigger = sin(midA + midB + midC + midD);
		inputSampleL = sampleEL + (trigger*8.0);

		midA = sampleAR - sampleER;
		midB = sampleIR - sampleER;
		midC = sampleCR - sampleER;
		midD = sampleGR - sampleER;
		midA *= ((((sampleBR + sampleCR + sampleDR)/3) - ((sampleAR + sampleER)/2.0))*gain);
		midB *= ((((sampleFR + sampleGR + sampleHR)/3) - ((sampleER + sampleIR)/2.0))*gain);
		midC *= ((sampleDR - ((sampleCR + sampleER)/2.0))*gain);
		midD *= ((sampleFR - ((sampleER + sampleGR)/2.0))*gain);
		trigger = sin(midA + midB + midC + midD);
		inputSampleR = sampleER + (trigger*8.0);

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
