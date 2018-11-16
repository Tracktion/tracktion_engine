/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


void Renderer::turnOffAllPlugins (Edit& edit)
{
    for (auto f : getAllPlugins (edit, true))
        f->deinitialise();
}

static void resetFPU() noexcept
{
   #if JUCE_WINDOWS
    resetFP();
    #if JUCE_32BIT
     _control87 (_PC_64, _MCW_PC);
    #endif
   #endif
}

//==============================================================================
struct Ditherers
{
    Ditherers (int num, int bitDepth)
    {
        while (ditherers.size() < num)
            ditherers.add ({});

        for (auto& d : ditherers)
            d.reset (bitDepth);
    }

    void apply (juce::AudioBuffer<float>& buffer, int numSamples)
    {
        auto numChannels = buffer.getNumChannels();

        for (int i = 0; i < numChannels; ++i)
            ditherers.getReference(i).process (buffer.getWritePointer (i), numSamples);
    }

    juce::Array<Ditherer> ditherers;
};

//==============================================================================
static bool trackLoopsBackInto (const Array<Track*>& allTracks, AudioTrack& t, const BigInteger* tracksToCheck)
{
    for (int j = allTracks.size(); --j >= 0;)
        if (tracksToCheck == nullptr || (*tracksToCheck)[j])
            if (auto other = dynamic_cast<AudioTrack*> (allTracks.getUnchecked (j)))
                if (t.getOutput().feedsInto (other))
                    return true;

    return false;
}

static Plugin::Array findAllPlugins (AudioNode& node)
{
    Plugin::Array plugins, insideRacks;

    node.visitNodes ([&] (AudioNode& n)
                      {
                          if (auto af = n.getPlugin())
                              plugins.add (af);
                      });

    for (auto plugin : plugins)
        if (auto rack = dynamic_cast<RackInstance*> (plugin))
            if (auto type = rack->type)
                for (auto p : type->getPlugins())
                    insideRacks.addIfNotAlreadyThere (p);

    plugins.addArray (insideRacks);
    return plugins;
}

//==============================================================================
Renderer::RenderTask::RenderTask (const String& taskDescription, const Renderer::Parameters& rp, AudioNode* n)
   : ThreadPoolJobWithProgress (taskDescription),
     params (rp), node (n), progress (progressInternal)
{
}

Renderer::RenderTask::RenderTask (const String& taskDescription, const Renderer::Parameters& rp, AudioNode* n,
                                  std::atomic<float>& progressToUpdate, AudioFormatWriter::ThreadedWriter::IncomingDataReceiver* source)
   : ThreadPoolJobWithProgress (taskDescription),
     params (rp), node (n), progress (progressToUpdate), sourceToUpdate (source)
{
}

Renderer::RenderTask::~RenderTask()
{
}

ThreadPoolJob::JobStatus Renderer::RenderTask::runJob()
{
    CRASH_TRACER
    FloatVectorOperations::disableDenormalisedNumberSupport();

    if (params.createMidiFile)
        renderMidi (params);
    else if (! renderAudio (params))
        return jobNeedsRunningAgain;

    context = nullptr;
    return jobHasFinished;
}

