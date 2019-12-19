/* ========================================
 *  RawTimbers - RawTimbers.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __RawTimbers_H
#include "RawTimbers.h"
#endif

void RawTimbers::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    while (--sampleFrames >= 0)
    {
        double inputSampleL = *in1 * 8388608.0;
        double inputSampleR = *in2 * 8388608.0;
        double outputSampleL;
        double outputSampleR;

        inputSampleL += 0.381966011250105;
        inputSampleR += 0.381966011250105;

        if ((lastSampleL+lastSampleL) >= (inputSampleL+lastSample2L)) outputSampleL = floor(lastSampleL);
        else outputSampleL = floor(lastSampleL+1.0); //round down or up based on whether it softens treble angles

        if ((lastSampleR+lastSampleR) >= (inputSampleR+lastSample2R)) outputSampleR = floor(lastSampleR);
        else outputSampleR = floor(lastSampleR+1.0); //round down or up based on whether it softens treble angles

        lastSample2L = lastSampleL;
        lastSampleL = inputSampleL; //we retain three samples in a row

        lastSample2R = lastSampleR;
        lastSampleR = inputSampleR; //we retain three samples in a row

        *out1 = outputSampleL / 8388608.0;
        *out2 = outputSampleR / 8388608.0;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}

void RawTimbers::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    while (--sampleFrames >= 0)
    {
        double inputSampleL = *in1 * 8388608.0;
        double inputSampleR = *in2 * 8388608.0;
        double outputSampleL;
        double outputSampleR;

        inputSampleL += 0.381966011250105;
        inputSampleR += 0.381966011250105;

        if ((lastSampleL+lastSampleL) >= (inputSampleL+lastSample2L)) outputSampleL = floor(lastSampleL);
        else outputSampleL = floor(lastSampleL+1.0); //round down or up based on whether it softens treble angles

        if ((lastSampleR+lastSampleR) >= (inputSampleR+lastSample2R)) outputSampleR = floor(lastSampleR);
        else outputSampleR = floor(lastSampleR+1.0); //round down or up based on whether it softens treble angles

        lastSample2L = lastSampleL;
        lastSampleL = inputSampleL; //we retain three samples in a row

        lastSample2R = lastSampleR;
        lastSampleR = inputSampleR; //we retain three samples in a row

        *out1 = outputSampleL / 8388608.0;
        *out2 = outputSampleR / 8388608.0;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}
