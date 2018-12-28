/* ========================================
 *  Ditherbox - Ditherbox.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Ditherbox_H
#include "Ditherbox.h"
#endif

void Ditherbox::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	int dtype = (int)(A * 24.999)+1; // +1 for Reaper bug workaround
	long double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	long double iirAmount = 2250/44100.0;
	long double gaintarget = 1.42;
	long double gain;
	iirAmount /= overallscale;
	long double altAmount = 1.0 - iirAmount;
	long double outputSampleL;
	long double outputSampleR;
	long double silhouette;
	long double smoother;
	long double bridgerectifier;
	long double benfordize;
	int hotbinA;
	int hotbinB;
	long double totalA;
	long double totalB;
	long double contingentRnd;
	long double absSample;
	long double contingent;
	long double randyConstant = 1.61803398874989484820458683436563811772030917980576;
	long double omegaConstant = 0.56714329040978387299996866221035554975381578718651;
	long double expConstant = 0.06598803584531253707679018759684642493857704825279;
	long double trim = 2.302585092994045684017991; //natural logarithm of 10
	bool highRes = false;
	bool dithering = true;
	if (dtype > 11){highRes = true; dtype -= 11;}
	if (dtype > 11){dithering = false; highRes = false;}
	//follow up by switching high res back off for the monitoring

    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
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
		float drySampleL = inputSampleL;
		float drySampleR = inputSampleR;

		if (dtype == 8) {inputSampleL -= noiseShapingL; inputSampleR -= noiseShapingR;}

		if (dithering) {inputSampleL *= 32768.0; inputSampleR *= 32768.0;}
		//denormalizing as way of controlling insane detail boosting
		if (highRes) {inputSampleL *= 256.0; inputSampleR *= 256.0;} //256 for 16/24 version

		switch (dtype)
		{
			case 1:
				inputSampleL = floor(inputSampleL);
				inputSampleR = floor(inputSampleR);
				//truncate
				break;

			case 2:
				inputSampleL += (rand()/(double)RAND_MAX);
				inputSampleL -= 0.5;
				inputSampleL = floor(inputSampleL);
				inputSampleR += (rand()/(double)RAND_MAX);
				inputSampleR -= 0.5;
				inputSampleR = floor(inputSampleR);
				//flat dither
				break;

			case 3:
				inputSampleL += (rand()/(double)RAND_MAX);
				inputSampleL += (rand()/(double)RAND_MAX);
				inputSampleL -= 1.0;
				inputSampleL = floor(inputSampleL);
				inputSampleR += (rand()/(double)RAND_MAX);
				inputSampleR += (rand()/(double)RAND_MAX);
				inputSampleR -= 1.0;
				inputSampleR = floor(inputSampleR);
				//TPDF dither
				break;

			case 4:
				currentDitherL = (rand()/(double)RAND_MAX);
				inputSampleL += currentDitherL;
				inputSampleL -= lastSampleL;
				inputSampleL = floor(inputSampleL);
				lastSampleL = currentDitherL;
				currentDitherR = (rand()/(double)RAND_MAX);
				inputSampleR += currentDitherR;
				inputSampleR -= lastSampleR;
				inputSampleR = floor(inputSampleR);
				lastSampleR = currentDitherR;
				//Paul dither
				break;

			case 5:
				nsL[9] = nsL[8]; nsL[8] = nsL[7]; nsL[7] = nsL[6]; nsL[6] = nsL[5];
				nsL[5] = nsL[4]; nsL[4] = nsL[3]; nsL[3] = nsL[2]; nsL[2] = nsL[1];
				nsL[1] = nsL[0]; nsL[0] = (rand()/(double)RAND_MAX);

				currentDitherL  = (nsL[0] * 0.061);
				currentDitherL -= (nsL[1] * 0.11);
				currentDitherL += (nsL[8] * 0.126);
				currentDitherL -= (nsL[7] * 0.23);
				currentDitherL += (nsL[2] * 0.25);
				currentDitherL -= (nsL[3] * 0.43);
				currentDitherL += (nsL[6] * 0.5);
				currentDitherL -= nsL[5];
				currentDitherL += nsL[4];
				//this sounds different from doing it in order of sample position
				//cumulative tiny errors seem to build up even at this buss depth
				//considerably more pronounced at 32 bit float.
				//Therefore we add the most significant components LAST.
				//trying to keep values on like exponents of the floating point value.
				inputSampleL += currentDitherL;

				inputSampleL = floor(inputSampleL);
				//done with L

				nsR[9] = nsR[8]; nsR[8] = nsR[7]; nsR[7] = nsR[6]; nsR[6] = nsR[5];
				nsR[5] = nsR[4]; nsR[4] = nsR[3]; nsR[3] = nsR[2]; nsR[2] = nsR[1];
				nsR[1] = nsR[0]; nsR[0] = (rand()/(double)RAND_MAX);

				currentDitherR  = (nsR[0] * 0.061);
				currentDitherR -= (nsR[1] * 0.11);
				currentDitherR += (nsR[8] * 0.126);
				currentDitherR -= (nsR[7] * 0.23);
				currentDitherR += (nsR[2] * 0.25);
				currentDitherR -= (nsR[3] * 0.43);
				currentDitherR += (nsR[6] * 0.5);
				currentDitherR -= nsR[5];
				currentDitherR += nsR[4];
				//this sounds different from doing it in order of sample position
				//cumulative tiny errors seem to build up even at this buss depth
				//considerably more pronounced at 32 bit float.
				//Therefore we add the most significant components LAST.
				//trying to keep values on like exponents of the floating point value.
				inputSampleR += currentDitherR;

				inputSampleR = floor(inputSampleR);
				//done with R

				//DoublePaul dither
				break;

			case 6:
				currentDitherL = (rand()/(double)RAND_MAX);
				currentDitherR = (rand()/(double)RAND_MAX);

				inputSampleL += currentDitherL;
				inputSampleR += currentDitherR;
				inputSampleL -= nsL[4];
				inputSampleR -= nsR[4];

				inputSampleL = floor(inputSampleL);
				inputSampleR = floor(inputSampleR);

				nsL[4] = nsL[3];
				nsL[3] = nsL[2];
				nsL[2] = nsL[1];
				nsL[1] = currentDitherL;

				nsR[4] = nsR[3];
				nsR[3] = nsR[2];
				nsR[2] = nsR[1];
				nsR[1] = currentDitherR;
				//Tape dither
				break;

			case 7:
				Position += 1;
				//Note- uses integer overflow as a 'mod' operator
				hotbinA = Position * Position;
				hotbinA = hotbinA % 170003; //% is C++ mod operator
				hotbinA *= hotbinA;
				hotbinA = hotbinA % 17011; //% is C++ mod operator
				hotbinA *= hotbinA;
				hotbinA = hotbinA % 1709; //% is C++ mod operator
				hotbinA *= hotbinA;
				hotbinA = hotbinA % 173; //% is C++ mod operator
				hotbinA *= hotbinA;
				hotbinA = hotbinA % 17;
				hotbinA *= 0.0635;
				if (flip) hotbinA = -hotbinA;
				inputSampleL += hotbinA;
				inputSampleR += hotbinA;
				inputSampleL = floor(inputSampleL);
				inputSampleR = floor(inputSampleR);
				//Quadratic dither
				break;

			case 8:
				absSample = ((rand()/(double)RAND_MAX) - 0.5);
				nsL[0] += absSample; nsL[0] /= 2; absSample -= nsL[0];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[1] += absSample; nsL[1] /= 2; absSample -= nsL[1];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[2] += absSample; nsL[2] /= 2; absSample -= nsL[2];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[3] += absSample; nsL[3] /= 2; absSample -= nsL[3];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[4] += absSample; nsL[4] /= 2; absSample -= nsL[4];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[5] += absSample; nsL[5] /= 2; absSample -= nsL[5];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[6] += absSample; nsL[6] /= 2; absSample -= nsL[6];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[7] += absSample; nsL[7] /= 2; absSample -= nsL[7];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[8] += absSample; nsL[8] /= 2; absSample -= nsL[8];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[9] += absSample; nsL[9] /= 2; absSample -= nsL[9];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[10] += absSample; nsL[10] /= 2; absSample -= nsL[10];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[11] += absSample; nsL[11] /= 2; absSample -= nsL[11];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[12] += absSample; nsL[12] /= 2; absSample -= nsL[12];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[13] += absSample; nsL[13] /= 2; absSample -= nsL[13];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[14] += absSample; nsL[14] /= 2; absSample -= nsL[14];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[15] += absSample; nsL[15] /= 2; absSample -= nsL[15];
				//install noise and then shape it
				absSample += inputSampleL;

				if (NSOddL > 0) NSOddL -= 0.97;
				if (NSOddL < 0) NSOddL += 0.97;

				NSOddL -= (NSOddL * NSOddL * NSOddL * 0.475);

				NSOddL += prevL;
				absSample += (NSOddL*0.475);
				prevL = floor(absSample) - inputSampleL;
				inputSampleL = floor(absSample);
				//TenNines dither L


				absSample = ((rand()/(double)RAND_MAX) - 0.5);
				nsR[0] += absSample; nsR[0] /= 2; absSample -= nsR[0];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[1] += absSample; nsR[1] /= 2; absSample -= nsR[1];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[2] += absSample; nsR[2] /= 2; absSample -= nsR[2];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[3] += absSample; nsR[3] /= 2; absSample -= nsR[3];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[4] += absSample; nsR[4] /= 2; absSample -= nsR[4];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[5] += absSample; nsR[5] /= 2; absSample -= nsR[5];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[6] += absSample; nsR[6] /= 2; absSample -= nsR[6];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[7] += absSample; nsR[7] /= 2; absSample -= nsR[7];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[8] += absSample; nsR[8] /= 2; absSample -= nsR[8];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[9] += absSample; nsR[9] /= 2; absSample -= nsR[9];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[10] += absSample; nsR[10] /= 2; absSample -= nsR[10];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[11] += absSample; nsR[11] /= 2; absSample -= nsR[11];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[12] += absSample; nsR[12] /= 2; absSample -= nsR[12];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[13] += absSample; nsR[13] /= 2; absSample -= nsR[13];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[14] += absSample; nsR[14] /= 2; absSample -= nsR[14];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[15] += absSample; nsR[15] /= 2; absSample -= nsR[15];
				//install noise and then shape it
				absSample += inputSampleR;

				if (NSOddR > 0) NSOddR -= 0.97;
				if (NSOddR < 0) NSOddR += 0.97;

				NSOddR -= (NSOddR * NSOddR * NSOddR * 0.475);

				NSOddR += prevR;
				absSample += (NSOddR*0.475);
				prevR = floor(absSample) - inputSampleR;
				inputSampleR = floor(absSample);
				//TenNines dither R
				break;

			case 9:
				if (inputSampleL > 0) inputSampleL += 0.383;
				if (inputSampleL < 0) inputSampleL -= 0.383;
				if (inputSampleR > 0) inputSampleR += 0.383;
				if (inputSampleR < 0) inputSampleR -= 0.383;
				//adjusting to permit more information drug outta the noisefloor

				contingentRnd = (((rand()/(double)RAND_MAX)+(rand()/(double)RAND_MAX))-1.0) * randyConstant; //produce TPDF dist, scale
				contingentRnd -= contingentErrL*omegaConstant; //include err
				absSample = fabs(inputSampleL);
				contingentErrL = absSample - floor(absSample); //get next err
				contingent = contingentErrL * 2.0; //scale of quantization levels
				if (contingent > 1.0) contingent = ((-contingent+2.0)*omegaConstant) + expConstant;
				else contingent = (contingent * omegaConstant) + expConstant;
				//zero is next to a quantization level, one is exactly between them
				if (flip) contingentRnd = (contingentRnd * (1.0-contingent)) + contingent + 0.5;
				else contingentRnd = (contingentRnd * (1.0-contingent)) - contingent + 0.5;
				inputSampleL += (contingentRnd * contingent);
				//Contingent Dither
				inputSampleL = floor(inputSampleL);

				contingentRnd = (((rand()/(double)RAND_MAX)+(rand()/(double)RAND_MAX))-1.0) * randyConstant; //produce TPDF dist, scale
				contingentRnd -= contingentErrR*omegaConstant; //include err
				absSample = fabs(inputSampleR);
				contingentErrR = absSample - floor(absSample); //get next err
				contingent = contingentErrR * 2.0; //scale of quantization levels
				if (contingent > 1.0) contingent = ((-contingent+2.0)*omegaConstant) + expConstant;
				else contingent = (contingent * omegaConstant) + expConstant;
				//zero is next to a quantization level, one is exactly between them
				if (flip) contingentRnd = (contingentRnd * (1.0-contingent)) + contingent + 0.5;
				else contingentRnd = (contingentRnd * (1.0-contingent)) - contingent + 0.5;
				inputSampleR += (contingentRnd * contingent);
				//Contingent Dither
				inputSampleR = floor(inputSampleR);

				//note: this does not dither for values exactly the same as 16 bit values-
				//which forces the dither to gate at 0.0. It goes to digital black,
				//and does a teeny parallel-compression thing when almost at digital black.
				break;

			case 10: //this one is the original Naturalize
				if (inputSampleL > 0) inputSampleL += (0.3333333333);
				if (inputSampleL < 0) inputSampleL -= (0.3333333333);
				inputSampleL += (rand()/(double)RAND_MAX)*0.6666666666;

				if (inputSampleR > 0) inputSampleR += (0.3333333333);
				if (inputSampleR < 0) inputSampleR -= (0.3333333333);
				inputSampleR += (rand()/(double)RAND_MAX)*0.6666666666;

				//begin L
				benfordize = floor(inputSampleL);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinA = floor(benfordize);
				//hotbin becomes the Benford bin value for this number floored
				totalA = 0;
				if ((hotbinA > 0) && (hotbinA < 10))
				{
					bynL[hotbinA] += 1;
					totalA += (301-bynL[1]);
					totalA += (176-bynL[2]);
					totalA += (125-bynL[3]);
					totalA += (97-bynL[4]);
					totalA += (79-bynL[5]);
					totalA += (67-bynL[6]);
					totalA += (58-bynL[7]);
					totalA += (51-bynL[8]);
					totalA += (46-bynL[9]);
					bynL[hotbinA] -= 1;
				} else {hotbinA = 10;}
				//produce total number- smaller is closer to Benford real

				benfordize = ceil(inputSampleL);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinB = floor(benfordize);
				//hotbin becomes the Benford bin value for this number ceiled
				totalB = 0;
				if ((hotbinB > 0) && (hotbinB < 10))
				{
					bynL[hotbinB] += 1;
					totalB += (301-bynL[1]);
					totalB += (176-bynL[2]);
					totalB += (125-bynL[3]);
					totalB += (97-bynL[4]);
					totalB += (79-bynL[5]);
					totalB += (67-bynL[6]);
					totalB += (58-bynL[7]);
					totalB += (51-bynL[8]);
					totalB += (46-bynL[9]);
					bynL[hotbinB] -= 1;
				} else {hotbinB = 10;}
				//produce total number- smaller is closer to Benford real

				if (totalA < totalB)
				{
					bynL[hotbinA] += 1;
					inputSampleL = floor(inputSampleL);
				}
				else
				{
					bynL[hotbinB] += 1;
					inputSampleL = ceil(inputSampleL);
				}
				//assign the relevant one to the delay line
				//and floor/ceil signal accordingly

				totalA = bynL[1] + bynL[2] + bynL[3] + bynL[4] + bynL[5] + bynL[6] + bynL[7] + bynL[8] + bynL[9];
				totalA /= 1000;
				if (totalA = 0) totalA = 1;
				bynL[1] /= totalA;
				bynL[2] /= totalA;
				bynL[3] /= totalA;
				bynL[4] /= totalA;
				bynL[5] /= totalA;
				bynL[6] /= totalA;
				bynL[7] /= totalA;
				bynL[8] /= totalA;
				bynL[9] /= totalA;
				bynL[10] /= 2; //catchall for garbage data
				//end L

				//begin R
				benfordize = floor(inputSampleR);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinA = floor(benfordize);
				//hotbin becomes the Benford bin value for this number floored
				totalA = 0;
				if ((hotbinA > 0) && (hotbinA < 10))
				{
					bynR[hotbinA] += 1;
					totalA += (301-bynR[1]);
					totalA += (176-bynR[2]);
					totalA += (125-bynR[3]);
					totalA += (97-bynR[4]);
					totalA += (79-bynR[5]);
					totalA += (67-bynR[6]);
					totalA += (58-bynR[7]);
					totalA += (51-bynR[8]);
					totalA += (46-bynR[9]);
					bynR[hotbinA] -= 1;
				} else {hotbinA = 10;}
				//produce total number- smaller is closer to Benford real

				benfordize = ceil(inputSampleR);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinB = floor(benfordize);
				//hotbin becomes the Benford bin value for this number ceiled
				totalB = 0;
				if ((hotbinB > 0) && (hotbinB < 10))
				{
					bynR[hotbinB] += 1;
					totalB += (301-bynR[1]);
					totalB += (176-bynR[2]);
					totalB += (125-bynR[3]);
					totalB += (97-bynR[4]);
					totalB += (79-bynR[5]);
					totalB += (67-bynR[6]);
					totalB += (58-bynR[7]);
					totalB += (51-bynR[8]);
					totalB += (46-bynR[9]);
					bynR[hotbinB] -= 1;
				} else {hotbinB = 10;}
				//produce total number- smaller is closer to Benford real

				if (totalA < totalB)
				{
					bynR[hotbinA] += 1;
					inputSampleR = floor(inputSampleR);
				}
				else
				{
					bynR[hotbinB] += 1;
					inputSampleR = ceil(inputSampleR);
				}
				//assign the relevant one to the delay line
				//and floor/ceil signal accordingly

				totalA = bynR[1] + bynR[2] + bynR[3] + bynR[4] + bynR[5] + bynR[6] + bynR[7] + bynR[8] + bynR[9];
				totalA /= 1000;
				if (totalA = 0) totalA = 1;
				bynR[1] /= totalA;
				bynR[2] /= totalA;
				bynR[3] /= totalA;
				bynR[4] /= totalA;
				bynR[5] /= totalA;
				bynR[6] /= totalA;
				bynR[7] /= totalA;
				bynR[8] /= totalA;
				bynR[9] /= totalA;
				bynR[10] /= 2; //catchall for garbage data
				//end R
				break;

			case 11: //this one is the Not Just Another Dither

				//begin L
				benfordize = floor(inputSampleL);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinA = floor(benfordize);
				//hotbin becomes the Benford bin value for this number floored
				totalA = 0;
				if ((hotbinA > 0) && (hotbinA < 10))
				{
					bynL[hotbinA] += 1;
					totalA += (301-bynL[1]);
					totalA += (176-bynL[2]);
					totalA += (125-bynL[3]);
					totalA += (97-bynL[4]);
					totalA += (79-bynL[5]);
					totalA += (67-bynL[6]);
					totalA += (58-bynL[7]);
					totalA += (51-bynL[8]);
					totalA += (46-bynL[9]);
					bynL[hotbinA] -= 1;
				} else {hotbinA = 10;}
				//produce total number- smaller is closer to Benford real

				benfordize = ceil(inputSampleL);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinB = floor(benfordize);
				//hotbin becomes the Benford bin value for this number ceiled
				totalB = 0;
				if ((hotbinB > 0) && (hotbinB < 10))
				{
					bynL[hotbinB] += 1;
					totalB += (301-bynL[1]);
					totalB += (176-bynL[2]);
					totalB += (125-bynL[3]);
					totalB += (97-bynL[4]);
					totalB += (79-bynL[5]);
					totalB += (67-bynL[6]);
					totalB += (58-bynL[7]);
					totalB += (51-bynL[8]);
					totalB += (46-bynL[9]);
					bynL[hotbinB] -= 1;
				} else {hotbinB = 10;}
				//produce total number- smaller is closer to Benford real

				if (totalA < totalB)
				{
					bynL[hotbinA] += 1;
					inputSampleL = floor(inputSampleL);
				}
				else
				{
					bynL[hotbinB] += 1;
					inputSampleL = ceil(inputSampleL);
				}
				//assign the relevant one to the delay line
				//and floor/ceil signal accordingly

				totalA = bynL[1] + bynL[2] + bynL[3] + bynL[4] + bynL[5] + bynL[6] + bynL[7] + bynL[8] + bynL[9];
				totalA /= 1000;
				if (totalA = 0) totalA = 1;
				bynL[1] /= totalA;
				bynL[2] /= totalA;
				bynL[3] /= totalA;
				bynL[4] /= totalA;
				bynL[5] /= totalA;
				bynL[6] /= totalA;
				bynL[7] /= totalA;
				bynL[8] /= totalA;
				bynL[9] /= totalA;
				bynL[10] /= 2; //catchall for garbage data
				//end L

				//begin R
				benfordize = floor(inputSampleR);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinA = floor(benfordize);
				//hotbin becomes the Benford bin value for this number floored
				totalA = 0;
				if ((hotbinA > 0) && (hotbinA < 10))
				{
					bynR[hotbinA] += 1;
					totalA += (301-bynR[1]);
					totalA += (176-bynR[2]);
					totalA += (125-bynR[3]);
					totalA += (97-bynR[4]);
					totalA += (79-bynR[5]);
					totalA += (67-bynR[6]);
					totalA += (58-bynR[7]);
					totalA += (51-bynR[8]);
					totalA += (46-bynR[9]);
					bynR[hotbinA] -= 1;
				} else {hotbinA = 10;}
				//produce total number- smaller is closer to Benford real

				benfordize = ceil(inputSampleR);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinB = floor(benfordize);
				//hotbin becomes the Benford bin value for this number ceiled
				totalB = 0;
				if ((hotbinB > 0) && (hotbinB < 10))
				{
					bynR[hotbinB] += 1;
					totalB += (301-bynR[1]);
					totalB += (176-bynR[2]);
					totalB += (125-bynR[3]);
					totalB += (97-bynR[4]);
					totalB += (79-bynR[5]);
					totalB += (67-bynR[6]);
					totalB += (58-bynR[7]);
					totalB += (51-bynR[8]);
					totalB += (46-bynR[9]);
					bynR[hotbinB] -= 1;
				} else {hotbinB = 10;}
				//produce total number- smaller is closer to Benford real

				if (totalA < totalB)
				{
					bynR[hotbinA] += 1;
					inputSampleR = floor(inputSampleR);
				}
				else
				{
					bynR[hotbinB] += 1;
					inputSampleR = ceil(inputSampleR);
				}
				//assign the relevant one to the delay line
				//and floor/ceil signal accordingly

				totalA = bynR[1] + bynR[2] + bynR[3] + bynR[4] + bynR[5] + bynR[6] + bynR[7] + bynR[8] + bynR[9];
				totalA /= 1000;
				if (totalA = 0) totalA = 1;
				bynR[1] /= totalA;
				bynR[2] /= totalA;
				bynR[3] /= totalA;
				bynR[4] /= totalA;
				bynR[5] /= totalA;
				bynR[6] /= totalA;
				bynR[7] /= totalA;
				bynR[8] /= totalA;
				bynR[9] /= totalA;
				bynR[10] /= 2; //catchall for garbage data
				//end R
				break;

			case 12:
				//slew only
				outputSampleL = (inputSampleL - lastSampleL)*trim;
				outputSampleR = (inputSampleR - lastSampleR)*trim;
				lastSampleL = inputSampleL;
				lastSampleR = inputSampleR;
				if (outputSampleL > 1.0) outputSampleL = 1.0;
				if (outputSampleR > 1.0) outputSampleR = 1.0;
				if (outputSampleL < -1.0) outputSampleL = -1.0;
				if (outputSampleR < -1.0) outputSampleR = -1.0;
				inputSampleL = outputSampleL;
				inputSampleR = outputSampleR;
				break;

			case 13:
				//subs only
				gain = gaintarget;

				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				iirSampleAL = (iirSampleAL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleAL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleBL = (iirSampleBL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleBL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleCL = (iirSampleCL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleCL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleDL = (iirSampleDL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleDL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleEL = (iirSampleEL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleEL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleFL = (iirSampleFL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleFL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleGL = (iirSampleGL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleGL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleHL = (iirSampleHL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleHL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleIL = (iirSampleIL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleIL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleJL = (iirSampleJL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleJL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleKL = (iirSampleKL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleKL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleLL = (iirSampleLL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleLL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleML = (iirSampleML * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleML;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleNL = (iirSampleNL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleNL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleOL = (iirSampleOL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleOL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSamplePL = (iirSamplePL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSamplePL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleQL = (iirSampleQL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleQL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleRL = (iirSampleRL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleRL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleSL = (iirSampleSL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleSL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleTL = (iirSampleTL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleTL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleUL = (iirSampleUL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleUL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleVL = (iirSampleVL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleVL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleWL = (iirSampleWL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleWL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleXL = (iirSampleXL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleXL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleYL = (iirSampleYL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleYL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleZL = (iirSampleZL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleZL;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				gain = gaintarget;

				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				iirSampleAR = (iirSampleAR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleAR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleBR = (iirSampleBR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleBR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleCR = (iirSampleCR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleCR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleDR = (iirSampleDR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleDR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleER = (iirSampleER * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleER;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleFR = (iirSampleFR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleFR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleGR = (iirSampleGR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleGR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleHR = (iirSampleHR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleHR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleIR = (iirSampleIR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleIR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleJR = (iirSampleJR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleJR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleKR = (iirSampleKR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleKR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleLR = (iirSampleLR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleLR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleMR = (iirSampleMR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleMR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleNR = (iirSampleNR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleNR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleOR = (iirSampleOR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleOR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSamplePR = (iirSamplePR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSamplePR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleQR = (iirSampleQR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleQR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleRR = (iirSampleRR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleRR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleSR = (iirSampleSR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleSR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleTR = (iirSampleTR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleTR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleUR = (iirSampleUR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleUR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleVR = (iirSampleVR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleVR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleWR = (iirSampleWR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleWR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleXR = (iirSampleXR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleXR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleYR = (iirSampleYR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleYR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleZR = (iirSampleZR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleZR;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				break;

			case 14:
				//silhouette
				//begin L
				bridgerectifier = fabs(inputSampleL)*1.57079633;
				if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
				bridgerectifier = 1.0-cos(bridgerectifier);
				if (inputSampleL > 0.0) inputSampleL = bridgerectifier;
				else inputSampleL = -bridgerectifier;

				silhouette = rand()/(double)RAND_MAX;
				silhouette -= 0.5;
				silhouette *= 2.0;
				silhouette *= fabs(inputSampleL);

				smoother = rand()/(double)RAND_MAX;
				smoother -= 0.5;
				smoother *= 2.0;
				smoother *= fabs(lastSampleL);
				lastSampleL = inputSampleL;

				silhouette += smoother;

				bridgerectifier = fabs(silhouette)*1.57079633;
				if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
				bridgerectifier = sin(bridgerectifier);
				if (silhouette > 0.0) silhouette = bridgerectifier;
				else silhouette = -bridgerectifier;

				inputSampleL = (silhouette + outSampleL) / 2.0;
				outSampleL = silhouette;
				//end L

				//begin R
				bridgerectifier = fabs(inputSampleR)*1.57079633;
				if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
				bridgerectifier = 1.0-cos(bridgerectifier);
				if (inputSampleR > 0.0) inputSampleR = bridgerectifier;
				else inputSampleR = -bridgerectifier;

				silhouette = rand()/(double)RAND_MAX;
				silhouette -= 0.5;
				silhouette *= 2.0;
				silhouette *= fabs(inputSampleR);

				smoother = rand()/(double)RAND_MAX;
				smoother -= 0.5;
				smoother *= 2.0;
				smoother *= fabs(lastSampleR);
				lastSampleR = inputSampleR;

				silhouette += smoother;

				bridgerectifier = fabs(silhouette)*1.57079633;
				if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
				bridgerectifier = sin(bridgerectifier);
				if (silhouette > 0.0) silhouette = bridgerectifier;
				else silhouette = -bridgerectifier;

				inputSampleR = (silhouette + outSampleR) / 2.0;
				outSampleR = silhouette;
				//end R
				break;
		}

		flip = !flip;
		//several dithers use this

		if (highRes) {inputSampleL /= 256.0; inputSampleR /= 256.0;} //256 for 16/24 version
		if (dithering) {inputSampleL /= 32768.0; inputSampleR /= 32768.0;}

		if (dtype == 8) {
			noiseShapingL += inputSampleL - drySampleL;
			noiseShapingR += inputSampleR - drySampleR;
		}

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}

void Ditherbox::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	int dtype = (int)(A * 24.999)+1; // +1 for Reaper bug workaround
	long double overallscale = 1.0;
	overallscale /= 44100.0;
	overallscale *= getSampleRate();
	long double iirAmount = 2250/44100.0;
	long double gaintarget = 1.42;
	long double gain;
	iirAmount /= overallscale;
	long double altAmount = 1.0 - iirAmount;
	long double outputSampleL;
	long double outputSampleR;
	long double silhouette;
	long double smoother;
	long double bridgerectifier;
	long double benfordize;
	int hotbinA;
	int hotbinB;
	long double totalA;
	long double totalB;
	long double contingentRnd;
	long double absSample;
	long double contingent;
	long double randyConstant = 1.61803398874989484820458683436563811772030917980576;
	long double omegaConstant = 0.56714329040978387299996866221035554975381578718651;
	long double expConstant = 0.06598803584531253707679018759684642493857704825279;
	long double trim = 2.302585092994045684017991; //natural logarithm of 10
	bool highRes = false;
	bool dithering = true;
	if (dtype > 11){highRes = true; dtype -= 11;}
	if (dtype > 11){dithering = false; highRes = false;}
	//follow up by switching high res back off for the monitoring

    while (--sampleFrames >= 0)
    {
		long double inputSampleL = *in1;
		long double inputSampleR = *in2;
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
		double drySampleL = inputSampleL;
		double drySampleR = inputSampleR;

		if (dtype == 8) {inputSampleL -= noiseShapingL; inputSampleR -= noiseShapingR;}

		if (dithering) {inputSampleL *= 32768.0; inputSampleR *= 32768.0;}
		//denormalizing as way of controlling insane detail boosting
		if (highRes) {inputSampleL *= 256.0; inputSampleR *= 256.0;} //256 for 16/24 version

		switch (dtype)
		{
			case 1:
				inputSampleL = floor(inputSampleL);
				inputSampleR = floor(inputSampleR);
				//truncate
				break;

			case 2:
				inputSampleL += (rand()/(double)RAND_MAX);
				inputSampleL -= 0.5;
				inputSampleL = floor(inputSampleL);
				inputSampleR += (rand()/(double)RAND_MAX);
				inputSampleR -= 0.5;
				inputSampleR = floor(inputSampleR);
				//flat dither
				break;

			case 3:
				inputSampleL += (rand()/(double)RAND_MAX);
				inputSampleL += (rand()/(double)RAND_MAX);
				inputSampleL -= 1.0;
				inputSampleL = floor(inputSampleL);
				inputSampleR += (rand()/(double)RAND_MAX);
				inputSampleR += (rand()/(double)RAND_MAX);
				inputSampleR -= 1.0;
				inputSampleR = floor(inputSampleR);
				//TPDF dither
				break;

			case 4:
				currentDitherL = (rand()/(double)RAND_MAX);
				inputSampleL += currentDitherL;
				inputSampleL -= lastSampleL;
				inputSampleL = floor(inputSampleL);
				lastSampleL = currentDitherL;
				currentDitherR = (rand()/(double)RAND_MAX);
				inputSampleR += currentDitherR;
				inputSampleR -= lastSampleR;
				inputSampleR = floor(inputSampleR);
				lastSampleR = currentDitherR;
				//Paul dither
				break;

			case 5:
				nsL[9] = nsL[8]; nsL[8] = nsL[7]; nsL[7] = nsL[6]; nsL[6] = nsL[5];
				nsL[5] = nsL[4]; nsL[4] = nsL[3]; nsL[3] = nsL[2]; nsL[2] = nsL[1];
				nsL[1] = nsL[0]; nsL[0] = (rand()/(double)RAND_MAX);

				currentDitherL  = (nsL[0] * 0.061);
				currentDitherL -= (nsL[1] * 0.11);
				currentDitherL += (nsL[8] * 0.126);
				currentDitherL -= (nsL[7] * 0.23);
				currentDitherL += (nsL[2] * 0.25);
				currentDitherL -= (nsL[3] * 0.43);
				currentDitherL += (nsL[6] * 0.5);
				currentDitherL -= nsL[5];
				currentDitherL += nsL[4];
				//this sounds different from doing it in order of sample position
				//cumulative tiny errors seem to build up even at this buss depth
				//considerably more pronounced at 32 bit float.
				//Therefore we add the most significant components LAST.
				//trying to keep values on like exponents of the floating point value.
				inputSampleL += currentDitherL;

				inputSampleL = floor(inputSampleL);
				//done with L

				nsR[9] = nsR[8]; nsR[8] = nsR[7]; nsR[7] = nsR[6]; nsR[6] = nsR[5];
				nsR[5] = nsR[4]; nsR[4] = nsR[3]; nsR[3] = nsR[2]; nsR[2] = nsR[1];
				nsR[1] = nsR[0]; nsR[0] = (rand()/(double)RAND_MAX);

				currentDitherR  = (nsR[0] * 0.061);
				currentDitherR -= (nsR[1] * 0.11);
				currentDitherR += (nsR[8] * 0.126);
				currentDitherR -= (nsR[7] * 0.23);
				currentDitherR += (nsR[2] * 0.25);
				currentDitherR -= (nsR[3] * 0.43);
				currentDitherR += (nsR[6] * 0.5);
				currentDitherR -= nsR[5];
				currentDitherR += nsR[4];
				//this sounds different from doing it in order of sample position
				//cumulative tiny errors seem to build up even at this buss depth
				//considerably more pronounced at 32 bit float.
				//Therefore we add the most significant components LAST.
				//trying to keep values on like exponents of the floating point value.
				inputSampleR += currentDitherR;

				inputSampleR = floor(inputSampleR);
				//done with R
				//DoublePaul dither
				break;

			case 6:
				currentDitherL = (rand()/(double)RAND_MAX);
				currentDitherR = (rand()/(double)RAND_MAX);

				inputSampleL += currentDitherL;
				inputSampleR += currentDitherR;
				inputSampleL -= nsL[4];
				inputSampleR -= nsR[4];

				inputSampleL = floor(inputSampleL);
				inputSampleR = floor(inputSampleR);

				nsL[4] = nsL[3];
				nsL[3] = nsL[2];
				nsL[2] = nsL[1];
				nsL[1] = currentDitherL;

				nsR[4] = nsR[3];
				nsR[3] = nsR[2];
				nsR[2] = nsR[1];
				nsR[1] = currentDitherR;
				//Tape dither
				break;

			case 7:
				Position += 1;
				//Note- uses integer overflow as a 'mod' operator
				hotbinA = Position * Position;
				hotbinA = hotbinA % 170003; //% is C++ mod operator
				hotbinA *= hotbinA;
				hotbinA = hotbinA % 17011; //% is C++ mod operator
				hotbinA *= hotbinA;
				hotbinA = hotbinA % 1709; //% is C++ mod operator
				hotbinA *= hotbinA;
				hotbinA = hotbinA % 173; //% is C++ mod operator
				hotbinA *= hotbinA;
				hotbinA = hotbinA % 17;
				hotbinA *= 0.0635;
				if (flip) hotbinA = -hotbinA;
				inputSampleL += hotbinA;
				inputSampleR += hotbinA;
				inputSampleL = floor(inputSampleL);
				inputSampleR = floor(inputSampleR);
				//Quadratic dither
				break;

			case 8:
				absSample = ((rand()/(double)RAND_MAX) - 0.5);
				nsL[0] += absSample; nsL[0] /= 2; absSample -= nsL[0];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[1] += absSample; nsL[1] /= 2; absSample -= nsL[1];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[2] += absSample; nsL[2] /= 2; absSample -= nsL[2];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[3] += absSample; nsL[3] /= 2; absSample -= nsL[3];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[4] += absSample; nsL[4] /= 2; absSample -= nsL[4];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[5] += absSample; nsL[5] /= 2; absSample -= nsL[5];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[6] += absSample; nsL[6] /= 2; absSample -= nsL[6];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[7] += absSample; nsL[7] /= 2; absSample -= nsL[7];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[8] += absSample; nsL[8] /= 2; absSample -= nsL[8];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[9] += absSample; nsL[9] /= 2; absSample -= nsL[9];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[10] += absSample; nsL[10] /= 2; absSample -= nsL[10];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[11] += absSample; nsL[11] /= 2; absSample -= nsL[11];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[12] += absSample; nsL[12] /= 2; absSample -= nsL[12];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[13] += absSample; nsL[13] /= 2; absSample -= nsL[13];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[14] += absSample; nsL[14] /= 2; absSample -= nsL[14];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsL[15] += absSample; nsL[15] /= 2; absSample -= nsL[15];
				//install noise and then shape it
				absSample += inputSampleL;

				if (NSOddL > 0) NSOddL -= 0.97;
				if (NSOddL < 0) NSOddL += 0.97;

				NSOddL -= (NSOddL * NSOddL * NSOddL * 0.475);

				NSOddL += prevL;
				absSample += (NSOddL*0.475);
				prevL = floor(absSample) - inputSampleL;
				inputSampleL = floor(absSample);
				//TenNines dither L


				absSample = ((rand()/(double)RAND_MAX) - 0.5);
				nsR[0] += absSample; nsR[0] /= 2; absSample -= nsR[0];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[1] += absSample; nsR[1] /= 2; absSample -= nsR[1];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[2] += absSample; nsR[2] /= 2; absSample -= nsR[2];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[3] += absSample; nsR[3] /= 2; absSample -= nsR[3];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[4] += absSample; nsR[4] /= 2; absSample -= nsR[4];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[5] += absSample; nsR[5] /= 2; absSample -= nsR[5];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[6] += absSample; nsR[6] /= 2; absSample -= nsR[6];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[7] += absSample; nsR[7] /= 2; absSample -= nsR[7];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[8] += absSample; nsR[8] /= 2; absSample -= nsR[8];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[9] += absSample; nsR[9] /= 2; absSample -= nsR[9];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[10] += absSample; nsR[10] /= 2; absSample -= nsR[10];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[11] += absSample; nsR[11] /= 2; absSample -= nsR[11];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[12] += absSample; nsR[12] /= 2; absSample -= nsR[12];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[13] += absSample; nsR[13] /= 2; absSample -= nsR[13];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[14] += absSample; nsR[14] /= 2; absSample -= nsR[14];
				absSample += ((rand()/(double)RAND_MAX) - 0.5);
				nsR[15] += absSample; nsR[15] /= 2; absSample -= nsR[15];
				//install noise and then shape it
				absSample += inputSampleR;

				if (NSOddR > 0) NSOddR -= 0.97;
				if (NSOddR < 0) NSOddR += 0.97;

				NSOddR -= (NSOddR * NSOddR * NSOddR * 0.475);

				NSOddR += prevR;
				absSample += (NSOddR*0.475);
				prevR = floor(absSample) - inputSampleR;
				inputSampleR = floor(absSample);
				//TenNines dither R
				break;

			case 9:
				if (inputSampleL > 0) inputSampleL += 0.383;
				if (inputSampleL < 0) inputSampleL -= 0.383;
				if (inputSampleR > 0) inputSampleR += 0.383;
				if (inputSampleR < 0) inputSampleR -= 0.383;
				//adjusting to permit more information drug outta the noisefloor

				contingentRnd = (((rand()/(double)RAND_MAX)+(rand()/(double)RAND_MAX))-1.0) * randyConstant; //produce TPDF dist, scale
				contingentRnd -= contingentErrL*omegaConstant; //include err
				absSample = fabs(inputSampleL);
				contingentErrL = absSample - floor(absSample); //get next err
				contingent = contingentErrL * 2.0; //scale of quantization levels
				if (contingent > 1.0) contingent = ((-contingent+2.0)*omegaConstant) + expConstant;
				else contingent = (contingent * omegaConstant) + expConstant;
				//zero is next to a quantization level, one is exactly between them
				if (flip) contingentRnd = (contingentRnd * (1.0-contingent)) + contingent + 0.5;
				else contingentRnd = (contingentRnd * (1.0-contingent)) - contingent + 0.5;
				inputSampleL += (contingentRnd * contingent);
				//Contingent Dither
				inputSampleL = floor(inputSampleL);

				contingentRnd = (((rand()/(double)RAND_MAX)+(rand()/(double)RAND_MAX))-1.0) * randyConstant; //produce TPDF dist, scale
				contingentRnd -= contingentErrR*omegaConstant; //include err
				absSample = fabs(inputSampleR);
				contingentErrR = absSample - floor(absSample); //get next err
				contingent = contingentErrR * 2.0; //scale of quantization levels
				if (contingent > 1.0) contingent = ((-contingent+2.0)*omegaConstant) + expConstant;
				else contingent = (contingent * omegaConstant) + expConstant;
				//zero is next to a quantization level, one is exactly between them
				if (flip) contingentRnd = (contingentRnd * (1.0-contingent)) + contingent + 0.5;
				else contingentRnd = (contingentRnd * (1.0-contingent)) - contingent + 0.5;
				inputSampleR += (contingentRnd * contingent);
				//Contingent Dither
				inputSampleR = floor(inputSampleR);

				//note: this does not dither for values exactly the same as 16 bit values-
				//which forces the dither to gate at 0.0. It goes to digital black,
				//and does a teeny parallel-compression thing when almost at digital black.
				break;

			case 10: //this one is the original Naturalize
				if (inputSampleL > 0) inputSampleL += (0.3333333333);
				if (inputSampleL < 0) inputSampleL -= (0.3333333333);
				inputSampleL += (rand()/(double)RAND_MAX)*0.6666666666;

				if (inputSampleR > 0) inputSampleR += (0.3333333333);
				if (inputSampleR < 0) inputSampleR -= (0.3333333333);
				inputSampleR += (rand()/(double)RAND_MAX)*0.6666666666;

				//begin L
				benfordize = floor(inputSampleL);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinA = floor(benfordize);
				//hotbin becomes the Benford bin value for this number floored
				totalA = 0;
				if ((hotbinA > 0) && (hotbinA < 10))
				{
					bynL[hotbinA] += 1;
					totalA += (301-bynL[1]);
					totalA += (176-bynL[2]);
					totalA += (125-bynL[3]);
					totalA += (97-bynL[4]);
					totalA += (79-bynL[5]);
					totalA += (67-bynL[6]);
					totalA += (58-bynL[7]);
					totalA += (51-bynL[8]);
					totalA += (46-bynL[9]);
					bynL[hotbinA] -= 1;
				} else {hotbinA = 10;}
				//produce total number- smaller is closer to Benford real

				benfordize = ceil(inputSampleL);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinB = floor(benfordize);
				//hotbin becomes the Benford bin value for this number ceiled
				totalB = 0;
				if ((hotbinB > 0) && (hotbinB < 10))
				{
					bynL[hotbinB] += 1;
					totalB += (301-bynL[1]);
					totalB += (176-bynL[2]);
					totalB += (125-bynL[3]);
					totalB += (97-bynL[4]);
					totalB += (79-bynL[5]);
					totalB += (67-bynL[6]);
					totalB += (58-bynL[7]);
					totalB += (51-bynL[8]);
					totalB += (46-bynL[9]);
					bynL[hotbinB] -= 1;
				} else {hotbinB = 10;}
				//produce total number- smaller is closer to Benford real

				if (totalA < totalB)
				{
					bynL[hotbinA] += 1;
					inputSampleL = floor(inputSampleL);
				}
				else
				{
					bynL[hotbinB] += 1;
					inputSampleL = ceil(inputSampleL);
				}
				//assign the relevant one to the delay line
				//and floor/ceil signal accordingly

				totalA = bynL[1] + bynL[2] + bynL[3] + bynL[4] + bynL[5] + bynL[6] + bynL[7] + bynL[8] + bynL[9];
				totalA /= 1000;
				if (totalA = 0) totalA = 1;
				bynL[1] /= totalA;
				bynL[2] /= totalA;
				bynL[3] /= totalA;
				bynL[4] /= totalA;
				bynL[5] /= totalA;
				bynL[6] /= totalA;
				bynL[7] /= totalA;
				bynL[8] /= totalA;
				bynL[9] /= totalA;
				bynL[10] /= 2; //catchall for garbage data
				//end L

				//begin R
				benfordize = floor(inputSampleR);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinA = floor(benfordize);
				//hotbin becomes the Benford bin value for this number floored
				totalA = 0;
				if ((hotbinA > 0) && (hotbinA < 10))
				{
					bynR[hotbinA] += 1;
					totalA += (301-bynR[1]);
					totalA += (176-bynR[2]);
					totalA += (125-bynR[3]);
					totalA += (97-bynR[4]);
					totalA += (79-bynR[5]);
					totalA += (67-bynR[6]);
					totalA += (58-bynR[7]);
					totalA += (51-bynR[8]);
					totalA += (46-bynR[9]);
					bynR[hotbinA] -= 1;
				} else {hotbinA = 10;}
				//produce total number- smaller is closer to Benford real

				benfordize = ceil(inputSampleR);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinB = floor(benfordize);
				//hotbin becomes the Benford bin value for this number ceiled
				totalB = 0;
				if ((hotbinB > 0) && (hotbinB < 10))
				{
					bynR[hotbinB] += 1;
					totalB += (301-bynR[1]);
					totalB += (176-bynR[2]);
					totalB += (125-bynR[3]);
					totalB += (97-bynR[4]);
					totalB += (79-bynR[5]);
					totalB += (67-bynR[6]);
					totalB += (58-bynR[7]);
					totalB += (51-bynR[8]);
					totalB += (46-bynR[9]);
					bynR[hotbinB] -= 1;
				} else {hotbinB = 10;}
				//produce total number- smaller is closer to Benford real

				if (totalA < totalB)
				{
					bynR[hotbinA] += 1;
					inputSampleR = floor(inputSampleR);
				}
				else
				{
					bynR[hotbinB] += 1;
					inputSampleR = ceil(inputSampleR);
				}
				//assign the relevant one to the delay line
				//and floor/ceil signal accordingly

				totalA = bynR[1] + bynR[2] + bynR[3] + bynR[4] + bynR[5] + bynR[6] + bynR[7] + bynR[8] + bynR[9];
				totalA /= 1000;
				if (totalA = 0) totalA = 1;
				bynR[1] /= totalA;
				bynR[2] /= totalA;
				bynR[3] /= totalA;
				bynR[4] /= totalA;
				bynR[5] /= totalA;
				bynR[6] /= totalA;
				bynR[7] /= totalA;
				bynR[8] /= totalA;
				bynR[9] /= totalA;
				bynR[10] /= 2; //catchall for garbage data
				//end R
				break;

			case 11: //this one is the Not Just Another Dither

				//begin L
				benfordize = floor(inputSampleL);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinA = floor(benfordize);
				//hotbin becomes the Benford bin value for this number floored
				totalA = 0;
				if ((hotbinA > 0) && (hotbinA < 10))
				{
					bynL[hotbinA] += 1;
					totalA += (301-bynL[1]);
					totalA += (176-bynL[2]);
					totalA += (125-bynL[3]);
					totalA += (97-bynL[4]);
					totalA += (79-bynL[5]);
					totalA += (67-bynL[6]);
					totalA += (58-bynL[7]);
					totalA += (51-bynL[8]);
					totalA += (46-bynL[9]);
					bynL[hotbinA] -= 1;
				} else {hotbinA = 10;}
				//produce total number- smaller is closer to Benford real

				benfordize = ceil(inputSampleL);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinB = floor(benfordize);
				//hotbin becomes the Benford bin value for this number ceiled
				totalB = 0;
				if ((hotbinB > 0) && (hotbinB < 10))
				{
					bynL[hotbinB] += 1;
					totalB += (301-bynL[1]);
					totalB += (176-bynL[2]);
					totalB += (125-bynL[3]);
					totalB += (97-bynL[4]);
					totalB += (79-bynL[5]);
					totalB += (67-bynL[6]);
					totalB += (58-bynL[7]);
					totalB += (51-bynL[8]);
					totalB += (46-bynL[9]);
					bynL[hotbinB] -= 1;
				} else {hotbinB = 10;}
				//produce total number- smaller is closer to Benford real

				if (totalA < totalB)
				{
					bynL[hotbinA] += 1;
					inputSampleL = floor(inputSampleL);
				}
				else
				{
					bynL[hotbinB] += 1;
					inputSampleL = ceil(inputSampleL);
				}
				//assign the relevant one to the delay line
				//and floor/ceil signal accordingly

				totalA = bynL[1] + bynL[2] + bynL[3] + bynL[4] + bynL[5] + bynL[6] + bynL[7] + bynL[8] + bynL[9];
				totalA /= 1000;
				if (totalA = 0) totalA = 1;
				bynL[1] /= totalA;
				bynL[2] /= totalA;
				bynL[3] /= totalA;
				bynL[4] /= totalA;
				bynL[5] /= totalA;
				bynL[6] /= totalA;
				bynL[7] /= totalA;
				bynL[8] /= totalA;
				bynL[9] /= totalA;
				bynL[10] /= 2; //catchall for garbage data
				//end L

				//begin R
				benfordize = floor(inputSampleR);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinA = floor(benfordize);
				//hotbin becomes the Benford bin value for this number floored
				totalA = 0;
				if ((hotbinA > 0) && (hotbinA < 10))
				{
					bynR[hotbinA] += 1;
					totalA += (301-bynR[1]);
					totalA += (176-bynR[2]);
					totalA += (125-bynR[3]);
					totalA += (97-bynR[4]);
					totalA += (79-bynR[5]);
					totalA += (67-bynR[6]);
					totalA += (58-bynR[7]);
					totalA += (51-bynR[8]);
					totalA += (46-bynR[9]);
					bynR[hotbinA] -= 1;
				} else {hotbinA = 10;}
				//produce total number- smaller is closer to Benford real

				benfordize = ceil(inputSampleR);
				while (benfordize >= 1.0) {benfordize /= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				if (benfordize < 1.0) {benfordize *= 10;}
				hotbinB = floor(benfordize);
				//hotbin becomes the Benford bin value for this number ceiled
				totalB = 0;
				if ((hotbinB > 0) && (hotbinB < 10))
				{
					bynR[hotbinB] += 1;
					totalB += (301-bynR[1]);
					totalB += (176-bynR[2]);
					totalB += (125-bynR[3]);
					totalB += (97-bynR[4]);
					totalB += (79-bynR[5]);
					totalB += (67-bynR[6]);
					totalB += (58-bynR[7]);
					totalB += (51-bynR[8]);
					totalB += (46-bynR[9]);
					bynR[hotbinB] -= 1;
				} else {hotbinB = 10;}
				//produce total number- smaller is closer to Benford real

				if (totalA < totalB)
				{
					bynR[hotbinA] += 1;
					inputSampleR = floor(inputSampleR);
				}
				else
				{
					bynR[hotbinB] += 1;
					inputSampleR = ceil(inputSampleR);
				}
				//assign the relevant one to the delay line
				//and floor/ceil signal accordingly

				totalA = bynR[1] + bynR[2] + bynR[3] + bynR[4] + bynR[5] + bynR[6] + bynR[7] + bynR[8] + bynR[9];
				totalA /= 1000;
				if (totalA = 0) totalA = 1;
				bynR[1] /= totalA;
				bynR[2] /= totalA;
				bynR[3] /= totalA;
				bynR[4] /= totalA;
				bynR[5] /= totalA;
				bynR[6] /= totalA;
				bynR[7] /= totalA;
				bynR[8] /= totalA;
				bynR[9] /= totalA;
				bynR[10] /= 2; //catchall for garbage data
				//end R
				break;

			case 12:
				//slew only
				outputSampleL = (inputSampleL - lastSampleL)*trim;
				outputSampleR = (inputSampleR - lastSampleR)*trim;
				lastSampleL = inputSampleL;
				lastSampleR = inputSampleR;
				if (outputSampleL > 1.0) outputSampleL = 1.0;
				if (outputSampleR > 1.0) outputSampleR = 1.0;
				if (outputSampleL < -1.0) outputSampleL = -1.0;
				if (outputSampleR < -1.0) outputSampleR = -1.0;
				inputSampleL = outputSampleL;
				inputSampleR = outputSampleR;
				break;

			case 13:
				//subs only
				gain = gaintarget;

				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				iirSampleAL = (iirSampleAL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleAL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleBL = (iirSampleBL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleBL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleCL = (iirSampleCL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleCL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleDL = (iirSampleDL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleDL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleEL = (iirSampleEL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleEL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleFL = (iirSampleFL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleFL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleGL = (iirSampleGL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleGL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleHL = (iirSampleHL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleHL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleIL = (iirSampleIL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleIL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleJL = (iirSampleJL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleJL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleKL = (iirSampleKL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleKL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleLL = (iirSampleLL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleLL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleML = (iirSampleML * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleML;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleNL = (iirSampleNL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleNL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleOL = (iirSampleOL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleOL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSamplePL = (iirSamplePL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSamplePL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleQL = (iirSampleQL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleQL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleRL = (iirSampleRL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleRL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleSL = (iirSampleSL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleSL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleTL = (iirSampleTL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleTL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleUL = (iirSampleUL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleUL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleVL = (iirSampleVL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleVL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleWL = (iirSampleWL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleWL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleXL = (iirSampleXL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleXL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleYL = (iirSampleYL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleYL;
				inputSampleL *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				iirSampleZL = (iirSampleZL * altAmount) + (inputSampleL * iirAmount); inputSampleL = iirSampleZL;
				if (inputSampleL > 1.0) inputSampleL = 1.0;
				if (inputSampleL < -1.0) inputSampleL = -1.0;
				gain = gaintarget;

				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				iirSampleAR = (iirSampleAR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleAR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleBR = (iirSampleBR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleBR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleCR = (iirSampleCR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleCR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleDR = (iirSampleDR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleDR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleER = (iirSampleER * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleER;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleFR = (iirSampleFR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleFR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleGR = (iirSampleGR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleGR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleHR = (iirSampleHR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleHR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleIR = (iirSampleIR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleIR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleJR = (iirSampleJR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleJR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleKR = (iirSampleKR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleKR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleLR = (iirSampleLR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleLR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleMR = (iirSampleMR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleMR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleNR = (iirSampleNR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleNR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleOR = (iirSampleOR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleOR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSamplePR = (iirSamplePR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSamplePR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleQR = (iirSampleQR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleQR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleRR = (iirSampleRR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleRR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleSR = (iirSampleSR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleSR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleTR = (iirSampleTR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleTR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleUR = (iirSampleUR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleUR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleVR = (iirSampleVR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleVR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleWR = (iirSampleWR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleWR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleXR = (iirSampleXR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleXR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleYR = (iirSampleYR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleYR;
				inputSampleR *= gain; gain = ((gain-1)*0.75)+1;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				iirSampleZR = (iirSampleZR * altAmount) + (inputSampleR * iirAmount); inputSampleR = iirSampleZR;
				if (inputSampleR > 1.0) inputSampleR = 1.0;
				if (inputSampleR < -1.0) inputSampleR = -1.0;
				break;

			case 14:
				//silhouette
				//begin L
				bridgerectifier = fabs(inputSampleL)*1.57079633;
				if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
				bridgerectifier = 1.0-cos(bridgerectifier);
				if (inputSampleL > 0.0) inputSampleL = bridgerectifier;
				else inputSampleL = -bridgerectifier;

				silhouette = rand()/(double)RAND_MAX;
				silhouette -= 0.5;
				silhouette *= 2.0;
				silhouette *= fabs(inputSampleL);

				smoother = rand()/(double)RAND_MAX;
				smoother -= 0.5;
				smoother *= 2.0;
				smoother *= fabs(lastSampleL);
				lastSampleL = inputSampleL;

				silhouette += smoother;

				bridgerectifier = fabs(silhouette)*1.57079633;
				if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
				bridgerectifier = sin(bridgerectifier);
				if (silhouette > 0.0) silhouette = bridgerectifier;
				else silhouette = -bridgerectifier;

				inputSampleL = (silhouette + outSampleL) / 2.0;
				outSampleL = silhouette;
				//end L

				//begin R
				bridgerectifier = fabs(inputSampleR)*1.57079633;
				if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
				bridgerectifier = 1.0-cos(bridgerectifier);
				if (inputSampleR > 0.0) inputSampleR = bridgerectifier;
				else inputSampleR = -bridgerectifier;

				silhouette = rand()/(double)RAND_MAX;
				silhouette -= 0.5;
				silhouette *= 2.0;
				silhouette *= fabs(inputSampleR);

				smoother = rand()/(double)RAND_MAX;
				smoother -= 0.5;
				smoother *= 2.0;
				smoother *= fabs(lastSampleR);
				lastSampleR = inputSampleR;

				silhouette += smoother;

				bridgerectifier = fabs(silhouette)*1.57079633;
				if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
				bridgerectifier = sin(bridgerectifier);
				if (silhouette > 0.0) silhouette = bridgerectifier;
				else silhouette = -bridgerectifier;

				inputSampleR = (silhouette + outSampleR) / 2.0;
				outSampleR = silhouette;
				//end R
				break;
		}

		flip = !flip;
		//several dithers use this

		if (highRes) {inputSampleL /= 256.0; inputSampleR /= 256.0;} //256 for 16/24 version
		if (dithering) {inputSampleL /= 32768.0; inputSampleR /= 32768.0;}

		if (dtype == 8) {
			noiseShapingL += inputSampleL - drySampleL;
			noiseShapingR += inputSampleR - drySampleR;
		}

		*out1 = inputSampleL;
		*out2 = inputSampleR;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}
