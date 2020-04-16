/*
  ==============================================================================

    RackNode.h
    Created: 8 Apr 2020 3:41:40pm
    Author:  David Rowland

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// To process a Rack we need to:
//  1. Find all the plugins in the Rack
//  2. Start by creating an output node (with the num channels as the num outputs) and then find all plugins attached to it
//        - This is a SummingAudioNode
//  3. Inputs are handled by a SendAudioNode feeding in to the output SummingAudioNode
//      - If there are no direct connections between the input and output, the ChannelMappingAudioNode will be empty
//      - Otherwise it will need to map input to output channels
//      - There needs to be a speacial InputAudioNode which is processed first and receives the graph audio/MIDI input
//      - This then passes to the SendAudioNode
//      - Which looks like: InputAudioNode -> SendAudioNode -> ChannelMappingAudioNode -> SummingAudioNode
//  4. Iterate each plugin, if it connects to the output add a channel mapper and then add this to the output summer
//      - InputAudioNode -> SendAudioNode -> ChannelMappingAudioNode -> SummingAudioNode
//                        PluginAudioNode -> ChannelMappingAudioNode -^
//                        PluginAudioNode -> ChannelMappingAudioNode -^
//  5. Whilst iterating each plugin, also iterate each of its input plugins and add ChannelMappingAudioNode and SumminAudioNodes
//      -                           InputAudioNode -> SendAudioNode -> ChannelMappingAudioNode -> SummingAudioNode
//                                                  PluginAudioNode -> ChannelMappingAudioNode -^
//     ChannelMappingAudioNode -> SummingAudioNode -^
//  6. Quickly, all plugins should have been iterated and the summind audionodes will have balanced the latency
