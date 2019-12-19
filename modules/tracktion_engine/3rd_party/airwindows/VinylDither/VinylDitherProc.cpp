/* ========================================
 *  VinylDither - VinylDither.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __VinylDither_H
#include "VinylDither.h"
#endif

void VinylDither::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double absSample;

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

        inputSampleL *= 8388608.0;
        inputSampleR *= 8388608.0;
        //0-1 is now one bit, now we dither

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

        inputSampleL /= 8388608.0;
        inputSampleR /= 8388608.0;


        *out1 = inputSampleL;
        *out2 = inputSampleR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}

void VinylDither::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double absSample;

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

        inputSampleL *= 8388608.0;
        inputSampleR *= 8388608.0;
        //0-1 is now one bit, now we dither

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

        inputSampleL /= 8388608.0;
        inputSampleR /= 8388608.0;


        *out1 = inputSampleL;
        *out2 = inputSampleR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}
