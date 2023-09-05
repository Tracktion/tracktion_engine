#include "DrumMachinePlugin.h"
/////////////////////////////////////////////////////////////////////////////////////////////
//     ____             _    ____                            _                             //
//    | __ )  ___  __ _| |_ / ___|___  _ __  _ __   ___  ___| |_                           //
//    |  _ \ / _ \/ _` | __| |   / _ \| '_ \| '_ \ / _ \/ __| __|     Copyright 2023       //
//    | |_) |  __/ (_| | |_| |__| (_) | | | | | | |  __/ (__| |_    www.beatconnect.com    //
//    |____/ \___|\__,_|\__|\____\___/|_| |_|_| |_|\___|\___|\__|                          //
//                                                                                         //
/////////////////////////////////////////////////////////////////////////////////////////////

namespace tracktion { inline namespace engine
{
DrumMachinePlugin::DrumMachinePlugin(PluginCreationInfo pluginCreationInfo) : SamplerPlugin(pluginCreationInfo) {}

const int DrumMachinePlugin::pitchWheelSemitoneRange = 25.0;
const char* DrumMachinePlugin::uniqueId = "adf30650-4fd8-4cce-933d-fa8aa598c6c9";
const char* DrumMachinePlugin::xmlTypeName = "drum machine";
}} // namespace tracktion { inline namespace engine
