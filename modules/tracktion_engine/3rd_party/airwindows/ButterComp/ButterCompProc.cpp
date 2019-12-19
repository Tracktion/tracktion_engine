/* ========================================
 *  ButterComp - ButterComp.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __ButterComp_H
#include "ButterComp.h"
#endif

void ButterComp::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double inputposL;
    double inputnegL;
    double calcposL;
    double calcnegL;
    double outputposL;
    double outputnegL;
    long double totalmultiplierL;
    long double inputSampleL;
    double drySampleL;

    double inputposR;
    double inputnegR;
    double calcposR;
    double calcnegR;
    double outputposR;
    double outputnegR;
    long double totalmultiplierR;
    long double inputSampleR;
    double drySampleR;

    double inputgain = pow(10.0,(A*14.0)/20.0);
    double wet = B;
    double dry = 1.0 - wet;
    double outputgain = inputgain;
    outputgain -= 1.0;
    outputgain /= 1.5;
    outputgain += 1.0;
    double divisor = 0.012 * (A / 135.0);
    divisor /= overallscale;
    double remainder = divisor;
    divisor = 1.0 - divisor;

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

        inputSampleL *= inputgain;
        inputSampleR *= inputgain;

        inputposL = inputSampleL + 1.0;
        if (inputposL < 0.0) inputposL = 0.0;
        outputposL = inputposL / 2.0;
        if (outputposL > 1.0) outputposL = 1.0;
        inputposL *= inputposL;
        targetposL *= divisor;
        targetposL += (inputposL * remainder);
        calcposL = pow((1.0/targetposL),2);

        inputnegL = (-inputSampleL) + 1.0;
        if (inputnegL < 0.0) inputnegL = 0.0;
        outputnegL = inputnegL / 2.0;
        if (outputnegL > 1.0) outputnegL = 1.0;
        inputnegL *= inputnegL;
        targetnegL *= divisor;
        targetnegL += (inputnegL * remainder);
        calcnegL = pow((1.0/targetnegL),2);
        //now we have mirrored targets for comp
        //outputpos and outputneg go from 0 to 1

        inputposR = inputSampleR + 1.0;
        if (inputposR < 0.0) inputposR = 0.0;
        outputposR = inputposR / 2.0;
        if (outputposR > 1.0) outputposR = 1.0;
        inputposR *= inputposR;
        targetposR *= divisor;
        targetposR += (inputposR * remainder);
        calcposR = pow((1.0/targetposR),2);

        inputnegR = (-inputSampleR) + 1.0;
        if (inputnegR < 0.0) inputnegR = 0.0;
        outputnegR = inputnegR / 2.0;
        if (outputnegR > 1.0) outputnegR = 1.0;
        inputnegR *= inputnegR;
        targetnegR *= divisor;
        targetnegR += (inputnegR * remainder);
        calcnegR = pow((1.0/targetnegR),2);
        //now we have mirrored targets for comp
        //outputpos and outputneg go from 0 to 1


        if (inputSampleL > 0)
        { //working on pos
            controlAposL *= divisor;
            controlAposL += (calcposL*remainder);
        }
        else
        { //working on neg
            controlAnegL *= divisor;
            controlAnegL += (calcnegL*remainder);
        }
        //this causes each of the four to update only when active and in the correct 'flip'

        if (inputSampleR > 0)
        { //working on pos
            controlAposR *= divisor;
            controlAposR += (calcposR*remainder);
        }
        else
        { //working on neg
            controlAnegR *= divisor;
            controlAnegR += (calcnegR*remainder);
        }
        //this causes each of the four to update only when active and in the correct 'flip'

        totalmultiplierL = (controlAposL * outputposL) + (controlAnegL * outputnegL);
        totalmultiplierR = (controlAposR * outputposR) + (controlAnegR * outputnegR);
        //this combines the sides according to flip, blending relative to the input value

        inputSampleL *= totalmultiplierL;
        inputSampleL /= outputgain;

        inputSampleR *= totalmultiplierR;
        inputSampleR /= outputgain;

        if (wet !=1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
            inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
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

void ButterComp::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double inputposL;
    double inputnegL;
    double calcposL;
    double calcnegL;
    double outputposL;
    double outputnegL;
    long double totalmultiplierL;
    long double inputSampleL;
    double drySampleL;

    double inputposR;
    double inputnegR;
    double calcposR;
    double calcnegR;
    double outputposR;
    double outputnegR;
    long double totalmultiplierR;
    long double inputSampleR;
    double drySampleR;

    double inputgain = pow(10.0,(A*14.0)/20.0);
    double wet = B;
    double dry = 1.0 - wet;
    double outputgain = inputgain;
    outputgain -= 1.0;
    outputgain /= 1.5;
    outputgain += 1.0;
    double divisor = 0.012 * (A / 135.0);
    divisor /= overallscale;
    double remainder = divisor;
    divisor = 1.0 - divisor;

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

        inputSampleL *= inputgain;
        inputSampleR *= inputgain;

        inputposL = inputSampleL + 1.0;
        if (inputposL < 0.0) inputposL = 0.0;
        outputposL = inputposL / 2.0;
        if (outputposL > 1.0) outputposL = 1.0;
        inputposL *= inputposL;
        targetposL *= divisor;
        targetposL += (inputposL * remainder);
        calcposL = pow((1.0/targetposL),2);

        inputnegL = (-inputSampleL) + 1.0;
        if (inputnegL < 0.0) inputnegL = 0.0;
        outputnegL = inputnegL / 2.0;
        if (outputnegL > 1.0) outputnegL = 1.0;
        inputnegL *= inputnegL;
        targetnegL *= divisor;
        targetnegL += (inputnegL * remainder);
        calcnegL = pow((1.0/targetnegL),2);
        //now we have mirrored targets for comp
        //outputpos and outputneg go from 0 to 1

        inputposR = inputSampleR + 1.0;
        if (inputposR < 0.0) inputposR = 0.0;
        outputposR = inputposR / 2.0;
        if (outputposR > 1.0) outputposR = 1.0;
        inputposR *= inputposR;
        targetposR *= divisor;
        targetposR += (inputposR * remainder);
        calcposR = pow((1.0/targetposR),2);

        inputnegR = (-inputSampleR) + 1.0;
        if (inputnegR < 0.0) inputnegR = 0.0;
        outputnegR = inputnegR / 2.0;
        if (outputnegR > 1.0) outputnegR = 1.0;
        inputnegR *= inputnegR;
        targetnegR *= divisor;
        targetnegR += (inputnegR * remainder);
        calcnegR = pow((1.0/targetnegR),2);
        //now we have mirrored targets for comp
        //outputpos and outputneg go from 0 to 1


        if (inputSampleL > 0)
        { //working on pos
            controlAposL *= divisor;
            controlAposL += (calcposL*remainder);
        }
        else
        { //working on neg
            controlAnegL *= divisor;
            controlAnegL += (calcnegL*remainder);
        }
        //this causes each of the four to update only when active and in the correct 'flip'

        if (inputSampleR > 0)
        { //working on pos
            controlAposR *= divisor;
            controlAposR += (calcposR*remainder);
        }
        else
        { //working on neg
            controlAnegR *= divisor;
            controlAnegR += (calcnegR*remainder);
        }
        //this causes each of the four to update only when active and in the correct 'flip'

        totalmultiplierL = (controlAposL * outputposL) + (controlAnegL * outputnegL);
        totalmultiplierR = (controlAposR * outputposR) + (controlAnegR * outputnegR);
        //this combines the sides according to flip, blending relative to the input value

        inputSampleL *= totalmultiplierL;
        inputSampleL /= outputgain;

        inputSampleR *= totalmultiplierR;
        inputSampleR /= outputgain;

        if (wet !=1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * dry);
            inputSampleR = (inputSampleR * wet) + (drySampleR * dry);
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
