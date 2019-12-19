/* ========================================
 *  ResEQ - ResEQ.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __ResEQ_H
#include "ResEQ.h"
#endif

void ResEQ::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double v1 = A;
    double v2 = B;
    double v3 = C;
    double v4 = D;
    double v5 = E;
    double v6 = F;
    double v7 = G;
    double v8 = H;
    double f1 = pow(v1,2);
    double f2 = pow(v2,2);
    double f3 = pow(v3,2);
    double f4 = pow(v4,2);
    double f5 = pow(v5,2);
    double f6 = pow(v6,2);
    double f7 = pow(v7,2);
    double f8 = pow(v8,2);
    double wet = I;
    double falloff;
    v1 += 0.2; v2 += 0.2; v3 += 0.2; v4 += 0.2; v5 += 0.2; v6 += 0.2; v7 += 0.2; v8 += 0.2;
    v1 /= overallscale;
    v2 /= overallscale;
    v3 /= overallscale;
    v4 /= overallscale;
    v5 /= overallscale;
    v6 /= overallscale;
    v7 /= overallscale;
    v8 /= overallscale;
    //each process frame we'll update some of the kernel frames. That way we don't have to crunch the whole thing at once,
    //and we can load a LOT more resonant peaks into the kernel.

    framenumber += 1; if (framenumber > 59) framenumber = 1;
    falloff = sin(framenumber / 19.098992);
    fL[framenumber] = 0.0;
    if ((framenumber * f1) < 1.57079633) fL[framenumber]  += (sin((framenumber * f1)*2.0) * falloff * v1);
    else fL[framenumber]  += (cos(framenumber * f1) * falloff * v1);
    if ((framenumber * f2) < 1.57079633) fL[framenumber]  += (sin((framenumber * f2)*2.0) * falloff * v2);
    else fL[framenumber]  += (cos(framenumber * f2) * falloff * v2);
    if ((framenumber * f3) < 1.57079633) fL[framenumber]  += (sin((framenumber * f3)*2.0) * falloff * v3);
    else fL[framenumber]  += (cos(framenumber * f3) * falloff * v3);
    if ((framenumber * f4) < 1.57079633) fL[framenumber]  += (sin((framenumber * f4)*2.0) * falloff * v4);
    else fL[framenumber]  += (cos(framenumber * f4) * falloff * v4);
    if ((framenumber * f5) < 1.57079633) fL[framenumber]  += (sin((framenumber * f5)*2.0) * falloff * v5);
    else fL[framenumber]  += (cos(framenumber * f5) * falloff * v5);
    if ((framenumber * f6) < 1.57079633) fL[framenumber]  += (sin((framenumber * f6)*2.0) * falloff * v6);
    else fL[framenumber]  += (cos(framenumber * f6) * falloff * v6);
    if ((framenumber * f7) < 1.57079633) fL[framenumber]  += (sin((framenumber * f7)*2.0) * falloff * v7);
    else fL[framenumber]  += (cos(framenumber * f7) * falloff * v7);
    if ((framenumber * f8) < 1.57079633) fL[framenumber]  += (sin((framenumber * f8)*2.0) * falloff * v8);
    else fL[framenumber]  += (cos(framenumber * f8) * falloff * v8);

    fR[framenumber] = 0.0;
    if ((framenumber * f1) < 1.57079633) fR[framenumber]  += (sin((framenumber * f1)*2.0) * falloff * v1);
    else fR[framenumber]  += (cos(framenumber * f1) * falloff * v1);
    if ((framenumber * f2) < 1.57079633) fR[framenumber]  += (sin((framenumber * f2)*2.0) * falloff * v2);
    else fR[framenumber]  += (cos(framenumber * f2) * falloff * v2);
    if ((framenumber * f3) < 1.57079633) fR[framenumber]  += (sin((framenumber * f3)*2.0) * falloff * v3);
    else fR[framenumber]  += (cos(framenumber * f3) * falloff * v3);
    if ((framenumber * f4) < 1.57079633) fR[framenumber]  += (sin((framenumber * f4)*2.0) * falloff * v4);
    else fR[framenumber]  += (cos(framenumber * f4) * falloff * v4);
    if ((framenumber * f5) < 1.57079633) fR[framenumber]  += (sin((framenumber * f5)*2.0) * falloff * v5);
    else fR[framenumber]  += (cos(framenumber * f5) * falloff * v5);
    if ((framenumber * f6) < 1.57079633) fR[framenumber]  += (sin((framenumber * f6)*2.0) * falloff * v6);
    else fR[framenumber]  += (cos(framenumber * f6) * falloff * v6);
    if ((framenumber * f7) < 1.57079633) fR[framenumber]  += (sin((framenumber * f7)*2.0) * falloff * v7);
    else fR[framenumber]  += (cos(framenumber * f7) * falloff * v7);
    if ((framenumber * f8) < 1.57079633) fR[framenumber]  += (sin((framenumber * f8)*2.0) * falloff * v8);
    else fR[framenumber]  += (cos(framenumber * f8) * falloff * v8);

    framenumber += 1; if (framenumber > 59) framenumber = 1;
    falloff = sin(framenumber / 19.098992);
    fL[framenumber] = 0.0;
    if ((framenumber * f1) < 1.57079633) fL[framenumber]  += (sin((framenumber * f1)*2.0) * falloff * v1);
    else fL[framenumber]  += (cos(framenumber * f1) * falloff * v1);
    if ((framenumber * f2) < 1.57079633) fL[framenumber]  += (sin((framenumber * f2)*2.0) * falloff * v2);
    else fL[framenumber]  += (cos(framenumber * f2) * falloff * v2);
    if ((framenumber * f3) < 1.57079633) fL[framenumber]  += (sin((framenumber * f3)*2.0) * falloff * v3);
    else fL[framenumber]  += (cos(framenumber * f3) * falloff * v3);
    if ((framenumber * f4) < 1.57079633) fL[framenumber]  += (sin((framenumber * f4)*2.0) * falloff * v4);
    else fL[framenumber]  += (cos(framenumber * f4) * falloff * v4);
    if ((framenumber * f5) < 1.57079633) fL[framenumber]  += (sin((framenumber * f5)*2.0) * falloff * v5);
    else fL[framenumber]  += (cos(framenumber * f5) * falloff * v5);
    if ((framenumber * f6) < 1.57079633) fL[framenumber]  += (sin((framenumber * f6)*2.0) * falloff * v6);
    else fL[framenumber]  += (cos(framenumber * f6) * falloff * v6);
    if ((framenumber * f7) < 1.57079633) fL[framenumber]  += (sin((framenumber * f7)*2.0) * falloff * v7);
    else fL[framenumber]  += (cos(framenumber * f7) * falloff * v7);
    if ((framenumber * f8) < 1.57079633) fL[framenumber]  += (sin((framenumber * f8)*2.0) * falloff * v8);
    else fL[framenumber]  += (cos(framenumber * f8) * falloff * v8);

    fR[framenumber] = 0.0;
    if ((framenumber * f1) < 1.57079633) fR[framenumber]  += (sin((framenumber * f1)*2.0) * falloff * v1);
    else fR[framenumber]  += (cos(framenumber * f1) * falloff * v1);
    if ((framenumber * f2) < 1.57079633) fR[framenumber]  += (sin((framenumber * f2)*2.0) * falloff * v2);
    else fR[framenumber]  += (cos(framenumber * f2) * falloff * v2);
    if ((framenumber * f3) < 1.57079633) fR[framenumber]  += (sin((framenumber * f3)*2.0) * falloff * v3);
    else fR[framenumber]  += (cos(framenumber * f3) * falloff * v3);
    if ((framenumber * f4) < 1.57079633) fR[framenumber]  += (sin((framenumber * f4)*2.0) * falloff * v4);
    else fR[framenumber]  += (cos(framenumber * f4) * falloff * v4);
    if ((framenumber * f5) < 1.57079633) fR[framenumber]  += (sin((framenumber * f5)*2.0) * falloff * v5);
    else fR[framenumber]  += (cos(framenumber * f5) * falloff * v5);
    if ((framenumber * f6) < 1.57079633) fR[framenumber]  += (sin((framenumber * f6)*2.0) * falloff * v6);
    else fR[framenumber]  += (cos(framenumber * f6) * falloff * v6);
    if ((framenumber * f7) < 1.57079633) fR[framenumber]  += (sin((framenumber * f7)*2.0) * falloff * v7);
    else fR[framenumber]  += (cos(framenumber * f7) * falloff * v7);
    if ((framenumber * f8) < 1.57079633) fR[framenumber]  += (sin((framenumber * f8)*2.0) * falloff * v8);
    else fR[framenumber]  += (cos(framenumber * f8) * falloff * v8);

    //done updating the kernel for this go-round

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;
        if (fabs(inputSampleL)<1.18e-37) inputSampleL = fpd * 1.18e-37;
        if (fabs(inputSampleR)<1.18e-37) inputSampleR = fpd * 1.18e-37;
        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

        bL[59] = bL[58]; bL[58] = bL[57]; bL[57] = bL[56]; bL[56] = bL[55]; bL[55] = bL[54]; bL[54] = bL[53]; bL[53] = bL[52]; bL[52] = bL[51]; bL[51] = bL[50]; bL[50] = bL[49]; bL[49] = bL[48];
        bL[48] = bL[47]; bL[47] = bL[46]; bL[46] = bL[45]; bL[45] = bL[44]; bL[44] = bL[43]; bL[43] = bL[42]; bL[42] = bL[41]; bL[41] = bL[40]; bL[40] = bL[39]; bL[39] = bL[38];
        bL[38] = bL[37]; bL[37] = bL[36]; bL[36] = bL[35]; bL[35] = bL[34]; bL[34] = bL[33]; bL[33] = bL[32]; bL[32] = bL[31]; bL[31] = bL[30]; bL[30] = bL[29]; bL[29] = bL[28];
        bL[28] = bL[27]; bL[27] = bL[26]; bL[26] = bL[25]; bL[25] = bL[24]; bL[24] = bL[23]; bL[23] = bL[22]; bL[22] = bL[21]; bL[21] = bL[20]; bL[20] = bL[19]; bL[19] = bL[18];
        bL[18] = bL[17]; bL[17] = bL[16]; bL[16] = bL[15]; bL[15] = bL[14]; bL[14] = bL[13]; bL[13] = bL[12]; bL[12] = bL[11]; bL[11] = bL[10]; bL[10] = bL[9]; bL[9] = bL[8]; bL[8] = bL[7];
        bL[7] = bL[6]; bL[6] = bL[5]; bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1]; bL[1] = bL[0]; bL[0] = inputSampleL;

        inputSampleL = (bL[1] * fL[1]);
        inputSampleL += (bL[2] * fL[2]);
        inputSampleL += (bL[3] * fL[3]);
        inputSampleL += (bL[4] * fL[4]);
        inputSampleL += (bL[5] * fL[5]);
        inputSampleL += (bL[6] * fL[6]);
        inputSampleL += (bL[7] * fL[7]);
        inputSampleL += (bL[8] * fL[8]);
        inputSampleL += (bL[9] * fL[9]);
        inputSampleL += (bL[10] * fL[10]);
        inputSampleL += (bL[11] * fL[11]);
        inputSampleL += (bL[12] * fL[12]);
        inputSampleL += (bL[13] * fL[13]);
        inputSampleL += (bL[14] * fL[14]);
        inputSampleL += (bL[15] * fL[15]);
        inputSampleL += (bL[16] * fL[16]);
        inputSampleL += (bL[17] * fL[17]);
        inputSampleL += (bL[18] * fL[18]);
        inputSampleL += (bL[19] * fL[19]);
        inputSampleL += (bL[20] * fL[20]);
        inputSampleL += (bL[21] * fL[21]);
        inputSampleL += (bL[22] * fL[22]);
        inputSampleL += (bL[23] * fL[23]);
        inputSampleL += (bL[24] * fL[24]);
        inputSampleL += (bL[25] * fL[25]);
        inputSampleL += (bL[26] * fL[26]);
        inputSampleL += (bL[27] * fL[27]);
        inputSampleL += (bL[28] * fL[28]);
        inputSampleL += (bL[29] * fL[29]);
        inputSampleL += (bL[30] * fL[30]);
        inputSampleL += (bL[31] * fL[31]);
        inputSampleL += (bL[32] * fL[32]);
        inputSampleL += (bL[33] * fL[33]);
        inputSampleL += (bL[34] * fL[34]);
        inputSampleL += (bL[35] * fL[35]);
        inputSampleL += (bL[36] * fL[36]);
        inputSampleL += (bL[37] * fL[37]);
        inputSampleL += (bL[38] * fL[38]);
        inputSampleL += (bL[39] * fL[39]);
        inputSampleL += (bL[40] * fL[40]);
        inputSampleL += (bL[41] * fL[41]);
        inputSampleL += (bL[42] * fL[42]);
        inputSampleL += (bL[43] * fL[43]);
        inputSampleL += (bL[44] * fL[44]);
        inputSampleL += (bL[45] * fL[45]);
        inputSampleL += (bL[46] * fL[46]);
        inputSampleL += (bL[47] * fL[47]);
        inputSampleL += (bL[48] * fL[48]);
        inputSampleL += (bL[49] * fL[49]);
        inputSampleL += (bL[50] * fL[50]);
        inputSampleL += (bL[51] * fL[51]);
        inputSampleL += (bL[52] * fL[52]);
        inputSampleL += (bL[53] * fL[53]);
        inputSampleL += (bL[54] * fL[54]);
        inputSampleL += (bL[55] * fL[55]);
        inputSampleL += (bL[56] * fL[56]);
        inputSampleL += (bL[57] * fL[57]);
        inputSampleL += (bL[58] * fL[58]);
        inputSampleL += (bL[59] * fL[59]);
        inputSampleL /= 12.0;
        //inlined- this is our little EQ kernel. Longer will give better tightness on bass frequencies.
        //Note that normal programmers will make this a loop, which isn't much slower if at all, on modern CPUs.
        //It was unrolled more or less to show how much is done when you define a loop like that: it's easy to specify stuff where a lot of grinding is done.

        bR[59] = bR[58]; bR[58] = bR[57]; bR[57] = bR[56]; bR[56] = bR[55]; bR[55] = bR[54]; bR[54] = bR[53]; bR[53] = bR[52]; bR[52] = bR[51]; bR[51] = bR[50]; bR[50] = bR[49]; bR[49] = bR[48];
        bR[48] = bR[47]; bR[47] = bR[46]; bR[46] = bR[45]; bR[45] = bR[44]; bR[44] = bR[43]; bR[43] = bR[42]; bR[42] = bR[41]; bR[41] = bR[40]; bR[40] = bR[39]; bR[39] = bR[38];
        bR[38] = bR[37]; bR[37] = bR[36]; bR[36] = bR[35]; bR[35] = bR[34]; bR[34] = bR[33]; bR[33] = bR[32]; bR[32] = bR[31]; bR[31] = bR[30]; bR[30] = bR[29]; bR[29] = bR[28];
        bR[28] = bR[27]; bR[27] = bR[26]; bR[26] = bR[25]; bR[25] = bR[24]; bR[24] = bR[23]; bR[23] = bR[22]; bR[22] = bR[21]; bR[21] = bR[20]; bR[20] = bR[19]; bR[19] = bR[18];
        bR[18] = bR[17]; bR[17] = bR[16]; bR[16] = bR[15]; bR[15] = bR[14]; bR[14] = bR[13]; bR[13] = bR[12]; bR[12] = bR[11]; bR[11] = bR[10]; bR[10] = bR[9]; bR[9] = bR[8]; bR[8] = bR[7];
        bR[7] = bR[6]; bR[6] = bR[5]; bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1]; bR[1] = bR[0]; bR[0] = inputSampleR;

        inputSampleR = (bR[1] * fR[1]);
        inputSampleR += (bR[2] * fR[2]);
        inputSampleR += (bR[3] * fR[3]);
        inputSampleR += (bR[4] * fR[4]);
        inputSampleR += (bR[5] * fR[5]);
        inputSampleR += (bR[6] * fR[6]);
        inputSampleR += (bR[7] * fR[7]);
        inputSampleR += (bR[8] * fR[8]);
        inputSampleR += (bR[9] * fR[9]);
        inputSampleR += (bR[10] * fR[10]);
        inputSampleR += (bR[11] * fR[11]);
        inputSampleR += (bR[12] * fR[12]);
        inputSampleR += (bR[13] * fR[13]);
        inputSampleR += (bR[14] * fR[14]);
        inputSampleR += (bR[15] * fR[15]);
        inputSampleR += (bR[16] * fR[16]);
        inputSampleR += (bR[17] * fR[17]);
        inputSampleR += (bR[18] * fR[18]);
        inputSampleR += (bR[19] * fR[19]);
        inputSampleR += (bR[20] * fR[20]);
        inputSampleR += (bR[21] * fR[21]);
        inputSampleR += (bR[22] * fR[22]);
        inputSampleR += (bR[23] * fR[23]);
        inputSampleR += (bR[24] * fR[24]);
        inputSampleR += (bR[25] * fR[25]);
        inputSampleR += (bR[26] * fR[26]);
        inputSampleR += (bR[27] * fR[27]);
        inputSampleR += (bR[28] * fR[28]);
        inputSampleR += (bR[29] * fR[29]);
        inputSampleR += (bR[30] * fR[30]);
        inputSampleR += (bR[31] * fR[31]);
        inputSampleR += (bR[32] * fR[32]);
        inputSampleR += (bR[33] * fR[33]);
        inputSampleR += (bR[34] * fR[34]);
        inputSampleR += (bR[35] * fR[35]);
        inputSampleR += (bR[36] * fR[36]);
        inputSampleR += (bR[37] * fR[37]);
        inputSampleR += (bR[38] * fR[38]);
        inputSampleR += (bR[39] * fR[39]);
        inputSampleR += (bR[40] * fR[40]);
        inputSampleR += (bR[41] * fR[41]);
        inputSampleR += (bR[42] * fR[42]);
        inputSampleR += (bR[43] * fR[43]);
        inputSampleR += (bR[44] * fR[44]);
        inputSampleR += (bR[45] * fR[45]);
        inputSampleR += (bR[46] * fR[46]);
        inputSampleR += (bR[47] * fR[47]);
        inputSampleR += (bR[48] * fR[48]);
        inputSampleR += (bR[49] * fR[49]);
        inputSampleR += (bR[50] * fR[50]);
        inputSampleR += (bR[51] * fR[51]);
        inputSampleR += (bR[52] * fR[52]);
        inputSampleR += (bR[53] * fR[53]);
        inputSampleR += (bR[54] * fR[54]);
        inputSampleR += (bR[55] * fR[55]);
        inputSampleR += (bR[56] * fR[56]);
        inputSampleR += (bR[57] * fR[57]);
        inputSampleR += (bR[58] * fR[58]);
        inputSampleR += (bR[59] * fR[59]);
        inputSampleR /= 12.0;
        //inlined- this is our little EQ kernel. Longer will give better tightness on bass frequencies.
        //Note that normal programmers will make this a loop, which isn't much slower if at all, on modern CPUs.
        //It was unrolled more or less to show how much is done when you define a loop like that: it's easy to specify stuff where a lot of grinding is done.

        if (wet !=1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
            inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
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

void ResEQ::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    double overallscale = 1.0;
    overallscale /= 44100.0;
    overallscale *= getSampleRate();

    double v1 = A;
    double v2 = B;
    double v3 = C;
    double v4 = D;
    double v5 = E;
    double v6 = F;
    double v7 = G;
    double v8 = H;
    double f1 = pow(v1,2);
    double f2 = pow(v2,2);
    double f3 = pow(v3,2);
    double f4 = pow(v4,2);
    double f5 = pow(v5,2);
    double f6 = pow(v6,2);
    double f7 = pow(v7,2);
    double f8 = pow(v8,2);
    double wet = I;
    double falloff;
    v1 += 0.2; v2 += 0.2; v3 += 0.2; v4 += 0.2; v5 += 0.2; v6 += 0.2; v7 += 0.2; v8 += 0.2;
    v1 /= overallscale;
    v2 /= overallscale;
    v3 /= overallscale;
    v4 /= overallscale;
    v5 /= overallscale;
    v6 /= overallscale;
    v7 /= overallscale;
    v8 /= overallscale;
    //each process frame we'll update some of the kernel frames. That way we don't have to crunch the whole thing at once,
    //and we can load a LOT more resonant peaks into the kernel.

    framenumber += 1; if (framenumber > 59) framenumber = 1;
    falloff = sin(framenumber / 19.098992);
    fL[framenumber] = 0.0;
    if ((framenumber * f1) < 1.57079633) fL[framenumber]  += (sin((framenumber * f1)*2.0) * falloff * v1);
    else fL[framenumber]  += (cos(framenumber * f1) * falloff * v1);
    if ((framenumber * f2) < 1.57079633) fL[framenumber]  += (sin((framenumber * f2)*2.0) * falloff * v2);
    else fL[framenumber]  += (cos(framenumber * f2) * falloff * v2);
    if ((framenumber * f3) < 1.57079633) fL[framenumber]  += (sin((framenumber * f3)*2.0) * falloff * v3);
    else fL[framenumber]  += (cos(framenumber * f3) * falloff * v3);
    if ((framenumber * f4) < 1.57079633) fL[framenumber]  += (sin((framenumber * f4)*2.0) * falloff * v4);
    else fL[framenumber]  += (cos(framenumber * f4) * falloff * v4);
    if ((framenumber * f5) < 1.57079633) fL[framenumber]  += (sin((framenumber * f5)*2.0) * falloff * v5);
    else fL[framenumber]  += (cos(framenumber * f5) * falloff * v5);
    if ((framenumber * f6) < 1.57079633) fL[framenumber]  += (sin((framenumber * f6)*2.0) * falloff * v6);
    else fL[framenumber]  += (cos(framenumber * f6) * falloff * v6);
    if ((framenumber * f7) < 1.57079633) fL[framenumber]  += (sin((framenumber * f7)*2.0) * falloff * v7);
    else fL[framenumber]  += (cos(framenumber * f7) * falloff * v7);
    if ((framenumber * f8) < 1.57079633) fL[framenumber]  += (sin((framenumber * f8)*2.0) * falloff * v8);
    else fL[framenumber]  += (cos(framenumber * f8) * falloff * v8);

    fR[framenumber] = 0.0;
    if ((framenumber * f1) < 1.57079633) fR[framenumber]  += (sin((framenumber * f1)*2.0) * falloff * v1);
    else fR[framenumber]  += (cos(framenumber * f1) * falloff * v1);
    if ((framenumber * f2) < 1.57079633) fR[framenumber]  += (sin((framenumber * f2)*2.0) * falloff * v2);
    else fR[framenumber]  += (cos(framenumber * f2) * falloff * v2);
    if ((framenumber * f3) < 1.57079633) fR[framenumber]  += (sin((framenumber * f3)*2.0) * falloff * v3);
    else fR[framenumber]  += (cos(framenumber * f3) * falloff * v3);
    if ((framenumber * f4) < 1.57079633) fR[framenumber]  += (sin((framenumber * f4)*2.0) * falloff * v4);
    else fR[framenumber]  += (cos(framenumber * f4) * falloff * v4);
    if ((framenumber * f5) < 1.57079633) fR[framenumber]  += (sin((framenumber * f5)*2.0) * falloff * v5);
    else fR[framenumber]  += (cos(framenumber * f5) * falloff * v5);
    if ((framenumber * f6) < 1.57079633) fR[framenumber]  += (sin((framenumber * f6)*2.0) * falloff * v6);
    else fR[framenumber]  += (cos(framenumber * f6) * falloff * v6);
    if ((framenumber * f7) < 1.57079633) fR[framenumber]  += (sin((framenumber * f7)*2.0) * falloff * v7);
    else fR[framenumber]  += (cos(framenumber * f7) * falloff * v7);
    if ((framenumber * f8) < 1.57079633) fR[framenumber]  += (sin((framenumber * f8)*2.0) * falloff * v8);
    else fR[framenumber]  += (cos(framenumber * f8) * falloff * v8);

    framenumber += 1; if (framenumber > 59) framenumber = 1;
    falloff = sin(framenumber / 19.098992);
    fL[framenumber] = 0.0;
    if ((framenumber * f1) < 1.57079633) fL[framenumber]  += (sin((framenumber * f1)*2.0) * falloff * v1);
    else fL[framenumber]  += (cos(framenumber * f1) * falloff * v1);
    if ((framenumber * f2) < 1.57079633) fL[framenumber]  += (sin((framenumber * f2)*2.0) * falloff * v2);
    else fL[framenumber]  += (cos(framenumber * f2) * falloff * v2);
    if ((framenumber * f3) < 1.57079633) fL[framenumber]  += (sin((framenumber * f3)*2.0) * falloff * v3);
    else fL[framenumber]  += (cos(framenumber * f3) * falloff * v3);
    if ((framenumber * f4) < 1.57079633) fL[framenumber]  += (sin((framenumber * f4)*2.0) * falloff * v4);
    else fL[framenumber]  += (cos(framenumber * f4) * falloff * v4);
    if ((framenumber * f5) < 1.57079633) fL[framenumber]  += (sin((framenumber * f5)*2.0) * falloff * v5);
    else fL[framenumber]  += (cos(framenumber * f5) * falloff * v5);
    if ((framenumber * f6) < 1.57079633) fL[framenumber]  += (sin((framenumber * f6)*2.0) * falloff * v6);
    else fL[framenumber]  += (cos(framenumber * f6) * falloff * v6);
    if ((framenumber * f7) < 1.57079633) fL[framenumber]  += (sin((framenumber * f7)*2.0) * falloff * v7);
    else fL[framenumber]  += (cos(framenumber * f7) * falloff * v7);
    if ((framenumber * f8) < 1.57079633) fL[framenumber]  += (sin((framenumber * f8)*2.0) * falloff * v8);
    else fL[framenumber]  += (cos(framenumber * f8) * falloff * v8);

    fR[framenumber] = 0.0;
    if ((framenumber * f1) < 1.57079633) fR[framenumber]  += (sin((framenumber * f1)*2.0) * falloff * v1);
    else fR[framenumber]  += (cos(framenumber * f1) * falloff * v1);
    if ((framenumber * f2) < 1.57079633) fR[framenumber]  += (sin((framenumber * f2)*2.0) * falloff * v2);
    else fR[framenumber]  += (cos(framenumber * f2) * falloff * v2);
    if ((framenumber * f3) < 1.57079633) fR[framenumber]  += (sin((framenumber * f3)*2.0) * falloff * v3);
    else fR[framenumber]  += (cos(framenumber * f3) * falloff * v3);
    if ((framenumber * f4) < 1.57079633) fR[framenumber]  += (sin((framenumber * f4)*2.0) * falloff * v4);
    else fR[framenumber]  += (cos(framenumber * f4) * falloff * v4);
    if ((framenumber * f5) < 1.57079633) fR[framenumber]  += (sin((framenumber * f5)*2.0) * falloff * v5);
    else fR[framenumber]  += (cos(framenumber * f5) * falloff * v5);
    if ((framenumber * f6) < 1.57079633) fR[framenumber]  += (sin((framenumber * f6)*2.0) * falloff * v6);
    else fR[framenumber]  += (cos(framenumber * f6) * falloff * v6);
    if ((framenumber * f7) < 1.57079633) fR[framenumber]  += (sin((framenumber * f7)*2.0) * falloff * v7);
    else fR[framenumber]  += (cos(framenumber * f7) * falloff * v7);
    if ((framenumber * f8) < 1.57079633) fR[framenumber]  += (sin((framenumber * f8)*2.0) * falloff * v8);
    else fR[framenumber]  += (cos(framenumber * f8) * falloff * v8);

    //done updating the kernel for this go-round

    while (--sampleFrames >= 0)
    {
        long double inputSampleL = *in1;
        long double inputSampleR = *in2;

        if (fabs(inputSampleL)<1.18e-43) inputSampleL = fpd * 1.18e-43;
        if (fabs(inputSampleR)<1.18e-43) inputSampleR = fpd * 1.18e-43;

        long double drySampleL = inputSampleL;
        long double drySampleR = inputSampleR;

        bL[59] = bL[58]; bL[58] = bL[57]; bL[57] = bL[56]; bL[56] = bL[55]; bL[55] = bL[54]; bL[54] = bL[53]; bL[53] = bL[52]; bL[52] = bL[51]; bL[51] = bL[50]; bL[50] = bL[49]; bL[49] = bL[48];
        bL[48] = bL[47]; bL[47] = bL[46]; bL[46] = bL[45]; bL[45] = bL[44]; bL[44] = bL[43]; bL[43] = bL[42]; bL[42] = bL[41]; bL[41] = bL[40]; bL[40] = bL[39]; bL[39] = bL[38];
        bL[38] = bL[37]; bL[37] = bL[36]; bL[36] = bL[35]; bL[35] = bL[34]; bL[34] = bL[33]; bL[33] = bL[32]; bL[32] = bL[31]; bL[31] = bL[30]; bL[30] = bL[29]; bL[29] = bL[28];
        bL[28] = bL[27]; bL[27] = bL[26]; bL[26] = bL[25]; bL[25] = bL[24]; bL[24] = bL[23]; bL[23] = bL[22]; bL[22] = bL[21]; bL[21] = bL[20]; bL[20] = bL[19]; bL[19] = bL[18];
        bL[18] = bL[17]; bL[17] = bL[16]; bL[16] = bL[15]; bL[15] = bL[14]; bL[14] = bL[13]; bL[13] = bL[12]; bL[12] = bL[11]; bL[11] = bL[10]; bL[10] = bL[9]; bL[9] = bL[8]; bL[8] = bL[7];
        bL[7] = bL[6]; bL[6] = bL[5]; bL[5] = bL[4]; bL[4] = bL[3]; bL[3] = bL[2]; bL[2] = bL[1]; bL[1] = bL[0]; bL[0] = inputSampleL;

        inputSampleL = (bL[1] * fL[1]);
        inputSampleL += (bL[2] * fL[2]);
        inputSampleL += (bL[3] * fL[3]);
        inputSampleL += (bL[4] * fL[4]);
        inputSampleL += (bL[5] * fL[5]);
        inputSampleL += (bL[6] * fL[6]);
        inputSampleL += (bL[7] * fL[7]);
        inputSampleL += (bL[8] * fL[8]);
        inputSampleL += (bL[9] * fL[9]);
        inputSampleL += (bL[10] * fL[10]);
        inputSampleL += (bL[11] * fL[11]);
        inputSampleL += (bL[12] * fL[12]);
        inputSampleL += (bL[13] * fL[13]);
        inputSampleL += (bL[14] * fL[14]);
        inputSampleL += (bL[15] * fL[15]);
        inputSampleL += (bL[16] * fL[16]);
        inputSampleL += (bL[17] * fL[17]);
        inputSampleL += (bL[18] * fL[18]);
        inputSampleL += (bL[19] * fL[19]);
        inputSampleL += (bL[20] * fL[20]);
        inputSampleL += (bL[21] * fL[21]);
        inputSampleL += (bL[22] * fL[22]);
        inputSampleL += (bL[23] * fL[23]);
        inputSampleL += (bL[24] * fL[24]);
        inputSampleL += (bL[25] * fL[25]);
        inputSampleL += (bL[26] * fL[26]);
        inputSampleL += (bL[27] * fL[27]);
        inputSampleL += (bL[28] * fL[28]);
        inputSampleL += (bL[29] * fL[29]);
        inputSampleL += (bL[30] * fL[30]);
        inputSampleL += (bL[31] * fL[31]);
        inputSampleL += (bL[32] * fL[32]);
        inputSampleL += (bL[33] * fL[33]);
        inputSampleL += (bL[34] * fL[34]);
        inputSampleL += (bL[35] * fL[35]);
        inputSampleL += (bL[36] * fL[36]);
        inputSampleL += (bL[37] * fL[37]);
        inputSampleL += (bL[38] * fL[38]);
        inputSampleL += (bL[39] * fL[39]);
        inputSampleL += (bL[40] * fL[40]);
        inputSampleL += (bL[41] * fL[41]);
        inputSampleL += (bL[42] * fL[42]);
        inputSampleL += (bL[43] * fL[43]);
        inputSampleL += (bL[44] * fL[44]);
        inputSampleL += (bL[45] * fL[45]);
        inputSampleL += (bL[46] * fL[46]);
        inputSampleL += (bL[47] * fL[47]);
        inputSampleL += (bL[48] * fL[48]);
        inputSampleL += (bL[49] * fL[49]);
        inputSampleL += (bL[50] * fL[50]);
        inputSampleL += (bL[51] * fL[51]);
        inputSampleL += (bL[52] * fL[52]);
        inputSampleL += (bL[53] * fL[53]);
        inputSampleL += (bL[54] * fL[54]);
        inputSampleL += (bL[55] * fL[55]);
        inputSampleL += (bL[56] * fL[56]);
        inputSampleL += (bL[57] * fL[57]);
        inputSampleL += (bL[58] * fL[58]);
        inputSampleL += (bL[59] * fL[59]);
        inputSampleL /= 12.0;
        //inlined- this is our little EQ kernel. Longer will give better tightness on bass frequencies.
        //Note that normal programmers will make this a loop, which isn't much slower if at all, on modern CPUs.
        //It was unrolled more or less to show how much is done when you define a loop like that: it's easy to specify stuff where a lot of grinding is done.

        bR[59] = bR[58]; bR[58] = bR[57]; bR[57] = bR[56]; bR[56] = bR[55]; bR[55] = bR[54]; bR[54] = bR[53]; bR[53] = bR[52]; bR[52] = bR[51]; bR[51] = bR[50]; bR[50] = bR[49]; bR[49] = bR[48];
        bR[48] = bR[47]; bR[47] = bR[46]; bR[46] = bR[45]; bR[45] = bR[44]; bR[44] = bR[43]; bR[43] = bR[42]; bR[42] = bR[41]; bR[41] = bR[40]; bR[40] = bR[39]; bR[39] = bR[38];
        bR[38] = bR[37]; bR[37] = bR[36]; bR[36] = bR[35]; bR[35] = bR[34]; bR[34] = bR[33]; bR[33] = bR[32]; bR[32] = bR[31]; bR[31] = bR[30]; bR[30] = bR[29]; bR[29] = bR[28];
        bR[28] = bR[27]; bR[27] = bR[26]; bR[26] = bR[25]; bR[25] = bR[24]; bR[24] = bR[23]; bR[23] = bR[22]; bR[22] = bR[21]; bR[21] = bR[20]; bR[20] = bR[19]; bR[19] = bR[18];
        bR[18] = bR[17]; bR[17] = bR[16]; bR[16] = bR[15]; bR[15] = bR[14]; bR[14] = bR[13]; bR[13] = bR[12]; bR[12] = bR[11]; bR[11] = bR[10]; bR[10] = bR[9]; bR[9] = bR[8]; bR[8] = bR[7];
        bR[7] = bR[6]; bR[6] = bR[5]; bR[5] = bR[4]; bR[4] = bR[3]; bR[3] = bR[2]; bR[2] = bR[1]; bR[1] = bR[0]; bR[0] = inputSampleR;

        inputSampleR = (bR[1] * fR[1]);
        inputSampleR += (bR[2] * fR[2]);
        inputSampleR += (bR[3] * fR[3]);
        inputSampleR += (bR[4] * fR[4]);
        inputSampleR += (bR[5] * fR[5]);
        inputSampleR += (bR[6] * fR[6]);
        inputSampleR += (bR[7] * fR[7]);
        inputSampleR += (bR[8] * fR[8]);
        inputSampleR += (bR[9] * fR[9]);
        inputSampleR += (bR[10] * fR[10]);
        inputSampleR += (bR[11] * fR[11]);
        inputSampleR += (bR[12] * fR[12]);
        inputSampleR += (bR[13] * fR[13]);
        inputSampleR += (bR[14] * fR[14]);
        inputSampleR += (bR[15] * fR[15]);
        inputSampleR += (bR[16] * fR[16]);
        inputSampleR += (bR[17] * fR[17]);
        inputSampleR += (bR[18] * fR[18]);
        inputSampleR += (bR[19] * fR[19]);
        inputSampleR += (bR[20] * fR[20]);
        inputSampleR += (bR[21] * fR[21]);
        inputSampleR += (bR[22] * fR[22]);
        inputSampleR += (bR[23] * fR[23]);
        inputSampleR += (bR[24] * fR[24]);
        inputSampleR += (bR[25] * fR[25]);
        inputSampleR += (bR[26] * fR[26]);
        inputSampleR += (bR[27] * fR[27]);
        inputSampleR += (bR[28] * fR[28]);
        inputSampleR += (bR[29] * fR[29]);
        inputSampleR += (bR[30] * fR[30]);
        inputSampleR += (bR[31] * fR[31]);
        inputSampleR += (bR[32] * fR[32]);
        inputSampleR += (bR[33] * fR[33]);
        inputSampleR += (bR[34] * fR[34]);
        inputSampleR += (bR[35] * fR[35]);
        inputSampleR += (bR[36] * fR[36]);
        inputSampleR += (bR[37] * fR[37]);
        inputSampleR += (bR[38] * fR[38]);
        inputSampleR += (bR[39] * fR[39]);
        inputSampleR += (bR[40] * fR[40]);
        inputSampleR += (bR[41] * fR[41]);
        inputSampleR += (bR[42] * fR[42]);
        inputSampleR += (bR[43] * fR[43]);
        inputSampleR += (bR[44] * fR[44]);
        inputSampleR += (bR[45] * fR[45]);
        inputSampleR += (bR[46] * fR[46]);
        inputSampleR += (bR[47] * fR[47]);
        inputSampleR += (bR[48] * fR[48]);
        inputSampleR += (bR[49] * fR[49]);
        inputSampleR += (bR[50] * fR[50]);
        inputSampleR += (bR[51] * fR[51]);
        inputSampleR += (bR[52] * fR[52]);
        inputSampleR += (bR[53] * fR[53]);
        inputSampleR += (bR[54] * fR[54]);
        inputSampleR += (bR[55] * fR[55]);
        inputSampleR += (bR[56] * fR[56]);
        inputSampleR += (bR[57] * fR[57]);
        inputSampleR += (bR[58] * fR[58]);
        inputSampleR += (bR[59] * fR[59]);
        inputSampleR /= 12.0;
        //inlined- this is our little EQ kernel. Longer will give better tightness on bass frequencies.
        //Note that normal programmers will make this a loop, which isn't much slower if at all, on modern CPUs.
        //It was unrolled more or less to show how much is done when you define a loop like that: it's easy to specify stuff where a lot of grinding is done.

        if (wet !=1.0) {
            inputSampleL = (inputSampleL * wet) + (drySampleL * (1.0-wet));
            inputSampleR = (inputSampleR * wet) + (drySampleR * (1.0-wet));
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