//==============================================================================
/** Holds the state of an audio render procedure so it can be rendered in blocks. */
struct Renderer::RenderTask::RendererContext
{
    RendererContext (RenderTask& owner_, Renderer::Parameters& p, AudioNode* n,
                     AudioFormatWriter::ThreadedWriter::IncomingDataReceiver* sourceToUpdate_)
        : owner (owner_),
          r (p), originalParams (p),
          node (n),
          status (Result::ok()),
          ditherers (256, r.bitDepth),
          sourceToUpdate (sourceToUpdate_)
    {
        CRASH_TRACER
        TRACKTION_ASSERT_MESSAGE_THREAD
        jassert (r.engine != nullptr);

        if (r.edit->getTransport().isPlayContextActive())
        {
            jassertfalse;
            TRACKTION_LOG_ERROR("Rendering whilst attached to audio device");
        }

        if (r.shouldNormalise || r.trimSilenceAtEnds || r.shouldNormaliseByRMS)
        {
            needsToNormaliseAndTrim = true;

            r.audioFormat = r.engine->getAudioFileFormatManager().getFrozenFileFormat();

            intermediateFile = std::make_unique<TemporaryFile> (r.destFile.withFileExtension (r.audioFormat->getFileExtensions()[0]));
            r.destFile = intermediateFile->getFile();

            r.shouldNormalise = false;
            r.trimSilenceAtEnds = false;
            r.shouldNormaliseByRMS = false;
        }

        node->purgeSubNodes (true, false);

        numOutputChans = 2;

        {
            AudioNodeProperties info;
            node->getAudioNodeProperties (info);

            if (! info.hasAudio)
            {
                status = Result::fail (TRANS("Didn't find any audio to render"));
                return;
            }

            if (r.mustRenderInMono || (r.canRenderInMono && (info.numberOfChannels < 2)))
                numOutputChans = 1;
        }

        AudioFileUtils::addBWAVStartToMetadata (r.metadata, (int64) (r.time.getStart() * r.sampleRateForAudio));

        writer = std::make_unique<AudioFileWriter> (AudioFile (r.destFile),
                                                    r.audioFormat, numOutputChans, r.sampleRateForAudio,
                                                    r.bitDepth, r.metadata, r.quality);

        if (r.destFile != File() && ! writer->isOpen())
        {
            status = Result::fail (TRANS("Couldn't write to target file"));
            return;
        }

        thresholdForStopping = dbToGain (-70.0f);

        renderingBuffer.setSize (numOutputChans, r.blockSizeForAudio + 256);
        blockLength = r.blockSizeForAudio / r.sampleRateForAudio;

        // number of blank blocks to play before starting, to give pluginss time to warm up
        numPreRenderBlocks = (int) ((r.sampleRateForAudio / 2) / r.blockSizeForAudio + 1);

        // how long each block must take in real-time
        realTimePerBlock = (int) (blockLength * 1000.0 + 0.99);
        lastTime = Time::getMillisecondCounterHiRes();
        sleepCounter = 10;

        currentTempoPosition = std::make_unique<TempoSequencePosition> (r.edit->tempoSequence);

        peak = 0.0001f;
        rmsTotal = 0.0;
        rmsNumSamps = 0;
        numNonZeroSamps = 0;
        streamTime = r.time.getStart();

        precount = numPreRenderBlocks;
        streamTime -= precount * blockLength;

        {
            AudioNodeProperties info;
            node->getAudioNodeProperties (info);
        }

        plugins = findAllPlugins (*node);

        // Set the realtime property before preparing to play
        setAllPluginsRealtime (plugins, r.realTimeRender);

        {
            Array<AudioNode*> allNodes;
            allNodes.add (node);

            PlaybackInitialisationInfo info =
            {
                streamTime,
                r.sampleRateForAudio,
                r.blockSizeForAudio,
                &allNodes,
                localPlayhead
            };

            node->prepareAudioNodeToPlay (info);
        }

        flushAllPlugins (localPlayhead, plugins, r.sampleRateForAudio, r.blockSizeForAudio);

        samplesTrimmed = 0;
        hasStartedSavingToFile = ! r.trimSilenceAtEnds;

        localPlayhead.stop();
        localPlayhead.setPosition (streamTime);

        samplesToWrite = roundToInt ((r.time.getLength() + r.endAllowance) * r.sampleRateForAudio);

        if (sourceToUpdate != nullptr)
            sourceToUpdate->reset (numOutputChans, r.sampleRateForAudio, samplesToWrite);

        auto channels = AudioChannelSet::canonicalChannelSet (numOutputChans);

        rc.reset (new AudioRenderContext (localPlayhead, {}, &renderingBuffer, channels,
                                          0, r.blockSizeForAudio, &midiBuffer, 0,
                                          AudioRenderContext::playheadJumped, true));
    }

    ~RendererContext()
    {
        CRASH_TRACER
        r.resultMagnitude = owner.params.resultMagnitude = peak;
        r.resultRMS = owner.params.resultRMS = rmsNumSamps > 0 ? (float) (rmsTotal / rmsNumSamps) : 0.0f;
        r.resultAudioDuration = owner.params.resultAudioDuration = float (numNonZeroSamps / owner.params.sampleRateForAudio);

        localPlayhead.stop();
        setAllPluginsRealtime (plugins, true);

        if (writer != nullptr)
            writer->closeForWriting();

        if (node != nullptr)
            callBlocking ([this] { node->releaseAudioNodeResources(); });

        if (needsToNormaliseAndTrim)
            owner.performNormalisingAndTrimming (originalParams, r);
    }

    RenderTask& owner;
    Renderer::Parameters r, originalParams;
    bool needsToNormaliseAndTrim = false;
    AudioNode* node = nullptr;
    int numOutputChans = 0;
    std::unique_ptr<AudioFileWriter> writer;
    Plugin::Array plugins;
    Result status;

    PlayHead localPlayhead;
    Ditherers ditherers;
    juce::AudioBuffer<float> renderingBuffer;
    MidiMessageArray midiBuffer;
    std::unique_ptr<AudioRenderContext> rc;

    float thresholdForStopping = 0;
    double blockLength = 0;
    int numPreRenderBlocks = 0;
    int realTimePerBlock = 0;

