/* ========================================
 *  DeRez - DeRez.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DeRez_H
#include "DeRez.h"
#endif

void DeRez::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double targetA = pow(A,3)+0.0005;
    if (targetA > 1.0) targetA = 1.0;
    double soften = (1.0 + targetA)/2;
    double targetB = pow(1.0-B,3) / 3;
    targetA /= overallscale;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38 && (targetB == 0)) {
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
        if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38 && (targetB == 0)) {
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
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

        incrementA = ((incrementA*999.0)+targetA)/1000.0;
        incrementB = ((incrementB*999.0)+targetB)/1000.0;
        //incrementA is the frequency derez
        //incrementB is the bit depth derez
        position += incrementA;

        long double outputSampleL = heldSampleL;
        long double outputSampleR = heldSampleR;
        if (position > 1.0)
        {
            position -= 1.0;
            heldSampleL = (lastSampleL * position) + (inputSampleL * (1.0-position));
            outputSampleL = (outputSampleL * (1.0-soften)) + (heldSampleL * soften);
            //softens the edge of the derez
            heldSampleR = (lastSampleR * position) + (inputSampleR * (1.0-position));
            outputSampleR = (outputSampleR * (1.0-soften)) + (heldSampleR * soften);
            //softens the edge of the derez
        }
        inputSampleL = outputSampleL;
        inputSampleR = outputSampleR;

        long double offset;
        if (incrementB > 0.0005)
        {
            if (inputSampleL > 0)
            {
                offset = inputSampleL;
                while (offset > 0) {offset -= incrementB;}
                inputSampleL -= offset;
                //it's below 0 so subtracting adds the remainder
            }
            if (inputSampleR > 0)
            {
                offset = inputSampleR;
                while (offset > 0) {offset -= incrementB;}
                inputSampleR -= offset;
                //it's below 0 so subtracting adds the remainder
            }

            if (inputSampleL < 0)
            {
                offset = inputSampleL;
                while (offset < 0) {offset += incrementB;}
                inputSampleL -= offset;
                //it's above 0 so subtracting subtracts the remainder
            }
            if (inputSampleR < 0)
            {
                offset = inputSampleR;
                while (offset < 0) {offset += incrementB;}
                inputSampleR -= offset;
                //it's above 0 so subtracting subtracts the remainder
            }

            inputSampleL *= (1.0 - incrementB);
            inputSampleR *= (1.0 - incrementB);
        }

        lastSampleL = drySampleL;
        lastSampleR = drySampleR;

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

void DeRez::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double targetA = pow(A,3)+0.0005;
    if (targetA > 1.0) targetA = 1.0;
    double soften = (1.0 + targetA)/2;
    double targetB = pow(1.0-B,3) / 3;
    targetA /= overallscale;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38 && (targetB == 0)) {
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
        if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38 && (targetB == 0)) {
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
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

        incrementA = ((incrementA*999.0)+targetA)/1000.0;
        incrementB = ((incrementB*999.0)+targetB)/1000.0;
        //incrementA is the frequency derez
        //incrementB is the bit depth derez
        position += incrementA;

        long double outputSampleL = heldSampleL;
        long double outputSampleR = heldSampleR;
        if (position > 1.0)
        {
            position -= 1.0;
            heldSampleL = (lastSampleL * position) + (inputSampleL * (1.0-position));
            outputSampleL = (outputSampleL * (1.0-soften)) + (heldSampleL * soften);
            //softens the edge of the derez
            heldSampleR = (lastSampleR * position) + (inputSampleR * (1.0-position));
            outputSampleR = (outputSampleR * (1.0-soften)) + (heldSampleR * soften);
            //softens the edge of the derez
        }
        inputSampleL = outputSampleL;
        inputSampleR = outputSampleR;

        long double offset;
        if (incrementB > 0.0005)
        {
            if (inputSampleL > 0)
            {
                offset = inputSampleL;
                while (offset > 0) {offset -= incrementB;}
                inputSampleL -= offset;
                //it's below 0 so subtracting adds the remainder
            }
            if (inputSampleR > 0)
            {
                offset = inputSampleR;
                while (offset > 0) {offset -= incrementB;}
                inputSampleR -= offset;
                //it's below 0 so subtracting adds the remainder
            }

            if (inputSampleL < 0)
            {
                offset = inputSampleL;
                while (offset < 0) {offset += incrementB;}
                inputSampleL -= offset;
                //it's above 0 so subtracting subtracts the remainder
            }
            if (inputSampleR < 0)
            {
                offset = inputSampleR;
                while (offset < 0) {offset += incrementB;}
                inputSampleR -= offset;
                //it's above 0 so subtracting subtracts the remainder
            }

            inputSampleL *= (1.0 - incrementB);
            inputSampleR *= (1.0 - incrementB);
        }

        lastSampleL = drySampleL;
        lastSampleR = drySampleR;

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
