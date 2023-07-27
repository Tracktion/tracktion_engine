/////////////////////////////////////////////////////////////////////////////////////////////
//     ____             _    ____                            _                             //
//    | __ )  ___  __ _| |_ / ___|___  _ __  _ __   ___  ___| |_                           //
//    |  _ \ / _ \/ _` | __| |   / _ \| '_ \| '_ \ / _ \/ __| __|     Copyright 2023       //
//    | |_) |  __/ (_| | |_| |__| (_) | | | | | | |  __/ (__| |_    www.beatconnect.com    //
//    |____/ \___|\__,_|\__|\____\___/|_| |_|_| |_|\___|\___|\__|                          //
//                                                                                         //
/////////////////////////////////////////////////////////////////////////////////////////////

#include "DrumMachinePlugin.h"

DrumMachinePlugin::DrumMachinePlugin(te::PluginCreationInfo pluginCreationInfo) : SamplerPlugin(pluginCreationInfo)
{}

const int DrumMachinePlugin::pitchWheelSemitoneRange = 25.0;
const char* DrumMachinePlugin::uniqueId = "adf30650-4fd8-4cce-933d-fa8aa598c6c9";
const char* DrumMachinePlugin::xmlTypeName = "drum machine";
