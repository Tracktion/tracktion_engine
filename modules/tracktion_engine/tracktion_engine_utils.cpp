/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


#if ! JUCE_PROJUCER_LIVE_BUILD

#ifndef _MSC_VER
 #include <fenv.h>

 #if TRACKTION_ENABLE_REWIRE || TRACKTION_ENABLE_REX
  #define Button CarbonButton
  #define Component CarbonComponent
  #define MemoryBlock CarbonMemoryBlock
  #include <Carbon/Carbon.h>
 #endif
#endif

#if TRACKTION_ENABLE_REWIRE
 extern "C"
 {
   #include "ReWire/ReWireSDK/ReWireMixerAPI.c"
 }
#endif

#undef Button
#undef Component
#undef MemoryBlock
#undef check

//==============================================================================
#include "tracktion_engine.h"

//==============================================================================
#if JUCE_MAC && TRACKTION_ENABLE_REX
extern "C" char MacGetMacFSRefForREXDLL (FSRef* fsRef)
{
    juce::File f ("~/Library/Application Support/Propellerhead Software/Rex/REX Shared Library.bundle");

    if (! f.exists())
        f = juce::File ("/Library/Application Support/Propellerhead Software/Rex/REX Shared Library.bundle");

   #pragma clang diagnostic push
   #pragma clang diagnostic ignored "-Wdeprecated"
    return FSPathMakeRef ((const UInt8*) f.getFullPathName().toRawUTF8(), fsRef, 0) == noErr;
   #pragma clang diagnostic pop
}
#endif

//==============================================================================
namespace tracktion_engine
{
  using namespace juce;

  #include "selection/tracktion_Clipboard.cpp"
  #include "selection/tracktion_SelectionManager.cpp"

  #include "utilities/tracktion_AppFunctions.cpp"
  #include "utilities/tracktion_AudioUtilities.cpp"
  #include "utilities/tracktion_ConstrainedCachedValue.cpp"
  #include "utilities/tracktion_CrashTracer.cpp"
  #include "utilities/tracktion_CurveEditor.cpp"
  #include "utilities/tracktion_FileUtilities.cpp"
  #include "utilities/tracktion_PropertyStorage.cpp"
  #include "utilities/tracktion_UIBehaviour.cpp"
  #include "utilities/tracktion_TemporaryFileManager.cpp"
  #include "utilities/tracktion_Engine.cpp"
  #include "utilities/tracktion_BinaryData.cpp"
}

#endif