    double lastTime = 0;
    static const int sleepCounterMax = 100;
    int sleepCounter = 0;

    std::unique_ptr<TempoSequencePosition> currentTempoPosition;
    float peak = 0;
    double rmsTotal = 0;
    int64 rmsNumSamps = 0;
    int64 numNonZeroSamps = 0;
    int precount = 0;
    double streamTime = 0;

    int64 samplesTrimmed = 0;
    bool hasStartedSavingToFile = 0;
    int64 samplesToWrite = 0;

    std::unique_ptr<TemporaryFile> intermediateFile;
    AudioFormatWriter::ThreadedWriter::IncomingDataReceiver* sourceToUpdate;

    /** Returns the opening status of the render.
        If somthing went wrong during set-up this will contain the error message to display.
    */
    const Result& getStatus() const noexcept                { return status; }

    /** Renders the next block of audio. Returns true when finished, false if it needs to run again. */
    bool renderNextBlock (std::atomic<float>& progressToUpdate)
    {
        CRASH_TRACER
        jassert (! r.edit->getTransport().isPlayContextActive());

        if (--sleepCounter <= 0)
        {
            sleepCounter = sleepCounterMax;
            Thread::sleep (1);
        }

        if (owner.shouldExit())
        {
            node->releaseAudioNodeResources();

            writer->closeForWriting();
            r.destFile.deleteFile();

            localPlayhead.stop();
            setAllPluginsRealtime (plugins, true);

            return true;
        }

        auto blockEnd = streamTime + blockLength;

        if (precount > 0)
            blockEnd = jmin (r.time.getStart(), blockEnd);

        if (precount > numPreRenderBlocks / 2)
            localPlayhead.setPosition (streamTime);
        else if (precount == numPreRenderBlocks / 2)
            localPlayhead.playLockedToEngine ({ streamTime, Edit::maximumLength });

        if (precount == 0)
        {
            streamTime = r.time.getStart();
            localPlayhead.playLockedToEngine ({ streamTime, Edit::maximumLength });
        }

        if (r.realTimeRender)
        {
            auto timeNow = Time::getMillisecondCounterHiRes();
            auto timeToWait = (int) (realTimePerBlock - (timeNow - lastTime));
            lastTime = timeNow;

            if (timeToWait > 0)
                Thread::sleep (timeToWait);
        }

        currentTempoPosition->setTime (streamTime);

        midiBuffer.clear();
        resetFPU();

        rc->streamTime = { streamTime, blockEnd };

        r.edit->updateModifierTimers (localPlayhead, rc->streamTime, r.blockSizeForAudio);
        node->prepareForNextBlock (*rc);

        // wait for any nodes to render their sources or proxies
        while (! (node->isReadyToRender() || owner.shouldExit()))
            return false;

        node->renderOver (*rc);

        rc->continuity = AudioRenderContext::contiguous;

        streamTime = blockEnd;

        if (precount <= 0)
        {
            CRASH_TRACER

            if (r.ditheringEnabled && r.bitDepth < 32)
                ditherers.apply (renderingBuffer, r.blockSizeForAudio);

            auto mag = renderingBuffer.getMagnitude (0, r.blockSizeForAudio);
            peak = jmax (peak, mag);

            if (! hasStartedSavingToFile)
                hasStartedSavingToFile = (mag > 0.0f);

            for (int i = renderingBuffer.getNumChannels(); --i >= 0;)
            {
                rmsTotal += renderingBuffer.getRMSLevel (i, 0, r.blockSizeForAudio);
                ++rmsNumSamps;
            }

            for (int i = renderingBuffer.getNumSamples(); --i >= 0;)
            {
                if (renderingBuffer.getMagnitude (i, 1) > 0.0001)
                    numNonZeroSamps++;
            }

            if (! hasStartedSavingToFile)
                samplesTrimmed += r.blockSizeForAudio;

            int numSamplesDone = (int) jmin (samplesToWrite, (int64) r.blockSizeForAudio);
            samplesToWrite -= numSamplesDone;

            if (sourceToUpdate != nullptr && numSamplesDone > 0)
            {
                float* chans[32];

                for (int i = 0; i < numOutputChans; ++i)
                    chans[i] = renderingBuffer.getWritePointer (i, 0);

                const juce::AudioBuffer<float> buffer (chans, numOutputChans, numSamplesDone);
                auto samplesDone = int64 ((streamTime - r.time.getStart()) * r.sampleRateForAudio);
                sourceToUpdate->addBlock (samplesDone, buffer, 0, numSamplesDone);
            }

            // NB buffer gets trashed by this call
            if (numSamplesDone > 0 && hasStartedSavingToFile
                 && writer->isOpen()
                 && ! writer->appendBuffer (renderingBuffer, numSamplesDone))
                return true;
        }
        else
        {
            // for the pre-count blocks, sleep to give things a chance to get going
            Thread::sleep ((int) (blockLength * 1000));
        }

        if (streamTime > r.time.getEnd() + r.endAllowance
            || (streamTime > r.time.getEnd()
                && renderingBuffer.getMagnitude (0, r.blockSizeForAudio) <= thresholdForStopping))
            return true;

        auto prog = (float) ((streamTime - r.time.getStart()) / r.time.getLength());

        if (needsToNormaliseAndTrim)
            prog *= 0.9f;

        progressToUpdate = jlimit (0.0f, 1.0f, prog);
        --precount;

        return false;
    }
};

