/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.txt file.


 BEGIN_JUCE_MODULE_DECLARATION

  ID:               tracktion_core
  vendor:           Tracktion Corporation
  version:          1.0.0
  name:             The Tracktion core object types
  description:      Classes for creating and processing time based applications
  website:          http://www.tracktion.com
  license:          Proprietary

  dependencies:     juce_audio_formats

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/


#pragma once
#define TRACKTION_CORE_H_INCLUDED

//==============================================================================
//==============================================================================

//==============================================================================
#include "audio/tracktion_AudioReader.h"

#include "utilities/tracktion_CPU.h"
#include "utilities/tracktion_Hash.h"
#include "utilities/tracktion_Tempo.h"
#include "utilities/tracktion_Time.h"
#include "utilities/tracktion_TimeRange.h"
