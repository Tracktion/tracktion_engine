/* ========================================
 *  PeaksOnly - PeaksOnly.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PeaksOnly_H
#include "PeaksOnly.h"
#endif

void PeaksOnly::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    int am = (int)149.0 * overallscale;
    int bm = (int)179.0 * overallscale;
    int cm = (int)191.0 * overallscale;
    int dm = (int)223.0 * overallscale; //these are 'good' primes, spacing out the allpasses
    int allpasstemp = 0;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        allpasstemp = ax - 1; if (allpasstemp < 0 || allpasstemp > am) allpasstemp = am;
        inputSampleL -= aL[allpasstemp]*0.5;
        inputSampleR -= aR[allpasstemp]*0.5;
        aL[ax] = inputSampleL;
        aR[ax] = inputSampleR;
        inputSampleL *= 0.5;
        inputSampleR *= 0.5;
        ax--; if (ax < 0 || ax > am) {ax = am;}
        inputSampleL += (aL[ax]);
        inputSampleR += (aR[ax]);
        //a single Midiverb-style allpass

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        allpasstemp = bx - 1; if (allpasstemp < 0 || allpasstemp > bm) allpasstemp = bm;
        inputSampleL -= bL[allpasstemp]*0.5;
        inputSampleR -= bR[allpasstemp]*0.5;
        bL[bx] = inputSampleL;
        bR[bx] = inputSampleR;
        inputSampleL *= 0.5;
        inputSampleR *= 0.5;
        bx--; if (bx < 0 || bx > bm) {bx = bm;}
        inputSampleL += (bL[bx]);
        inputSampleR += (bR[bx]);
        //a single Midiverb-style allpass

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        allpasstemp = cx - 1; if (allpasstemp < 0 || allpasstemp > cm) allpasstemp = cm;
        inputSampleL -= cL[allpasstemp]*0.5;
        inputSampleR -= cR[allpasstemp]*0.5;
        cL[cx] = inputSampleL;
        cR[cx] = inputSampleR;
        inputSampleL *= 0.5;
        inputSampleR *= 0.5;
        cx--; if (cx < 0 || cx > cm) {cx = cm;}
        inputSampleL += (cL[cx]);
        inputSampleR += (cR[cx]);
        //a single Midiverb-style allpass

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        allpasstemp = dx - 1; if (allpasstemp < 0 || allpasstemp > dm) allpasstemp = dm;
        inputSampleL -= dL[allpasstemp]*0.5;
        inputSampleR -= dR[allpasstemp]*0.5;
        dL[dx] = inputSampleL;
        dR[dx] = inputSampleR;
        inputSampleL *= 0.5;
        inputSampleR *= 0.5;
        dx--; if (dx < 0 || dx > dm) {dx = dm;}
        inputSampleL += (dL[dx]);
        inputSampleR += (dR[dx]);
        //a single Midiverb-style allpass

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        inputSampleL *= 0.63679; //scale it to 0dB output at full blast
        inputSampleR *= 0.63679; //scale it to 0dB output at full blast

        //begin 32 bit stereo floating point dither
        int expon; frexpf((float)inputSampleL, &expon);
        fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
        inputSampleL += ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
        frexpf((float)inputSampleR, &expon);
        fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
        inputSampleR += ((double(fpd)-uint32_t(0x7fffffff)) * 5.5e-36l * pow(2,expon+62));
        //end 32 bit stereo floating point dither

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}

void PeaksOnly::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    int am = (int)149.0 * overallscale;
    int bm = (int)179.0 * overallscale;
    int cm = (int)191.0 * overallscale;
    int dm = (int)223.0 * overallscale; //these are 'good' primes, spacing out the allpasses
    int allpasstemp = 0;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        allpasstemp = ax - 1; if (allpasstemp < 0 || allpasstemp > am) allpasstemp = am;
        inputSampleL -= aL[allpasstemp]*0.5;
        inputSampleR -= aR[allpasstemp]*0.5;
        aL[ax] = inputSampleL;
        aR[ax] = inputSampleR;
        inputSampleL *= 0.5;
        inputSampleR *= 0.5;
        ax--; if (ax < 0 || ax > am) {ax = am;}
        inputSampleL += (aL[ax]);
        inputSampleR += (aR[ax]);
        //a single Midiverb-style allpass

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        allpasstemp = bx - 1; if (allpasstemp < 0 || allpasstemp > bm) allpasstemp = bm;
        inputSampleL -= bL[allpasstemp]*0.5;
        inputSampleR -= bR[allpasstemp]*0.5;
        bL[bx] = inputSampleL;
        bR[bx] = inputSampleR;
        inputSampleL *= 0.5;
        inputSampleR *= 0.5;
        bx--; if (bx < 0 || bx > bm) {bx = bm;}
        inputSampleL += (bL[bx]);
        inputSampleR += (bR[bx]);
        //a single Midiverb-style allpass

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        allpasstemp = cx - 1; if (allpasstemp < 0 || allpasstemp > cm) allpasstemp = cm;
        inputSampleL -= cL[allpasstemp]*0.5;
        inputSampleR -= cR[allpasstemp]*0.5;
        cL[cx] = inputSampleL;
        cR[cx] = inputSampleR;
        inputSampleL *= 0.5;
        inputSampleR *= 0.5;
        cx--; if (cx < 0 || cx > cm) {cx = cm;}
        inputSampleL += (cL[cx]);
        inputSampleR += (cR[cx]);
        //a single Midiverb-style allpass

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        allpasstemp = dx - 1; if (allpasstemp < 0 || allpasstemp > dm) allpasstemp = dm;
        inputSampleL -= dL[allpasstemp]*0.5;
        inputSampleR -= dR[allpasstemp]*0.5;
        dL[dx] = inputSampleL;
        dR[dx] = inputSampleR;
        inputSampleL *= 0.5;
        inputSampleR *= 0.5;
        dx--; if (dx < 0 || dx > dm) {dx = dm;}
        inputSampleL += (dL[dx]);
        inputSampleR += (dR[dx]);
        //a single Midiverb-style allpass

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        inputSampleL *= 0.63679; //scale it to 0dB output at full blast
        inputSampleR *= 0.63679; //scale it to 0dB output at full blast

        //begin 64 bit stereo floating point dither
        int expon; frexp((double)inputSampleL, &expon);
        fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
        inputSampleL += ((double(fpd)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
        frexp((double)inputSampleR, &expon);
        fpd ^= fpd << 13; fpd ^= fpd >> 17; fpd ^= fpd << 5;
        inputSampleR += ((double(fpd)-uint32_t(0x7fffffff)) * 1.1e-44l * pow(2,expon+62));
        //end 64 bit stereo floating point dither

        *out1 = inputSampleL;
        *out2 = inputSampleR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}