//==============================================================================
bool Renderer::RenderTask::performNormalisingAndTrimming (const Renderer::Parameters& target,
                                                          const Renderer::Parameters& intermediate)
{
    CRASH_TRACER

    if (target.trimSilenceAtEnds)
    {
        setJobName (TRANS("Trimming silence") + "...");
        progress = 0.94f;

        auto doneRange = AudioFileUtils::trimSilence (intermediate.destFile, -70.0f);

        if (doneRange.getLength() == 0)
        {
            errorMessage = TRANS("The rendered section was completely silent - no file was produced");
            return false;
        }

        AudioFileUtils::applyBWAVStartTime (intermediate.destFile,
                                            (int64) (intermediate.time.getStart() * intermediate.sampleRateForAudio)
                                               + doneRange.getStart());
    }

    if (target.shouldNormalise || target.shouldNormaliseByRMS)
        setJobName (TRANS("Normalising") + "...");

    std::unique_ptr<AudioFormatReader> reader (AudioFileUtils::createReaderFor (intermediate.destFile));

    if (reader == nullptr)
    {
        errorMessage = TRANS("Couldn't read intermediate file");
        return false;
    }

    AudioFileWriter writer (AudioFile (target.destFile),
                            target.audioFormat, (int) reader->numChannels, target.sampleRateForAudio,
                            target.bitDepth, target.metadata, target.quality);

    if (! writer.isOpen())
    {
        errorMessage = TRANS("Couldn't write to target file");
        return false;
    }

    progress = 0.96f;
    float gain = 1.0f;

    if (target.shouldNormaliseByRMS)
        gain = jlimit (0.0f, 100.0f, dbToGain (target.normaliseToLevelDb) / (intermediate.resultRMS + 2.0f / 32768.0f));
    else if (target.shouldNormalise)
        gain = jlimit (0.0f, 100.0f, dbToGain (target.normaliseToLevelDb) * (1.0f / (intermediate.resultMagnitude * 1.005f + 2.0f / 32768.0f)));

    Ditherers ditherers ((int) reader->numChannels, target.bitDepth);

    const int blockSize = 16384;
    juce::AudioBuffer<float> tempBuffer ((int) reader->numChannels, blockSize + 256);

    for (int64 pos = 0; pos < reader->lengthInSamples;)
    {
        auto numLeft = static_cast<int> (reader->lengthInSamples - pos);
        auto samps = jmin (tempBuffer.getNumSamples(), blockSize, numLeft);

        reader->read (&tempBuffer, 0, samps, pos, true, reader->numChannels > 1);

        tempBuffer.applyGain (0, samps, gain);

        if (target.ditheringEnabled && target.bitDepth < 32)
            ditherers.apply (tempBuffer, samps);

        writer.appendBuffer (tempBuffer, samps);
        pos += samps;
    }

    return true;
}

//==============================================================================
bool Renderer::RenderTask::renderAudio (Renderer::Parameters& r)
{
    CRASH_TRACER

    if (context == nullptr)
    {
        callBlocking ([&, this] { context = new RendererContext (*this, r, node, sourceToUpdate); });

        if (! context->getStatus().wasOk())
        {
            errorMessage = context->getStatus().getErrorMessage();
            return true;
        }
    }

    if (! context->renderNextBlock (progress))
        return false;

    context = nullptr;
    progress = 1.0f;
    Thread::sleep (150); // no idea why this is here..

    return true;
}

