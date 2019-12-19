/* ========================================
 *  DoublePaul - DoublePaul.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DoublePaul_H
#include "DoublePaul.h"
#endif

void DoublePaul::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double currentDitherL;
    double currentDitherR;

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

        bL[9] = bL[8]; bL[8] = bL[7]; bL[7] = bL[6]; bL[6] = bL[5];
        bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1];
        bL[1] = bL[0]; bL[0] = (rand()/(double)RAND_MAX);

        currentDitherL  = (bL[0] * 0.061);
        currentDitherL -= (bL[1] * 0.11);
        currentDitherL += (bL[8] * 0.126);
        currentDitherL -= (bL[7] * 0.23);
        currentDitherL += (bL[2] * 0.25);
        currentDitherL -= (bL[3] * 0.43);
        currentDitherL += (bL[6] * 0.5);
        currentDitherL -= bL[5];
        currentDitherL += bL[4];
        //this sounds different from doing it in order of sample position
        //cumulative tiny errors seem to build up even at this buss depth
        //considerably more pronounced at 32 bit float.
        //Therefore we add the most significant components LAST.
        //trying to keep values on like exponents of the floating point value.
        inputSampleL += currentDitherL;

        inputSampleL = floor(inputSampleL);
            //done with L

        bR[9] = bR[8]; bR[8] = bR[7]; bR[7] = bR[6]; bR[6] = bR[5];
        bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1];
        bR[1] = bR[0]; bR[0] = (rand()/(double)RAND_MAX);

        currentDitherR  = (bR[0] * 0.061);
        currentDitherR -= (bR[1] * 0.11);
        currentDitherR += (bR[8] * 0.126);
        currentDitherR -= (bR[7] * 0.23);
        currentDitherR += (bR[2] * 0.25);
        currentDitherR -= (bR[3] * 0.43);
        currentDitherR += (bR[6] * 0.5);
        currentDitherR -= bR[5];
        currentDitherR += bR[4];
        //this sounds different from doing it in order of sample position
        //cumulative tiny errors seem to build up even at this buss depth
        //considerably more pronounced at 32 bit float.
        //Therefore we add the most significant components LAST.
        //trying to keep values on like exponents of the floating point value.
        inputSampleR += currentDitherR;

        inputSampleR = floor(inputSampleR);
            //done with R

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

void DoublePaul::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double currentDitherL;
    double currentDitherR;

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

        bL[9] = bL[8]; bL[8] = bL[7]; bL[7] = bL[6]; bL[6] = bL[5];
        bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1];
        bL[1] = bL[0]; bL[0] = (rand()/(double)RAND_MAX);

        currentDitherL  = (bL[0] * 0.061);
        currentDitherL -= (bL[1] * 0.11);
        currentDitherL += (bL[8] * 0.126);
        currentDitherL -= (bL[7] * 0.23);
        currentDitherL += (bL[2] * 0.25);
        currentDitherL -= (bL[3] * 0.43);
        currentDitherL += (bL[6] * 0.5);
        currentDitherL -= bL[5];
        currentDitherL += bL[4];
        //this sounds different from doing it in order of sample position
        //cumulative tiny errors seem to build up even at this buss depth
        //considerably more pronounced at 32 bit float.
        //Therefore we add the most significant components LAST.
        //trying to keep values on like exponents of the floating point value.
        inputSampleL += currentDitherL;

        inputSampleL = floor(inputSampleL);
        //done with L

        bR[9] = bR[8]; bR[8] = bR[7]; bR[7] = bR[6]; bR[6] = bR[5];
        bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1];
        bR[1] = bR[0]; bR[0] = (rand()/(double)RAND_MAX);

        currentDitherR  = (bR[0] * 0.061);
        currentDitherR -= (bR[1] * 0.11);
        currentDitherR += (bR[8] * 0.126);
        currentDitherR -= (bR[7] * 0.23);
        currentDitherR += (bR[2] * 0.25);
        currentDitherR -= (bR[3] * 0.43);
        currentDitherR += (bR[6] * 0.5);
        currentDitherR -= bR[5];
        currentDitherR += bR[4];
        //this sounds different from doing it in order of sample position
        //cumulative tiny errors seem to build up even at this buss depth
        //considerably more pronounced at 32 bit float.
        //Therefore we add the most significant components LAST.
        //trying to keep values on like exponents of the floating point value.
        inputSampleR += currentDitherR;

        inputSampleR = floor(inputSampleR);
        //done with R

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
