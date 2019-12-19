/* ========================================
 *  Wider - Wider.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Wider_H
#include "Wider.h"
#endif

void Wider::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    long double inputSampleL;
    long double inputSampleR;
    double drySampleL;
    double drySampleR;
    long double mid;
    long double side;
    double out;
    double densityside = (A*2.0)-1.0;
    double densitymid = (B*2.0)-1.0;
    double wet = C;
    double dry = 1.0 - wet;
    wet *= 0.5; //we make mid-side by adding/subtracting both channels into each channel
    //and that's why we gotta divide it by 2: otherwise everything's doubled. So, premultiply it to save an extra 'math'
    double offset = (densityside-densitymid)/2;
    if (offset > 0) offset = sin(offset);
    if (offset < 0) offset = -sin(-offset);
    offset = -(pow(offset,4) * 20 * overallscale);
    int near = (int)floor(fabs(offset));
    double farLevel = fabs(offset) - near;
    int far = near + 1;
    double nearLevel = 1.0 - farLevel;
    double bridgerectifier;
    //interpolating the sample

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
        //assign working variables
        mid = inputSampleL + inputSampleR;
        side = inputSampleL - inputSampleR;
        //assign mid and side. Now, High Impact code

        if (densityside != 0.0)
        {
            out = fabs(densityside);
            bridgerectifier = fabs(side)*1.57079633;
            if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
            //max value for sine function
            if (densityside > 0) bridgerectifier = sin(bridgerectifier);
            else bridgerectifier = 1-cos(bridgerectifier);
            //produce either boosted or starved version
            if (side > 0) side = (side*(1-out))+(bridgerectifier*out);
            else side = (side*(1-out))-(bridgerectifier*out);
            //blend according to density control
        }

        if (densitymid != 0.0)
        {
            out = fabs(densitymid);
            bridgerectifier = fabs(mid)*1.57079633;
            if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
            //max value for sine function
            if (densitymid > 0) bridgerectifier = sin(bridgerectifier);
            else bridgerectifier = 1-cos(bridgerectifier);
            //produce either boosted or starved version
            if (mid > 0) mid = (mid*(1-out))+(bridgerectifier*out);
            else mid = (mid*(1-out))-(bridgerectifier*out);
            //blend according to density control
        }

        if (count < 1 || count > 2048) {count = 2048;}
        if (offset > 0)
        {
            p[count+2048] = p[count] = mid;
            mid = p[count+near]*nearLevel;
            mid += p[count+far]*farLevel;
        }

        if (offset < 0)
        {
            p[count+2048] = p[count] = side;
            side = p[count+near]*nearLevel;
            side += p[count+far]*farLevel;
        }
        count -= 1;

        inputSampleL = (drySampleL * dry) + ((mid+side) * wet);
        inputSampleR = (drySampleR * dry) + ((mid-side) * wet);

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

void Wider::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    long double inputSampleL;
    long double inputSampleR;
    double drySampleL;
    double drySampleR;
    long double mid;
    long double side;
    double out;
    double densityside = (A*2.0)-1.0;
    double densitymid = (B*2.0)-1.0;
    double wet = C;
    double dry = 1.0 - wet;
    wet *= 0.5; //we make mid-side by adding/subtracting both channels into each channel
    //and that's why we gotta divide it by 2: otherwise everything's doubled. So, premultiply it to save an extra 'math'
    double offset = (densityside-densitymid)/2;
    if (offset > 0) offset = sin(offset);
    if (offset < 0) offset = -sin(-offset);
    offset = -(pow(offset,4) * 20 * overallscale);
    int near = (int)floor(fabs(offset));
    double farLevel = fabs(offset) - near;
    int far = near + 1;
    double nearLevel = 1.0 - farLevel;
    double bridgerectifier;
    //interpolating the sample

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
        //assign working variables
        mid = inputSampleL + inputSampleR;
        side = inputSampleL - inputSampleR;
        //assign mid and side. Now, High Impact code

        if (densityside != 0.0)
        {
            out = fabs(densityside);
            bridgerectifier = fabs(side)*1.57079633;
            if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
            //max value for sine function
            if (densityside > 0) bridgerectifier = sin(bridgerectifier);
            else bridgerectifier = 1-cos(bridgerectifier);
            //produce either boosted or starved version
            if (side > 0) side = (side*(1-out))+(bridgerectifier*out);
            else side = (side*(1-out))-(bridgerectifier*out);
            //blend according to density control
        }

        if (densitymid != 0.0)
        {
            out = fabs(densitymid);
            bridgerectifier = fabs(mid)*1.57079633;
            if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
            //max value for sine function
            if (densitymid > 0) bridgerectifier = sin(bridgerectifier);
            else bridgerectifier = 1-cos(bridgerectifier);
            //produce either boosted or starved version
            if (mid > 0) mid = (mid*(1-out))+(bridgerectifier*out);
            else mid = (mid*(1-out))-(bridgerectifier*out);
            //blend according to density control
        }

        if (count < 1 || count > 2048) {count = 2048;}
        if (offset > 0)
        {
            p[count+2048] = p[count] = mid;
            mid = p[count+near]*nearLevel;
            mid += p[count+far]*farLevel;
        }

        if (offset < 0)
        {
            p[count+2048] = p[count] = side;
            side = p[count+near]*nearLevel;
            side += p[count+far]*farLevel;
        }
        count -= 1;

        inputSampleL = (drySampleL * dry) + ((mid+side) * wet);
        inputSampleR = (drySampleR * dry) + ((mid-side) * wet);

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
