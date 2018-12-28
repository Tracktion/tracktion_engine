/* ========================================
 *  DCVoltage - DCVoltage.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __DCVoltage_H
#include "DCVoltage.h"
#endif

void DCVoltage::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

	double voltage = (A*2.0)-1.0;

    while (--sampleFrames >= 0)
    {
		*out1 = *in1 + voltage;
		*out2 = *in2 + voltage;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}

void DCVoltage::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

	double voltage = (A*2.0)-1.0;

    while (--sampleFrames >= 0)
    {
		*out1 = *in1 + voltage;
		*out2 = *in2 + voltage;

		*in1++;
		*in2++;
		*out1++;
		*out2++;
    }
}
