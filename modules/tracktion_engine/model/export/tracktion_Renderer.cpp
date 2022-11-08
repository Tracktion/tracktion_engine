/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

void Renderer::turnOffAllPlugins (Edit& edit)
{
    for (auto f : getAllPlugins (edit, true))
        while (! f->baseClassNeedsInitialising())
            f->baseClassDeinitialise();
}

namespace render_utils
{
    inline std::unique_ptr<Renderer::RenderTask> createRenderTask (Renderer::Parameters r, juce::String desc,
                                                                   std::atomic<float>* progressToUpdate,
                                                                   juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver* thumbnail)
    {
        auto tracksToDo = toTrackArray (*r.edit, r.tracksToDo);
        
        // Initialise playhead and continuity
        auto playHead = std::make_unique<tracktion::graph::PlayHead>();
        auto playHeadState = std::make_unique<tracktion::graph::PlayHeadState> (*playHead);
        auto processState = std::make_unique<ProcessState> (*playHeadState, r.edit->tempoSequence);

        CreateNodeParams cnp { *processState };
        cnp.sampleRate = r.sampleRateForAudio;
        cnp.blockSize = r.blockSizeForAudio;
        cnp.allowedClips = r.allowedClips.isEmpty() ? nullptr : &r.allowedClips;
        cnp.allowedTracks = r.tracksToDo.isZero() ? nullptr : &tracksToDo;
        cnp.forRendering = true;
        cnp.includePlugins = r.usePlugins;
        cnp.includeMasterPlugins = r.useMasterPlugins;
        cnp.addAntiDenormalisationNoise = r.addAntiDenormalisationNoise;
        cnp.includeBypassedPlugins = false;

        std::unique_ptr<tracktion::graph::Node> node;
        callBlocking ([&r, &node, &cnp] { node = createNodeForEdit (*r.edit, cnp); });

        if (! node)
            return {};
        
        return std::make_unique<Renderer::RenderTask> (desc, r,
                                                       std::move (node), std::move (playHead), std::move (playHeadState), std::move (processState),
                                                       progressToUpdate, thumbnail);
    }
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
            ditherers.getReference (i).process (buffer.getWritePointer (i), numSamples);
    }

    juce::Array<Ditherer> ditherers;
};

//==============================================================================
static void addAcidInfo (Edit& edit, Renderer::Parameters& r)
{
    if (r.destFile.hasFileExtension (".wav") && r.endAllowance == 0s)
    {
        auto& pitch = edit.pitchSequence.getPitchAt (r.time.getStart());
        auto& tempo = edit.tempoSequence.getTempoAt (r.time.getStart());
        auto& timeSig = edit.tempoSequence.getTimeSigAt (r.time.getStart());

        r.metadata.set (juce::WavAudioFormat::acidOneShot, "0");
        r.metadata.set (juce::WavAudioFormat::acidRootSet, "1");
        r.metadata.set (juce::WavAudioFormat::acidDiskBased, "1");
        r.metadata.set (juce::WavAudioFormat::acidizerFlag, "1");
        r.metadata.set (juce::WavAudioFormat::acidRootNote, juce::String (pitch.getPitch()));

        auto beats = tempo.getBpm() * (r.time.getLength().inSeconds() / 60);

        if (std::abs (beats - int (beats)) < 0.001)
        {
            r.metadata.set (juce::WavAudioFormat::acidStretch, "1");
            r.metadata.set (juce::WavAudioFormat::acidBeats, juce::String (juce::roundToInt (beats)));
            r.metadata.set (juce::WavAudioFormat::acidDenominator, juce::String (timeSig.denominator.get()));
            r.metadata.set (juce::WavAudioFormat::acidNumerator, juce::String (timeSig.numerator.get()));
            r.metadata.set (juce::WavAudioFormat::acidTempo, juce::String (tempo.getBpm()));
        }
        else
        {
            r.metadata.set (juce::WavAudioFormat::acidStretch, "0");
        }
    }
}

