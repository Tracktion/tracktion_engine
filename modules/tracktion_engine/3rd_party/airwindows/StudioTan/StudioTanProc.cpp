/* ========================================
 *  StudioTan - StudioTan.h
 *  Copyright (c) 2016 airwindows, All rights reserved
 * ======================================== */

#ifndef __StudioTan_H
#include "StudioTan.h"
#endif

void StudioTan::processReplacing(float **inputs, float **outputs, VstInt32 sampleFrames)
{
    float* in1  =  inputs[0];
    float* in2  =  inputs[1];
    float* out1 = outputs[0];
    float* out2 = outputs[1];

    bool highres = true; //for 24 bit: false for 16 bit
    bool brightfloor = true; //for Studio Tan: false for Dither Me Timbers
    bool benford = true; //for Not Just Another Dither: false for newer two
    bool cutbins = false; //for NJAD: only attenuate bins if one gets very full
    switch ((VstInt32)( A * 5.999 ))
    {
        case 0: benford = false; break; //Studio Tan 24
        case 1: benford = false; brightfloor = false; break; //Dither Me Timbers 24
        case 2: break; //Not Just Another Dither 24
        case 3: benford = false; highres = false; break; //Studio Tan 16
        case 4: benford = false; brightfloor = false; highres = false; break; //Dither Me Timbers 16
        case 5: highres = false; break; //Not Just Another Dither 16
    }

    while (--sampleFrames >= 0)
    {
        long double inputSampleL;
        long double outputSampleL;
        long double drySampleL;
        long double inputSampleR;
        long double outputSampleR;
        long double drySampleR;

        if (highres) {
            inputSampleL = *in1 * 8388608.0;
            inputSampleR = *in2 * 8388608.0;
        } else {
            inputSampleL = *in1 * 32768.0;
            inputSampleR = *in2 * 32768.0;
        }
        //shared input stage

        if (benford) {
            //begin Not Just Another Dither
            drySampleL = inputSampleL;
            drySampleR = inputSampleR;
            inputSampleL -= noiseShapingL;
            inputSampleR -= noiseShapingR;

            cutbins = false;
            long double benfordize; //we get to re-use this for each channel

            //begin left channel NJAD
            benfordize = floor(inputSampleL);
            while (benfordize >= 1.0) {benfordize /= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            int hotbinA = floor(benfordize);
            //hotbin becomes the Benford bin value for this number floored
            long double totalA = 0;
            if ((hotbinA > 0) && (hotbinA < 10))
            {
                bynL[hotbinA] += 1;
                if (bynL[hotbinA] > 982) cutbins = true;
                totalA += (301-bynL[1]);
                totalA += (176-bynL[2]);
                totalA += (125-bynL[3]);
                totalA += (97-bynL[4]);
                totalA += (79-bynL[5]);
                totalA += (67-bynL[6]);
                totalA += (58-bynL[7]);
                totalA += (51-bynL[8]);
                totalA += (46-bynL[9]);
                bynL[hotbinA] -= 1;
            } else {hotbinA = 10;}
            //produce total number- smaller is closer to Benford real

            benfordize = ceil(inputSampleL);
            while (benfordize >= 1.0) {benfordize /= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            int hotbinB = floor(benfordize);
            //hotbin becomes the Benford bin value for this number ceiled
            long double totalB = 0;
            if ((hotbinB > 0) && (hotbinB < 10))
            {
                bynL[hotbinB] += 1;
                if (bynL[hotbinB] > 982) cutbins = true;
                totalB += (301-bynL[1]);
                totalB += (176-bynL[2]);
                totalB += (125-bynL[3]);
                totalB += (97-bynL[4]);
                totalB += (79-bynL[5]);
                totalB += (67-bynL[6]);
                totalB += (58-bynL[7]);
                totalB += (51-bynL[8]);
                totalB += (46-bynL[9]);
                bynL[hotbinB] -= 1;
            } else {hotbinB = 10;}
            //produce total number- smaller is closer to Benford real

            if (totalA < totalB)
            {
                bynL[hotbinA] += 1;
                outputSampleL = floor(inputSampleL);
            }
            else
            {
                bynL[hotbinB] += 1;
                outputSampleL = floor(inputSampleL+1);
            }
            //assign the relevant one to the delay line
            //and floor/ceil signal accordingly
            if (cutbins) {
                bynL[1] *= 0.99;
                bynL[2] *= 0.99;
                bynL[3] *= 0.99;
                bynL[4] *= 0.99;
                bynL[5] *= 0.99;
                bynL[6] *= 0.99;
                bynL[7] *= 0.99;
                bynL[8] *= 0.99;
                bynL[9] *= 0.99;
                bynL[10] *= 0.99; //catchall for garbage data
            }
            noiseShapingL += outputSampleL - drySampleL;
            //end left channel NJAD

            //begin right channel NJAD
            cutbins = false;
            benfordize = floor(inputSampleR);
            while (benfordize >= 1.0) {benfordize /= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            hotbinA = floor(benfordize);
            //hotbin becomes the Benford bin value for this number floored
            totalA = 0;
            if ((hotbinA > 0) && (hotbinA < 10))
            {
                bynR[hotbinA] += 1;
                if (bynR[hotbinA] > 982) cutbins = true;
                totalA += (301-bynR[1]);
                totalA += (176-bynR[2]);
                totalA += (125-bynR[3]);
                totalA += (97-bynR[4]);
                totalA += (79-bynR[5]);
                totalA += (67-bynR[6]);
                totalA += (58-bynR[7]);
                totalA += (51-bynR[8]);
                totalA += (46-bynR[9]);
                bynR[hotbinA] -= 1;
            } else {hotbinA = 10;}
            //produce total number- smaller is closer to Benford real

            benfordize = ceil(inputSampleR);
            while (benfordize >= 1.0) {benfordize /= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            hotbinB = floor(benfordize);
            //hotbin becomes the Benford bin value for this number ceiled
            totalB = 0;
            if ((hotbinB > 0) && (hotbinB < 10))
            {
                bynR[hotbinB] += 1;
                if (bynR[hotbinB] > 982) cutbins = true;
                totalB += (301-bynR[1]);
                totalB += (176-bynR[2]);
                totalB += (125-bynR[3]);
                totalB += (97-bynR[4]);
                totalB += (79-bynR[5]);
                totalB += (67-bynR[6]);
                totalB += (58-bynR[7]);
                totalB += (51-bynR[8]);
                totalB += (46-bynR[9]);
                bynR[hotbinB] -= 1;
            } else {hotbinB = 10;}
            //produce total number- smaller is closer to Benford real

            if (totalA < totalB)
            {
                bynR[hotbinA] += 1;
                outputSampleR = floor(inputSampleR);
            }
            else
            {
                bynR[hotbinB] += 1;
                outputSampleR = floor(inputSampleR+1);
            }
            //assign the relevant one to the delay line
            //and floor/ceil signal accordingly
            if (cutbins) {
                bynR[1] *= 0.99;
                bynR[2] *= 0.99;
                bynR[3] *= 0.99;
                bynR[4] *= 0.99;
                bynR[5] *= 0.99;
                bynR[6] *= 0.99;
                bynR[7] *= 0.99;
                bynR[8] *= 0.99;
                bynR[9] *= 0.99;
                bynR[10] *= 0.99; //catchall for garbage data
            }
            noiseShapingR += outputSampleR - drySampleR;
            //end right channel NJAD

            //end Not Just Another Dither
        } else {
            //begin StudioTan or Dither Me Timbers
            if (brightfloor) {
                lastSampleL -= (noiseShapingL*0.8);
                lastSampleR -= (noiseShapingR*0.8);
                if ((lastSampleL+lastSampleL) <= (inputSampleL+lastSample2L)) outputSampleL = floor(lastSampleL); //StudioTan
                else outputSampleL = floor(lastSampleL+1.0); //round down or up based on whether it softens treble angles
                if ((lastSampleR+lastSampleR) <= (inputSampleR+lastSample2R)) outputSampleR = floor(lastSampleR); //StudioTan
                else outputSampleR = floor(lastSampleR+1.0); //round down or up based on whether it softens treble angles
            } else {
                lastSampleL -= (noiseShapingL*0.11);
                lastSampleR -= (noiseShapingR*0.11);
                if ((lastSampleL+lastSampleL) >= (inputSampleL+lastSample2L)) outputSampleL = floor(lastSampleL); //DitherMeTimbers
                else outputSampleL = floor(lastSampleL+1.0); //round down or up based on whether it softens treble angles
                if ((lastSampleR+lastSampleR) >= (inputSampleR+lastSample2R)) outputSampleR = floor(lastSampleR); //DitherMeTimbers
                else outputSampleR = floor(lastSampleR+1.0); //round down or up based on whether it softens treble angles
            }
            noiseShapingL += outputSampleL;
            noiseShapingL -= lastSampleL; //apply noise shaping
            lastSample2L = lastSampleL;
            lastSampleL = inputSampleL; //we retain three samples in a row

            noiseShapingR += outputSampleR;
            noiseShapingR -= lastSampleR; //apply noise shaping
            lastSample2R = lastSampleR;
            lastSampleR = inputSampleR; //we retain three samples in a row
            //end StudioTan or Dither Me Timbers
        }

        //shared output stage
        long double noiseSuppressL = fabs(inputSampleL);
        if (noiseShapingL > noiseSuppressL) noiseShapingL = noiseSuppressL;
        if (noiseShapingL < -noiseSuppressL) noiseShapingL = -noiseSuppressL;

        long double noiseSuppressR = fabs(inputSampleR);
        if (noiseShapingR > noiseSuppressR) noiseShapingR = noiseSuppressR;
        if (noiseShapingR < -noiseSuppressR) noiseShapingR = -noiseSuppressR;

        float ironBarL;
        float ironBarR;
        if (highres) {
            ironBarL = outputSampleL / 8388608.0;
            ironBarR = outputSampleR / 8388608.0;
        } else {
            ironBarL = outputSampleL / 32768.0;
            ironBarR = outputSampleR / 32768.0;
        }

        if (ironBarL > 1.0) ironBarL = 1.0;
        if (ironBarL < -1.0) ironBarL = -1.0;
        if (ironBarR > 1.0) ironBarR = 1.0;
        if (ironBarR < -1.0) ironBarR = -1.0;

        *out1 = ironBarL;
        *out2 = ironBarR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}

void StudioTan::processDoubleReplacing(double **inputs, double **outputs, VstInt32 sampleFrames)
{
    double* in1  =  inputs[0];
    double* in2  =  inputs[1];
    double* out1 = outputs[0];
    double* out2 = outputs[1];

    bool highres = true; //for 24 bit: false for 16 bit
    bool brightfloor = true; //for Studio Tan: false for Dither Me Timbers
    bool benford = true; //for Not Just Another Dither: false for newer two
    bool cutbins = false; //for NJAD: only attenuate bins if one gets very full
    switch ((VstInt32)( A * 5.999 ))
    {
        case 0: benford = false; break; //Studio Tan 24
        case 1: benford = false; brightfloor = false; break; //Dither Me Timbers 24
        case 2: break; //Not Just Another Dither 24
        case 3: benford = false; highres = false; break; //Studio Tan 16
        case 4: benford = false; brightfloor = false; highres = false; break; //Dither Me Timbers 16
        case 5: highres = false; break; //Not Just Another Dither 16
    }

    while (--sampleFrames >= 0)
    {
        long double inputSampleL;
        long double outputSampleL;
        long double drySampleL;
        long double inputSampleR;
        long double outputSampleR;
        long double drySampleR;

        if (highres) {
            inputSampleL = *in1 * 8388608.0;
            inputSampleR = *in2 * 8388608.0;
        } else {
            inputSampleL = *in1 * 32768.0;
            inputSampleR = *in2 * 32768.0;
        }
        //shared input stage

        if (benford) {
            //begin Not Just Another Dither
            drySampleL = inputSampleL;
            drySampleR = inputSampleR;
            inputSampleL -= noiseShapingL;
            inputSampleR -= noiseShapingR;

            cutbins = false;
            long double benfordize; //we get to re-use this for each channel

            //begin left channel NJAD
            benfordize = floor(inputSampleL);
            while (benfordize >= 1.0) {benfordize /= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            int hotbinA = floor(benfordize);
            //hotbin becomes the Benford bin value for this number floored
            long double totalA = 0;
            if ((hotbinA > 0) && (hotbinA < 10))
            {
                bynL[hotbinA] += 1;
                if (bynL[hotbinA] > 982) cutbins = true;
                totalA += (301-bynL[1]);
                totalA += (176-bynL[2]);
                totalA += (125-bynL[3]);
                totalA += (97-bynL[4]);
                totalA += (79-bynL[5]);
                totalA += (67-bynL[6]);
                totalA += (58-bynL[7]);
                totalA += (51-bynL[8]);
                totalA += (46-bynL[9]);
                bynL[hotbinA] -= 1;
            } else {hotbinA = 10;}
            //produce total number- smaller is closer to Benford real

            benfordize = ceil(inputSampleL);
            while (benfordize >= 1.0) {benfordize /= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            int hotbinB = floor(benfordize);
            //hotbin becomes the Benford bin value for this number ceiled
            long double totalB = 0;
            if ((hotbinB > 0) && (hotbinB < 10))
            {
                bynL[hotbinB] += 1;
                if (bynL[hotbinB] > 982) cutbins = true;
                totalB += (301-bynL[1]);
                totalB += (176-bynL[2]);
                totalB += (125-bynL[3]);
                totalB += (97-bynL[4]);
                totalB += (79-bynL[5]);
                totalB += (67-bynL[6]);
                totalB += (58-bynL[7]);
                totalB += (51-bynL[8]);
                totalB += (46-bynL[9]);
                bynL[hotbinB] -= 1;
            } else {hotbinB = 10;}
            //produce total number- smaller is closer to Benford real

            if (totalA < totalB)
            {
                bynL[hotbinA] += 1;
                outputSampleL = floor(inputSampleL);
            }
            else
            {
                bynL[hotbinB] += 1;
                outputSampleL = floor(inputSampleL+1);
            }
            //assign the relevant one to the delay line
            //and floor/ceil signal accordingly
            if (cutbins) {
                bynL[1] *= 0.99;
                bynL[2] *= 0.99;
                bynL[3] *= 0.99;
                bynL[4] *= 0.99;
                bynL[5] *= 0.99;
                bynL[6] *= 0.99;
                bynL[7] *= 0.99;
                bynL[8] *= 0.99;
                bynL[9] *= 0.99;
                bynL[10] *= 0.99; //catchall for garbage data
            }
            noiseShapingL += outputSampleL - drySampleL;
            //end left channel NJAD

            //begin right channel NJAD
            cutbins = false;
            benfordize = floor(inputSampleR);
            while (benfordize >= 1.0) {benfordize /= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            hotbinA = floor(benfordize);
            //hotbin becomes the Benford bin value for this number floored
            totalA = 0;
            if ((hotbinA > 0) && (hotbinA < 10))
            {
                bynR[hotbinA] += 1;
                if (bynR[hotbinA] > 982) cutbins = true;
                totalA += (301-bynR[1]);
                totalA += (176-bynR[2]);
                totalA += (125-bynR[3]);
                totalA += (97-bynR[4]);
                totalA += (79-bynR[5]);
                totalA += (67-bynR[6]);
                totalA += (58-bynR[7]);
                totalA += (51-bynR[8]);
                totalA += (46-bynR[9]);
                bynR[hotbinA] -= 1;
            } else {hotbinA = 10;}
            //produce total number- smaller is closer to Benford real

            benfordize = ceil(inputSampleR);
            while (benfordize >= 1.0) {benfordize /= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            if (benfordize < 1.0) {benfordize *= 10;}
            hotbinB = floor(benfordize);
            //hotbin becomes the Benford bin value for this number ceiled
            totalB = 0;
            if ((hotbinB > 0) && (hotbinB < 10))
            {
                bynR[hotbinB] += 1;
                if (bynR[hotbinB] > 982) cutbins = true;
                totalB += (301-bynR[1]);
                totalB += (176-bynR[2]);
                totalB += (125-bynR[3]);
                totalB += (97-bynR[4]);
                totalB += (79-bynR[5]);
                totalB += (67-bynR[6]);
                totalB += (58-bynR[7]);
                totalB += (51-bynR[8]);
                totalB += (46-bynR[9]);
                bynR[hotbinB] -= 1;
            } else {hotbinB = 10;}
            //produce total number- smaller is closer to Benford real

            if (totalA < totalB)
            {
                bynR[hotbinA] += 1;
                outputSampleR = floor(inputSampleR);
            }
            else
            {
                bynR[hotbinB] += 1;
                outputSampleR = floor(inputSampleR+1);
            }
            //assign the relevant one to the delay line
            //and floor/ceil signal accordingly
            if (cutbins) {
                bynR[1] *= 0.99;
                bynR[2] *= 0.99;
                bynR[3] *= 0.99;
                bynR[4] *= 0.99;
                bynR[5] *= 0.99;
                bynR[6] *= 0.99;
                bynR[7] *= 0.99;
                bynR[8] *= 0.99;
                bynR[9] *= 0.99;
                bynR[10] *= 0.99; //catchall for garbage data
            }
            noiseShapingR += outputSampleR - drySampleR;
            //end right channel NJAD

            //end Not Just Another Dither
        } else {
            //begin StudioTan or Dither Me Timbers
            if (brightfloor) {
                lastSampleL -= (noiseShapingL*0.8);
                lastSampleR -= (noiseShapingR*0.8);
                if ((lastSampleL+lastSampleL) <= (inputSampleL+lastSample2L)) outputSampleL = floor(lastSampleL); //StudioTan
                else outputSampleL = floor(lastSampleL+1.0); //round down or up based on whether it softens treble angles
                if ((lastSampleR+lastSampleR) <= (inputSampleR+lastSample2R)) outputSampleR = floor(lastSampleR); //StudioTan
                else outputSampleR = floor(lastSampleR+1.0); //round down or up based on whether it softens treble angles
            } else {
                lastSampleL -= (noiseShapingL*0.11);
                lastSampleR -= (noiseShapingR*0.11);
                if ((lastSampleL+lastSampleL) >= (inputSampleL+lastSample2L)) outputSampleL = floor(lastSampleL); //DitherMeTimbers
                else outputSampleL = floor(lastSampleL+1.0); //round down or up based on whether it softens treble angles
                if ((lastSampleR+lastSampleR) >= (inputSampleR+lastSample2R)) outputSampleR = floor(lastSampleR); //DitherMeTimbers
                else outputSampleR = floor(lastSampleR+1.0); //round down or up based on whether it softens treble angles
            }
            noiseShapingL += outputSampleL;
            noiseShapingL -= lastSampleL; //apply noise shaping
            lastSample2L = lastSampleL;
            lastSampleL = inputSampleL; //we retain three samples in a row

            noiseShapingR += outputSampleR;
            noiseShapingR -= lastSampleR; //apply noise shaping
            lastSample2R = lastSampleR;
            lastSampleR = inputSampleR; //we retain three samples in a row
            //end StudioTan or Dither Me Timbers
        }

        //shared output stage
        long double noiseSuppressL = fabs(inputSampleL);
        if (noiseShapingL > noiseSuppressL) noiseShapingL = noiseSuppressL;
        if (noiseShapingL < -noiseSuppressL) noiseShapingL = -noiseSuppressL;

        long double noiseSuppressR = fabs(inputSampleR);
        if (noiseShapingR > noiseSuppressR) noiseShapingR = noiseSuppressR;
        if (noiseShapingR < -noiseSuppressR) noiseShapingR = -noiseSuppressR;

        double ironBarL;
        double ironBarR;
        if (highres) {
            ironBarL = outputSampleL / 8388608.0;
            ironBarR = outputSampleR / 8388608.0;
        } else {
            ironBarL = outputSampleL / 32768.0;
            ironBarR = outputSampleR / 32768.0;
        }

        if (ironBarL > 1.0) ironBarL = 1.0;
        if (ironBarL < -1.0) ironBarL = -1.0;
        if (ironBarR > 1.0) ironBarR = 1.0;
        if (ironBarR < -1.0) ironBarR = -1.0;

        *out1 = ironBarL;
        *out2 = ironBarR;

        *in1++;
        *in2++;
        *out1++;
        *out2++;
    }
}
