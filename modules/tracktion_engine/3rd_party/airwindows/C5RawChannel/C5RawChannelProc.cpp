/* ========================================
 *  C5RawChannel - C5RawChannel.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __C5RawChannel_H
#include "C5RawChannel.h"
#endif

void C5RawChannel::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];


    long double centering = A * 0.5;
    centering = 1.0 - pow(centering,5);
    //we can set our centering force from zero to rather high, but
    //there's a really intense taper on it forcing it to mostly be almost 1.0.
    //If it's literally 1.0, we don't even apply it, and you get the original
    //Xmas Morning bugged-out Console5, which is the default setting for Raw Console5.
    double differenceL;
    double differenceR;

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

        differenceL = lastSampleChannelL - inputSampleL;
        lastSampleChannelL = inputSampleL;
        //derive slew part off direct sample measurement + from last time
        differenceR = lastSampleChannelR - inputSampleR;
        lastSampleChannelR = inputSampleR;
        //derive slew part off direct sample measurement + from last time

        if (differenceL > 1.0) differenceL = 1.0;
        if (differenceL < -1.0) differenceL = -1.0;
        if (differenceR > 1.0) differenceR = 1.0;
        if (differenceR < -1.0) differenceR = -1.0;

        inputSampleL = lastFXChannelL + asin(differenceL);
        lastFXChannelL = inputSampleL;
        if (centering < 1.0) lastFXChannelL *= centering;
        //if we're using the crude centering force, it's applied here
        inputSampleR = lastFXChannelR + asin(differenceR);
        lastFXChannelR = inputSampleR;
        if (centering < 1.0) lastFXChannelR *= centering;
        //if we're using the crude centering force, it's applied here

        if (lastFXChannelL > 1.0) lastFXChannelL = 1.0;
        if (lastFXChannelL < -1.0) lastFXChannelL = -1.0;
        if (lastFXChannelR > 1.0) lastFXChannelR = 1.0;
        if (lastFXChannelR < -1.0) lastFXChannelR = -1.0;
        //build new signal off what was present in output last time
        //slew aspect

        if (inputSampleL > 1.57079633) inputSampleL = 1.57079633;
        if (inputSampleL < -1.57079633) inputSampleL = -1.57079633;
        inputSampleL = sin(inputSampleL);
        //amplitude aspect
        if (inputSampleR > 1.57079633) inputSampleR = 1.57079633;
        if (inputSampleR < -1.57079633) inputSampleR = -1.57079633;
        inputSampleR = sin(inputSampleR);
        //amplitude aspect

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

void C5RawChannel::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];


    long double centering = A * 0.5;
    centering = 1.0 - pow(centering,5);
    //we can set our centering force from zero to rather high, but
    //there's a really intense taper on it forcing it to mostly be almost 1.0.
    //If it's literally 1.0, we don't even apply it, and you get the original
    //Xmas Morning bugged-out Console5, which is the default setting for Raw Console5.
    double differenceL;
    double differenceR;

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

        differenceL = lastSampleChannelL - inputSampleL;
        lastSampleChannelL = inputSampleL;
        //derive slew part off direct sample measurement + from last time
        differenceR = lastSampleChannelR - inputSampleR;
        lastSampleChannelR = inputSampleR;
        //derive slew part off direct sample measurement + from last time

        if (differenceL > 1.0) differenceL = 1.0;
        if (differenceL < -1.0) differenceL = -1.0;
        if (differenceR > 1.0) differenceR = 1.0;
        if (differenceR < -1.0) differenceR = -1.0;

        inputSampleL = lastFXChannelL + asin(differenceL);
        lastFXChannelL = inputSampleL;
        if (centering < 1.0) lastFXChannelL *= centering;
        //if we're using the crude centering force, it's applied here
        inputSampleR = lastFXChannelR + asin(differenceR);
        lastFXChannelR = inputSampleR;
        if (centering < 1.0) lastFXChannelR *= centering;
        //if we're using the crude centering force, it's applied here

        if (lastFXChannelL > 1.0) lastFXChannelL = 1.0;
        if (lastFXChannelL < -1.0) lastFXChannelL = -1.0;
        if (lastFXChannelR > 1.0) lastFXChannelR = 1.0;
        if (lastFXChannelR < -1.0) lastFXChannelR = -1.0;
        //build new signal off what was present in output last time
        //slew aspect

        if (inputSampleL > 1.57079633) inputSampleL = 1.57079633;
        if (inputSampleL < -1.57079633) inputSampleL = -1.57079633;
        inputSampleL = sin(inputSampleL);
        //amplitude aspect
        if (inputSampleR > 1.57079633) inputSampleR = 1.57079633;
        if (inputSampleR < -1.57079633) inputSampleR = -1.57079633;
        inputSampleR = sin(inputSampleR);
        //amplitude aspect

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
