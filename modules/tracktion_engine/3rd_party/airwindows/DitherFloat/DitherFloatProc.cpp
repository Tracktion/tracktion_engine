/* ========================================
 *  DitherFloat - DitherFloat.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DitherFloat_H
#include "DitherFloat.h"
#endif

void DitherFloat::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    int floatOffset = (A * 32);
    long double blend = B;

    long double gain = 0;

    switch (floatOffset)
    {
        case 0: gain = 1.0; break;
        case 1: gain = 2.0; break;
        case 2: gain = 4.0; break;
        case 3: gain = 8.0; break;
        case 4: gain = 16.0; break;
        case 5: gain = 32.0; break;
        case 6: gain = 64.0; break;
        case 7: gain = 128.0; break;
        case 8: gain = 256.0; break;
        case 9: gain = 512.0; break;
        case 10: gain = 1024.0; break;
        case 11: gain = 2048.0; break;
        case 12: gain = 4096.0; break;
        case 13: gain = 8192.0; break;
        case 14: gain = 16384.0; break;
        case 15: gain = 32768.0; break;
        case 16: gain = 65536.0; break;
        case 17: gain = 131072.0; break;
        case 18: gain = 262144.0; break;
        case 19: gain = 524288.0; break;
        case 20: gain = 1048576.0; break;
        case 21: gain = 2097152.0; break;
        case 22: gain = 4194304.0; break;
        case 23: gain = 8388608.0; break;
        case 24: gain = 16777216.0; break;
        case 25: gain = 33554432.0; break;
        case 26: gain = 67108864.0; break;
        case 27: gain = 134217728.0; break;
        case 28: gain = 268435456.0; break;
        case 29: gain = 536870912.0; break;
        case 30: gain = 1073741824.0; break;
        case 31: gain = 2147483648.0; break;
        case 32: gain = 4294967296.0; break;
    }
    //we are directly punching in the gain values rather than calculating them

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1 + (gain-1);
        long double inputSampleR = *in2 + (gain-1);



        //begin stereo 32 bit floating point dither
        int expon; frexpf((float)inputSampleL, &expon);
        fpd ^= fpd<<13; fpd ^= fpd>>17; fpd ^= fpd<<5;
        inputSampleL += (fpd*3.4e-36l*pow(2,expon+62)* blend);  //remove 'blend' for real use, it's for the demo;
        frexpf((float)inputSampleR, &expon);
        fpd ^= fpd<<13; fpd ^= fpd>>17; fpd ^= fpd<<5;
        inputSampleR += (fpd*3.4e-36l*pow(2,expon+62)* blend);  //remove 'blend' for real use, it's for the demo;
        //end stereo 32 bit floating point dither


        inputSampleL = (float)inputSampleL; //equivalent of 'floor' for 32 bit floating point
        inputSampleR = (float)inputSampleR; //equivalent of 'floor' for 32 bit floating point
        //We do that separately, we're truncating to floating point WHILE heavily offset.

        *out1 = inputSampleL - (gain-1);
        *out2 = inputSampleR - (gain-1);

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}

void DitherFloat::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    int floatOffset = (A * 32);
    long double blend = B;

    long double gain = 0;

    switch (floatOffset)
    {
        case 0: gain = 1.0; break;
        case 1: gain = 2.0; break;
        case 2: gain = 4.0; break;
        case 3: gain = 8.0; break;
        case 4: gain = 16.0; break;
        case 5: gain = 32.0; break;
        case 6: gain = 64.0; break;
        case 7: gain = 128.0; break;
        case 8: gain = 256.0; break;
        case 9: gain = 512.0; break;
        case 10: gain = 1024.0; break;
        case 11: gain = 2048.0; break;
        case 12: gain = 4096.0; break;
        case 13: gain = 8192.0; break;
        case 14: gain = 16384.0; break;
        case 15: gain = 32768.0; break;
        case 16: gain = 65536.0; break;
        case 17: gain = 131072.0; break;
        case 18: gain = 262144.0; break;
        case 19: gain = 524288.0; break;
        case 20: gain = 1048576.0; break;
        case 21: gain = 2097152.0; break;
        case 22: gain = 4194304.0; break;
        case 23: gain = 8388608.0; break;
        case 24: gain = 16777216.0; break;
        case 25: gain = 33554432.0; break;
        case 26: gain = 67108864.0; break;
        case 27: gain = 134217728.0; break;
        case 28: gain = 268435456.0; break;
        case 29: gain = 536870912.0; break;
        case 30: gain = 1073741824.0; break;
        case 31: gain = 2147483648.0; break;
        case 32: gain = 4294967296.0; break;
    }
    //we are directly punching in the gain values rather than calculating them

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1 + (gain-1);
        long double inputSampleR = *in2 + (gain-1);



        //begin stereo 32 bit floating point dither
        int expon; frexpf((float)inputSampleL, &expon);
        fpd ^= fpd<<13; fpd ^= fpd>>17; fpd ^= fpd<<5;
        inputSampleL += (fpd*3.4e-36l*pow(2,expon+62)* blend);  //remove 'blend' for real use, it's for the demo;
        frexpf((float)inputSampleR, &expon);
        fpd ^= fpd<<13; fpd ^= fpd>>17; fpd ^= fpd<<5;
        inputSampleR += (fpd*3.4e-36l*pow(2,expon+62)* blend);  //remove 'blend' for real use, it's for the demo;
        //end stereo 32 bit floating point dither


        inputSampleL = (float)inputSampleL; //equivalent of 'floor' for 32 bit floating point
        inputSampleR = (float)inputSampleR; //equivalent of 'floor' for 32 bit floating point
        //We do that separately, we're truncating to floating point WHILE heavily offset.

        //note for 64 bit version: this is not for actually dithering 64 bit floats!
        //This is specifically for demonstrating the sound of 32 bit floating point dither
        //even over a 64 bit buss. Therefore it should be using float, above!

        *out1 = inputSampleL - (gain-1);
        *out2 = inputSampleR - (gain-1);

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}