void Renderer::RenderTask::flushAllPlugins (PlayHead& playhead, const Plugin::Array& plugins,
                                            double sampleRate, int samplesPerBlock)
{
    CRASH_TRACER
    juce::AudioBuffer<float> buffer (1, samplesPerBlock);

    for (int i = 0; i < plugins.size(); ++i)
    {
        if (auto ep = dynamic_cast<ExternalPlugin*> (plugins.getUnchecked (i).get()))
        {
            if (! ep->baseClassNeedsInitialising() && ep->isEnabled())
            {
                ep->reset();

                if (ep->getNumInputs() == 0 && ep->getNumOutputs() == 0)
                    continue;

                auto blockLength = samplesPerBlock / (double) sampleRate;
                auto blocks = (int) (20 / blockLength + 1);

                for (int j = 0; j < blocks; j++)
                {
                    buffer.setSize (jmax (ep->getNumInputs(), ep->getNumOutputs()), samplesPerBlock);
                    buffer.clear();
                    const AudioChannelSet channels = AudioChannelSet::canonicalChannelSet (buffer.getNumChannels());

                    ep->applyToBuffer (AudioRenderContext (playhead,
                                                           { -1.0, samplesPerBlock / sampleRate - 1.0 },
                                                           &buffer, channels, 0, samplesPerBlock,
                                                           nullptr, 0, true, true));

                    if (isAudioDataAlmostSilent (buffer.getReadPointer (0), samplesPerBlock))
                        break;
                }
            }
        }
    }
}

void Renderer::RenderTask::setAllPluginsRealtime (const Plugin::Array& plugins, bool realtime)
{
    CRASH_TRACER
    for (auto* af : plugins)
        if (auto ep = dynamic_cast<ExternalPlugin*> (af))
            if (ep->isEnabled())
                if (auto p = ep->getAudioPluginInstance())
                    p->setNonRealtime (! realtime);
}

//==============================================================================
bool Renderer::RenderTask::renderMidi (Renderer::Parameters& r)
{
    CRASH_TRACER
    node->purgeSubNodes (false, true);

    {
        AudioNodeProperties info;
        node->getAudioNodeProperties (info);

        if (! info.hasMidi)
        {
            errorMessage = TRANS("No MIDI was found within the selected region");
            return false;
        }
    }

    MidiMessageSequence outputSequence;
    MidiMessageArray midiBuffer;

    const int sampleRate = 44100; // use any old sample rate as this shouldn't matter to the midi nodes
    const int samplesPerBlock = 256;
    const double blockLength = samplesPerBlock / (double)sampleRate;
    double streamTime = r.time.getStart();

    PlayHead localPlayhead;

    {
        Array<AudioNode*> allNodes;
        allNodes.add (node.get());

        PlaybackInitialisationInfo info =
        {
            streamTime,
            sampleRate,
            samplesPerBlock,
            &allNodes,
            localPlayhead
        };

        node->prepareAudioNodeToPlay (info);
    }

    localPlayhead.stop();
    localPlayhead.setPosition (streamTime);
    localPlayhead.playLockedToEngine ({ streamTime, Edit::maximumLength });

    {
        TempoSequencePosition currentTempoPosition (r.edit->tempoSequence);

        AudioRenderContext rc (localPlayhead, {},
                               nullptr, AudioChannelSet::disabled(), 0, samplesPerBlock,
                               &midiBuffer, 0,
                               AudioRenderContext::playheadJumped, true);

        for (;;)
        {
            CRASH_TRACER

            if (shouldExit())
            {
                node->releaseAudioNodeResources();
                return false;
            }

            if (streamTime > r.time.getEnd())
                break;

            const double blockEnd = streamTime + blockLength;

            currentTempoPosition.setTime (streamTime);
            resetFPU();

            rc.streamTime = { streamTime, blockEnd };

            node->prepareForNextBlock (rc);
            node->renderOver (rc);

            rc.continuity = AudioRenderContext::contiguous;

            for (auto& m : midiBuffer)
            {
                TempoSequencePosition eventPos (currentTempoPosition);
                eventPos.setTime (m.getTimeStamp() + streamTime - r.time.getStart());

                outputSequence.addEvent (MidiMessage (m, Edit::ticksPerQuarterNote * eventPos.getPPQTime()));
            }

            streamTime = blockEnd;

            progress = jlimit (0.0f, 1.0f, (float) ((streamTime - r.time.getStart()) / r.time.getLength()));
        }
    }

    node->releaseAudioNodeResources();
    localPlayhead.stop();

    outputSequence.updateMatchedPairs();

    if (outputSequence.getNumEvents() > 0)
    {
        FileOutputStream out (r.destFile);

        if (out.openedOk())
        {
            MidiMessageSequence tempoSequence;
            TempoSequencePosition currentTempoPosition (r.edit->tempoSequence);

            for (int i = 0; i < r.edit->tempoSequence.getNumTempos(); ++i)
            {
                auto ts = r.edit->tempoSequence.getTempo (i);
                auto& matchingTimeSig = ts->getMatchingTimeSig();

                currentTempoPosition.setTime (ts->getStartTime());

                const double time = Edit::ticksPerQuarterNote * currentTempoPosition.getPPQTime();
                const double beatLengthMicrosecs = 60000000.0 / ts->getBpm();
                const double microsecondsPerQuarterNote = beatLengthMicrosecs * matchingTimeSig.denominator / 4.0;

                MidiMessage m (MidiMessage::timeSignatureMetaEvent (matchingTimeSig.numerator,
                                                                    matchingTimeSig.denominator));
                m.setTimeStamp (time);
                tempoSequence.addEvent (m);

                m = MidiMessage::tempoMetaEvent (roundToInt (microsecondsPerQuarterNote));
                m.setTimeStamp (time);
                tempoSequence.addEvent (m);
            }

            MidiFile mf;
            mf.addTrack (tempoSequence);
            mf.addTrack (outputSequence);

            mf.setTicksPerQuarterNote (Edit::ticksPerQuarterNote);
            mf.writeTo (out);

            return true;
        }
    }

    return false;
}

