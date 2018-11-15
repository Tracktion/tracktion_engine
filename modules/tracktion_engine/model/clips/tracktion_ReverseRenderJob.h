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
class ReverseRenderJob   : public RenderManager::Job
{
public:
    /** Returns a job that will have been started to generate the Render described by the params.
        To be notified of when the job completes add yourself as a listener.
        This job will continue to run untill all references to it are deleted. Once this happens the
        render will be abandoned. If you delete yourself, make sure to unregister as a listener too.
    */
    static Ptr getOrCreateRenderJob (Engine& e,
                                     const juce::File& source,
                                     const juce::File& destination)
    {
        AudioFile targetFile (destination);

        if (auto ptr = e.getRenderManager().getRenderJobWithoutCreating (targetFile))
            return ptr;

        return *new ReverseRenderJob (e, source, targetFile);
    }

protected:
    //==============================================================================
    bool setUpRender() override                     { return true; }

    bool renderNextBlock() override
    {
        CRASH_TRACER

        juce::TemporaryFile tempFile (proxy.getFile(), juce::TemporaryFile::useHiddenFile);
        success = AudioFileUtils::reverse (source, tempFile.getFile(), progress, this);

        if (success)
        {
            tempFile.overwriteTargetFileWithTemporary();
        }
        else
        {
            // if we fail here just copy the source to the destination to avoid loops
            source.copyFileTo (proxy.getFile());
            jassertfalse;
        }

        // validates the AudioFile by giving it a sample rate etc.
        engine.getAudioFileManager().checkFileForChangesAsync (proxy);

        return true;
    }

    bool completeRender() override                  { return success; }

private:
    //==============================================================================
    ReverseRenderJob (Engine& e, const juce::File& src, const AudioFile& destination)
        : Job (e, destination), source (src)
    {
    }

    juce::File source;
    bool success = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverseRenderJob)
};

} // namespace tracktion_engine
