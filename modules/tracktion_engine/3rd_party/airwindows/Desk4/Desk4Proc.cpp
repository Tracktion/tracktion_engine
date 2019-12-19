/* ========================================
 *  Desk4 - Desk4.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Desk4_H
#include "Desk4.h"
#endif

void Desk4::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double gain = (pow(A,2)*10)+0.0001;
    double gaintrim = (pow(A,2)*2)+1.0;
    double slewgain = (pow(B,3)*40)+0.0001;
    double prevslew = 0.105;
    double intensity = (pow(C,6)*15)+0.0001;
    double depthA = (pow(D,4)*940)+0.00001;
    int offsetA = (int)(depthA * overallscale);
    if (offsetA < 1) offsetA = 1;
    if (offsetA > 4880) offsetA = 4880;
    double balanceB = 0.0001;
    slewgain *= overallscale;
    prevslew *= overallscale;
    balanceB /= overallscale;
    double outputgain = E;
    double wet = F;
    double dry = 1.0 - wet;

    double clampL;
    double clampR;
    double thicknessL;
    double thicknessR;
    double out;
    double balanceA = 1.0 - balanceB;
    double bridgerectifier;
    double slewL;
    double slewR;
    double combSampleL;
    double combSampleR;
    double drySampleL;
    double drySampleR;
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
        drySampleL = inputSampleL;
        drySampleR = inputSampleR;

        if (gcount < 0 || gcount > 4900) {gcount = 4900;}

        dL[gcount+4900] = dL[gcount] = fabs(inputSampleL)*intensity;
        controlL += (dL[gcount] / offsetA);
        controlL -= (dL[gcount+offsetA] / offsetA);
        controlL -= 0.000001;
        clampL = 1;
        if (controlL < 0) {controlL = 0;}
        if (controlL > 1) {clampL -= (controlL - 1); controlL = 1;}
        if (clampL < 0.5) {clampL = 0.5;}

        dR[gcount+4900] = dR[gcount] = fabs(inputSampleR)*intensity;
        controlR += (dR[gcount] / offsetA);
        controlR -= (dR[gcount+offsetA] / offsetA);
        controlR -= 0.000001;
        clampR = 1;
        if (controlR < 0) {controlR = 0;}
        if (controlR > 1) {clampR -= (controlR - 1); controlR = 1;}
        if (clampR < 0.5) {clampR = 0.5;}


        gcount--;
        //control = 0 to 1
        thicknessL = ((1.0 - controlL) * 2.0) - 1.0;
        thicknessR = ((1.0 - controlR) * 2.0) - 1.0;

        out = fabs(thicknessL);
        bridgerectifier = fabs(inputSampleL);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
        //max value for sine function
        if (thicknessL > 0) bridgerectifier = sin(bridgerectifier);
        else bridgerectifier = 1-cos(bridgerectifier);
        //produce either boosted or starved version
        if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-out))+(bridgerectifier*out);
        else inputSampleL = (inputSampleL*(1-out))-(bridgerectifier*out);
        //blend according to density control

        out = fabs(thicknessR);
        bridgerectifier = fabs(inputSampleR);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
        //max value for sine function
        if (thicknessR > 0) bridgerectifier = sin(bridgerectifier);
        else bridgerectifier = 1-cos(bridgerectifier);
        //produce either boosted or starved version
        if (inputSampleR > 0) inputSampleR = (inputSampleR*(1-out))+(bridgerectifier*out);
        else inputSampleR = (inputSampleR*(1-out))-(bridgerectifier*out);
        //blend according to density control

        inputSampleL *= clampL;
        inputSampleR *= clampR;

        slewL = inputSampleL - lastSampleL;
        lastSampleL = inputSampleL;
        //Set up direct reference for slew

        slewR = inputSampleR - lastSampleR;
        lastSampleR = inputSampleR;
        //Set up direct reference for slew

        bridgerectifier = fabs(slewL*slewgain);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
        else bridgerectifier = sin(bridgerectifier);
        if (slewL > 0) slewL = bridgerectifier/slewgain;
        else slewL = -(bridgerectifier/slewgain);

        bridgerectifier = fabs(slewR*slewgain);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
        else bridgerectifier = sin(bridgerectifier);
        if (slewR > 0) slewR = bridgerectifier/slewgain;
        else slewR = -(bridgerectifier/slewgain);

        inputSampleL = (lastOutSampleL*balanceA) + (lastSampleL*balanceB) + slewL;
        //go from last slewed, but include some raw values
        lastOutSampleL = inputSampleL;
        //Set up slewed reference

        inputSampleR = (lastOutSampleR*balanceA) + (lastSampleR*balanceB) + slewR;
        //go from last slewed, but include some raw values
        lastOutSampleR = inputSampleR;
        //Set up slewed reference

        combSampleL = fabs(drySampleL*lastSampleL);
        if (combSampleL > 1.0) combSampleL = 1.0;
        //bailout for very high input gains

        combSampleR = fabs(drySampleR*lastSampleR);
        if (combSampleR > 1.0) combSampleR = 1.0;
        //bailout for very high input gains

        inputSampleL -= (lastSlewL * combSampleL * prevslew);
        lastSlewL = slewL;
        //slew interaction with previous slew

        inputSampleR -= (lastSlewR * combSampleR * prevslew);
        lastSlewR = slewR;
        //slew interaction with previous slew

        inputSampleL *= gain;
        bridgerectifier = fabs(inputSampleL);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
        else bridgerectifier = sin(bridgerectifier);
        if (inputSampleL > 0) inputSampleL = bridgerectifier;
        else inputSampleL = -bridgerectifier;
        //drive section
        inputSampleL /= gain;
        inputSampleL *= gaintrim;
        //end of Desk section

        inputSampleR *= gain;
        bridgerectifier = fabs(inputSampleR);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
        else bridgerectifier = sin(bridgerectifier);
        if (inputSampleR > 0) inputSampleR = bridgerectifier;
        else inputSampleR = -bridgerectifier;
        //drive section
        inputSampleR /= gain;
        inputSampleR *= gaintrim;
        //end of Desk section

        if (outputgain != 1.0) {
            inputSampleL *= outputgain;
            inputSampleR *= outputgain;
        }

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

void Desk4::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double gain = (pow(A,2)*10)+0.0001;
    double gaintrim = (pow(A,2)*2)+1.0;
    double slewgain = (pow(B,3)*40)+0.0001;
    double prevslew = 0.105;
    double intensity = (pow(C,6)*15)+0.0001;
    double depthA = (pow(D,4)*940)+0.00001;
    int offsetA = (int)(depthA * overallscale);
    if (offsetA < 1) offsetA = 1;
    if (offsetA > 4880) offsetA = 4880;
    double balanceB = 0.0001;
    slewgain *= overallscale;
    prevslew *= overallscale;
    balanceB /= overallscale;
    double outputgain = E;
    double wet = F;
    double dry = 1.0 - wet;

    double clampL;
    double clampR;
    double thicknessL;
    double thicknessR;
    double out;
    double balanceA = 1.0 - balanceB;
    double bridgerectifier;
    double slewL;
    double slewR;
    double combSampleL;
    double combSampleR;
    double drySampleL;
    double drySampleR;
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
        drySampleL = inputSampleL;
        drySampleR = inputSampleR;

        if (gcount < 0 || gcount > 4900) {gcount = 4900;}

        dL[gcount+4900] = dL[gcount] = fabs(inputSampleL)*intensity;
        controlL += (dL[gcount] / offsetA);
        controlL -= (dL[gcount+offsetA] / offsetA);
        controlL -= 0.000001;
        clampL = 1;
        if (controlL < 0) {controlL = 0;}
        if (controlL > 1) {clampL -= (controlL - 1); controlL = 1;}
        if (clampL < 0.5) {clampL = 0.5;}

        dR[gcount+4900] = dR[gcount] = fabs(inputSampleR)*intensity;
        controlR += (dR[gcount] / offsetA);
        controlR -= (dR[gcount+offsetA] / offsetA);
        controlR -= 0.000001;
        clampR = 1;
        if (controlR < 0) {controlR = 0;}
        if (controlR > 1) {clampR -= (controlR - 1); controlR = 1;}
        if (clampR < 0.5) {clampR = 0.5;}


        gcount--;
        //control = 0 to 1
        thicknessL = ((1.0 - controlL) * 2.0) - 1.0;
        thicknessR = ((1.0 - controlR) * 2.0) - 1.0;

        out = fabs(thicknessL);
        bridgerectifier = fabs(inputSampleL);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
        //max value for sine function
        if (thicknessL > 0) bridgerectifier = sin(bridgerectifier);
        else bridgerectifier = 1-cos(bridgerectifier);
        //produce either boosted or starved version
        if (inputSampleL > 0) inputSampleL = (inputSampleL*(1-out))+(bridgerectifier*out);
        else inputSampleL = (inputSampleL*(1-out))-(bridgerectifier*out);
        //blend according to density control

        out = fabs(thicknessR);
        bridgerectifier = fabs(inputSampleR);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.57079633;
        //max value for sine function
        if (thicknessR > 0) bridgerectifier = sin(bridgerectifier);
        else bridgerectifier = 1-cos(bridgerectifier);
        //produce either boosted or starved version
        if (inputSampleR > 0) inputSampleR = (inputSampleR*(1-out))+(bridgerectifier*out);
        else inputSampleR = (inputSampleR*(1-out))-(bridgerectifier*out);
        //blend according to density control

        inputSampleL *= clampL;
        inputSampleR *= clampR;

        slewL = inputSampleL - lastSampleL;
        lastSampleL = inputSampleL;
        //Set up direct reference for slew

        slewR = inputSampleR - lastSampleR;
        lastSampleR = inputSampleR;
        //Set up direct reference for slew

        bridgerectifier = fabs(slewL*slewgain);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
        else bridgerectifier = sin(bridgerectifier);
        if (slewL > 0) slewL = bridgerectifier/slewgain;
        else slewL = -(bridgerectifier/slewgain);

        bridgerectifier = fabs(slewR*slewgain);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
        else bridgerectifier = sin(bridgerectifier);
        if (slewR > 0) slewR = bridgerectifier/slewgain;
        else slewR = -(bridgerectifier/slewgain);

        inputSampleL = (lastOutSampleL*balanceA) + (lastSampleL*balanceB) + slewL;
        //go from last slewed, but include some raw values
        lastOutSampleL = inputSampleL;
        //Set up slewed reference

        inputSampleR = (lastOutSampleR*balanceA) + (lastSampleR*balanceB) + slewR;
        //go from last slewed, but include some raw values
        lastOutSampleR = inputSampleR;
        //Set up slewed reference

        combSampleL = fabs(drySampleL*lastSampleL);
        if (combSampleL > 1.0) combSampleL = 1.0;
        //bailout for very high input gains

        combSampleR = fabs(drySampleR*lastSampleR);
        if (combSampleR > 1.0) combSampleR = 1.0;
        //bailout for very high input gains

        inputSampleL -= (lastSlewL * combSampleL * prevslew);
        lastSlewL = slewL;
        //slew interaction with previous slew

        inputSampleR -= (lastSlewR * combSampleR * prevslew);
        lastSlewR = slewR;
        //slew interaction with previous slew

        inputSampleL *= gain;
        bridgerectifier = fabs(inputSampleL);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
        else bridgerectifier = sin(bridgerectifier);
        if (inputSampleL > 0) inputSampleL = bridgerectifier;
        else inputSampleL = -bridgerectifier;
        //drive section
        inputSampleL /= gain;
        inputSampleL *= gaintrim;
        //end of Desk section

        inputSampleR *= gain;
        bridgerectifier = fabs(inputSampleR);
        if (bridgerectifier > 1.57079633) bridgerectifier = 1.0;
        else bridgerectifier = sin(bridgerectifier);
        if (inputSampleR > 0) inputSampleR = bridgerectifier;
        else inputSampleR = -bridgerectifier;
        //drive section
        inputSampleR /= gain;
        inputSampleR *= gaintrim;
        //end of Desk section

        if (outputgain != 1.0) {
            inputSampleL *= outputgain;
            inputSampleR *= outputgain;
        }

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