//==============================================================================
static AudioNode* createRenderingNodeFromEdit (Edit& edit,
                                               const CreateAudioNodeParams& params,
                                               bool includeMasterPlugins)
{
    CRASH_TRACER
    MixerAudioNode* mixer = nullptr;

    const auto allTracks = getAllTracks (edit);

    Array<Track*> sidechainSourceTracks;
    Array<AudioTrack*> tracksToBeRendered;

    jassert (params.forRendering);

    for (int i = 0; i < allTracks.size(); ++i)
    {
        if (params.allowedTracks == nullptr || (*params.allowedTracks)[i])
        {
            auto track = allTracks.getUnchecked (i);

            if (! track->isProcessing (true) || track->isPartOfSubmix())
                continue;

            if (auto at = dynamic_cast<AudioTrack*> (track))
            {
                if (! trackLoopsBackInto (allTracks, *at, params.allowedTracks))
                {
                    if (mixer == nullptr)
                        mixer = new MixerAudioNode (true, edit.engine.getEngineBehaviour().getNumberOfCPUsToUseForAudio() > 1);

                    auto trackNode = at->createAudioNode (params);

                    trackNode = new TrackMutingAudioNode (*at, trackNode, false);
                    mixer->addInput (trackNode);

                    // find an tracks required to feed sidechains
                    Array<AudioTrack*> todo;
                    todo.add (at);

                    while (todo.size() > 0)
                    {
                        auto first = todo.getFirst();
                        auto srcTracks = first->findSidechainSourceTracks();
                        tracksToBeRendered.add (first);

                        for (auto* t : srcTracks)
                            sidechainSourceTracks.addIfNotAlreadyThere (t);

                        todo.addArray (first->getInputTracks());
                        todo.remove (0);
                    }
                }
            }
            else if (auto ft = dynamic_cast<FolderTrack*> (track))
            {
                if (auto n = ft->createAudioNode (params))
                {
                    if (mixer == nullptr)
                        mixer = new MixerAudioNode (true, edit.engine.getEngineBehaviour().getNumberOfCPUsToUseForAudio() > 1);

                    mixer->addInput (n);

                    // find an tracks required to feed sidechains
                    auto subTracks = ft->getAllAudioSubTracks (true);
                    tracksToBeRendered.addArray (subTracks);

                    for (auto subTrack : subTracks)
                        for (auto t : subTrack->findSidechainSourceTracks())
                            sidechainSourceTracks.addIfNotAlreadyThere (t);
                }
            }
        }
    }

    // create audio nodes for an sidechain source tracks that aren't already being rendered
    for (auto t : sidechainSourceTracks)
    {
        if (auto at = dynamic_cast<AudioTrack*> (t))
        {
            if (! tracksToBeRendered.contains (at))
            {
                CreateAudioNodeParams p (params);
                p.allowedTracks = nullptr;
                p.allowedClips = nullptr;

                if (auto* n = at->createAudioNode (p))
                {
                    n = new MuteAudioNode (n);
                    mixer->addInput (n);
                }
            }
        }
    }

    AudioNode* finalNode = mixer;

    if (includeMasterPlugins && finalNode != nullptr)
    {
        // add any master plugins..
        finalNode = edit.getMasterPluginList().createAudioNode (finalNode, params.addAntiDenormalisationNoise);
        finalNode = edit.getMasterVolumePlugin()->createAudioNode (finalNode, false);
        finalNode = FadeInOutAudioNode::createForEdit (edit, finalNode);
    }

    return finalNode;
}