//==============================================================================
Renderer::RenderTask::RenderTask (const juce::String& taskDescription,
                                  const Renderer::Parameters& r,
                                  std::atomic<float>* progressToUpdate,
                                  juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver* source)
    : ThreadPoolJobWithProgress (taskDescription),
      params (r),
      progress (progressToUpdate == nullptr ? progressInternal : *progressToUpdate),
      sourceToUpdate (source)
{
    auto tracksToDo = toTrackArray (*r.edit, r.tracksToDo);
    
    // Initialise playhead and continuity
    playHead = std::make_unique<tracktion::graph::PlayHead>();
    playHeadState = std::make_unique<tracktion::graph::PlayHeadState> (*playHead);
    processState = std::make_unique<ProcessState> (*playHeadState, r.edit->tempoSequence);

    CreateNodeParams cnp { *processState };
    cnp.sampleRate = r.sampleRateForAudio;
    cnp.blockSize = r.blockSizeForAudio;
    cnp.allowedClips = r.allowedClips.isEmpty() ? nullptr : &r.allowedClips;
    cnp.allowedTracks = r.tracksToDo.isZero() ? nullptr : &tracksToDo;
    cnp.forRendering = true;
    cnp.includePlugins = r.usePlugins;
    cnp.includeMasterPlugins = r.useMasterPlugins;
    cnp.addAntiDenormalisationNoise = r.addAntiDenormalisationNoise;
    cnp.includeBypassedPlugins = false;

    callBlocking ([this, &r, &cnp] { graphNode = createNodeForEdit (*r.edit, cnp); });
}

Renderer::RenderTask::RenderTask (const juce::String& taskDescription,
                                  const Renderer::Parameters& rp,
                                  std::unique_ptr<tracktion::graph::Node> n,
                                  std::unique_ptr<tracktion::graph::PlayHead> playHead_,
                                  std::unique_ptr<tracktion::graph::PlayHeadState> playHeadState_,
                                  std::unique_ptr<ProcessState> processState_,
                                  std::atomic<float>* progressToUpdate,
                                  juce::AudioFormatWriter::ThreadedWriter::IncomingDataReceiver* source)
   : ThreadPoolJobWithProgress (taskDescription),
     params (rp),
     graphNode (std::move (n)), playHead (std::move (playHead_)), playHeadState (std::move (playHeadState_)), processState (std::move (processState_)),
     progress (progressToUpdate == nullptr ? progressInternal : *progressToUpdate),
     sourceToUpdate (source)
{
}

Renderer::RenderTask::~RenderTask()
{
}

juce::ThreadPoolJob::JobStatus Renderer::RenderTask::runJob()
{
    CRASH_TRACER
    juce::FloatVectorOperations::disableDenormalisedNumberSupport();

    if (params.createMidiFile)
        renderMidi (params);
    else if (! renderAudio (params))
        return jobNeedsRunningAgain;

    return jobHasFinished;
}

