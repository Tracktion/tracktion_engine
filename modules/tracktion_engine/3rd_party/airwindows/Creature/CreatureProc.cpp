/* ========================================
 *  Creature - Creature.h
 *  Copyright (c) airwindows, Airwindows uses the MIT license
 * ======================================== */

#ifndef __Creature_H
#include "Creature.h"
#endif

void Creature::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double source = 1.0-pow(1.0-A,5);
	int stages = (pow(B,2)*32.0*sqrt(overallscale))+1;
	double wet = (C*2.0)-1.0; //inv-dry-wet for highpass
	double dry = 2.0-(C*2.0);
	if (dry > 1.0) dry = 1.0; //full dry for use with inv, to 0.0 at full wet
	
    while (--sampleFrames >= 0)
    {
		double inputSampleL = *in1;
		double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;
		
		for (int x = 0; x < stages; x++) {
			inputSampleL = (slewL[x]+(sin(slewL[x]-inputSampleL)*0.5))*source;
			slewL[x] = inputSampleL*0.5;
			inputSampleR = (slewR[x]+(sin(slewR[x]-inputSampleR)*0.5))*source;
			slewR[x] = inputSampleR*0.5;
		}
		if (stages % 2 > 0) {
			inputSampleL = -inputSampleL;
			inputSampleR = -inputSampleR;
		}
		
		inputSampleL *= wet;
		inputSampleR *= wet;
		drySampleL *= dry;
		drySampleR *= dry;
		inputSampleL += drySampleL;
		inputSampleR += drySampleR;
		
		//begin 32 bit stereo floating point dither
		int expon; frexpf((float)inputSampleL, &expon);
		fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
		inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		frexpf((float)inputSampleR, &expon);
		fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
		inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
		//end 32 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}

void Creature::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double source = 1.0-pow(1.0-A,5);
	int stages = (pow(B,2)*32.0*sqrt(overallscale))+1;
	double wet = (C*2.0)-1.0; //inv-dry-wet for highpass
	double dry = 2.0-(C*2.0);
	if (dry > 1.0) dry = 1.0; //full dry for use with inv, to 0.0 at full wet
	
    while (--sampleFrames >= 0)
    {
		double inputSampleL = *in1;
		double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;
		
		for (int x = 0; x < stages; x++) {
			inputSampleL = (slewL[x]+(sin(slewL[x]-inputSampleL)*0.5))*source;
			slewL[x] = inputSampleL*0.5;
			inputSampleR = (slewR[x]+(sin(slewR[x]-inputSampleR)*0.5))*source;
			slewR[x] = inputSampleR*0.5;
		}
		if (stages % 2 > 0) {
			inputSampleL = -inputSampleL;
			inputSampleR = -inputSampleR;
		}
		
		inputSampleL *= wet;
		inputSampleR *= wet;
		drySampleL *= dry;
		drySampleR *= dry;
		inputSampleL += drySampleL;
		inputSampleR += drySampleR;
		
		//begin 64 bit stereo floating point dither
		//int expon; frexp((double)inputSampleL, &expon);
		fpdL ^= fpdL << 13; fpdL ^= fpdL >> 17; fpdL ^= fpdL << 5;
		//inputSampleL += ((double(fpdL)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		//frexp((double)inputSampleR, &expon);
		fpdR ^= fpdR << 13; fpdR ^= fpdR >> 17; fpdR ^= fpdR << 5;
		//inputSampleR += ((double(fpdR)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
		//end 64 bit stereo floating point dither
		
		*out1 = inputSampleL;
		*out2 = inputSampleR;

		in1++;
		in2++;
		out1++;
		out2++;
    }
}
