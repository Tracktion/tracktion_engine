/* ========================================
 *  LeftoMono - LeftoMono.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __LeftoMono_H
#include "LeftoMono.h"
#endif

void LeftoMono::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    while (--sampleFrames >= 0)
    {

        *out1 = *in1;
        *out2 = *in1;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}

void LeftoMono::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    while (--sampleFrames >= 0)
    {

        *out1 = *in1;
        *out2 = *in1;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}
