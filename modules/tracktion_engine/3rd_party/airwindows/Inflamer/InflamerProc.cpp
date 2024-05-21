/* ========================================
 *  Inflamer - Inflamer.h
 *  Copyright (c) airwindows, Airwindows uses the MIT license
 * ======================================== */

#ifndef __Inflamer_H
#include "Inflamer.h"
#endif

void Inflamer::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames) 
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];
	
	double gainControl = (A*0.5)+0.05; //0.0 to 1.0
	int gainBits = 20; //start beyond maximum attenuation
	if (gainControl > 0.0) gainBits = floor(1.0 / gainControl)-2;
	if (gainBits < -2) gainBits = -2; if (gainBits > 17) gainBits = 17;
	double gain = 1.0;
	switch (gainBits)
	{
		case 17: gain = 0.0; break;
		case 16: gain = 0.0000152587890625; break;
		case 15: gain = 0.000030517578125; break;
		case 14: gain = 0.00006103515625; break;
		case 13: gain = 0.0001220703125; break;
		case 12: gain = 0.000244140625; break;
		case 11: gain = 0.00048828125; break;
		case 10: gain = 0.0009765625; break;
		case 9: gain = 0.001953125; break;
		case 8: gain = 0.00390625; break;
		case 7: gain = 0.0078125; break;
		case 6: gain = 0.015625; break;
		case 5: gain = 0.03125; break;
		case 4: gain = 0.0625; break;
		case 3: gain = 0.125; break;
		case 2: gain = 0.25; break;
		case 1: gain = 0.5; break;
		case 0: gain = 1.0; break;
		case -1: gain = 2.0; break;
		case -2: gain = 4.0; break;
	} //now we have our input trim
	
	int bitshiftL = 1;
	int bitshiftR = 1;
	double panControl = (B*2.0)-1.0; //-1.0 to 1.0
	double panAttenuation = (1.0-fabs(panControl));
	int panBits = 20; //start centered
	if (panAttenuation > 0.0) panBits = floor(1.0 / panAttenuation);
	if (panControl > 0.25) bitshiftL += panBits;
	if (panControl < -0.25) bitshiftR += panBits;
	if (bitshiftL < -2) bitshiftL = -2; if (bitshiftL > 17) bitshiftL = 17;
	if (bitshiftR < -2) bitshiftR = -2; if (bitshiftR > 17) bitshiftR = 17;
	double negacurve = 0.5;
	double curve = 0.5;
	switch (bitshiftL)
	{
		case 17: negacurve = 0.0; break;
		case 16: negacurve = 0.0000152587890625; break;
		case 15: negacurve = 0.000030517578125; break;
		case 14: negacurve = 0.00006103515625; break;
		case 13: negacurve = 0.0001220703125; break;
		case 12: negacurve = 0.000244140625; break;
		case 11: negacurve = 0.00048828125; break;
		case 10: negacurve = 0.0009765625; break;
		case 9: negacurve = 0.001953125; break;
		case 8: negacurve = 0.00390625; break;
		case 7: negacurve = 0.0078125; break;
		case 6: negacurve = 0.015625; break;
		case 5: negacurve = 0.03125; break;
		case 4: negacurve = 0.0625; break;
		case 3: negacurve = 0.125; break;
		case 2: negacurve = 0.25; break;
		case 1: negacurve = 0.5; break;
		case 0: negacurve = 1.0; break;
		case -1: negacurve = 2.0; break;
		case -2: negacurve = 4.0; break;
	}
	switch (bitshiftR)
	{
		case 17: curve = 0.0; break;
		case 16: curve = 0.0000152587890625; break;
		case 15: curve = 0.000030517578125; break;
		case 14: curve = 0.00006103515625; break;
		case 13: curve = 0.0001220703125; break;
		case 12: curve = 0.000244140625; break;
		case 11: curve = 0.00048828125; break;
		case 10: curve = 0.0009765625; break;
		case 9: curve = 0.001953125; break;
		case 8: curve = 0.00390625; break;
		case 7: curve = 0.0078125; break;
		case 6: curve = 0.015625; break;
		case 5: curve = 0.03125; break;
		case 4: curve = 0.0625; break;
		case 3: curve = 0.125; break;
		case 2: curve = 0.25; break;
		case 1: curve = 0.5; break;
		case 0: curve = 1.0; break;
		case -1: curve = 2.0; break;
		case -2: curve = 4.0; break;
	}
	double effectOut = C;
	
    while (--sampleFrames >= 0)
    {
		double inputSampleL = *in1;
		double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;
		
		inputSampleL *= gain; //input trim
		double inflamerPlus = (inputSampleL*2.0)-pow(inputSampleL,2); //+50, very much just second harmonic
		double inflamerMinus = inputSampleL+(pow(inputSampleL,3)*0.25)-((pow(inputSampleL,2)+pow(inputSampleL,4))*0.0625); //-50
		inputSampleL = (inflamerPlus * curve) + (inflamerMinus * negacurve);
		inputSampleL = (inputSampleL * effectOut) + (drySampleL * (1.0-effectOut));
		
		inputSampleR *= gain; //input trim
		inflamerPlus = (inputSampleR*2.0)-pow(inputSampleR,2); //+50, very much just second harmonic
		inflamerMinus = inputSampleR+(pow(inputSampleR,3)*0.25)-((pow(inputSampleR,2)+pow(inputSampleR,4))*0.0625); //-50
		inputSampleR = (inflamerPlus * curve) + (inflamerMinus * negacurve);
		inputSampleR = (inputSampleR * effectOut) + (drySampleR * (1.0-effectOut));
		
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

void Inflamer::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames) 
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];
	
	double gainControl = (A*0.5)+0.05; //0.0 to 1.0
	int gainBits = 20; //start beyond maximum attenuation
	if (gainControl > 0.0) gainBits = floor(1.0 / gainControl)-2;
	if (gainBits < -2) gainBits = -2; if (gainBits > 17) gainBits = 17;
	double gain = 1.0;
	switch (gainBits)
	{
		case 17: gain = 0.0; break;
		case 16: gain = 0.0000152587890625; break;
		case 15: gain = 0.000030517578125; break;
		case 14: gain = 0.00006103515625; break;
		case 13: gain = 0.0001220703125; break;
		case 12: gain = 0.000244140625; break;
		case 11: gain = 0.00048828125; break;
		case 10: gain = 0.0009765625; break;
		case 9: gain = 0.001953125; break;
		case 8: gain = 0.00390625; break;
		case 7: gain = 0.0078125; break;
		case 6: gain = 0.015625; break;
		case 5: gain = 0.03125; break;
		case 4: gain = 0.0625; break;
		case 3: gain = 0.125; break;
		case 2: gain = 0.25; break;
		case 1: gain = 0.5; break;
		case 0: gain = 1.0; break;
		case -1: gain = 2.0; break;
		case -2: gain = 4.0; break;
	} //now we have our input trim
	
	int bitshiftL = 1;
	int bitshiftR = 1;
	double panControl = (B*2.0)-1.0; //-1.0 to 1.0
	double panAttenuation = (1.0-fabs(panControl));
	int panBits = 20; //start centered
	if (panAttenuation > 0.0) panBits = floor(1.0 / panAttenuation);
	if (panControl > 0.25) bitshiftL += panBits;
	if (panControl < -0.25) bitshiftR += panBits;
	if (bitshiftL < -2) bitshiftL = -2; if (bitshiftL > 17) bitshiftL = 17;
	if (bitshiftR < -2) bitshiftR = -2; if (bitshiftR > 17) bitshiftR = 17;
	double negacurve = 0.5;
	double curve = 0.5;
	switch (bitshiftL)
	{
		case 17: negacurve = 0.0; break;
		case 16: negacurve = 0.0000152587890625; break;
		case 15: negacurve = 0.000030517578125; break;
		case 14: negacurve = 0.00006103515625; break;
		case 13: negacurve = 0.0001220703125; break;
		case 12: negacurve = 0.000244140625; break;
		case 11: negacurve = 0.00048828125; break;
		case 10: negacurve = 0.0009765625; break;
		case 9: negacurve = 0.001953125; break;
		case 8: negacurve = 0.00390625; break;
		case 7: negacurve = 0.0078125; break;
		case 6: negacurve = 0.015625; break;
		case 5: negacurve = 0.03125; break;
		case 4: negacurve = 0.0625; break;
		case 3: negacurve = 0.125; break;
		case 2: negacurve = 0.25; break;
		case 1: negacurve = 0.5; break;
		case 0: negacurve = 1.0; break;
		case -1: negacurve = 2.0; break;
		case -2: negacurve = 4.0; break;
	}
	switch (bitshiftR)
	{
		case 17: curve = 0.0; break;
		case 16: curve = 0.0000152587890625; break;
		case 15: curve = 0.000030517578125; break;
		case 14: curve = 0.00006103515625; break;
		case 13: curve = 0.0001220703125; break;
		case 12: curve = 0.000244140625; break;
		case 11: curve = 0.00048828125; break;
		case 10: curve = 0.0009765625; break;
		case 9: curve = 0.001953125; break;
		case 8: curve = 0.00390625; break;
		case 7: curve = 0.0078125; break;
		case 6: curve = 0.015625; break;
		case 5: curve = 0.03125; break;
		case 4: curve = 0.0625; break;
		case 3: curve = 0.125; break;
		case 2: curve = 0.25; break;
		case 1: curve = 0.5; break;
		case 0: curve = 1.0; break;
		case -1: curve = 2.0; break;
		case -2: curve = 4.0; break;
	}
	double effectOut = C;
	
    while (--sampleFrames >= 0)
    {
		double inputSampleL = *in1;
		double inputSampleR = *in2;
		if (fabs(inputSampleL)<1.18e-23) inputSampleL = fpdL * 1.18e-17;
		if (fabs(inputSampleR)<1.18e-23) inputSampleR = fpdR * 1.18e-17;
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;
		
		inputSampleL *= gain; //input trim
		double inflamerPlus = (inputSampleL*2.0)-pow(inputSampleL,2); //+50, very much just second harmonic
		double inflamerMinus = inputSampleL+(pow(inputSampleL,3)*0.25)-((pow(inputSampleL,2)+pow(inputSampleL,4))*0.0625); //-50
		inputSampleL = (inflamerPlus * curve) + (inflamerMinus * negacurve);
		inputSampleL = (inputSampleL * effectOut) + (drySampleL * (1.0-effectOut));
		
		inputSampleR *= gain; //input trim
		inflamerPlus = (inputSampleR*2.0)-pow(inputSampleR,2); //+50, very much just second harmonic
		inflamerMinus = inputSampleR+(pow(inputSampleR,3)*0.25)-((pow(inputSampleR,2)+pow(inputSampleR,4))*0.0625); //-50
		inputSampleR = (inflamerPlus * curve) + (inflamerMinus * negacurve);
		inputSampleR = (inputSampleR * effectOut) + (drySampleR * (1.0-effectOut));
		
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
