/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#if TRACKTION_UNIT_TESTS && TRACKTION_UNIT_TESTS_ALGORITHM

#include "../../3rd_party/doctest/tracktion_doctest.hpp"

namespace tracktion::inline core
{

TEST_SUITE ("tracktion_core")
{
    TEST_CASE ("Algorithm")
    {
        SUBCASE ("stable_remove_duplicates")
        {
            int i1 = 1, i2 = 2, i3 = 3, i4 = 4;
            std::vector<int*> v;
            v.push_back (&i2);
            v.push_back (&i1);
            v.push_back (&i1);
            v.push_back (&i4);
            v.push_back (&i3);
            v.push_back (&i4);
            v.push_back (&i2);

            CHECK_EQ (*v[0], 2);
            CHECK_EQ (*v[1], 1);
            CHECK_EQ (*v[2], 1);
            CHECK_EQ (*v[3], 4);
            CHECK_EQ (*v[4], 3);
            CHECK_EQ (*v[5], 4);
            CHECK_EQ (*v[6], 2);

            stable_remove_duplicates (v);
            CHECK_EQ (*v[0], 2);
            CHECK_EQ (*v[1], 1);
            CHECK_EQ (*v[2], 4);
            CHECK_EQ (*v[3], 3);
        }
    }
}

}

#endif // TRACKTION_UNIT_TESTS && TRACKTION_UNIT_TESTS_ALGORITHM
