/* ========================================
 *  Facet - Facet.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Facet_H
#include "Facet.h"
#endif

void Facet::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double pos = A;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;





        if (inputSampleL > pos) {
            inputSampleL = ((inputSampleL-pos)*pos)+pos;
        } //facet algorithm: put a corner distortion in producing an artifact
        if (inputSampleL < -pos) {
            inputSampleL = ((inputSampleL+pos)*pos)-pos;
        } //we increasingly produce a sharp 'angle' in the transfer function

        if (inputSampleR > pos) {
            inputSampleR = ((inputSampleR-pos)*pos)+pos;
        } //facet algorithm: put a corner distortion in producing an artifact
        if (inputSampleR < -pos) {
            inputSampleR = ((inputSampleR+pos)*pos)-pos;
        } //we increasingly produce a sharp 'angle' in the transfer function





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

void Facet::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double pos = A;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;





        if (inputSampleL > pos) {
            inputSampleL = ((inputSampleL-pos)*pos)+pos;
        } //facet algorithm: put a corner distortion in producing an artifact
        if (inputSampleL < -pos) {
            inputSampleL = ((inputSampleL+pos)*pos)-pos;
        } //we increasingly produce a sharp 'angle' in the transfer function

        if (inputSampleR > pos) {
            inputSampleR = ((inputSampleR-pos)*pos)+pos;
        } //facet algorithm: put a corner distortion in producing an artifact
        if (inputSampleR < -pos) {
            inputSampleR = ((inputSampleR+pos)*pos)-pos;
        } //we increasingly produce a sharp 'angle' in the transfer function





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
