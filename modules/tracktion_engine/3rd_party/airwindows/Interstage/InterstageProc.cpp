/* ========================================
 *  Interstage - Interstage.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Interstage_H
#include "Interstage.h"
#endif

void Interstage::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double firstStage = 0.381966011250105 / overallscale;
    double iirAmount = 0.00295 / overallscale;
    double threshold = 0.381966011250105;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

        inputSampleL = (inputSampleL+lastSampleL)*0.5;
        inputSampleR = (inputSampleR+lastSampleR)*0.5; //start the lowpassing with an average

        if (flip) {
            iirSampleAL = (iirSampleAL * (1 - firstStage)) + (inputSampleL * firstStage); inputSampleL = iirSampleAL;
            iirSampleCL = (iirSampleCL * (1 - iirAmount)) + (inputSampleL * iirAmount); inputSampleL = iirSampleCL;
            iirSampleEL = (iirSampleEL * (1 - iirAmount)) + (inputSampleL * iirAmount); inputSampleL = iirSampleEL;
            inputSampleL = drySampleL - inputSampleL;
            //make highpass
            if (inputSampleL - iirSampleAL > threshold) inputSampleL = iirSampleAL + threshold;
            if (inputSampleL - iirSampleAL < -threshold) inputSampleL = iirSampleAL - threshold;
            //slew limit against lowpassed reference point

            iirSampleAR = (iirSampleAR * (1 - firstStage)) + (inputSampleR * firstStage); inputSampleR = iirSampleAR;
            iirSampleCR = (iirSampleCR * (1 - iirAmount)) + (inputSampleR * iirAmount); inputSampleR = iirSampleCR;
            iirSampleER = (iirSampleER * (1 - iirAmount)) + (inputSampleR * iirAmount); inputSampleR = iirSampleER;
            inputSampleR = drySampleR - inputSampleR;
            //make highpass
            if (inputSampleR - iirSampleAR > threshold) inputSampleR = iirSampleAR + threshold;
            if (inputSampleR - iirSampleAR < -threshold) inputSampleR = iirSampleAR - threshold;
            //slew limit against lowpassed reference point
        } else {
            iirSampleBL = (iirSampleBL * (1 - firstStage)) + (inputSampleL * firstStage); inputSampleL = iirSampleBL;
            iirSampleDL = (iirSampleDL * (1 - iirAmount)) + (inputSampleL * iirAmount); inputSampleL = iirSampleDL;
            iirSampleFL = (iirSampleFL * (1 - iirAmount)) + (inputSampleL * iirAmount); inputSampleL = iirSampleFL;
            inputSampleL = drySampleL - inputSampleL;
            //make highpass
            if (inputSampleL - iirSampleBL > threshold) inputSampleL = iirSampleBL + threshold;
            if (inputSampleL - iirSampleBL < -threshold) inputSampleL = iirSampleBL - threshold;
            //slew limit against lowpassed reference point

            iirSampleBR = (iirSampleBR * (1 - firstStage)) + (inputSampleR * firstStage); inputSampleR = iirSampleBR;
            iirSampleDR = (iirSampleDR * (1 - iirAmount)) + (inputSampleR * iirAmount); inputSampleR = iirSampleDR;
            iirSampleFR = (iirSampleFR * (1 - iirAmount)) + (inputSampleR * iirAmount); inputSampleR = iirSampleFR;
            inputSampleR = drySampleR - inputSampleR;
            //make highpass
            if (inputSampleR - iirSampleBR > threshold) inputSampleR = iirSampleBR + threshold;
            if (inputSampleR - iirSampleBR < -threshold) inputSampleR = iirSampleBR - threshold;
            //slew limit against lowpassed reference point
        }
        flip = !flip;
        lastSampleL = inputSampleL;
        lastSampleR = inputSampleR;

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

void Interstage::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double firstStage = 0.381966011250105 / overallscale;
    double iirAmount = 0.00295 / overallscale;
    double threshold = 0.381966011250105;

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

        inputSampleL = (inputSampleL+lastSampleL)*0.5;
        inputSampleR = (inputSampleR+lastSampleR)*0.5; //start the lowpassing with an average

        if (flip) {
            iirSampleAL = (iirSampleAL * (1 - firstStage)) + (inputSampleL * firstStage); inputSampleL = iirSampleAL;
            iirSampleCL = (iirSampleCL * (1 - iirAmount)) + (inputSampleL * iirAmount); inputSampleL = iirSampleCL;
            iirSampleEL = (iirSampleEL * (1 - iirAmount)) + (inputSampleL * iirAmount); inputSampleL = iirSampleEL;
            inputSampleL = drySampleL - inputSampleL;
            //make highpass
            if (inputSampleL - iirSampleAL > threshold) inputSampleL = iirSampleAL + threshold;
            if (inputSampleL - iirSampleAL < -threshold) inputSampleL = iirSampleAL - threshold;
            //slew limit against lowpassed reference point

            iirSampleAR = (iirSampleAR * (1 - firstStage)) + (inputSampleR * firstStage); inputSampleR = iirSampleAR;
            iirSampleCR = (iirSampleCR * (1 - iirAmount)) + (inputSampleR * iirAmount); inputSampleR = iirSampleCR;
            iirSampleER = (iirSampleER * (1 - iirAmount)) + (inputSampleR * iirAmount); inputSampleR = iirSampleER;
            inputSampleR = drySampleR - inputSampleR;
            //make highpass
            if (inputSampleR - iirSampleAR > threshold) inputSampleR = iirSampleAR + threshold;
            if (inputSampleR - iirSampleAR < -threshold) inputSampleR = iirSampleAR - threshold;
            //slew limit against lowpassed reference point
        } else {
            iirSampleBL = (iirSampleBL * (1 - firstStage)) + (inputSampleL * firstStage); inputSampleL = iirSampleBL;
            iirSampleDL = (iirSampleDL * (1 - iirAmount)) + (inputSampleL * iirAmount); inputSampleL = iirSampleDL;
            iirSampleFL = (iirSampleFL * (1 - iirAmount)) + (inputSampleL * iirAmount); inputSampleL = iirSampleFL;
            inputSampleL = drySampleL - inputSampleL;
            //make highpass
            if (inputSampleL - iirSampleBL > threshold) inputSampleL = iirSampleBL + threshold;
            if (inputSampleL - iirSampleBL < -threshold) inputSampleL = iirSampleBL - threshold;
            //slew limit against lowpassed reference point

            iirSampleBR = (iirSampleBR * (1 - firstStage)) + (inputSampleR * firstStage); inputSampleR = iirSampleBR;
            iirSampleDR = (iirSampleDR * (1 - iirAmount)) + (inputSampleR * iirAmount); inputSampleR = iirSampleDR;
            iirSampleFR = (iirSampleFR * (1 - iirAmount)) + (inputSampleR * iirAmount); inputSampleR = iirSampleFR;
            inputSampleR = drySampleR - inputSampleR;
            //make highpass
            if (inputSampleR - iirSampleBR > threshold) inputSampleR = iirSampleBR + threshold;
            if (inputSampleR - iirSampleBR < -threshold) inputSampleR = iirSampleBR - threshold;
            //slew limit against lowpassed reference point
        }
        flip = !flip;
        lastSampleL = inputSampleL;
        lastSampleR = inputSampleR;

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
