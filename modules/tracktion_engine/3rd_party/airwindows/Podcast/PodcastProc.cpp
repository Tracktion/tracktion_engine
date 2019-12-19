/* ========================================
 *  Podcast - Podcast.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Podcast_H
#include "Podcast.h"
#endif

void Podcast::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double compress = 1.0 + pow(A,2);
    double wet = B;
    double speed1 = 64.0 / pow(compress,2);
    speed1 *= overallscale;
    double speed2 = speed1 * 1.4;
    double speed3 = speed2 * 1.5;
    double speed4 = speed3 * 1.6;
    double speed5 = speed4 * 1.7;
    double trigger;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

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

        if (wet !=1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
            inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
        }

        if (inputSampleL > 0.999) inputSampleL = 0.999;
        if (inputSampleL < -0.999) inputSampleL = -0.999;
        if (inputSampleR > 0.999) inputSampleR = 0.999;
        if (inputSampleR < -0.999) inputSampleR = -0.999;

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

void Podcast::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double compress = 1.0 + pow(A,2);
    double wet = B;
    double speed1 = 64.0 / pow(compress,2);
    speed1 *= overallscale;
    double speed2 = speed1 * 1.4;
    double speed3 = speed2 * 1.5;
    double speed4 = speed3 * 1.6;
    double speed5 = speed4 * 1.7;
    double trigger;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

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

        if (wet !=1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
            inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
        }

        if (inputSampleL > 0.999) inputSampleL = 0.999;
        if (inputSampleL < -0.999) inputSampleL = -0.999;
        if (inputSampleR > 0.999) inputSampleR = 0.999;
        if (inputSampleR < -0.999) inputSampleR = -0.999;

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
