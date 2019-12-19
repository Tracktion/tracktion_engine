/* ========================================
 *  DitherMeTimbers - DitherMeTimbers.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DitherMeTimbers_H
#include "DitherMeTimbers.h"
#endif

void DitherMeTimbers::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
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

        lastSampleL -= (noiseShapingL*0.125);
        lastSampleR -= (noiseShapingR*0.125);

        if ((lastSampleL+lastSampleL) >= (inputSampleL+lastSample2L)) outputSampleL = floor(lastSampleL);
        else outputSampleL = floor(lastSampleL+1.0); //round down or up based on whether it softens treble angles

        if ((lastSampleR+lastSampleR) >= (inputSampleR+lastSample2R)) outputSampleR = floor(lastSampleR);
        else outputSampleR = floor(lastSampleR+1.0); //round down or up based on whether it softens treble angles

        lastSample2L = lastSampleL;
        lastSampleL = inputSampleL; //we retain three samples in a row

        lastSample2R = lastSampleR;
        lastSampleR = inputSampleR; //we retain three samples in a row

        noiseShapingL += outputSampleL;
        noiseShapingL -= lastSampleL;

        noiseShapingR += outputSampleR;
        noiseShapingR -= lastSampleR;

        if (outputSampleL > 8388600.0) {
            outputSampleL = 8388600.0;
            noiseShapingL *= 0.5;
        }
        if (outputSampleR > 8388600.0) {
            outputSampleR = 8388600.0;
            noiseShapingR *= 0.5;
        }
        if (outputSampleL < -8388600.0) {
            outputSampleL = -8388600.0;
            noiseShapingL *= 0.5;
        }
        if (outputSampleR < -8388600.0) {
            outputSampleR = -8388600.0;
            noiseShapingR *= 0.5;
        }

        *out1 = outputSampleL / 8388608.0;
        *out2 = outputSampleR / 8388608.0;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}

void DitherMeTimbers::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
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

        lastSampleL -= (noiseShapingL*0.125);
        lastSampleR -= (noiseShapingR*0.125);

        if ((lastSampleL+lastSampleL) >= (inputSampleL+lastSample2L)) outputSampleL = floor(lastSampleL);
        else outputSampleL = floor(lastSampleL+1.0); //round down or up based on whether it softens treble angles

        if ((lastSampleR+lastSampleR) >= (inputSampleR+lastSample2R)) outputSampleR = floor(lastSampleR);
        else outputSampleR = floor(lastSampleR+1.0); //round down or up based on whether it softens treble angles

        lastSample2L = lastSampleL;
        lastSampleL = inputSampleL; //we retain three samples in a row

        lastSample2R = lastSampleR;
        lastSampleR = inputSampleR; //we retain three samples in a row

        noiseShapingL += outputSampleL;
        noiseShapingL -= lastSampleL;

        noiseShapingR += outputSampleR;
        noiseShapingR -= lastSampleR;

        if (outputSampleL > 8388600.0) {
            outputSampleL = 8388600.0;
            noiseShapingL *= 0.5;
        }
        if (outputSampleR > 8388600.0) {
            outputSampleR = 8388600.0;
            noiseShapingR *= 0.5;
        }
        if (outputSampleL < -8388600.0) {
            outputSampleL = -8388600.0;
            noiseShapingL *= 0.5;
        }
        if (outputSampleR < -8388600.0) {
            outputSampleR = -8388600.0;
            noiseShapingR *= 0.5;
        }

        *out1 = outputSampleL / 8388608.0;
        *out2 = outputSampleR / 8388608.0;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}
