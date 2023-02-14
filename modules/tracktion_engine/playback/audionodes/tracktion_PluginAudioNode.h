/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

#pragma once

#include "tracktion_AudioNode.h"


namespace tracktion { inline namespace engine
{

//==============================================================================
class PluginAudioNode   : public AudioNode
{
public:
    PluginAudioNode (const Plugin::Ptr& p, AudioNode* in, bool denormalisationNoise)
        : plugin (p), input (in), applyAntiDenormalisationNoise (denormalisationNoise)
    {
        jassert (input != nullptr);
        jassert (plugin != nullptr);
    }

    ~PluginAudioNode() override
    {
        input = nullptr;
        releaseAudioNodeResources();
    }

    void getAudioNodeProperties (AudioNodeProperties& info) override
    {
        if (input != nullptr)
        {
            input->getAudioNodeProperties (info);
        }
        else
        {
            info.hasAudio = false;
            info.hasMidi = false;
            info.numberOfChannels = 0;
        }

        info.numberOfChannels = std::max (info.numberOfChannels, plugin->getNumOutputChannelsGivenInputs (info.numberOfChannels));
        info.hasAudio = info.hasAudio || plugin->producesAudioWhenNoAudioInput();
        info.hasMidi  = info.hasMidi  || plugin->takesMidiInput();
        hasAudioInput = info.hasAudio;
        hasMidiInput  = info.hasMidi;
    }

    void visitNodes (const VisitorFn& v) override
    {
        v (*this);

        if (input != nullptr)
            input->visitNodes (v);
    }

    Plugin::Ptr getPlugin() const override    { return plugin; }

    bool purgeSubNodes (bool keepAudio, bool keepMidi) override
    {
        const bool pluginWantsMidi = plugin->takesMidiInput() || plugin->isSynth();

        return (input != nullptr && input->purgeSubNodes (keepAudio, keepMidi || pluginWantsMidi))
                || pluginWantsMidi
                || ! plugin->noTail();
    }

    void prepareAudioNodeToPlay (const PlaybackInitialisationInfo& info) override
    {
        if (! hasInitialised)
        {
            hasInitialised = true;
            plugin->baseClassInitialise ({ TimePosition::fromSeconds (info.startTime), info.sampleRate, info.blockSizeSamples });
            latencySeconds = plugin->getLatencySeconds();

            if (input != nullptr)
                input->prepareAudioNodeToPlay (info);
        }
    }

    bool isReadyToRender() override
    {
        if (input != nullptr)
            return input->isReadyToRender();

        return true;
    }

    double getLatencySeconds() const noexcept
    {
        return latencySeconds;
    }

    void releaseAudioNodeResources() override
    {
        if (hasInitialised)
        {
            hasInitialised = false;

            if (input != nullptr)
                input->releaseAudioNodeResources();

            plugin->baseClassDeinitialise();
        }
    }

    void renderAdding (const AudioRenderContext& rc) override
    {
        if (plugin->isEnabled() && (rc.isRendering || (! plugin->isFrozen())))
        {
            callRenderOver (rc);
        }
        else
        {
            if (rc.didPlayheadJump())
                plugin->reset();

            if (input != nullptr)
                input->renderAdding (rc);
        }
    }

    void renderOver (const AudioRenderContext& rc) override
    {
        if (rc.didPlayheadJump())
            plugin->reset();

        if (plugin->isEnabled() && (rc.isRendering || (! plugin->isFrozen())))
        {
            if (latencySeconds > 0)
            {
                AudioRenderContext rc2 (rc);
                rc2.streamTime = rc2.streamTime + latencySeconds;

                input->renderOver (rc2);
                renderPlugin (rc2);
            }
            else
            {
                input->renderOver (rc);
                renderPlugin (rc);
            }
        }
        else
        {
            input->renderOver (rc);
        }
    }

    virtual void renderPlugin (const AudioRenderContext& rc)
    {
        SCOPED_REALTIME_CHECK

        if (applyAntiDenormalisationNoise)
            rc.addAntiDenormalisationNoise();

        if (! rc.isContiguousWithPreviousBlock())
            plugin->updateParameterStreams (TimePosition::fromSeconds (rc.getEditTime().editRange1.getStart()));

        plugin->applyToBufferWithAutomation (createPluginRenderContext (rc));
    }

    void prepareForNextBlock (const AudioRenderContext& rc) override
    {
        plugin->prepareForNextBlock (TimePosition::fromSeconds (rc.getEditTime().editRange1.getStart()));
        input->prepareForNextBlock (rc);
    }

protected:
    Plugin::Ptr plugin;
    std::unique_ptr<AudioNode> input;

    bool hasAudioInput = false, hasMidiInput = false, applyAntiDenormalisationNoise = false, hasInitialised = false;
    double latencySeconds = 0.0;

    PluginRenderContext createPluginRenderContext (const AudioRenderContext& rc)
    {
        return { rc.destBuffer, rc.destBufferChannels, rc.bufferStartSample, rc.bufferNumSamples,
                 rc.bufferForMidiMessages, rc.midiBufferOffset,
                 timeRangeFromSeconds (rc.getEditTime().editRange1),
                 rc.playhead.isPlaying(), rc.playhead.isUserDragging(), rc.isRendering,
                 false };
    }
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginAudioNode)
};

}} // namespace tracktion { inline namespace engine
