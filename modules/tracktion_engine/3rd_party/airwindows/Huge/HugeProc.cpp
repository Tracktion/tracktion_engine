/* ========================================
 *  Huge - Huge.h
 *  Copyright (c) airwindows, Airwindows uses the MIT license
 * ======================================== */

#ifndef __Huge_H
#include "Huge.h"
#endif

void Huge::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();

	double huge = A;
	double threshold = (B * 0.05) + 0.05;
	double attack = (threshold * 8.0) / overallscale;
	double release = (threshold * 8.0) / overallscale;
	    
    while (--sampleFrames >= 0)
    {
		double inputSampleL = *in1;
		double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		
		inputSampleL *= huge;
		inputSampleR *= huge;
		
		double inP3L = inputSampleL * inputSampleL * inputSampleL;
		double outP3L = ((4.0*inP3L)-(3.0*inputSampleL))*-0.36;
		double inP5L = inP3L * inputSampleL * inputSampleL;
		double outP5L = ((16.0*inP5L)-(20.0*inP3L)+(5.0*inputSampleL))*0.2025;
		double inP7L = inP5L * inputSampleL * inputSampleL;
		double outP7L = ((64.0*inP7L)-(112.0*inP5L)+(56.0*inP3L)-(7.0*inputSampleL))*-0.1444;
		double inP9L = inP7L * inputSampleL * inputSampleL;
		double outP9L = ((256.0*inP9L)-(576.0*inP7L)+(432.0*inP5L)-(120.0*inP3L)+(9.0*inputSampleL))*0.1225;
		double inP11L = inP9L * inputSampleL * inputSampleL;
		double outP11L = ((1024.0*inP11L)-(2816.0*inP9L)+(2816.0*inP7L)-(1232.0*inP5L)+(220.0*inP3L)-(11.0*inputSampleL))*-0.1024;
		double inP13L = inP11L * inputSampleL * inputSampleL;
		double outP13L = ((4096.0*inP13L)-(13312.0*inP11L)+(16640.0*inP9L)-(9984.0*inP7L)+(2912.0*inP5L)-(364.0*inP3L)+(13.0*inputSampleL))*0.09;
		
		double inP3R = inputSampleR * inputSampleR * inputSampleR;
		double outP3R = ((4.0*inP3R)-(3.0*inputSampleR))*-0.36;
		double inP5R = inP3R * inputSampleR * inputSampleR;
		double outP5R = ((16.0*inP5R)-(20.0*inP3R)+(5.0*inputSampleR))*0.2025;
		double inP7R = inP5R * inputSampleR * inputSampleR;
		double outP7R = ((64.0*inP7R)-(112.0*inP5R)+(56.0*inP3R)-(7.0*inputSampleR))*-0.1444;
		double inP9R = inP7R * inputSampleR * inputSampleR;
		double outP9R = ((256.0*inP9R)-(576.0*inP7R)+(432.0*inP5R)-(120.0*inP3R)+(9.0*inputSampleR))*0.1225;
		double inP11R = inP9R * inputSampleR * inputSampleR;
		double outP11R = ((1024.0*inP11R)-(2816.0*inP9R)+(2816.0*inP7R)-(1232.0*inP5R)+(220.0*inP3R)-(11.0*inputSampleR))*-0.1024;
		double inP13R = inP11R * inputSampleR * inputSampleR;
		double outP13R = ((4096.0*inP13R)-(13312.0*inP11R)+(16640.0*inP9R)-(9984.0*inP7R)+(2912.0*inP5R)-(364.0*inP3R)+(13.0*inputSampleR))*0.09;
		
		double slew3L = fabs(outP3L - prev3L); prev3L = outP3L;
		double slew5L = fabs(outP5L - prev5L); prev5L = outP5L;
		double slew7L = fabs(outP7L - prev7L); prev7L = outP7L;
		double slew9L = fabs(outP9L - prev9L); prev9L = outP9L;
		double slew11L = fabs(outP11L - prev11L); prev11L = outP11L;
		double slew13L = fabs(outP13L - prev13L); prev13L = outP13L;
		
		double slew3R = fabs(outP3R - prev3R); prev3R = outP3R;
		double slew5R = fabs(outP5R - prev5R); prev5R = outP5R;
		double slew7R = fabs(outP7R - prev7R); prev7R = outP7R;
		double slew9R = fabs(outP9R - prev9R); prev9R = outP9R;
		double slew11R = fabs(outP11R - prev11R); prev11R = outP11R;
		double slew13R = fabs(outP13R - prev13R); prev13R = outP13R;
		
		if (slew3L < threshold) limit3L += release;
		else limit3L -= attack;
		if (slew5L < threshold) limit5L += release;
		else limit5L -= attack;
		if (slew7L < threshold) limit7L += release;
		else limit7L -= attack;
		if (slew9L < threshold) limit9L += release;
		else limit9L -= attack;
		if (slew11L < threshold) limit11L += release;
		else limit11L -= attack;
		if (slew13L < threshold) limit13L += release;
		else limit13L -= attack;
		
		if (slew3R < threshold) limit3R += release;
		else limit3R -= attack;
		if (slew5R < threshold) limit5R += release;
		else limit5R -= attack;
		if (slew7R < threshold) limit7R += release;
		else limit7R -= attack;
		if (slew9R < threshold) limit9R += release;
		else limit9R -= attack;
		if (slew11R < threshold) limit11R += release;
		else limit11R -= attack;
		if (slew13R < threshold) limit13R += release;
		else limit13R -= attack;
		
		if (limit3L > 1.0)  {limit3L = 1.0;}
		if (limit5L > 1.0)  {limit3L = 1.0; limit5L = 1.0;}
		if (limit7L > 1.0)  {limit3L = 1.0; limit5L = 1.0; limit7L = 1.0;}
		if (limit9L > 1.0)  {limit3L = 1.0; limit5L = 1.0; limit7L = 1.0; limit9L = 1.0;}
		if (limit11L > 1.0) {limit3L = 1.0; limit5L = 1.0; limit7L = 1.0; limit9L = 1.0; limit11L = 1.0;}
		if (limit13L > 1.0) {limit3L = 1.0; limit5L = 1.0; limit7L = 1.0; limit9L = 1.0; limit11L = 1.0; limit13L = 1.0;}
		if (limit3L < 0.0) {limit3L = 0.0; limit5L = 0.0; limit7L = 0.0; limit9L = 0.0; limit11L = 0.0; limit13L = 0.0;}
		if (limit5L < 0.0) {limit5L = 0.0; limit7L = 0.0; limit9L = 0.0; limit11L = 0.0; limit13L = 0.0;}
		if (limit7L < 0.0) {limit7L = 0.0; limit9L = 0.0; limit11L = 0.0; limit13L = 0.0;}
		if (limit9L < 0.0) {limit9L = 0.0; limit11L = 0.0; limit13L = 0.0;}
		if (limit11L < 0.0) {limit11L = 0.0; limit13L = 0.0;}
		if (limit13L < 0.0) {limit13L = 0.0;}
		
		if (limit3R > 1.0)  {limit3R = 1.0;}
		if (limit5R > 1.0)  {limit3R = 1.0; limit5R = 1.0;}
		if (limit7R > 1.0)  {limit3R = 1.0; limit5R = 1.0; limit7R = 1.0;}
		if (limit9R > 1.0)  {limit3R = 1.0; limit5R = 1.0; limit7R = 1.0; limit9R = 1.0;}
		if (limit11R > 1.0) {limit3R = 1.0; limit5R = 1.0; limit7R = 1.0; limit9R = 1.0; limit11R = 1.0;}
		if (limit13R > 1.0) {limit3R = 1.0; limit5R = 1.0; limit7R = 1.0; limit9R = 1.0; limit11R = 1.0; limit13R = 1.0;}
		if (limit3R < 0.0) {limit3R = 0.0; limit5R = 0.0; limit7R = 0.0; limit9R = 0.0; limit11R = 0.0; limit13R = 0.0;}
		if (limit5R < 0.0) {limit5R = 0.0; limit7R = 0.0; limit9R = 0.0; limit11R = 0.0; limit13R = 0.0;}
		if (limit7R < 0.0) {limit7R = 0.0; limit9R = 0.0; limit11R = 0.0; limit13R = 0.0;}
		if (limit9R < 0.0) {limit9R = 0.0; limit11R = 0.0; limit13R = 0.0;}
		if (limit11R < 0.0) {limit11R = 0.0; limit13R = 0.0;}
		if (limit13R < 0.0) {limit13R = 0.0;}
		
		double chebyshev = (outP3L * limit3L);
		chebyshev += (outP5L * limit5L);
		chebyshev += (outP7L * limit7L);
		chebyshev += (outP9L * limit9L);
		chebyshev += (outP11L * limit11L);
		chebyshev += (outP13L * limit13L);
		inputSampleL += ((chebyshev+prevOutL)*0.5);
		prevOutL = chebyshev;
		
		chebyshev = (outP3R * limit3R);
		chebyshev += (outP5R * limit5R);
		chebyshev += (outP7R * limit7R);
		chebyshev += (outP9R * limit9R);
		chebyshev += (outP11R * limit11R);
		chebyshev += (outP13R * limit13R);
		inputSampleR += ((chebyshev+prevOutR)*0.5);
		prevOutR = chebyshev;
		
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

void Huge::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	
	double huge = A;
	double threshold = (B * 0.05) + 0.05;
	double attack = (threshold * 8.0) / overallscale;
	double release = (threshold * 8.0) / overallscale;
	
    while (--sampleFrames >= 0)
    {
		double inputSampleL = *in1;
		double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		
		inputSampleL *= huge;
		inputSampleR *= huge;
		
		double inP3L = inputSampleL * inputSampleL * inputSampleL;
		double outP3L = ((4.0*inP3L)-(3.0*inputSampleL))*-0.36;
		double inP5L = inP3L * inputSampleL * inputSampleL;
		double outP5L = ((16.0*inP5L)-(20.0*inP3L)+(5.0*inputSampleL))*0.2025;
		double inP7L = inP5L * inputSampleL * inputSampleL;
		double outP7L = ((64.0*inP7L)-(112.0*inP5L)+(56.0*inP3L)-(7.0*inputSampleL))*-0.1444;
		double inP9L = inP7L * inputSampleL * inputSampleL;
		double outP9L = ((256.0*inP9L)-(576.0*inP7L)+(432.0*inP5L)-(120.0*inP3L)+(9.0*inputSampleL))*0.1225;
		double inP11L = inP9L * inputSampleL * inputSampleL;
		double outP11L = ((1024.0*inP11L)-(2816.0*inP9L)+(2816.0*inP7L)-(1232.0*inP5L)+(220.0*inP3L)-(11.0*inputSampleL))*-0.1024;
		double inP13L = inP11L * inputSampleL * inputSampleL;
		double outP13L = ((4096.0*inP13L)-(13312.0*inP11L)+(16640.0*inP9L)-(9984.0*inP7L)+(2912.0*inP5L)-(364.0*inP3L)+(13.0*inputSampleL))*0.09;
		
		double inP3R = inputSampleR * inputSampleR * inputSampleR;
		double outP3R = ((4.0*inP3R)-(3.0*inputSampleR))*-0.36;
		double inP5R = inP3R * inputSampleR * inputSampleR;
		double outP5R = ((16.0*inP5R)-(20.0*inP3R)+(5.0*inputSampleR))*0.2025;
		double inP7R = inP5R * inputSampleR * inputSampleR;
		double outP7R = ((64.0*inP7R)-(112.0*inP5R)+(56.0*inP3R)-(7.0*inputSampleR))*-0.1444;
		double inP9R = inP7R * inputSampleR * inputSampleR;
		double outP9R = ((256.0*inP9R)-(576.0*inP7R)+(432.0*inP5R)-(120.0*inP3R)+(9.0*inputSampleR))*0.1225;
		double inP11R = inP9R * inputSampleR * inputSampleR;
		double outP11R = ((1024.0*inP11R)-(2816.0*inP9R)+(2816.0*inP7R)-(1232.0*inP5R)+(220.0*inP3R)-(11.0*inputSampleR))*-0.1024;
		double inP13R = inP11R * inputSampleR * inputSampleR;
		double outP13R = ((4096.0*inP13R)-(13312.0*inP11R)+(16640.0*inP9R)-(9984.0*inP7R)+(2912.0*inP5R)-(364.0*inP3R)+(13.0*inputSampleR))*0.09;
		
		double slew3L = fabs(outP3L - prev3L); prev3L = outP3L;
		double slew5L = fabs(outP5L - prev5L); prev5L = outP5L;
		double slew7L = fabs(outP7L - prev7L); prev7L = outP7L;
		double slew9L = fabs(outP9L - prev9L); prev9L = outP9L;
		double slew11L = fabs(outP11L - prev11L); prev11L = outP11L;
		double slew13L = fabs(outP13L - prev13L); prev13L = outP13L;
		
		double slew3R = fabs(outP3R - prev3R); prev3R = outP3R;
		double slew5R = fabs(outP5R - prev5R); prev5R = outP5R;
		double slew7R = fabs(outP7R - prev7R); prev7R = outP7R;
		double slew9R = fabs(outP9R - prev9R); prev9R = outP9R;
		double slew11R = fabs(outP11R - prev11R); prev11R = outP11R;
		double slew13R = fabs(outP13R - prev13R); prev13R = outP13R;
		
		if (slew3L < threshold) limit3L += release;
		else limit3L -= attack;
		if (slew5L < threshold) limit5L += release;
		else limit5L -= attack;
		if (slew7L < threshold) limit7L += release;
		else limit7L -= attack;
		if (slew9L < threshold) limit9L += release;
		else limit9L -= attack;
		if (slew11L < threshold) limit11L += release;
		else limit11L -= attack;
		if (slew13L < threshold) limit13L += release;
		else limit13L -= attack;
		
		if (slew3R < threshold) limit3R += release;
		else limit3R -= attack;
		if (slew5R < threshold) limit5R += release;
		else limit5R -= attack;
		if (slew7R < threshold) limit7R += release;
		else limit7R -= attack;
		if (slew9R < threshold) limit9R += release;
		else limit9R -= attack;
		if (slew11R < threshold) limit11R += release;
		else limit11R -= attack;
		if (slew13R < threshold) limit13R += release;
		else limit13R -= attack;
		
		if (limit3L > 1.0)  {limit3L = 1.0;}
		if (limit5L > 1.0)  {limit3L = 1.0; limit5L = 1.0;}
		if (limit7L > 1.0)  {limit3L = 1.0; limit5L = 1.0; limit7L = 1.0;}
		if (limit9L > 1.0)  {limit3L = 1.0; limit5L = 1.0; limit7L = 1.0; limit9L = 1.0;}
		if (limit11L > 1.0) {limit3L = 1.0; limit5L = 1.0; limit7L = 1.0; limit9L = 1.0; limit11L = 1.0;}
		if (limit13L > 1.0) {limit3L = 1.0; limit5L = 1.0; limit7L = 1.0; limit9L = 1.0; limit11L = 1.0; limit13L = 1.0;}
		if (limit3L < 0.0) {limit3L = 0.0; limit5L = 0.0; limit7L = 0.0; limit9L = 0.0; limit11L = 0.0; limit13L = 0.0;}
		if (limit5L < 0.0) {limit5L = 0.0; limit7L = 0.0; limit9L = 0.0; limit11L = 0.0; limit13L = 0.0;}
		if (limit7L < 0.0) {limit7L = 0.0; limit9L = 0.0; limit11L = 0.0; limit13L = 0.0;}
		if (limit9L < 0.0) {limit9L = 0.0; limit11L = 0.0; limit13L = 0.0;}
		if (limit11L < 0.0) {limit11L = 0.0; limit13L = 0.0;}
		if (limit13L < 0.0) {limit13L = 0.0;}
		
		if (limit3R > 1.0)  {limit3R = 1.0;}
		if (limit5R > 1.0)  {limit3R = 1.0; limit5R = 1.0;}
		if (limit7R > 1.0)  {limit3R = 1.0; limit5R = 1.0; limit7R = 1.0;}
		if (limit9R > 1.0)  {limit3R = 1.0; limit5R = 1.0; limit7R = 1.0; limit9R = 1.0;}
		if (limit11R > 1.0) {limit3R = 1.0; limit5R = 1.0; limit7R = 1.0; limit9R = 1.0; limit11R = 1.0;}
		if (limit13R > 1.0) {limit3R = 1.0; limit5R = 1.0; limit7R = 1.0; limit9R = 1.0; limit11R = 1.0; limit13R = 1.0;}
		if (limit3R < 0.0) {limit3R = 0.0; limit5R = 0.0; limit7R = 0.0; limit9R = 0.0; limit11R = 0.0; limit13R = 0.0;}
		if (limit5R < 0.0) {limit5R = 0.0; limit7R = 0.0; limit9R = 0.0; limit11R = 0.0; limit13R = 0.0;}
		if (limit7R < 0.0) {limit7R = 0.0; limit9R = 0.0; limit11R = 0.0; limit13R = 0.0;}
		if (limit9R < 0.0) {limit9R = 0.0; limit11R = 0.0; limit13R = 0.0;}
		if (limit11R < 0.0) {limit11R = 0.0; limit13R = 0.0;}
		if (limit13R < 0.0) {limit13R = 0.0;}
		
		double chebyshev = (outP3L * limit3L);
		chebyshev += (outP5L * limit5L);
		chebyshev += (outP7L * limit7L);
		chebyshev += (outP9L * limit9L);
		chebyshev += (outP11L * limit11L);
		chebyshev += (outP13L * limit13L);
		inputSampleL += ((chebyshev+prevOutL)*0.5);
		prevOutL = chebyshev;
		
		chebyshev = (outP3R * limit3R);
		chebyshev += (outP5R * limit5R);
		chebyshev += (outP7R * limit7R);
		chebyshev += (outP9R * limit9R);
		chebyshev += (outP11R * limit11R);
		chebyshev += (outP13R * limit13R);
		inputSampleR += ((chebyshev+prevOutR)*0.5);
		prevOutR = chebyshev;
		
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