//==============================================================================
bool Renderer::RenderTask::performNormalisingAndTrimming (const Renderer::Parameters& target,
                                                          const Renderer::Parameters& intermediate)
{
    CRASH_TRACER

    if (target.trimSilenceAtEnds)
    {
        setJobName (TRANS("Trimming silence") + "...");
        progress = 0.94f;

        auto doneRange = AudioFileUtils::trimSilence (params.edit->engine, intermediate.destFile, -70.0f);

        if (doneRange.getLength() == 0)
        {
            errorMessage = TRANS("The rendered section was completely silent - no file was produced");
            return false;
        }

        AudioFileUtils::applyBWAVStartTime (intermediate.destFile,
                                            (SampleCount) tracktion::toSamples (intermediate.time.getStart(), intermediate.sampleRateForAudio)
                                               + doneRange.getStart());
    }

    if (target.shouldNormalise || target.shouldNormaliseByRMS)
        setJobName (TRANS("Normalising") + "...");

    std::unique_ptr<juce::AudioFormatReader> reader (AudioFileUtils::createReaderFor (params.edit->engine,
                                                                                      intermediate.destFile));

    if (reader == nullptr)
    {
        errorMessage = TRANS("Couldn't read intermediate file");
        return false;
    }

    AudioFileWriter writer (AudioFile (params.edit->engine, target.destFile),
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
        gain = juce::jlimit (0.0f, 100.0f, dbToGain (target.normaliseToLevelDb) / (intermediate.resultRMS + 2.0f / 32768.0f));
    else if (target.shouldNormalise)
        gain = juce::jlimit (0.0f, 100.0f, dbToGain (target.normaliseToLevelDb) * (1.0f / (intermediate.resultMagnitude * 1.005f + 2.0f / 32768.0f)));

    Ditherers ditherers ((int) reader->numChannels, target.bitDepth);

    const int blockSize = 16384;
    juce::AudioBuffer<float> tempBuffer ((int) reader->numChannels, blockSize + 256);

    for (SampleCount pos = 0; pos < reader->lengthInSamples;)
    {
        auto numLeft = static_cast<int> (reader->lengthInSamples - pos);
        auto samps = std::min (tempBuffer.getNumSamples(), std::min (blockSize, numLeft));

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
    
    if (! nodeRenderContext)
    {
        callBlocking ([&, this] { nodeRenderContext = std::make_unique<NodeRenderContext> (*this, r,
                                                                                           std::move (graphNode),
                                                                                           std::move (playHead),
                                                                                           std::move (playHeadState),
                                                                                           std::move (processState),
                                                                                           sourceToUpdate); });

        if (! nodeRenderContext->getStatus().wasOk())
        {
            errorMessage = nodeRenderContext->getStatus().getErrorMessage();
            return true;
        }
    }
    
    if (! nodeRenderContext->renderNextBlock (progress))
        return false;
    
    nodeRenderContext.reset();
    progress = 1.0f;
    
    return true;
}

void Renderer::RenderTask::flushAllPlugins (const Plugin::Array& plugins,
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
                    buffer.setSize (std::max (ep->getNumInputs(), ep->getNumOutputs()), samplesPerBlock);
                    buffer.clear();
                    auto channels = juce::AudioChannelSet::canonicalChannelSet (buffer.getNumChannels());

                    ep->applyToBuffer (PluginRenderContext (&buffer, channels, 0, samplesPerBlock,
                                                            nullptr, 0.0,
                                                            TimeRange(), false, false, true, true));

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
    for (auto af : plugins)
        if (auto ep = dynamic_cast<ExternalPlugin*> (af))
            if (ep->isEnabled())
                if (auto p = ep->getAudioPluginInstance())
                    p->setNonRealtime (! realtime);
}

//==============================================================================
bool Renderer::RenderTask::renderMidi (Renderer::Parameters& r)
{
    CRASH_TRACER
    errorMessage = NodeRenderContext::renderMidi (*this, r,
                                                  std::move (graphNode),
                                                  std::move (playHead),
                                                  std::move (playHeadState),
                                                  std::move (processState),
                                                  progress);
    return errorMessage.isEmpty();
}

bool Renderer::RenderTask::addMidiMetaDataAndWriteToFile (juce::File destFile, juce::MidiMessageSequence outputSequence, const TempoSequence& tempoSequence)
{
    outputSequence.updateMatchedPairs();

    if (outputSequence.getNumEvents() == 0)
        return false;

    juce::FileOutputStream out (destFile);

    if (out.openedOk())
    {
        juce::MidiMessageSequence midiTempoSequence;
        auto currentTempoPosition = createPosition (tempoSequence);

        for (int i = 0; i < tempoSequence.getNumTempos(); ++i)
        {
            auto ts = tempoSequence.getTempo (i);
            auto& matchingTimeSig = ts->getMatchingTimeSig();

            currentTempoPosition.set (ts->getStartTime());

            const double time = Edit::ticksPerQuarterNote * currentTempoPosition.getPPQTime();
            const double beatLengthMicrosecs = 60000000.0 / ts->getBpm();
            const double microsecondsPerQuarterNote = beatLengthMicrosecs * matchingTimeSig.denominator / 4.0;

            auto m = juce::MidiMessage::timeSignatureMetaEvent (matchingTimeSig.numerator,
                                                                matchingTimeSig.denominator);
            m.setTimeStamp (time);
            midiTempoSequence.addEvent (m);

            m = juce::MidiMessage::tempoMetaEvent (juce::roundToInt (microsecondsPerQuarterNote));
            m.setTimeStamp (time);
            midiTempoSequence.addEvent (m);
        }

        auto name = destFile.getFileNameWithoutExtension();
        if (name.startsWith ("."))
            name = name.fromFirstOccurrenceOf (".", false, false).upToLastOccurrenceOf ("_temp", false, false);

        midiTempoSequence.addEvent (juce::MidiMessage::textMetaEvent (3, name));
        outputSequence.addEvent (juce::MidiMessage::textMetaEvent (3, name));

        juce::MidiFile mf;
        mf.addTrack (midiTempoSequence);
        mf.addTrack (outputSequence);

        mf.setTicksPerQuarterNote (Edit::ticksPerQuarterNote);
        mf.writeTo (out);

        return true;
    }

    return false;
}


//==============================================================================
bool Renderer::renderToFile (const juce::String& taskDescription,
                             const juce::File& outputFile,
                             Edit& edit,
                             TimeRange range,
                             const juce::BigInteger& tracksToDo,
                             bool usePlugins,
                             juce::Array<Clip*> clips,
                             bool useThread)
{
    CRASH_TRACER
    auto& engine = edit.engine;
    const Edit::ScopedRenderStatus srs (edit, true);
    Track::Array tracks;

    for (auto bit = tracksToDo.findNextSetBit (0); bit != -1; bit = tracksToDo.findNextSetBit (bit + 1))
        tracks.add (getAllTracks (edit)[bit]);

    const FreezePointPlugin::ScopedTrackSoloIsolator isolator (edit, tracks);

    TransportControl::stopAllTransports (engine, false, true);
    turnOffAllPlugins (edit);

    if (tracksToDo.countNumberOfSetBits() > 0)
    {
        Parameters r (edit);
        r.destFile = outputFile;
        r.audioFormat = edit.engine.getAudioFileFormatManager().getDefaultFormat();
        r.bitDepth = 24;
        r.sampleRateForAudio = edit.engine.getDeviceManager().getSampleRate();
        r.blockSizeForAudio  = edit.engine.getDeviceManager().getBlockSize();
        r.time = range;
        r.addAntiDenormalisationNoise = EditPlaybackContext::shouldAddAntiDenormalisationNoise (engine);
        r.usePlugins = usePlugins;
        r.useMasterPlugins = usePlugins;
        r.tracksToDo = tracksToDo;
        r.allowedClips = clips;
        r.createMidiFile = outputFile.hasFileExtension (".mid");

        addAcidInfo (edit, r);
        
        if (auto task = render_utils::createRenderTask (r, taskDescription, nullptr, nullptr))
        {
            if (useThread)
            {
                engine.getUIBehaviour().runTaskWithProgressBar (*task);
            }
            else
            {
                while (task->runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
                {}
            }
        }
    }

    turnOffAllPlugins (edit);

    return outputFile.existsAsFile();
}

juce::File Renderer::renderToFile (const juce::String& taskDescription, const Parameters& r)
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
        
        if (auto task = render_utils::createRenderTask (r, taskDescription, nullptr, nullptr))
        {
            ui.runTaskWithProgressBar (*task);
            turnOffAllPlugins (*r.edit);

            if (r.destFile.existsAsFile())
            {
                if (task->errorMessage.isNotEmpty())
                {
                    r.destFile.deleteFile();
                    ui.showWarningMessage (task->errorMessage);
                    return {};
                }

                return r.destFile;
            }

            if (task->getCurrentTaskProgress() >= 0.9f && task->errorMessage.isNotEmpty())
                ui.showWarningMessage (task->errorMessage);
        }
        else
        {
            ui.showWarningMessage (TRANS("Couldn't render, as the selected region was empty"));
        }
    }

    return {};
}

ProjectItem::Ptr Renderer::renderToProjectItem (const juce::String& taskDescription, const Parameters& r)
{
    CRASH_TRACER

    auto proj = r.engine->getProjectManager().getProject (*r.edit);

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
        juce::String desc;
        desc << TRANS("Rendered from edit") << r.edit->getName().quoted() << " "
             << TRANS("On") << " " << juce::Time::getCurrentTime().toString (true, true);

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
Renderer::Statistics Renderer::measureStatistics (const juce::String& taskDescription, Edit& edit,
                                                  TimeRange range, const juce::BigInteger& tracksToDo,
                                                  int blockSizeForAudio, double sampleRateForAudio)
{
    CRASH_TRACER
    Statistics result;

    const Edit::ScopedRenderStatus srs (edit, true);
    TransportControl::stopAllTransports (edit.engine, false, true);

    turnOffAllPlugins (edit);

    if (tracksToDo.countNumberOfSetBits() > 0)
    {
        Parameters r (edit);
        r.audioFormat = edit.engine.getAudioFileFormatManager().getDefaultFormat();
        r.blockSizeForAudio = blockSizeForAudio;
        r.sampleRateForAudio = sampleRateForAudio;
        r.time = range;
        r.addAntiDenormalisationNoise = EditPlaybackContext::shouldAddAntiDenormalisationNoise (edit.engine);
        r.tracksToDo = tracksToDo;
        
        if (auto task = render_utils::createRenderTask (r, taskDescription, nullptr, nullptr))
        {
            edit.engine.getUIBehaviour().runTaskWithProgressBar (*task);

            result.peak          = task->params.resultMagnitude;
            result.average       = task->params.resultRMS;
            result.audioDuration = task->params.resultAudioDuration;
        }
    }

    turnOffAllPlugins (edit);

    return result;
}

bool Renderer::checkTargetFile (Engine& e, const juce::File& file)
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

}} // namespace tracktion { inline namespace engine
