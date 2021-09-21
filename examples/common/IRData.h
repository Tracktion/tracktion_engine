/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace IRs
{
    extern const char*   AC30_brilliantbass_AT4033a_stalevel_dc_wav;
    const int            AC30_brilliantbass_AT4033a_stalevel_dc_wavSize = 53360;

    extern const char*   AC30_brilliantbass_BX44_neve_close_dc_wav;
    const int            AC30_brilliantbass_BX44_neve_close_dc_wavSize = 47140;

    extern const char*   AC30_brilliantbass_bx44_neve_far_dc_wav;
    const int            AC30_brilliantbass_bx44_neve_far_dc_wavSize = 61606;

    extern const char*   AC30_brilliantbass_bx44_stalevel__far_dc_wav;
    const int            AC30_brilliantbass_bx44_stalevel__far_dc_wavSize = 55634;

    extern const char*   AC30_brilliantbass_bx44_stalevel_close_dc_wav;
    const int            AC30_brilliantbass_bx44_stalevel_close_dc_wavSize = 54826;

    extern const char*   AC30_brilliantbass_U67_neve_dc_wav;
    const int            AC30_brilliantbass_U67_neve_dc_wavSize = 66878;

    extern const char*   AC30_brilliantbass_U67_stalevel_dc_wav;
    const int            AC30_brilliantbass_U67_stalevel_dc_wavSize = 70798;

    extern const char*   AC30_brilliant_bx44_neve_close_dc_wav;
    const int            AC30_brilliant_bx44_neve_close_dc_wavSize = 68800;

    extern const char*   AC30_brilliant_bx44_stalevel_close_dc_wav;
    const int            AC30_brilliant_bx44_stalevel_close_dc_wavSize = 59858;

    extern const char*   AC30_brilliantbass_AT4033a_neve_dc_wav;
    const int            AC30_brilliantbass_AT4033a_neve_dc_wavSize = 60008;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 10;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}

#include "IRData.cpp"