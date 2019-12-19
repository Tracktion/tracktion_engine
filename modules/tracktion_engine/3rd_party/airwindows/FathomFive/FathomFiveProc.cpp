/* ========================================
 *  FathomFive - FathomFive.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __FathomFive_H
#include "FathomFive.h"
#endif

void FathomFive::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double EQ = 0.01+((pow(C,4) / getSampleRate())*32000.0);
    double dcblock = EQ / 320.0;
    double wet = D*2.0;
    double dry = 2.0 - wet;
    double bridgerectifier;
    double tempL;
    double tempR;
    double basstrim = (0.01/EQ)+1.0;
    if (wet > 1.0) wet = 1.0;
    if (dry > 1.0) dry = 1.0;

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

        if (inputSampleL > 0)
        {if (WasNegativeL){SubOctaveL = !SubOctaveL;} WasNegativeL = false;}
        else {WasNegativeL = true;}
        if (inputSampleR > 0)
        {if (WasNegativeR){SubOctaveR = !SubOctaveR;} WasNegativeR = false;}
        else {WasNegativeR = true;}

        iirSampleLD = (iirSampleLD * (1 - EQ)) + (inputSampleL * EQ);
        bridgerectifier = fabs(iirSampleLD);
        if (SubOctaveL) tempL = bridgerectifier*B;
        else tempL = -bridgerectifier*B;
        iirSampleRD = (iirSampleRD * (1 - EQ)) + (inputSampleR * EQ);
        bridgerectifier = fabs(iirSampleRD);
        if (SubOctaveR) tempR = bridgerectifier*B;
        else tempR = -bridgerectifier*B;

        tempL += (inputSampleL*A);
        tempR += (inputSampleR*A);

        iirSampleLA += (tempL * EQ);
        iirSampleLA -= (iirSampleLA * iirSampleLA * iirSampleLA * EQ);
        if (iirSampleLA > 0) iirSampleLA -= dcblock;
        else iirSampleLA += dcblock;
        tempL = iirSampleLA*basstrim;

        iirSampleRA += (tempR * EQ);
        iirSampleRA -= (iirSampleRA * iirSampleRA * iirSampleRA * EQ);
        if (iirSampleRA > 0) iirSampleRA -= dcblock;
        else iirSampleRA += dcblock;
        tempR = iirSampleRA*basstrim;

        iirSampleLB = (iirSampleLB * (1 - EQ)) + (tempL * EQ);
        tempL = iirSampleLB;
        iirSampleRB = (iirSampleRB * (1 - EQ)) + (tempR * EQ);
        tempR = iirSampleRB;

        iirSampleLC = (iirSampleLC * (1 - EQ)) + (tempL * EQ);
        tempL = iirSampleLC;
        iirSampleRC = (iirSampleRC * (1 - EQ)) + (tempR * EQ);
        tempR = iirSampleRC;

        inputSampleL = (inputSampleL*dry) + (tempL*wet);
        inputSampleR = (inputSampleR*dry) + (tempR*wet);

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}

void FathomFive::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double EQ = 0.01+((pow(C,4) / getSampleRate())*32000.0);
    double dcblock = EQ / 320.0;
    double wet = D*2.0;
    double dry = 2.0 - wet;
    double bridgerectifier;
    double tempL;
    double tempR;
    double basstrim = (0.01/EQ)+1.0;
    if (wet > 1.0) wet = 1.0;
    if (dry > 1.0) dry = 1.0;

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

        if (inputSampleL > 0)
        {if (WasNegativeL){SubOctaveL = !SubOctaveL;} WasNegativeL = false;}
        else {WasNegativeL = true;}
        if (inputSampleR > 0)
        {if (WasNegativeR){SubOctaveR = !SubOctaveR;} WasNegativeR = false;}
        else {WasNegativeR = true;}

        iirSampleLD = (iirSampleLD * (1 - EQ)) + (inputSampleL * EQ);
        bridgerectifier = fabs(iirSampleLD);
        if (SubOctaveL) tempL = bridgerectifier*B;
        else tempL = -bridgerectifier*B;
        iirSampleRD = (iirSampleRD * (1 - EQ)) + (inputSampleR * EQ);
        bridgerectifier = fabs(iirSampleRD);
        if (SubOctaveR) tempR = bridgerectifier*B;
        else tempR = -bridgerectifier*B;

        tempL += (inputSampleL*A);
        tempR += (inputSampleR*A);

        iirSampleLA += (tempL * EQ);
        iirSampleLA -= (iirSampleLA * iirSampleLA * iirSampleLA * EQ);
        if (iirSampleLA > 0) iirSampleLA -= dcblock;
        else iirSampleLA += dcblock;
        tempL = iirSampleLA*basstrim;

        iirSampleRA += (tempR * EQ);
        iirSampleRA -= (iirSampleRA * iirSampleRA * iirSampleRA * EQ);
        if (iirSampleRA > 0) iirSampleRA -= dcblock;
        else iirSampleRA += dcblock;
        tempR = iirSampleRA*basstrim;

        iirSampleLB = (iirSampleLB * (1 - EQ)) + (tempL * EQ);
        tempL = iirSampleLB;
        iirSampleRB = (iirSampleRB * (1 - EQ)) + (tempR * EQ);
        tempR = iirSampleRB;

        iirSampleLC = (iirSampleLC * (1 - EQ)) + (tempL * EQ);
        tempL = iirSampleLC;
        iirSampleRC = (iirSampleRC * (1 - EQ)) + (tempR * EQ);
        tempR = iirSampleRC;

        inputSampleL = (inputSampleL*dry) + (tempL*wet);
        inputSampleR = (inputSampleR*dry) + (tempR*wet);

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}
