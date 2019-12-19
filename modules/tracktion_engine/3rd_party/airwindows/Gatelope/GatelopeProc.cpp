/* ========================================
 *  Gatelope - Gatelope.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Gatelope_H
#include "Gatelope.h"
#endif

void Gatelope::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();
    //speed settings around release
    double threshold = pow(A,2);
    //gain settings around threshold
    double trebledecay = pow(1.0-B,2)/4196.0;
    double bassdecay =  pow(1.0-C,2)/8192.0;
    double slowAttack = (pow(D,3)*3)+0.003;
    double wet = E;
    slowAttack /= overallscale;
    trebledecay /= overallscale;
    bassdecay /= overallscale;
    trebledecay += 1.0;
    bassdecay += 1.0;
    double attackSpeed;
    double highestSample;
    //this VST version comes from the AU, Gatelinked, because it's stereo.
    //if used on a mono track it'll act like the mono N to N

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
        double drySampleL = inputSampleL;
        double drySampleR = inputSampleR;

        if (fabs(inputSampleL) > fabs(inputSampleR)) {
            attackSpeed = slowAttack - (fabs(inputSampleL)*slowAttack*0.5);
            highestSample = fabs(inputSampleL);
        } else {
            attackSpeed = slowAttack - (fabs(inputSampleR)*slowAttack*0.5); //we're triggering off the highest amplitude
            highestSample = fabs(inputSampleR); //and making highestSample the abs() of that amplitude
        }

        if (attackSpeed < 0.0) attackSpeed = 0.0;
        //softening onset click depending on how hard we're getting it

        if (flip)
        {
            if (highestSample > threshold)
            {
                treblefreq += attackSpeed;
                if (treblefreq > 2.0) treblefreq = 2.0;
                bassfreq -= attackSpeed;
                bassfreq -= attackSpeed;
                if (bassfreq < 0.0) bassfreq = 0.0;
                iirLowpassAL = iirLowpassBL = inputSampleL;
                iirHighpassAL = iirHighpassBL = 0.0;
                iirLowpassAR = iirLowpassBR = inputSampleR;
                iirHighpassAR = iirHighpassBR = 0.0;
            }
            else
            {
                treblefreq -= bassfreq;
                treblefreq /= trebledecay;
                treblefreq += bassfreq;
                bassfreq -= treblefreq;
                bassfreq /= bassdecay;
                bassfreq += treblefreq;
            }

            if (treblefreq >= 1.0) {
                iirLowpassAL = inputSampleL;
                iirLowpassAR = inputSampleR;
            } else {
                iirLowpassAL = (iirLowpassAL * (1.0 - treblefreq)) + (inputSampleL * treblefreq);
                iirLowpassAR = (iirLowpassAR * (1.0 - treblefreq)) + (inputSampleR * treblefreq);
            }

            if (bassfreq > 1.0) bassfreq = 1.0;

            if (bassfreq > 0.0) {
                iirHighpassAL = (iirHighpassAL * (1.0 - bassfreq)) + (inputSampleL * bassfreq);
                iirHighpassAR = (iirHighpassAR * (1.0 - bassfreq)) + (inputSampleR * bassfreq);
            } else {
                iirHighpassAL = 0.0;
                iirHighpassAR = 0.0;
            }

            if (treblefreq > bassfreq) {
                inputSampleL = (iirLowpassAL - iirHighpassAL);
                inputSampleR = (iirLowpassAR - iirHighpassAR);
            } else {
                inputSampleL = 0.0;
                inputSampleR = 0.0;
            }
        }
        else
        {
            if (highestSample > threshold)
            {
                treblefreq += attackSpeed;
                if (treblefreq > 2.0) treblefreq = 2.0;
                bassfreq -= attackSpeed;
                bassfreq -= attackSpeed;
                if (bassfreq < 0.0) bassfreq = 0.0;
                iirLowpassAL = iirLowpassBL = inputSampleL;
                iirHighpassAL = iirHighpassBL = 0.0;
                iirLowpassAR = iirLowpassBR = inputSampleR;
                iirHighpassAR = iirHighpassBR = 0.0;
            }
            else
            {
                treblefreq -= bassfreq;
                treblefreq /= trebledecay;
                treblefreq += bassfreq;
                bassfreq -= treblefreq;
                bassfreq /= bassdecay;
                bassfreq += treblefreq;
            }

            if (treblefreq >= 1.0) {
                iirLowpassBL = inputSampleL;
                iirLowpassBR = inputSampleR;
            } else {
                iirLowpassBL = (iirLowpassBL * (1.0 - treblefreq)) + (inputSampleL * treblefreq);
                iirLowpassBR = (iirLowpassBR * (1.0 - treblefreq)) + (inputSampleR * treblefreq);
            }

            if (bassfreq > 1.0) bassfreq = 1.0;

            if (bassfreq > 0.0) {
                iirHighpassBL = (iirHighpassBL * (1.0 - bassfreq)) + (inputSampleL * bassfreq);
                iirHighpassBR = (iirHighpassBR * (1.0 - bassfreq)) + (inputSampleR * bassfreq);
            } else {
                iirHighpassBL = 0.0;
                iirHighpassBR = 0.0;
            }

            if (treblefreq > bassfreq) {
                inputSampleL = (iirLowpassBL - iirHighpassBL);
                inputSampleR = (iirLowpassBR - iirHighpassBR);
            } else {
                inputSampleL = 0.0;
                inputSampleR = 0.0;
            }
        }
        //done full gated envelope filtered effect
        inputSampleL  = ((1-wet)*drySampleL)+(wet*inputSampleL);
        inputSampleR  = ((1-wet)*drySampleR)+(wet*inputSampleR);
        //we're going to set up a dry/wet control instead of a min. threshold

        flip = !flip;

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

void Gatelope::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();
    //speed settings around release
    double threshold = pow(A,2);
    //gain settings around threshold
    double trebledecay = pow(1.0-B,2)/4196.0;
    double bassdecay =  pow(1.0-C,2)/8192.0;
    double slowAttack = (pow(D,3)*3)+0.003;
    double wet = E;
    slowAttack /= overallscale;
    trebledecay /= overallscale;
    bassdecay /= overallscale;
    trebledecay += 1.0;
    bassdecay += 1.0;
    double attackSpeed;
    double highestSample;
    //this VST version comes from the AU, Gatelinked, because it's stereo.
    //if used on a mono track it'll act like the mono N to N

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
        double drySampleL = inputSampleL;
        double drySampleR = inputSampleR;

        if (fabs(inputSampleL) > fabs(inputSampleR)) {
            attackSpeed = slowAttack - (fabs(inputSampleL)*slowAttack*0.5);
            highestSample = fabs(inputSampleL);
        } else {
            attackSpeed = slowAttack - (fabs(inputSampleR)*slowAttack*0.5); //we're triggering off the highest amplitude
            highestSample = fabs(inputSampleR); //and making highestSample the abs() of that amplitude
        }

        if (attackSpeed < 0.0) attackSpeed = 0.0;
        //softening onset click depending on how hard we're getting it

        if (flip)
        {
            if (highestSample > threshold)
            {
                treblefreq += attackSpeed;
                if (treblefreq > 2.0) treblefreq = 2.0;
                bassfreq -= attackSpeed;
                bassfreq -= attackSpeed;
                if (bassfreq < 0.0) bassfreq = 0.0;
                iirLowpassAL = iirLowpassBL = inputSampleL;
                iirHighpassAL = iirHighpassBL = 0.0;
                iirLowpassAR = iirLowpassBR = inputSampleR;
                iirHighpassAR = iirHighpassBR = 0.0;
            }
            else
            {
                treblefreq -= bassfreq;
                treblefreq /= trebledecay;
                treblefreq += bassfreq;
                bassfreq -= treblefreq;
                bassfreq /= bassdecay;
                bassfreq += treblefreq;
            }

            if (treblefreq >= 1.0) {
                iirLowpassAL = inputSampleL;
                iirLowpassAR = inputSampleR;
            } else {
                iirLowpassAL = (iirLowpassAL * (1.0 - treblefreq)) + (inputSampleL * treblefreq);
                iirLowpassAR = (iirLowpassAR * (1.0 - treblefreq)) + (inputSampleR * treblefreq);
            }

            if (bassfreq > 1.0) bassfreq = 1.0;

            if (bassfreq > 0.0) {
                iirHighpassAL = (iirHighpassAL * (1.0 - bassfreq)) + (inputSampleL * bassfreq);
                iirHighpassAR = (iirHighpassAR * (1.0 - bassfreq)) + (inputSampleR * bassfreq);
            } else {
                iirHighpassAL = 0.0;
                iirHighpassAR = 0.0;
            }

            if (treblefreq > bassfreq) {
                inputSampleL = (iirLowpassAL - iirHighpassAL);
                inputSampleR = (iirLowpassAR - iirHighpassAR);
            } else {
                inputSampleL = 0.0;
                inputSampleR = 0.0;
            }
        }
        else
        {
            if (highestSample > threshold)
            {
                treblefreq += attackSpeed;
                if (treblefreq > 2.0) treblefreq = 2.0;
                bassfreq -= attackSpeed;
                bassfreq -= attackSpeed;
                if (bassfreq < 0.0) bassfreq = 0.0;
                iirLowpassAL = iirLowpassBL = inputSampleL;
                iirHighpassAL = iirHighpassBL = 0.0;
                iirLowpassAR = iirLowpassBR = inputSampleR;
                iirHighpassAR = iirHighpassBR = 0.0;
            }
            else
            {
                treblefreq -= bassfreq;
                treblefreq /= trebledecay;
                treblefreq += bassfreq;
                bassfreq -= treblefreq;
                bassfreq /= bassdecay;
                bassfreq += treblefreq;
            }

            if (treblefreq >= 1.0) {
                iirLowpassBL = inputSampleL;
                iirLowpassBR = inputSampleR;
            } else {
                iirLowpassBL = (iirLowpassBL * (1.0 - treblefreq)) + (inputSampleL * treblefreq);
                iirLowpassBR = (iirLowpassBR * (1.0 - treblefreq)) + (inputSampleR * treblefreq);
            }

            if (bassfreq > 1.0) bassfreq = 1.0;

            if (bassfreq > 0.0) {
                iirHighpassBL = (iirHighpassBL * (1.0 - bassfreq)) + (inputSampleL * bassfreq);
                iirHighpassBR = (iirHighpassBR * (1.0 - bassfreq)) + (inputSampleR * bassfreq);
            } else {
                iirHighpassBL = 0.0;
                iirHighpassBR = 0.0;
            }

            if (treblefreq > bassfreq) {
                inputSampleL = (iirLowpassBL - iirHighpassBL);
                inputSampleR = (iirLowpassBR - iirHighpassBR);
            } else {
                inputSampleL = 0.0;
                inputSampleR = 0.0;
            }
        }
        //done full gated envelope filtered effect
        inputSampleL  = ((1-wet)*drySampleL)+(wet*inputSampleL);
        inputSampleR  = ((1-wet)*drySampleR)+(wet*inputSampleR);
        //we're going to set up a dry/wet control instead of a min. threshold

        flip = !flip;

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
