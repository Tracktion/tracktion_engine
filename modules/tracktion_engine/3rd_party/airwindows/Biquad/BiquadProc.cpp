/* ========================================
 *  Biquad - Biquad.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __Biquad_H
#include "Biquad.h"
#endif

void Biquad::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    int type = ceil((A*3.999)+0.00001);

    biquad[0] = ((B*B*B*0.9999)+0.0001)*0.499;
    if (biquad[0] < 0.0001) biquad[0] = 0.0001;

    biquad[1] = (C*C*C*29.99)+0.01;
    if (biquad[1] < 0.0001) biquad[1] = 0.0001;

    double wet = (D*2.0)-1.0;

    //biquad contains these values:
    //[0] is frequency: 0.000001 to 0.499999 is near-zero to near-Nyquist
    //[1] is resonance, 0.7071 is Butterworth. Also can't be zero
    //[2] is a0 but you need distinct ones for additional biquad instances so it's here
    //[3] is a1 but you need distinct ones for additional biquad instances so it's here
    //[4] is a2 but you need distinct ones for additional biquad instances so it's here
    //[5] is b1 but you need distinct ones for additional biquad instances so it's here
    //[6] is b2 but you need distinct ones for additional biquad instances so it's here
    //[7] is LEFT stored delayed sample (freq and res are stored so you can move them sample by sample)
    //[8] is LEFT stored delayed sample (you have to include the coefficient making code if you do that)
    //[9] is RIGHT stored delayed sample (freq and res are stored so you can move them sample by sample)
    //[10] is RIGHT stored delayed sample (you have to include the coefficient making code if you do that)

    //to build a dedicated filter, rename 'biquad' to whatever the new filter is, then
    //put this code either within the sample buffer (for smoothly modulating freq or res)
    //or in this 'read the controls' area (for letting you change freq and res with controls)
    //or in 'reset' if the freq and res are absolutely fixed (use GetSampleRate to define freq)

    if (type == 1) { //lowpass
        double K = tan(M_PI * biquad[0]);
        double norm = 1.0 / (1.0 + K / biquad[1] + K * K);
        biquad[2] = K * K * norm;
        biquad[3] = 2.0 * biquad[2];
        biquad[4] = biquad[2];
        biquad[5] = 2.0 * (K * K - 1.0) * norm;
        biquad[6] = (1.0 - K / biquad[1] + K * K) * norm;
    }

    if (type == 2) { //highpass
        double K = tan(M_PI * biquad[0]);
        double norm = 1.0 / (1.0 + K / biquad[1] + K * K);
        biquad[2] = norm;
        biquad[3] = -2.0 * biquad[2];
        biquad[4] = biquad[2];
        biquad[5] = 2.0 * (K * K - 1.0) * norm;
        biquad[6] = (1.0 - K / biquad[1] + K * K) * norm;
    }

    if (type == 3) { //bandpass
        double K = tan(M_PI * biquad[0]);
        double norm = 1.0 / (1.0 + K / biquad[1] + K * K);
        biquad[2] = K / biquad[1] * norm;
        biquad[3] = 0.0; //bandpass can simplify the biquad kernel: leave out this multiply
        biquad[4] = -biquad[2];
        biquad[5] = 2.0 * (K * K - 1.0) * norm;
        biquad[6] = (1.0 - K / biquad[1] + K * K) * norm;
    }

    if (type == 4) { //notch
        double K = tan(M_PI * biquad[0]);
        double norm = 1.0 / (1.0 + K / biquad[1] + K * K);
        biquad[2] = (1.0 + K * K) * norm;
        biquad[3] = 2.0 * (K * K - 1) * norm;
        biquad[4] = biquad[2];
        biquad[5] = biquad[3];
        biquad[6] = (1.0 - K / biquad[1] + K * K) * norm;
    }

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;


        inputSampleL = sin(inputSampleL);
        inputSampleR = sin(inputSampleR);
        //encode Console5: good cleanness

        /*
         long double mid = inputSampleL + inputSampleR;
         long double side = inputSampleL - inputSampleR;
         //assign mid and side.Between these sections, you can do mid/side processing

         long double tempSampleM = (mid * biquad[2]) + biquad[7];
         biquad[7] = (mid * biquad[3]) - (tempSampleM * biquad[5]) + biquad[8];
         biquad[8] = (mid * biquad[4]) - (tempSampleM * biquad[6]);
         mid = tempSampleM; //like mono AU, 7 and 8 store mid channel

         long double tempSampleS = (side * biquad[2]) + biquad[9];
         biquad[9] = (side * biquad[3]) - (tempSampleS * biquad[5]) + biquad[10];
         biquad[10] = (side * biquad[4]) - (tempSampleS * biquad[6]);
         inputSampleR = tempSampleS; //note: 9 and 10 store the side channel

         inputSampleL = (mid+side)/2.0;
         inputSampleR = (mid-side)/2.0;
         //unassign mid and side
         */

        long double tempSampleL = (inputSampleL * biquad[2]) + biquad[7];
        biquad[7] = (inputSampleL * biquad[3]) - (tempSampleL * biquad[5]) + biquad[8];
        biquad[8] = (inputSampleL * biquad[4]) - (tempSampleL * biquad[6]);
        inputSampleL = tempSampleL; //like mono AU, 7 and 8 store L channel

        long double tempSampleR = (inputSampleR * biquad[2]) + biquad[9];
        biquad[9] = (inputSampleR * biquad[3]) - (tempSampleR * biquad[5]) + biquad[10];
        biquad[10] = (inputSampleR * biquad[4]) - (tempSampleR * biquad[6]);
        inputSampleR = tempSampleR; //note: 9 and 10 store the R channel

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        if (wet < 1.0) {
            inputSampleL = (inputSampleL*wet) + (drySampleL*(1.0-fabs(wet)));
            inputSampleR = (inputSampleR*wet) + (drySampleR*(1.0-fabs(wet)));
            //inv/dry/wet lets us turn LP into HP and band into notch
        }

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

