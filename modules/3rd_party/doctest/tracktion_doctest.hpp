/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/


#pragma once

#include "../choc/platform/choc_DisableAllWarnings.h"
 #if __has_include(<doctest.h>)
 #include <doctest.h>
 #else
  #include "doctest.h"
 #endif
#include "../choc/platform/choc_ReenableAllWarnings.h"

#include "../choc/text/choc_OpenSourceLicenseList.h"

#ifdef CHOC_REGISTER_OPEN_SOURCE_LICENCE
 CHOC_REGISTER_OPEN_SOURCE_LICENCE (doctest, R"(
// doctest.h - the lightest feature-rich C++ single-header testing framework for unit tests and TDD
//
// Copyright (c) 2016-2023 Viktor Kirilov
//
// Distributed under the MIT Software License
// See accompanying file LICENSE.txt or copy at
// https://opensource.org/licenses/MIT
//
// The documentation can be found at the library's page:
// https://github.com/doctest/doctest/blob/master/doc/markdown/readme.md
)")
#endif

#undef DOCTEST_CONFIG_IMPLEMENT