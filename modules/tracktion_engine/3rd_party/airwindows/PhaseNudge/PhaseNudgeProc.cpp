/* ========================================
 *  PhaseNudge - PhaseNudge.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PhaseNudge_H
#include "PhaseNudge.h"
#endif

void PhaseNudge::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];


    int allpasstemp;
    double outallpass = 0.618033988749894848204586; //golden ratio!
    //if you see 0.6180 it's not a wild stretch to wonder whether you are working with a constant
    int maxdelayTarget = (int)(pow(A,3)*1501.0);
    double wet = B;
    double dry = 1.0 - wet;
    double bridgerectifier;

    long double inputSampleL;
    long double inputSampleR;
    long double drySampleL;
    long double drySampleR;

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

        inputSampleL /= 4.0;
        inputSampleR /= 4.0;

        bridgerectifier = fabs(inputSampleL);
        bridgerectifier = sin(bridgerectifier);
        if (inputSampleL > 0) inputSampleL = bridgerectifier;
        else inputSampleL = -bridgerectifier;

        bridgerectifier = fabs(inputSampleR);
        bridgerectifier = sin(bridgerectifier);
        if (inputSampleR > 0) inputSampleR = bridgerectifier;
        else inputSampleR = -bridgerectifier;

        if (fabs(maxdelay - maxdelayTarget) > 1500) maxdelay = maxdelayTarget;

        if (maxdelay < maxdelayTarget) {
            maxdelay++;
            dL[maxdelay] = (dL[0]+dL[maxdelay-1]) / 2.0;
            dR[maxdelay] = (dR[0]+dR[maxdelay-1]) / 2.0;
        }

        if (maxdelay > maxdelayTarget) {
            maxdelay--;
            dL[maxdelay] = (dL[0]+dL[maxdelay]) / 2.0;
            dR[maxdelay] = (dR[0]+dR[maxdelay]) / 2.0;
        }

        allpasstemp = one - 1;

        if (allpasstemp < 0 || allpasstemp > maxdelay) allpasstemp = maxdelay;

        inputSampleL -= dL[allpasstemp]*outallpass;
        inputSampleR -= dR[allpasstemp]*outallpass;
        dL[one] = inputSampleL;
        dR[one] = inputSampleR;
        inputSampleL *= outallpass;
        inputSampleR *= outallpass;
        one--; if (one < 0 || one > maxdelay) {one = maxdelay;}
        inputSampleL += (dL[one]);
        inputSampleR += (dR[one]);


        bridgerectifier = fabs(inputSampleL);
        bridgerectifier = 1.0-cos(bridgerectifier);
        if (inputSampleL > 0) inputSampleL -= bridgerectifier;
        else inputSampleL += bridgerectifier;

        bridgerectifier = fabs(inputSampleR);
        bridgerectifier = 1.0-cos(bridgerectifier);
        if (inputSampleR > 0) inputSampleR -= bridgerectifier;
        else inputSampleR += bridgerectifier;

        inputSampleL *= 4.0;
        inputSampleR *= 4.0;

        if (wet < 1.0) {
            inputSampleL = (drySampleL * dry)+(inputSampleL * wet);
            inputSampleR = (drySampleR * dry)+(inputSampleR * wet);
        }

        //stereo 32 bit dither, made small and tidy.
        int expon; frexpf((float)inputSampleL, &expon);
        long double dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
        inputSampleL += (dither-fpNShapeL); fpNShapeL = dither;
        frexpf((float)inputSampleR, &expon);
        dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
        inputSampleR += (dither-fpNShapeR); fpNShapeR = dither;
        //end 32 bit dither

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}

void PhaseNudge::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];


    int allpasstemp;
    double outallpass = 0.618033988749894848204586; //golden ratio!
    //if you see 0.6180 it's not a wild stretch to wonder whether you are working with a constant
    int maxdelayTarget = (int)(pow(A,3)*1501.0);
    double wet = B;
    double dry = 1.0 - wet;
    double bridgerectifier;

    long double inputSampleL;
    long double inputSampleR;
    long double drySampleL;
    long double drySampleR;

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

        inputSampleL /= 4.0;
        inputSampleR /= 4.0;

        bridgerectifier = fabs(inputSampleL);
        bridgerectifier = sin(bridgerectifier);
        if (inputSampleL > 0) inputSampleL = bridgerectifier;
        else inputSampleL = -bridgerectifier;

        bridgerectifier = fabs(inputSampleR);
        bridgerectifier = sin(bridgerectifier);
        if (inputSampleR > 0) inputSampleR = bridgerectifier;
        else inputSampleR = -bridgerectifier;

        if (fabs(maxdelay - maxdelayTarget) > 1500) maxdelay = maxdelayTarget;

        if (maxdelay < maxdelayTarget) {
            maxdelay++;
            dL[maxdelay] = (dL[0]+dL[maxdelay-1]) / 2.0;
            dR[maxdelay] = (dR[0]+dR[maxdelay-1]) / 2.0;
        }

        if (maxdelay > maxdelayTarget) {
            maxdelay--;
            dL[maxdelay] = (dL[0]+dL[maxdelay]) / 2.0;
            dR[maxdelay] = (dR[0]+dR[maxdelay]) / 2.0;
        }

        allpasstemp = one - 1;

        if (allpasstemp < 0 || allpasstemp > maxdelay) allpasstemp = maxdelay;

        inputSampleL -= dL[allpasstemp]*outallpass;
        inputSampleR -= dR[allpasstemp]*outallpass;
        dL[one] = inputSampleL;
        dR[one] = inputSampleR;
        inputSampleL *= outallpass;
        inputSampleR *= outallpass;
        one--; if (one < 0 || one > maxdelay) {one = maxdelay;}
        inputSampleL += (dL[one]);
        inputSampleR += (dR[one]);

        bridgerectifier = fabs(inputSampleL);
        bridgerectifier = 1.0-cos(bridgerectifier);
        if (inputSampleL > 0) inputSampleL -= bridgerectifier;
        else inputSampleL += bridgerectifier;

        bridgerectifier = fabs(inputSampleR);
        bridgerectifier = 1.0-cos(bridgerectifier);
        if (inputSampleR > 0) inputSampleR -= bridgerectifier;
        else inputSampleR += bridgerectifier;

        inputSampleL *= 4.0;
        inputSampleR *= 4.0;

        if (wet < 1.0) {
            inputSampleL = (drySampleL * dry)+(inputSampleL * wet);
            inputSampleR = (drySampleR * dry)+(inputSampleR * wet);
        }

        //stereo 64 bit dither, made small and tidy.
        int expon; frexp((double)inputSampleL, &expon);
        long double dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
        dither /= 536870912.0; //needs this to scale to 64 bit zone
        inputSampleL += (dither-fpNShapeL); fpNShapeL = dither;
        frexp((double)inputSampleR, &expon);
        dither = (rand()/(RAND_MAX*7.737125245533627e+25))*pow(2,expon+62);
        dither /= 536870912.0; //needs this to scale to 64 bit zone
        inputSampleR += (dither-fpNShapeR); fpNShapeR = dither;
        //end 64 bit dither

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}
