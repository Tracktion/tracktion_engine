/* ========================================
 *  Console5Buss - Console5Buss.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Console5Buss_H
#include "Console5Buss.h"
#endif

void Console5Buss::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double inputgain = A;
    double differenceL;
    double differenceR;
    double nearZeroL;
    double nearZeroR;
    double servoTrim = 0.0000001 / overallscale;
    double bassTrim = 0.005 / overallscale;
    long double inputSampleL;
    long double inputSampleR;

    if (settingchase != inputgain) {
        chasespeed *= 2.0;
        settingchase = inputgain;
    }
    if (chasespeed > 2500.0) chasespeed = 2500.0;
    if (gainchase < 0.0) gainchase = inputgain;

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

        chasespeed *= 0.9999;
        chasespeed -= 0.01;
        if (chasespeed < 350.0) chasespeed = 350.0;
        //we have our chase speed compensated for recent fader activity

        gainchase = (((gainchase*chasespeed)+inputgain)/(chasespeed+1.0));
        //gainchase is chasing the target, as a simple multiply gain factor

        if (1.0 != gainchase) {
            inputSampleL *= gainchase;
            inputSampleR *= gainchase;
        }
        //done with trim control

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        inputSampleL = asin(inputSampleL);
        //amplitude aspect

        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        differenceL = lastSampleBussL - inputSampleL;
        differenceR = lastSampleBussR - inputSampleR;
        lastSampleBussL = inputSampleL;
        lastSampleBussR = inputSampleR;
        //derive slew part off direct sample measurement + from last time

        if (differenceL > 1.57079633) differenceL = 1.57079633;
        if (differenceL < -1.57079633) differenceL = -1.57079633;
        if (differenceR > 1.57079633) differenceR = 1.57079633;
        if (differenceR < -1.57079633) differenceR = -1.57079633;

        differenceL = lastFXBussL + sin(differenceL);
        differenceR = lastFXBussR + sin(differenceR);
        //we're about to use this twice and then not use difference again, so we'll reuse it
        //enhance slew is arcsin(): cutting it back is sin()

        iirCorrectL += inputSampleL - differenceL;
        iirCorrectR += inputSampleR - differenceR;
        inputSampleL = differenceL;
        inputSampleR = differenceR;
        //apply the slew to stored value: can develop DC offsets.
        //store the change we made so we can dial it back

        lastFXBussL = inputSampleL;
        lastFXBussR = inputSampleR;
        if (lastFXBussL > 1.0) lastFXBussL = 1.0;
        if (lastFXBussL < -1.0) lastFXBussL = -1.0;
        if (lastFXBussR > 1.0) lastFXBussR = 1.0;
        if (lastFXBussR < -1.0) lastFXBussR = -1.0;
        //build new signal off what was present in output last time

        nearZeroL = pow(fabs(fabs(lastFXBussL)-1.0), 2);
        nearZeroR = pow(fabs(fabs(lastFXBussR)-1.0), 2);
        //if the sample is very near zero this number is higher.
        if (iirCorrectL > 0) iirCorrectL -= servoTrim;
        if (iirCorrectL < 0) iirCorrectL += servoTrim;
        if (iirCorrectR > 0) iirCorrectR -= servoTrim;
        if (iirCorrectR < 0) iirCorrectR += servoTrim;
        //cut back the servo by which we're pulling back the DC
        lastFXBussL += (iirCorrectL * 0.0000005);
        lastFXBussR += (iirCorrectR * 0.0000005);
        //apply the servo to the stored value, pulling back the DC
        lastFXBussL *= (1.0 - (nearZeroL * bassTrim));
        lastFXBussR *= (1.0 - (nearZeroR * bassTrim));
        //this cuts back the DC offset directly, relative to how near zero we are

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

void Console5Buss::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double inputgain = A;
    double differenceL;
    double differenceR;
    double nearZeroL;
    double nearZeroR;
    double servoTrim = 0.0000001 / overallscale;
    double bassTrim = 0.005 / overallscale;
    long double inputSampleL;
    long double inputSampleR;

    if (settingchase != inputgain) {
        chasespeed *= 2.0;
        settingchase = inputgain;
    }
    if (chasespeed > 2500.0) chasespeed = 2500.0;
    if (gainchase < 0.0) gainchase = inputgain;

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

        chasespeed *= 0.9999;
        chasespeed -= 0.01;
        if (chasespeed < 350.0) chasespeed = 350.0;
        //we have our chase speed compensated for recent fader activity

        gainchase = (((gainchase*chasespeed)+inputgain)/(chasespeed+1.0));
        //gainchase is chasing the target, as a simple multiply gain factor

        if (1.0 != gainchase) {
            inputSampleL *= gainchase;
            inputSampleR *= gainchase;
        }
        //done with trim control

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        inputSampleL = asin(inputSampleL);
        //amplitude aspect

        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        differenceL = lastSampleBussL - inputSampleL;
        differenceR = lastSampleBussR - inputSampleR;
        lastSampleBussL = inputSampleL;
        lastSampleBussR = inputSampleR;
        //derive slew part off direct sample measurement + from last time

        if (differenceL > 1.57079633) differenceL = 1.57079633;
        if (differenceL < -1.57079633) differenceL = -1.57079633;
        if (differenceR > 1.57079633) differenceR = 1.57079633;
        if (differenceR < -1.57079633) differenceR = -1.57079633;

        differenceL = lastFXBussL + sin(differenceL);
        differenceR = lastFXBussR + sin(differenceR);
        //we're about to use this twice and then not use difference again, so we'll reuse it
        //enhance slew is arcsin(): cutting it back is sin()

        iirCorrectL += inputSampleL - differenceL;
        iirCorrectR += inputSampleR - differenceR;
        inputSampleL = differenceL;
        inputSampleR = differenceR;
        //apply the slew to stored value: can develop DC offsets.
        //store the change we made so we can dial it back

        lastFXBussL = inputSampleL;
        lastFXBussR = inputSampleR;
        if (lastFXBussL > 1.0) lastFXBussL = 1.0;
        if (lastFXBussL < -1.0) lastFXBussL = -1.0;
        if (lastFXBussR > 1.0) lastFXBussR = 1.0;
        if (lastFXBussR < -1.0) lastFXBussR = -1.0;
        //build new signal off what was present in output last time

        nearZeroL = pow(fabs(fabs(lastFXBussL)-1.0), 2);
        nearZeroR = pow(fabs(fabs(lastFXBussR)-1.0), 2);
        //if the sample is very near zero this number is higher.
        if (iirCorrectL > 0) iirCorrectL -= servoTrim;
        if (iirCorrectL < 0) iirCorrectL += servoTrim;
        if (iirCorrectR > 0) iirCorrectR -= servoTrim;
        if (iirCorrectR < 0) iirCorrectR += servoTrim;
        //cut back the servo by which we're pulling back the DC
        lastFXBussL += (iirCorrectL * 0.0000005);
        lastFXBussR += (iirCorrectR * 0.0000005);
        //apply the servo to the stored value, pulling back the DC
        lastFXBussL *= (1.0 - (nearZeroL * bassTrim));
        lastFXBussR *= (1.0 - (nearZeroR * bassTrim));
        //this cuts back the DC offset directly, relative to how near zero we are

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
