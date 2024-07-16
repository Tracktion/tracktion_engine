/*
    ,--.                     ,--.     ,--.  ,--.
  ,-'  '-.,--.--.,--,--.,---.|  |,-.,-'  '-.`--' ,---. ,--,--,      Copyright 2024
  '-.  .-'|  .--' ,-.  | .--'|     /'-.  .-',--.| .-. ||      \   Tracktion Software
    |  |  |  |  \ '-'  \ `--.|  \  \  |  |  |  |' '-' '|  ||  |       Corporation
    `---' `--'   `--`--'`---'`--'`--' `---' `--' `---' `--''--'    www.tracktion.com

    Tracktion Engine uses a GPL/commercial licence - see LICENCE.md for details.
*/

namespace tracktion { inline namespace engine
{

//==============================================================================
juce::String getCmajorPatchPluginFormatName()
{
    return "Cmajor";
}

bool isCmajorPatchPluginFormat (const juce::String& formatName)
{
    return formatName == getCmajorPatchPluginFormatName();
}

bool isCmajorPatchPluginFormat (const juce::PluginDescription& pd)
{
    return isCmajorPatchPluginFormat (pd.pluginFormatName);
}

//==============================================================================
#if TRACKTION_ENABLE_CMAJOR

}} // namespace tracktion { inline namespace engine

#include "cmajor/helpers/cmaj_JUCEPluginFormat.h"
#include "cmajor/helpers/cmaj_FileBasedCacheDatabase.h"

namespace tracktion { inline namespace engine
{

std::string getCmajorVersion() { return cmaj::Library::getVersion(); }

cmaj::CacheDatabaseInterface::Ptr createCompilerCache (tracktion::Engine& engine)
{
    auto cacheFolder = engine.getTemporaryFileManager().getTempDirectory().getChildFile ("cmajor_patch_cache");

    if (cacheFolder.createDirectory())
        return choc::com::create<cmaj::FileBasedCacheDatabase> (cacheFolder.getFullPathName().toStdString(), 100u);

    return {};
}

std::unique_ptr<juce::AudioPluginFormat> createCmajorPatchPluginFormat (tracktion::Engine& engine)
{
    jassert (getCmajorPatchPluginFormatName() == cmaj::plugin::JUCEPluginFormat::pluginFormatName);

    auto onPatchChange = [&engine] (cmaj::plugin::SinglePatchJITPlugin& plugin)
    {
        for (auto& edit : engine.getActiveEdits().getEdits())
        {
            if (auto p = edit->getPluginCache().getPluginFor (plugin))
            {
                if (! p->isInitialising())
                {
                    if (auto ex = dynamic_cast<ExternalPlugin*> (p.get()))
                    {
                        plugin.suspendProcessing (true);
                        plugin.fillInPluginDescription (ex->desc);
                        ex->forceFullReinitialise();
                        plugin.suspendProcessing (false);
                    }
                    else
                    {
                        jassertfalse;
                    }
                }

                return;
            }
        }
    };

    return std::make_unique<cmaj::plugin::JUCEPluginFormat> (createCompilerCache (engine),
                                                             std::move (onPatchChange),
                                                             engine.getPropertyStorage().getApplicationVersion().toStdString());
}

juce::String getCmajorPatchCompileError (Plugin& p)
{
    if (auto ex = dynamic_cast<ExternalPlugin*> (&p))
        if (auto plugin = dynamic_cast<cmaj::plugin::SinglePatchJITPlugin*> (ex->getAudioPluginInstance()))
            if (plugin->isStatusMessageError)
                return plugin->statusMessage;

    return {};
}

#else

//==============================================================================
std::string getCmajorVersion() { return {}; }
juce::String getCmajorPatchCompileError (Plugin&) { return {}; }
std::unique_ptr<juce::AudioPluginFormat> createCmajorPatchPluginFormat (tracktion::Engine&)  { return {}; }

#endif

}} // namespace tracktion { inline namespace engine
