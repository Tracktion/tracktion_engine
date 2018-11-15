/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2018
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com
*/


namespace tracktion_engine
{

//==============================================================================
/**
*/
class WarpTimeRenderJob :   public RenderManager::Job
{
public:
    static Ptr getOrCreateRenderJob (AudioClipBase& clip,
                                     const juce::File& source,
                                     const juce::File& destination)
    {
        AudioFile targetFile (destination);

        if (auto ptr = clip.edit.engine.getRenderManager().getRenderJobWithoutCreating (targetFile))
            return ptr;

        return *new WarpTimeRenderJob (clip, source, targetFile);
    }

protected:
    bool setUpRender() override
    {
        return true;
    }

    bool renderNextBlock() override
    {
        CRASH_TRACER

        if (proxyInfo != nullptr)
        {
            AudioFileInfo sourceInfo (source.getInfo());

            // need to strip AIFF metadata to write to wav files
            if (sourceInfo.metadata.getValue ("MetaDataSource", "None") == "AIFF")
                sourceInfo.metadata.clear();

            AudioFileWriter writer (proxy, engine.getAudioFileFormatManager().getWavFormat(),
                                    sourceInfo.numChannels, sourceInfo.sampleRate,
                                    juce::jmax (16, sourceInfo.bitsPerSample),
                                    sourceInfo.metadata, 0);

            success = writer.isOpen() && proxyInfo->render (engine, source, writer, this, progress);
        }

        if (! shouldExit() && ! success)
        {
            // if we fail here just copy the source to the destination to avoid loops
            source.getFile().copyFileTo (proxy.getFile());
            jassertfalse;
        }

        return true;
    }

    bool completeRender() override                  { return true; }

private:
    WarpTimeRenderJob (AudioClipBase& clip, const juce::File& sourceFile, const AudioFile& destination)
        : Job (clip.edit.engine, destination), source (sourceFile)
    {
        auto tm = clip.getTimeStretchMode();
        proxyInfo = std::make_unique<AudioClipBase::ProxyRenderingInfo>();
        proxyInfo->audioSegmentList  = AudioSegmentList::create (clip);
        proxyInfo->clipTime          = { 0, clip.getWarpTimeManager().getWarpEndMarkerTime() };
        proxyInfo->speedRatio        = 1.0;
        proxyInfo->mode              = (tm != TimeStretcher::disabled && tm != TimeStretcher::melodyne)
                                         ? tm : TimeStretcher::defaultMode;
    }

    AudioFile source;
    bool success = false;
    std::unique_ptr<AudioClipBase::ProxyRenderingInfo> proxyInfo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WarpTimeRenderJob)
};

} // namespace tracktion_engine