AudioNode* Renderer::createRenderingAudioNode (const Parameters& r)
{
    CreateAudioNodeParams cnp;
    cnp.allowedClips = r.allowedClips.isEmpty() ? nullptr : &r.allowedClips;
    cnp.allowedTracks = &r.tracksToDo;
    cnp.forRendering = true;
    cnp.includePlugins = r.usePlugins;
    cnp.addAntiDenormalisationNoise = r.addAntiDenormalisationNoise;

    return createRenderingNodeFromEdit (*r.edit, cnp, r.useMasterPlugins);
}

//==============================================================================
bool Renderer::renderToFile (const String& taskDescription,
                             const juce::File& outputFile,
                             Edit& edit,
                             EditTimeRange range,
                             const BigInteger& tracksToDo,
                             bool usePlugins,
                             Array<Clip*> clips,
                             bool useThread)
{
    CRASH_TRACER
    auto& engine = edit.engine;
    const Edit::ScopedRenderStatus srs (edit);
    Track::Array tracks;

    for (auto bit = tracksToDo.findNextSetBit (0); bit != -1; bit = tracksToDo.findNextSetBit (bit + 1))
        tracks.add (getAllTracks (edit)[bit]);

    const FreezePointPlugin::ScopedTrackSoloIsolator isolator (edit, tracks);

    TransportControl::stopAllTransports (engine, false, true);
    turnOffAllPlugins (edit);

    if (tracksToDo.countNumberOfSetBits() > 0)
    {
        CreateAudioNodeParams cnp;
        cnp.allowedTracks = &tracksToDo;
        cnp.forRendering = true;
        cnp.includePlugins = usePlugins;
        cnp.addAntiDenormalisationNoise = EditPlaybackContext::shouldAddAntiDenormalisationNoise (engine);

        if (auto* node = createRenderingNodeFromEdit (edit, cnp, true))
        {
            Parameters r (edit);
            r.destFile = outputFile;
            r.audioFormat = Engine::getInstance().getAudioFileFormatManager().getDefaultFormat();
            r.bitDepth = 24;
            r.sampleRateForAudio = edit.engine.getDeviceManager().getSampleRate();
            r.blockSizeForAudio  = edit.engine.getDeviceManager().getBlockSize();
            r.time = range;
            r.addAntiDenormalisationNoise = EditPlaybackContext::shouldAddAntiDenormalisationNoise (engine);
            r.usePlugins = usePlugins;
            r.allowedClips = clips;
            r.createMidiFile = outputFile.hasFileExtension (".mid");

            auto& pitch = edit.pitchSequence.getPitchAt (range.start);
            auto& tempo = edit.tempoSequence.getTempoAt (range.start);
            auto& timeSig = edit.tempoSequence.getTimeSigAt (range.start);

            r.metadata.set (WavAudioFormat::acidOneShot, "0");
            r.metadata.set (WavAudioFormat::acidRootSet, "1");
            r.metadata.set (WavAudioFormat::acidStretch, "1");
            r.metadata.set (WavAudioFormat::acidDiskBased, "1");
            r.metadata.set (WavAudioFormat::acidizerFlag, "1");
            r.metadata.set (WavAudioFormat::acidRootNote, String (pitch.getPitch()));
            r.metadata.set (WavAudioFormat::acidBeats, String (tempo.getBpm() * (range.getLength() / 60)));
            r.metadata.set (WavAudioFormat::acidDenominator, String (timeSig.denominator.get()));
            r.metadata.set (WavAudioFormat::acidNumerator, String (timeSig.numerator.get()));
            r.metadata.set (WavAudioFormat::acidTempo, String (tempo.getBpm()));

            RenderTask task (taskDescription, r, node);

            if (useThread)
            {
                engine.getUIBehaviour().runTaskWithProgressBar (task);
            }
            else
            {
                while (task.runJob() == ThreadPoolJob::jobNeedsRunningAgain)
                {}
            }
        }
    }

    turnOffAllPlugins (edit);

    return outputFile.existsAsFile();
}

juce::File Renderer::renderToFile (const String& taskDescription, const Parameters& r)
{
    CRASH_TRACER

    jassert (r.sampleRateForAudio > 7000);
    jassert (r.edit != nullptr);
    jassert (r.engine != nullptr);

    TransportControl::stopAllTransports (*r.engine, false, true);

    turnOffAllPlugins (*r.edit);

    if (r.tracksToDo.countNumberOfSetBits() > 0
         && r.destFile.hasWriteAccess()
         && ! r.destFile.isDirectory())
    {
        auto& ui = r.edit->engine.getUIBehaviour();

        if (auto* node = createRenderingAudioNode (r))
        {
            RenderTask task (taskDescription, r, node);

            ui.runTaskWithProgressBar (task);

            turnOffAllPlugins (*r.edit);

            if (r.destFile.existsAsFile())
            {
                if (task.errorMessage.isNotEmpty())
                {
                    r.destFile.deleteFile();
                    ui.showWarningMessage (task.errorMessage);
                    return {};
                }

                return r.destFile;
            }

            if (task.getCurrentTaskProgress() >= 0.9f && task.errorMessage.isNotEmpty())
                ui.showWarningMessage (task.errorMessage);
        }
        else
        {
            ui.showWarningMessage (TRANS("Couldn't render, as the selected region was empty"));
        }
    }

    return {};
}