void Biquad::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    int type = ceil((A*3.999)+0.00001);

    biquad[0] = ((B*B*B*0.9999)+0.0001)*0.499;
    if (biquad[0] < 0.0001) biquad[0] = 0.0001;

    biquad[1] = (C*C*C*29.99)+0.01;
    if (biquad[1] < 0.0001) biquad[1] = 0.0001;

    double wet = (D*2.0)-1.0;

    //biquad contains these values:
    //[0] is frequency: 0.000001 to 0.499999 is near-zero to near-Nyquist
    //[1] is resonance, 0.7071 is Butterworth. Also can't be zero
    //[2] is a0 but you need distinct ones for additional biquad instances so it's here
    //[3] is a1 but you need distinct ones for additional biquad instances so it's here
    //[4] is a2 but you need distinct ones for additional biquad instances so it's here
    //[5] is b1 but you need distinct ones for additional biquad instances so it's here
    //[6] is b2 but you need distinct ones for additional biquad instances so it's here
    //[7] is LEFT stored delayed sample (freq and res are stored so you can move them sample by sample)
    //[8] is LEFT stored delayed sample (you have to include the coefficient making code if you do that)
    //[9] is RIGHT stored delayed sample (freq and res are stored so you can move them sample by sample)
    //[10] is RIGHT stored delayed sample (you have to include the coefficient making code if you do that)

    //to build a dedicated filter, rename 'biquad' to whatever the new filter is, then
    //put this code either within the sample buffer (for smoothly modulating freq or res)
    //or in this 'read the controls' area (for letting you change freq and res with controls)
    //or in 'reset' if the freq and res are absolutely fixed (use GetSampleRate to define freq)

    if (type == 1) { //lowpass
        double K = tan(M_PI * biquad[0]);
        double norm = 1.0 / (1.0 + K / biquad[1] + K * K);
        biquad[2] = K * K * norm;
        biquad[3] = 2.0 * biquad[2];
        biquad[4] = biquad[2];
        biquad[5] = 2.0 * (K * K - 1.0) * norm;
        biquad[6] = (1.0 - K / biquad[1] + K * K) * norm;
    }

    if (type == 2) { //highpass
        double K = tan(M_PI * biquad[0]);
        double norm = 1.0 / (1.0 + K / biquad[1] + K * K);
        biquad[2] = norm;
        biquad[3] = -2.0 * biquad[2];
        biquad[4] = biquad[2];
        biquad[5] = 2.0 * (K * K - 1.0) * norm;
        biquad[6] = (1.0 - K / biquad[1] + K * K) * norm;
    }

    if (type == 3) { //bandpass
        double K = tan(M_PI * biquad[0]);
        double norm = 1.0 / (1.0 + K / biquad[1] + K * K);
        biquad[2] = K / biquad[1] * norm;
        biquad[3] = 0.0; //bandpass can simplify the biquad kernel: leave out this multiply
        biquad[4] = -biquad[2];
        biquad[5] = 2.0 * (K * K - 1.0) * norm;
        biquad[6] = (1.0 - K / biquad[1] + K * K) * norm;
    }

    if (type == 4) { //notch
        double K = tan(M_PI * biquad[0]);
        double norm = 1.0 / (1.0 + K / biquad[1] + K * K);
        biquad[2] = (1.0 + K * K) * norm;
        biquad[3] = 2.0 * (K * K - 1) * norm;
        biquad[4] = biquad[2];
        biquad[5] = biquad[3];
        biquad[6] = (1.0 - K / biquad[1] + K * K) * norm;
    }

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;


        inputSampleL = sin(inputSampleL);
        inputSampleR = sin(inputSampleR);
        //encode Console5: good cleanness

        /*
         long double mid = inputSampleL + inputSampleR;
         long double side = inputSampleL - inputSampleR;
         //assign mid and side.Between these sections, you can do mid/side processing

         long double tempSampleM = (mid * biquad[2]) + biquad[7];
         biquad[7] = (mid * biquad[3]) - (tempSampleM * biquad[5]) + biquad[8];
         biquad[8] = (mid * biquad[4]) - (tempSampleM * biquad[6]);
         mid = tempSampleM; //like mono AU, 7 and 8 store mid channel

         long double tempSampleS = (side * biquad[2]) + biquad[9];
         biquad[9] = (side * biquad[3]) - (tempSampleS * biquad[5]) + biquad[10];
         biquad[10] = (side * biquad[4]) - (tempSampleS * biquad[6]);
         inputSampleR = tempSampleS; //note: 9 and 10 store the side channel

         inputSampleL = (mid+side)/2.0;
         inputSampleR = (mid-side)/2.0;
         //unassign mid and side
         */

        long double tempSampleL = (inputSampleL * biquad[2]) + biquad[7];
        biquad[7] = (inputSampleL * biquad[3]) - (tempSampleL * biquad[5]) + biquad[8];
        biquad[8] = (inputSampleL * biquad[4]) - (tempSampleL * biquad[6]);
        inputSampleL = tempSampleL; //like mono AU, 7 and 8 store L channel

        long double tempSampleR = (inputSampleR * biquad[2]) + biquad[9];
        biquad[9] = (inputSampleR * biquad[3]) - (tempSampleR * biquad[5]) + biquad[10];
        biquad[10] = (inputSampleR * biquad[4]) - (tempSampleR * biquad[6]);
        inputSampleR = tempSampleR; //note: 9 and 10 store the R channel

        if (inputSampleL > 1.0) inputSampleL = 1.0;
        if (inputSampleL < -1.0) inputSampleL = -1.0;
        if (inputSampleR > 1.0) inputSampleR = 1.0;
        if (inputSampleR < -1.0) inputSampleR = -1.0;
        //without this, you can get a NaN condition where it spits out DC offset at full blast!
        inputSampleL = asin(inputSampleL);
        inputSampleR = asin(inputSampleR);
        //amplitude aspect

        if (wet < 1.0) {
            inputSampleL = (inputSampleL*wet) + (drySampleL*(1.0-fabs(wet)));
            inputSampleR = (inputSampleR*wet) + (drySampleR*(1.0-fabs(wet)));
            //inv/dry/wet lets us turn LP into HP and band into notch
        }

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
