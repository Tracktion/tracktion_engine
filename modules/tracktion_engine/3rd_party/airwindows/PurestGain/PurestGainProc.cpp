/* ========================================
 *  PurestGain - PurestGain.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PurestGain_H
#include "PurestGain.h"
#endif

void PurestGain::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double inputgain = (A * 80.0)-40.0;
    if (settingchase != inputgain) {
        chasespeed *= 2.0;
        settingchase = inputgain;
        //increment the slowness for each fader movement
        //continuous alteration makes it react smoother
        //sudden jump to setting, not so much
    }
    if (chasespeed > 2500.0) chasespeed = 2500.0;
    //bail out if it's too extreme
    if (gainchase < -60.0) {
        gainchase = pow(10.0,inputgain/20.0);
        //shouldn't even be a negative number
        //this is about starting at whatever's set, when
        //plugin is instantiated.
        //Otherwise it's the target, in dB.
    }
    double targetgain;
    //done with top controller
    double targetBgain = B;
    if (gainBchase < 0.0) gainBchase = targetBgain;
    //this one is not a dB value, but straight multiplication
    //done with slow fade controller
    double outputgain;


    long double inputSampleL;
    long double inputSampleR;

    //A is 0-1 (you can't feed other values to VST hosts, it's always 0-1 internally)
    //B is 0-1 and you need to multiply it by 100 if you want to use the 'percent'
    //C is 0-1 and if you can use a 0-1 value you can use it directly
    //D is 0-1 and you must set global parameters in PurestGain.SetParameter() to use it as a 'popup'
    //assign values here, possibly using const values as they won't change in this context

    while (--sampleFrames >= 0)
    {
        targetgain = pow(10.0,settingchase/20.0);
        //now we have the target in our temp variable

        chasespeed *= 0.9999;
        chasespeed -= 0.01;
        if (chasespeed < 350.0) chasespeed = 350.0;
        //we have our chase speed compensated for recent fader activity

        gainchase = (((gainchase*chasespeed)+targetgain)/(chasespeed+1.0));
        //gainchase is chasing the target, as a simple multiply gain factor

        gainBchase = (((gainBchase*4000)+targetBgain)/4001);
        //gainchase is chasing the target, as a simple multiply gain factor

        outputgain = gainchase * gainBchase;
        //directly multiply the dB gain by the straight multiply gain

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

        if (1.0 == outputgain)
        {
            *out1 = *in1;
            *out2 = *in2;
        } else {
            inputSampleL *= outputgain;
            inputSampleR *= outputgain;
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
        }

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}

void PurestGain::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double inputgain = (A * 80.0)-40.0;
    if (settingchase != inputgain) {
        chasespeed *= 2.0;
        settingchase = inputgain;
        //increment the slowness for each fader movement
        //continuous alteration makes it react smoother
        //sudden jump to setting, not so much
    }
    if (chasespeed > 2500.0) chasespeed = 2500.0;
    //bail out if it's too extreme
    if (gainchase < -60.0) {
        gainchase = pow(10.0,inputgain/20.0);
        //shouldn't even be a negative number
        //this is about starting at whatever's set, when
        //plugin is instantiated.
        //Otherwise it's the target, in dB.
    }
    double targetgain;
    //done with top controller
    double targetBgain = B;
    if (gainBchase < 0.0) gainBchase = targetBgain;
    //this one is not a dB value, but straight multiplication
    //done with slow fade controller
    double outputgain;


    long double inputSampleL;
    long double inputSampleR;

    while (--sampleFrames >= 0)
    {
        targetgain = pow(10.0,settingchase/20.0);
        //now we have the target in our temp variable

        chasespeed *= 0.9999;
        chasespeed -= 0.01;
        if (chasespeed < 350.0) chasespeed = 350.0;
        //we have our chase speed compensated for recent fader activity

        gainchase = (((gainchase*chasespeed)+targetgain)/(chasespeed+1.0));
        //gainchase is chasing the target, as a simple multiply gain factor

        gainBchase = (((gainBchase*4000)+targetBgain)/4001);
        //gainchase is chasing the target, as a simple multiply gain factor

        outputgain = gainchase * gainBchase;
        //directly multiply the dB gain by the straight multiply gain

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

        if (1.0 == outputgain)
        {
            *out1 = *in1;
            *out2 = *in2;
        } else {
            inputSampleL *= outputgain;
            inputSampleR *= outputgain;
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
        }

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}
