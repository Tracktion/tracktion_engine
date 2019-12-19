/* ========================================
 *  PodcastDeluxe - PodcastDeluxe.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __PodcastDeluxe_H
#include "PodcastDeluxe.h"
#endif

void PodcastDeluxe::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    int allpasstemp;
    double outallpass = 0.618033988749894848204586;

    double compress = 1.0+pow(A*0.8,2);

    double speed1 = 128.0 / pow(compress,2);
    speed1 *= overallscale;
    double speed2 = speed1 * 1.4;
    double speed3 = speed2 * 1.5;
    double speed4 = speed3 * 1.6;
    double speed5 = speed4 * 1.7;

    maxdelay1 = (int)(23.0*overallscale);
    maxdelay2 = (int)(19.0*overallscale);
    maxdelay3 = (int)(17.0*overallscale);
    maxdelay4 = (int)(13.0*overallscale);
    maxdelay5 = (int)(11.0*overallscale);
    //set up the prime delays

    double refclip = 0.999;
    double softness = 0.435;
    double invsoft = 0.56;
    double outsoft = 0.545;
    double trigger;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;


        allpasstemp = tap1 - 1;
        if (allpasstemp < 0 || allpasstemp > maxdelay1) {allpasstemp = maxdelay1;}
        //set up the delay position
        //using 'tap' and 'allpasstemp' to position the tap
        inputSampleL -= d1L[allpasstemp]*outallpass;
        d1L[tap1] = inputSampleL;
        inputSampleL *= outallpass;
        inputSampleL += (d1L[allpasstemp]);
        //allpass stage
        inputSampleR -= d1R[allpasstemp]*outallpass;
        d1R[tap1] = inputSampleR;
        inputSampleR *= outallpass;
        inputSampleR += (d1R[allpasstemp]);
        //allpass stage
        tap1--; if (tap1 < 0 || tap1 > maxdelay1) {tap1 = maxdelay1;}
        //decrement the position for reals

        inputSampleL *= c1L;
        trigger = fabs(inputSampleL)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c1L += trigger/speed5;
        if (c1L > compress) c1L = compress;
        //compress stage
        inputSampleR *= c1R;
        trigger = fabs(inputSampleR)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c1R += trigger/speed5;
        if (c1R > compress) c1R = compress;
        //compress stage

        allpasstemp = tap2 - 1;
        if (allpasstemp < 0 || allpasstemp > maxdelay2) {allpasstemp = maxdelay2;}
        //set up the delay position
        //using 'tap' and 'allpasstemp' to position the tap
        inputSampleL -= d2L[allpasstemp]*outallpass;
        d2L[tap2] = inputSampleL;
        inputSampleL *= outallpass;
        inputSampleL += (d2L[allpasstemp]);
        //allpass stage
        inputSampleR -= d2R[allpasstemp]*outallpass;
        d2R[tap2] = inputSampleR;
        inputSampleR *= outallpass;
        inputSampleR += (d2R[allpasstemp]);
        //allpass stage
        tap2--; if (tap2 < 0 || tap2 > maxdelay2) {tap2 = maxdelay2;}
        //decrement the position for reals

        inputSampleL *= c2L;
        trigger = fabs(inputSampleL)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c2L += trigger/speed4;
        if (c2L > compress) c2L = compress;
        //compress stage
        inputSampleR *= c2R;
        trigger = fabs(inputSampleR)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c2R += trigger/speed4;
        if (c2R > compress) c2R = compress;
        //compress stage

        allpasstemp = tap3 - 1;
        if (allpasstemp < 0 || allpasstemp > maxdelay3) {allpasstemp = maxdelay3;}
        //set up the delay position
        //using 'tap' and 'allpasstemp' to position the tap
        inputSampleL -= d3L[allpasstemp]*outallpass;
        d3L[tap3] = inputSampleL;
        inputSampleL *= outallpass;
        inputSampleL += (d3L[allpasstemp]);
        //allpass stage
        inputSampleR -= d3R[allpasstemp]*outallpass;
        d3R[tap3] = inputSampleR;
        inputSampleR *= outallpass;
        inputSampleR += (d3R[allpasstemp]);
        //allpass stage
        tap3--; if (tap3 < 0 || tap3 > maxdelay3) {tap3 = maxdelay3;}
        //decrement the position for reals

        inputSampleL *= c3L;
        trigger = fabs(inputSampleL)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c3L += trigger/speed3;
        if (c3L > compress) c3L = compress;
        //compress stage
        inputSampleR *= c3R;
        trigger = fabs(inputSampleR)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c3R += trigger/speed3;
        if (c3R > compress) c3R = compress;
        //compress stage

        allpasstemp = tap4 - 1;
        if (allpasstemp < 0 || allpasstemp > maxdelay4) {allpasstemp = maxdelay4;}
        //set up the delay position
        //using 'tap' and 'allpasstemp' to position the tap
        inputSampleL -= d4L[allpasstemp]*outallpass;
        d4L[tap4] = inputSampleL;
        inputSampleL *= outallpass;
        inputSampleL += (d4L[allpasstemp]);
        //allpass stage
        inputSampleR -= d4R[allpasstemp]*outallpass;
        d4R[tap4] = inputSampleR;
        inputSampleR *= outallpass;
        inputSampleR += (d4R[allpasstemp]);
        //allpass stage
        tap4--; if (tap4 < 0 || tap4 > maxdelay4) {tap4 = maxdelay4;}
        //decrement the position for reals

        inputSampleL *= c4L;
        trigger = fabs(inputSampleL)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c4L += trigger/speed2;
        if (c4L > compress) c4L = compress;
        //compress stage
        inputSampleR *= c4R;
        trigger = fabs(inputSampleR)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c4R += trigger/speed2;
        if (c4R > compress) c4R = compress;
        //compress stage

        allpasstemp = tap5 - 1;
        if (allpasstemp < 0 || allpasstemp > maxdelay5) {allpasstemp = maxdelay5;}
        //set up the delay position
        //using 'tap' and 'allpasstemp' to position the tap
        inputSampleL -= d5L[allpasstemp]*outallpass;
        d5L[tap5] = inputSampleL;
        inputSampleL *= outallpass;
        inputSampleL += (d5L[allpasstemp]);
        //allpass stage
        inputSampleR -= d5R[allpasstemp]*outallpass;
        d5R[tap5] = inputSampleR;
        inputSampleR *= outallpass;
        inputSampleR += (d5R[allpasstemp]);
        //allpass stage
        tap5--; if (tap5 < 0 || tap5 > maxdelay5) {tap5 = maxdelay5;}
        //decrement the position for reals

        inputSampleL *= c5L;
        trigger = fabs(inputSampleL)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c5L += trigger/speed1;
        if (c5L > compress) c5L = compress;
        //compress stage
        inputSampleR *= c5R;
        trigger = fabs(inputSampleR)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c5R += trigger/speed1;
        if (c5R > compress) c5R = compress;
        //compress stage

        if (compress > 1.0) {
            inputSampleL /= compress;
            inputSampleR /= compress;
        }

        //Begin L ADClip
        if (lastSampleL >= refclip)
        {
            if (inputSampleL < refclip)
            {
                lastSampleL = (outsoft + (inputSampleL * softness));
            }
            else lastSampleL = refclip;
        }

        if (lastSampleL <= -refclip)
        {
            if (inputSampleL > -refclip)
            {
                lastSampleL = (-outsoft + (inputSampleL * softness));
            }
            else lastSampleL = -refclip;
        }

        if (inputSampleL > refclip)
        {
            if (lastSampleL < refclip)
            {
                inputSampleL = (invsoft + (lastSampleL * softness));
            }
            else inputSampleL = refclip;
        }

        if (inputSampleL < -refclip)
        {
            if (lastSampleL > -refclip)
            {
                inputSampleL = (-invsoft + (lastSampleL * softness));
            }
            else inputSampleL = -refclip;
        }
        //Completed L ADClip

        //Begin R ADClip
        if (lastSampleR >= refclip)
        {
            if (inputSampleR < refclip)
            {
                lastSampleR = (outsoft + (inputSampleR * softness));
            }
            else lastSampleR = refclip;
        }

        if (lastSampleR <= -refclip)
        {
            if (inputSampleR > -refclip)
            {
                lastSampleR = (-outsoft + (inputSampleR * softness));
            }
            else lastSampleR = -refclip;
        }

        if (inputSampleR > refclip)
        {
            if (lastSampleR < refclip)
            {
                inputSampleR = (invsoft + (lastSampleR * softness));
            }
            else inputSampleR = refclip;
        }

        if (inputSampleR < -refclip)
        {
            if (lastSampleR > -refclip)
            {
                inputSampleR = (-invsoft + (lastSampleR * softness));
            }
            else inputSampleR = -refclip;
        }
        //Completed R ADClip



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

void PodcastDeluxe::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    int allpasstemp;
    double outallpass = 0.618033988749894848204586;

    double compress = 1.0+pow(A*0.8,2);

    double speed1 = 128.0 / pow(compress,2);
    speed1 *= overallscale;
    double speed2 = speed1 * 1.4;
    double speed3 = speed2 * 1.5;
    double speed4 = speed3 * 1.6;
    double speed5 = speed4 * 1.7;

    maxdelay1 = (int)(23.0*overallscale);
    maxdelay2 = (int)(19.0*overallscale);
    maxdelay3 = (int)(17.0*overallscale);
    maxdelay4 = (int)(13.0*overallscale);
    maxdelay5 = (int)(11.0*overallscale);
    //set up the prime delays

    double refclip = 0.999;
    double softness = 0.435;
    double invsoft = 0.56;
    double outsoft = 0.545;
    double trigger;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;


        allpasstemp = tap1 - 1;
        if (allpasstemp < 0 || allpasstemp > maxdelay1) {allpasstemp = maxdelay1;}
        //set up the delay position
        //using 'tap' and 'allpasstemp' to position the tap
        inputSampleL -= d1L[allpasstemp]*outallpass;
        d1L[tap1] = inputSampleL;
        inputSampleL *= outallpass;
        inputSampleL += (d1L[allpasstemp]);
        //allpass stage
        inputSampleR -= d1R[allpasstemp]*outallpass;
        d1R[tap1] = inputSampleR;
        inputSampleR *= outallpass;
        inputSampleR += (d1R[allpasstemp]);
        //allpass stage
        tap1--; if (tap1 < 0 || tap1 > maxdelay1) {tap1 = maxdelay1;}
        //decrement the position for reals

        inputSampleL *= c1L;
        trigger = fabs(inputSampleL)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c1L += trigger/speed5;
        if (c1L > compress) c1L = compress;
        //compress stage
        inputSampleR *= c1R;
        trigger = fabs(inputSampleR)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c1R += trigger/speed5;
        if (c1R > compress) c1R = compress;
        //compress stage

        allpasstemp = tap2 - 1;
        if (allpasstemp < 0 || allpasstemp > maxdelay2) {allpasstemp = maxdelay2;}
        //set up the delay position
        //using 'tap' and 'allpasstemp' to position the tap
        inputSampleL -= d2L[allpasstemp]*outallpass;
        d2L[tap2] = inputSampleL;
        inputSampleL *= outallpass;
        inputSampleL += (d2L[allpasstemp]);
        //allpass stage
        inputSampleR -= d2R[allpasstemp]*outallpass;
        d2R[tap2] = inputSampleR;
        inputSampleR *= outallpass;
        inputSampleR += (d2R[allpasstemp]);
        //allpass stage
        tap2--; if (tap2 < 0 || tap2 > maxdelay2) {tap2 = maxdelay2;}
        //decrement the position for reals

        inputSampleL *= c2L;
        trigger = fabs(inputSampleL)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c2L += trigger/speed4;
        if (c2L > compress) c2L = compress;
        //compress stage
        inputSampleR *= c2R;
        trigger = fabs(inputSampleR)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c2R += trigger/speed4;
        if (c2R > compress) c2R = compress;
        //compress stage

        allpasstemp = tap3 - 1;
        if (allpasstemp < 0 || allpasstemp > maxdelay3) {allpasstemp = maxdelay3;}
        //set up the delay position
        //using 'tap' and 'allpasstemp' to position the tap
        inputSampleL -= d3L[allpasstemp]*outallpass;
        d3L[tap3] = inputSampleL;
        inputSampleL *= outallpass;
        inputSampleL += (d3L[allpasstemp]);
        //allpass stage
        inputSampleR -= d3R[allpasstemp]*outallpass;
        d3R[tap3] = inputSampleR;
        inputSampleR *= outallpass;
        inputSampleR += (d3R[allpasstemp]);
        //allpass stage
        tap3--; if (tap3 < 0 || tap3 > maxdelay3) {tap3 = maxdelay3;}
        //decrement the position for reals

        inputSampleL *= c3L;
        trigger = fabs(inputSampleL)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c3L += trigger/speed3;
        if (c3L > compress) c3L = compress;
        //compress stage
        inputSampleR *= c3R;
        trigger = fabs(inputSampleR)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c3R += trigger/speed3;
        if (c3R > compress) c3R = compress;
        //compress stage

        allpasstemp = tap4 - 1;
        if (allpasstemp < 0 || allpasstemp > maxdelay4) {allpasstemp = maxdelay4;}
        //set up the delay position
        //using 'tap' and 'allpasstemp' to position the tap
        inputSampleL -= d4L[allpasstemp]*outallpass;
        d4L[tap4] = inputSampleL;
        inputSampleL *= outallpass;
        inputSampleL += (d4L[allpasstemp]);
        //allpass stage
        inputSampleR -= d4R[allpasstemp]*outallpass;
        d4R[tap4] = inputSampleR;
        inputSampleR *= outallpass;
        inputSampleR += (d4R[allpasstemp]);
        //allpass stage
        tap4--; if (tap4 < 0 || tap4 > maxdelay4) {tap4 = maxdelay4;}
        //decrement the position for reals

        inputSampleL *= c4L;
        trigger = fabs(inputSampleL)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c4L += trigger/speed2;
        if (c4L > compress) c4L = compress;
        //compress stage
        inputSampleR *= c4R;
        trigger = fabs(inputSampleR)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c4R += trigger/speed2;
        if (c4R > compress) c4R = compress;
        //compress stage

        allpasstemp = tap5 - 1;
        if (allpasstemp < 0 || allpasstemp > maxdelay5) {allpasstemp = maxdelay5;}
        //set up the delay position
        //using 'tap' and 'allpasstemp' to position the tap
        inputSampleL -= d5L[allpasstemp]*outallpass;
        d5L[tap5] = inputSampleL;
        inputSampleL *= outallpass;
        inputSampleL += (d5L[allpasstemp]);
        //allpass stage
        inputSampleR -= d5R[allpasstemp]*outallpass;
        d5R[tap5] = inputSampleR;
        inputSampleR *= outallpass;
        inputSampleR += (d5R[allpasstemp]);
        //allpass stage
        tap5--; if (tap5 < 0 || tap5 > maxdelay5) {tap5 = maxdelay5;}
        //decrement the position for reals

        inputSampleL *= c5L;
        trigger = fabs(inputSampleL)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c5L += trigger/speed1;
        if (c5L > compress) c5L = compress;
        //compress stage
        inputSampleR *= c5R;
        trigger = fabs(inputSampleR)*4.7;
        if (trigger > 4.7) trigger = 4.7;
        trigger = sin(trigger);
        if (trigger < 0) trigger *= 8.0;
        if (trigger < -4.2) trigger = -4.2;
        c5R += trigger/speed1;
        if (c5R > compress) c5R = compress;
        //compress stage

        if (compress > 1.0) {
            inputSampleL /= compress;
            inputSampleR /= compress;
        }

        //Begin L ADClip
        if (lastSampleL >= refclip)
        {
            if (inputSampleL < refclip)
            {
                lastSampleL = (outsoft + (inputSampleL * softness));
            }
            else lastSampleL = refclip;
        }

        if (lastSampleL <= -refclip)
        {
            if (inputSampleL > -refclip)
            {
                lastSampleL = (-outsoft + (inputSampleL * softness));
            }
            else lastSampleL = -refclip;
        }

        if (inputSampleL > refclip)
        {
            if (lastSampleL < refclip)
            {
                inputSampleL = (invsoft + (lastSampleL * softness));
            }
            else inputSampleL = refclip;
        }

        if (inputSampleL < -refclip)
        {
            if (lastSampleL > -refclip)
            {
                inputSampleL = (-invsoft + (lastSampleL * softness));
            }
            else inputSampleL = -refclip;
        }
        //Completed L ADClip

        //Begin R ADClip
        if (lastSampleR >= refclip)
        {
            if (inputSampleR < refclip)
            {
                lastSampleR = (outsoft + (inputSampleR * softness));
            }
            else lastSampleR = refclip;
        }

        if (lastSampleR <= -refclip)
        {
            if (inputSampleR > -refclip)
            {
                lastSampleR = (-outsoft + (inputSampleR * softness));
            }
            else lastSampleR = -refclip;
        }

        if (inputSampleR > refclip)
        {
            if (lastSampleR < refclip)
            {
                inputSampleR = (invsoft + (lastSampleR * softness));
            }
            else inputSampleR = refclip;
        }

        if (inputSampleR < -refclip)
        {
            if (lastSampleR > -refclip)
            {
                inputSampleR = (-invsoft + (lastSampleR * softness));
            }
            else inputSampleR = -refclip;
        }
        //Completed R ADClip


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
