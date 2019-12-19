/* ========================================
 *  Holt - Holt.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Holt_H
#include "Holt.h"
#endif

void Holt::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double alpha = pow(A,4)+0.00001;
    if (alpha > 1.0) alpha = 1.0;
    double beta = (alpha * pow(B,2))+0.00001;
    alpha += ((1.0-beta)*pow(A,3)); //correct for droop in frequency
    if (alpha > 1.0) alpha = 1.0;

    long double trend;
    long double forecast; //defining these here because we're copying the routine four times

    double aWet = 1.0;
    double bWet = 1.0;
    double cWet = 1.0;
    double dWet = C*4.0;
    //four-stage wet/dry control using progressive stages that bypass when not engaged
    if (dWet < 1.0) {aWet = dWet; bWet = 0.0; cWet = 0.0; dWet = 0.0;}
    else if (dWet < 2.0) {bWet = dWet - 1.0; cWet = 0.0; dWet = 0.0;}
    else if (dWet < 3.0) {cWet = dWet - 2.0; dWet = 0.0;}
    else {dWet -= 3.0;}
    //this is one way to make a little set of dry/wet stages that are successively added to the
    //output as the control is turned up. Each one independently goes from 0-1 and stays at 1
    //beyond that point: this is a way to progressively add a 'black box' sound processing
    //which lets you fall through to simpler processing at lower settings.

    double gain = D;
    double wet = E;

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
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

        if (aWet > 0.0) {
            trend = (beta * (inputSampleL - previousSampleAL) + ((0.999-beta) * previousTrendAL));
            forecast = previousSampleAL + previousTrendAL;
            inputSampleL = (alpha * inputSampleL) + ((0.999-alpha) * forecast);
            previousSampleAL = inputSampleL; previousTrendAL = trend;
            inputSampleL = (inputSampleL * aWet) + (drySampleL * (1.0-aWet));

            trend = (beta * (inputSampleR - previousSampleAR) + ((0.999-beta) * previousTrendAR));
            forecast = previousSampleAR + previousTrendAR;
            inputSampleR = (alpha * inputSampleR) + ((0.999-alpha) * forecast);
            previousSampleAR = inputSampleR; previousTrendAR = trend;
            inputSampleR = (inputSampleR * aWet) + (drySampleR * (1.0-aWet));
        }

        if (bWet > 0.0) {
            trend = (beta * (inputSampleL - previousSampleBL) + ((0.999-beta) * previousTrendBL));
            forecast = previousSampleBL + previousTrendBL;
            inputSampleL = (alpha * inputSampleL) + ((0.999-alpha) * forecast);
            previousSampleBL = inputSampleL; previousTrendBL = trend;
            inputSampleL = (inputSampleL * bWet) + (previousSampleAL * (1.0-bWet));

            trend = (beta * (inputSampleR - previousSampleBR) + ((0.999-beta) * previousTrendBR));
            forecast = previousSampleBR + previousTrendBR;
            inputSampleR = (alpha * inputSampleR) + ((0.999-alpha) * forecast);
            previousSampleBR = inputSampleR; previousTrendBR = trend;
            inputSampleR = (inputSampleR * bWet) + (previousSampleAR * (1.0-bWet));
        }

        if (cWet > 0.0) {
            trend = (beta * (inputSampleL - previousSampleCL) + ((0.999-beta) * previousTrendCL));
            forecast = previousSampleCL + previousTrendCL;
            inputSampleL = (alpha * inputSampleL) + ((0.999-alpha) * forecast);
            previousSampleCL = inputSampleL; previousTrendCL = trend;
            inputSampleL = (inputSampleL * cWet) + (previousSampleBL * (1.0-cWet));

            trend = (beta * (inputSampleR - previousSampleCR) + ((0.999-beta) * previousTrendCR));
            forecast = previousSampleCR + previousTrendCR;
            inputSampleR = (alpha * inputSampleR) + ((0.999-alpha) * forecast);
            previousSampleCR = inputSampleR; previousTrendCR = trend;
            inputSampleR = (inputSampleR * cWet) + (previousSampleBR * (1.0-cWet));
        }

        if (dWet > 0.0) {
            trend = (beta * (inputSampleL - previousSampleDL) + ((0.999-beta) * previousTrendDL));
            forecast = previousSampleDL + previousTrendDL;
            inputSampleL = (alpha * inputSampleL) + ((0.999-alpha) * forecast);
            previousSampleDL = inputSampleL; previousTrendDL = trend;
            inputSampleL = (inputSampleL * dWet) + (previousSampleCL * (1.0-dWet));

            trend = (beta * (inputSampleR - previousSampleDR) + ((0.999-beta) * previousTrendDR));
            forecast = previousSampleDR + previousTrendDR;
            inputSampleR = (alpha * inputSampleR) + ((0.999-alpha) * forecast);
            previousSampleDR = inputSampleR; previousTrendDR = trend;
            inputSampleR = (inputSampleR * dWet) + (previousSampleCR * (1.0-dWet));
        }

        if (gain < 1.0) {
            inputSampleL *= gain;
            inputSampleR *= gain;
        }

        //clip to 1.2533141373155 to reach maximum output
        if (inputSampleL > 1.2533141373155) inputSampleL = 1.2533141373155;
        if (inputSampleL < -1.2533141373155) inputSampleL = -1.2533141373155;
        if (inputSampleR > 1.2533141373155) inputSampleR = 1.2533141373155;
        if (inputSampleR < -1.2533141373155) inputSampleR = -1.2533141373155;
        inputSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((inputSampleL == 0.0) ?1:fabs(inputSampleL));
        inputSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((inputSampleR == 0.0) ?1:fabs(inputSampleR));

        if (wet < 1.0) {
            inputSampleL = (inputSampleL*wet)+(drySampleL*(1.0-wet));
            inputSampleR = (inputSampleR*wet)+(drySampleR*(1.0-wet));
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

void Holt::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double alpha = pow(A,4)+0.00001;
    if (alpha > 1.0) alpha = 1.0;
    double beta = (alpha * pow(B,2))+0.00001;
    alpha += ((1.0-beta)*pow(A,3)); //correct for droop in frequency
    if (alpha > 1.0) alpha = 1.0;

    long double trend;
    long double forecast; //defining these here because we're copying the routine four times

    double aWet = 1.0;
    double bWet = 1.0;
    double cWet = 1.0;
    double dWet = C*4.0;
    //four-stage wet/dry control using progressive stages that bypass when not engaged
    if (dWet < 1.0) {aWet = dWet; bWet = 0.0; cWet = 0.0; dWet = 0.0;}
    else if (dWet < 2.0) {bWet = dWet - 1.0; cWet = 0.0; dWet = 0.0;}
    else if (dWet < 3.0) {cWet = dWet - 2.0; dWet = 0.0;}
    else {dWet -= 3.0;}
    //this is one way to make a little set of dry/wet stages that are successively added to the
    //output as the control is turned up. Each one independently goes from 0-1 and stays at 1
    //beyond that point: this is a way to progressively add a 'black box' sound processing
    //which lets you fall through to simpler processing at lower settings.

    double gain = D;
    double wet = E;

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
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

        if (aWet > 0.0) {
            trend = (beta * (inputSampleL - previousSampleAL) + ((0.999-beta) * previousTrendAL));
            forecast = previousSampleAL + previousTrendAL;
            inputSampleL = (alpha * inputSampleL) + ((0.999-alpha) * forecast);
            previousSampleAL = inputSampleL; previousTrendAL = trend;
            inputSampleL = (inputSampleL * aWet) + (drySampleL * (1.0-aWet));

            trend = (beta * (inputSampleR - previousSampleAR) + ((0.999-beta) * previousTrendAR));
            forecast = previousSampleAR + previousTrendAR;
            inputSampleR = (alpha * inputSampleR) + ((0.999-alpha) * forecast);
            previousSampleAR = inputSampleR; previousTrendAR = trend;
            inputSampleR = (inputSampleR * aWet) + (drySampleR * (1.0-aWet));
        }

        if (bWet > 0.0) {
            trend = (beta * (inputSampleL - previousSampleBL) + ((0.999-beta) * previousTrendBL));
            forecast = previousSampleBL + previousTrendBL;
            inputSampleL = (alpha * inputSampleL) + ((0.999-alpha) * forecast);
            previousSampleBL = inputSampleL; previousTrendBL = trend;
            inputSampleL = (inputSampleL * bWet) + (previousSampleAL * (1.0-bWet));

            trend = (beta * (inputSampleR - previousSampleBR) + ((0.999-beta) * previousTrendBR));
            forecast = previousSampleBR + previousTrendBR;
            inputSampleR = (alpha * inputSampleR) + ((0.999-alpha) * forecast);
            previousSampleBR = inputSampleR; previousTrendBR = trend;
            inputSampleR = (inputSampleR * bWet) + (previousSampleAR * (1.0-bWet));
        }

        if (cWet > 0.0) {
            trend = (beta * (inputSampleL - previousSampleCL) + ((0.999-beta) * previousTrendCL));
            forecast = previousSampleCL + previousTrendCL;
            inputSampleL = (alpha * inputSampleL) + ((0.999-alpha) * forecast);
            previousSampleCL = inputSampleL; previousTrendCL = trend;
            inputSampleL = (inputSampleL * cWet) + (previousSampleBL * (1.0-cWet));

            trend = (beta * (inputSampleR - previousSampleCR) + ((0.999-beta) * previousTrendCR));
            forecast = previousSampleCR + previousTrendCR;
            inputSampleR = (alpha * inputSampleR) + ((0.999-alpha) * forecast);
            previousSampleCR = inputSampleR; previousTrendCR = trend;
            inputSampleR = (inputSampleR * cWet) + (previousSampleBR * (1.0-cWet));
        }

        if (dWet > 0.0) {
            trend = (beta * (inputSampleL - previousSampleDL) + ((0.999-beta) * previousTrendDL));
            forecast = previousSampleDL + previousTrendDL;
            inputSampleL = (alpha * inputSampleL) + ((0.999-alpha) * forecast);
            previousSampleDL = inputSampleL; previousTrendDL = trend;
            inputSampleL = (inputSampleL * dWet) + (previousSampleCL * (1.0-dWet));

            trend = (beta * (inputSampleR - previousSampleDR) + ((0.999-beta) * previousTrendDR));
            forecast = previousSampleDR + previousTrendDR;
            inputSampleR = (alpha * inputSampleR) + ((0.999-alpha) * forecast);
            previousSampleDR = inputSampleR; previousTrendDR = trend;
            inputSampleR = (inputSampleR * dWet) + (previousSampleCR * (1.0-dWet));
        }

        if (gain < 1.0) {
            inputSampleL *= gain;
            inputSampleR *= gain;
        }

        //clip to 1.2533141373155 to reach maximum output
        if (inputSampleL > 1.2533141373155) inputSampleL = 1.2533141373155;
        if (inputSampleL < -1.2533141373155) inputSampleL = -1.2533141373155;
        if (inputSampleR > 1.2533141373155) inputSampleR = 1.2533141373155;
        if (inputSampleR < -1.2533141373155) inputSampleR = -1.2533141373155;
        inputSampleL = sin(inputSampleL * fabs(inputSampleL)) / ((inputSampleL == 0.0) ?1:fabs(inputSampleL));
        inputSampleR = sin(inputSampleR * fabs(inputSampleR)) / ((inputSampleR == 0.0) ?1:fabs(inputSampleR));

        if (wet < 1.0) {
            inputSampleL = (inputSampleL*wet)+(drySampleL*(1.0-wet));
            inputSampleR = (inputSampleR*wet)+(drySampleR*(1.0-wet));
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