ProjectItem::Ptr Renderer::renderToProjectItem (const String& taskDescription, const Parameters& r)
{
    CRASH_TRACER

    auto proj = ProjectManager::getInstance()->getProject (*r.edit);

    if (proj == nullptr)
    {
        jassertfalse;  // can't use this option if the edit isn't a member of a project
        return {};
    }

    auto& ui = r.edit->engine.getUIBehaviour();

    if (proj->isReadOnly())
    {
        ui.showWarningMessage (TRANS("Couldn't add the new file to the project (because this project is read-only)"));
        return {};
    }

    if (r.category == ProjectItem::Category::none)
    {
        // This test comes from some very old code which leaks the file and doesn't add the project item
        // if the category is 'none'... I've left the check in here but think it may be redundant - if you
        // hit this assertion, please figure out why it's happening and refactor; otherwise, if you see this
        // comment in a couple of years and the assertion never failed, please remove this whole check.
        jassertfalse;
        return {};
    }

    auto renderedFile = renderToFile (taskDescription, r);

    if (renderedFile.existsAsFile())
    {
        String desc;
        desc << TRANS("Rendered from edit") << r.edit->getName().quoted() << " "
             << TRANS("On") << " " << Time::getCurrentTime().toString (true, true);

        return proj->createNewItem (renderedFile,
                                    r.createMidiFile ? ProjectItem::midiItemType()
                                                     : ProjectItem::waveItemType(),
                                    renderedFile.getFileNameWithoutExtension().trim(),
                                    desc,
                                    r.category,
                                    true);
    }

    return {};
}

//==============================================================================
Renderer::Statistics Renderer::measureStatistics (const String& taskDescription, Edit& edit,
                                                  EditTimeRange range, const BigInteger& tracksToDo,
                                                  int blockSizeForAudio)
{
    CRASH_TRACER
    Statistics result;

    const Edit::ScopedRenderStatus srs (edit);

    TransportControl::stopAllTransports (edit.engine, false, true);

    turnOffAllPlugins (edit);

    if (tracksToDo.countNumberOfSetBits() > 0)
    {
        CreateAudioNodeParams cnp;
        cnp.allowedTracks = &tracksToDo;
        cnp.forRendering = true;
        cnp.includePlugins = true;
        cnp.addAntiDenormalisationNoise = EditPlaybackContext::shouldAddAntiDenormalisationNoise (edit.engine);

        if (auto* node = createRenderingNodeFromEdit (edit, cnp, true))
        {
            Parameters r (edit);

            r.audioFormat = Engine::getInstance().getAudioFileFormatManager().getDefaultFormat();
            r.blockSizeForAudio = blockSizeForAudio;
            r.time = range;
            r.addAntiDenormalisationNoise = cnp.addAntiDenormalisationNoise;

            RenderTask task (taskDescription, r, node);

            edit.engine.getUIBehaviour().runTaskWithProgressBar (task);

            result.peak          = task.params.resultMagnitude;
            result.average       = task.params.resultRMS;
            result.audioDuration = task.params.resultAudioDuration;
        }
    }

    turnOffAllPlugins (edit);

    return result;
}

bool Renderer::checkTargetFile (Engine& e, const File& file)
{
    auto& ui = e.getUIBehaviour();

    if (file.isDirectory() || ! file.hasWriteAccess())
    {
        ui.showWarningAlert (TRANS("Couldn't render"),
                             TRANS("Couldn't write to this file - check that it's not read-only and that you have permission to access it"));
        return false;
    }

    if (! file.getParentDirectory().createDirectory())
    {
        ui.showWarningAlert (TRANS("Rendering"),
                             TRANS("Couldn't render - couldn't create the directory specified"));
        return false;
    }

    if (file.exists())
    {
        if (! ui.showOkCancelAlertBox (TRANS("Rendering"),
                                       TRANS("The file\n\nXZZX\n\nalready exists - are you sure you want to overwrite it?")
                                          .replace ("XZZX", file.getFullPathName()),
                                       TRANS("Overwrite")))
            return false;
    }

    if (! (file.deleteFile() && file.hasWriteAccess()))
    {
        ui.showWarningAlert (TRANS("Rendering"),
                             TRANS("Couldn't render - the file chosen didn't have write permission"));
        return false;
    }

    return true;
}
