/* ========================================
 *  Channel6 - Channel6.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Channel6_H
#include "Channel6.h"
#endif

void Channel6::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();
    const double localiirAmount = iirAmount / overallscale;
    const double localthreshold = threshold / overallscale;
    const double density = pow(drive,2); //this doesn't relate to the plugins Density and Drive much

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;

        static int noisesourceL = 0;
        static int noisesourceR = 850010;
        int residue;
        double applyresidue;

        noisesourceL = noisesourceL % 1700021; noisesourceL++;
        residue = noisesourceL * noisesourceL;
        residue = residue % 170003; residue *= residue;
        residue = residue % 17011; residue *= residue;
        residue = residue % 1709; residue *= residue;
        residue = residue % 173; residue *= residue;
        residue = residue % 17;
        applyresidue = residue;
        applyresidue *= 0.00000001;
        applyresidue *= 0.00000001;
        inputSampleL += applyresidue;
        if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
            inputSampleL -= applyresidue;
        }

        noisesourceR = noisesourceR % 1700021; noisesourceR++;
        residue = noisesourceR * noisesourceR;
        residue = residue % 170003; residue *= residue;
        residue = residue % 17011; residue *= residue;
        residue = residue % 1709; residue *= residue;
        residue = residue % 173; residue *= residue;
        residue = residue % 17;
        applyresidue = residue;
        applyresidue *= 0.00000001;
        applyresidue *= 0.00000001;
        inputSampleR += applyresidue;
        if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
            inputSampleR -= applyresidue;
        }
        //for live air, we always apply the dither noise. Then, if our result is
        //effectively digital black, we'll subtract it again. We want a 'air' hiss

        if (flip)
        {
            iirSampleLA = (iirSampleLA * (1 - localiirAmount)) + (inputSampleL * localiirAmount);
            inputSampleL = inputSampleL - iirSampleLA;
            iirSampleRA = (iirSampleRA * (1 - localiirAmount)) + (inputSampleR * localiirAmount);
            inputSampleR = inputSampleR - iirSampleRA;
        }
        else
        {
            iirSampleLB = (iirSampleLB * (1 - localiirAmount)) + (inputSampleL * localiirAmount);
            inputSampleL = inputSampleL - iirSampleLB;
            iirSampleRB = (iirSampleRB * (1 - localiirAmount)) + (inputSampleR * localiirAmount);
            inputSampleR = inputSampleR - iirSampleRB;
        }
        //highpass section
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;


        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        inputSampleL *= 1.2533141373155;
        //clip to 1.2533141373155 to reach maximum output

        long double distSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((inputSampleL == 0.0) ?1:fabs(inputSampleL));
        inputSampleL = (drySampleL*(1-density))+(distSampleL*density);
        //drive section

        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        inputSampleR *= 1.2533141373155;
        //clip to 1.2533141373155 to reach maximum output

        long double distSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((inputSampleR == 0.0) ?1:fabs(inputSampleR));
        inputSampleR = (drySampleR*(1-density))+(distSampleR*density);
        //drive section

        double clamp = inputSampleL - lastSampleL;
        if (clamp > localthreshold)
            inputSampleL = lastSampleL + localthreshold;
        if (-clamp > localthreshold)
            inputSampleL = lastSampleL - localthreshold;
        lastSampleL = inputSampleL;

        clamp = inputSampleR - lastSampleR;
        if (clamp > localthreshold)
            inputSampleR = lastSampleR + localthreshold;
        if (-clamp > localthreshold)
            inputSampleR = lastSampleR - localthreshold;
        lastSampleR = inputSampleR;
        //slew section
        flip = !flip;

        if (output < 1.0) {
            inputSampleL *= output;
            inputSampleR *= output;
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

void Channel6::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();
    const double localiirAmount = iirAmount / overallscale;
    const double localthreshold = threshold / overallscale;
    const double density = pow(drive,2); //this doesn't relate to the plugins Density and Drive much

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;

        static int noisesourceL = 0;
        static int noisesourceR = 850010;
        int residue;
        double applyresidue;

        noisesourceL = noisesourceL % 1700021; noisesourceL++;
        residue = noisesourceL * noisesourceL;
        residue = residue % 170003; residue *= residue;
        residue = residue % 17011; residue *= residue;
        residue = residue % 1709; residue *= residue;
        residue = residue % 173; residue *= residue;
        residue = residue % 17;
        applyresidue = residue;
        applyresidue *= 0.00000001;
        applyresidue *= 0.00000001;
        inputSampleL += applyresidue;
        if (inputSampleL<1.2e-38 && -inputSampleL<1.2e-38) {
            inputSampleL -= applyresidue;
        }

        noisesourceR = noisesourceR % 1700021; noisesourceR++;
        residue = noisesourceR * noisesourceR;
        residue = residue % 170003; residue *= residue;
        residue = residue % 17011; residue *= residue;
        residue = residue % 1709; residue *= residue;
        residue = residue % 173; residue *= residue;
        residue = residue % 17;
        applyresidue = residue;
        applyresidue *= 0.00000001;
        applyresidue *= 0.00000001;
        inputSampleR += applyresidue;
        if (inputSampleR<1.2e-38 && -inputSampleR<1.2e-38) {
            inputSampleR -= applyresidue;
        }
        //for live air, we always apply the dither noise. Then, if our result is
        //effectively digital black, we'll subtract it again. We want a 'air' hiss

        if (flip)
        {
            iirSampleLA = (iirSampleLA * (1 - localiirAmount)) + (inputSampleL * localiirAmount);
            inputSampleL = inputSampleL - iirSampleLA;
            iirSampleRA = (iirSampleRA * (1 - localiirAmount)) + (inputSampleR * localiirAmount);
            inputSampleR = inputSampleR - iirSampleRA;
        }
        else
        {
            iirSampleLB = (iirSampleLB * (1 - localiirAmount)) + (inputSampleL * localiirAmount);
            inputSampleL = inputSampleL - iirSampleLB;
            iirSampleRB = (iirSampleRB * (1 - localiirAmount)) + (inputSampleR * localiirAmount);
            inputSampleR = inputSampleR - iirSampleRB;
        }
        //highpass section
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;


        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        inputSampleL *= 1.2533141373155;
        //clip to 1.2533141373155 to reach maximum output

        long double distSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((inputSampleL == 0.0) ?1:fabs(inputSampleL));
        inputSampleL = (drySampleL*(1-density))+(distSampleL*density);
        //drive section

        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        inputSampleR *= 1.2533141373155;
        //clip to 1.2533141373155 to reach maximum output

        long double distSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((inputSampleR == 0.0) ?1:fabs(inputSampleR));
        inputSampleR = (drySampleR*(1-density))+(distSampleR*density);
        //drive section

        double clamp = inputSampleL - lastSampleL;
        if (clamp > localthreshold)
            inputSampleL = lastSampleL + localthreshold;
        if (-clamp > localthreshold)
            inputSampleL = lastSampleL - localthreshold;
        lastSampleL = inputSampleL;

        clamp = inputSampleR - lastSampleR;
        if (clamp > localthreshold)
            inputSampleR = lastSampleR + localthreshold;
        if (-clamp > localthreshold)
            inputSampleR = lastSampleR - localthreshold;
        lastSampleR = inputSampleR;
        //slew section
        flip = !flip;

        if (output < 1.0) {
            inputSampleL *= output;
            inputSampleR *= output;
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
